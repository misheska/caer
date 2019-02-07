#include <libcaer/events/frame.h>

#include "dv-sdk/mainloop.h"

#include <libcaer/frame_utils.h>

struct FrameEnhancer_state {
	bool doDemosaic;
	enum caer_frame_utils_demosaic_types demosaicType;
	bool doContrast;
	enum caer_frame_utils_contrast_types contrastType;
};

typedef struct FrameEnhancer_state *FrameEnhancerState;

static void caerFrameEnhancerConfigInit(dvConfigNode moduleNode);
static bool caerFrameEnhancerInit(dvModuleData moduleData);
static void caerFrameEnhancerRun(dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out);
static void caerFrameEnhancerConfig(dvModuleData moduleData);
static void caerFrameEnhancerExit(dvModuleData moduleData);

static const struct dvModuleFunctionsS FrameEnhancerFunctions = {.moduleConfigInit = &caerFrameEnhancerConfigInit,
	.moduleInit                                                                       = &caerFrameEnhancerInit,
	.moduleRun                                                                        = &caerFrameEnhancerRun,
	.moduleConfig                                                                     = &caerFrameEnhancerConfig,
	.moduleExit                                                                       = &caerFrameEnhancerExit,
	.moduleReset                                                                      = NULL};

static const struct caer_event_stream_in FrameEnhancerInputs[] = {{.type = FRAME_EVENT, .number = 1, .readOnly = true}};
// The output frame here is a _different_ frame than the above input!
static const struct caer_event_stream_out FrameEnhancerOutputs[] = {{.type = FRAME_EVENT}};

static const struct dvModuleInfoS FrameEnhancerInfo = {
	.version = 1,
	.name    = "FrameEnhancer",
	.description
	= "Applies contrast enhancement techniques to frames, or interpolates colors to get an RGB frame (demoisaicing).",
	.type              = DV_MODULE_PROCESSOR,
	.memSize           = sizeof(struct FrameEnhancer_state),
	.functions         = &FrameEnhancerFunctions,
	.inputStreams      = FrameEnhancerInputs,
	.inputStreamsSize  = CAER_EVENT_STREAM_IN_SIZE(FrameEnhancerInputs),
	.outputStreams     = FrameEnhancerOutputs,
	.outputStreamsSize = CAER_EVENT_STREAM_OUT_SIZE(FrameEnhancerOutputs),
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&FrameEnhancerInfo);
}

static void caerFrameEnhancerConfigInit(dvConfigNode moduleNode) {
	dvConfigNodeCreateBool(
		moduleNode, "doDemosaic", false, DVCFG_FLAGS_NORMAL, "Do demosaicing (color interpolation) on frame.");
	dvConfigNodeCreateBool(moduleNode, "doContrast", false, DVCFG_FLAGS_NORMAL, "Do contrast enhancement on frame.");

#if defined(LIBCAER_HAVE_OPENCV) && LIBCAER_HAVE_OPENCV == 1
	dvConfigNodeCreateString(moduleNode, "demosaicType", "opencv_edge_aware", 7, 17, DVCFG_FLAGS_NORMAL,
		"Demoisaicing (color interpolation) algorithm to apply.");
	dvConfigNodeAttributeModifierListOptions(
		moduleNode, "demosaicType", "opencv_edge_aware,opencv_to_gray,opencv_standard,to_gray,standard", false);

	dvConfigNodeCreateString(moduleNode, "contrastType", "opencv_normalization", 8, 29, DVCFG_FLAGS_NORMAL,
		"Contrast enhancement algorithm to apply.");
	dvConfigNodeAttributeModifierListOptions(
		moduleNode, "contrastType", "opencv_normalization,opencv_histogram_equalization,opencv_clahe,standard", false);
#else
    dvConfigNodeCreateString(moduleNode, "demosaicType", "standard", 7, 8, DVCFG_FLAGS_NORMAL,
		"Demoisaicing (color interpolation) algorithm to apply.");
	dvConfigNodeAttributeModifierListOptions(
		moduleNode, "demosaicType", "to_gray,standard", false);

	dvConfigNodeCreateString(moduleNode, "contrastType", "standard", 8, 8, DVCFG_FLAGS_NORMAL,
		"Contrast enhancement algorithm to apply.");
	dvConfigNodeAttributeModifierListOptions(
		moduleNode, "contrastType", "standard", false);
#endif
}

static bool caerFrameEnhancerInit(dvModuleData moduleData) {
	// Wait for input to be ready. All inputs, once they are up and running, will
	// have a valid sourceInfo node to query, especially if dealing with data.
	dvConfigNode sourceInfoSource = dvMainloopModuleGetSourceInfoForInput(moduleData->moduleID, 0);
	if (sourceInfoSource == NULL) {
		return (false);
	}

	int16_t sizeX = dvConfigNodeGetInt(sourceInfoSource, "dataSizeX");
	int16_t sizeY = dvConfigNodeGetInt(sourceInfoSource, "dataSizeY");

	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	dvConfigNodeCreateInt(sourceInfoNode, "frameSizeX", sizeX, 1, 1024, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Output frame width.");
	dvConfigNodeCreateInt(sourceInfoNode, "frameSizeY", sizeY, 1, 1024, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Output frame height.");
	dvConfigNodeCreateInt(
		sourceInfoNode, "dataSizeX", sizeX, 1, 1024, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Output data width.");
	dvConfigNodeCreateInt(sourceInfoNode, "dataSizeY", sizeY, 1, 1024, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Output data height.");

	// Initialize configuration.
	caerFrameEnhancerConfig(moduleData);

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData, &dvModuleDefaultConfigListener);

	// Nothing that can fail here.
	return (true);
}

static void caerFrameEnhancerRun(
	dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
	caerFrameEventPacketConst inputFramePacket
		= (caerFrameEventPacketConst) caerEventPacketContainerFindEventPacketByTypeConst(in, FRAME_EVENT);

	// Only process packets with content.
	if (inputFramePacket == NULL) {
		return;
	}

	caerFrameEventPacket outputFramePacket
		= caerFrameEventPacketAllocateNumPixels(caerEventPacketHeaderGetEventValid(&inputFramePacket->packetHeader),
			moduleData->moduleID, caerEventPacketHeaderGetEventTSOverflow(&inputFramePacket->packetHeader),
			caerEventPacketHeaderGetEventSize(&inputFramePacket->packetHeader), RGBA);
	if (outputFramePacket == NULL) {
		return;
	}

	FrameEnhancerState state = moduleData->moduleState;

	int32_t outIdx = 0;

	for (int32_t inIdx = 0; inIdx < caerEventPacketHeaderGetEventNumber(&inputFramePacket->packetHeader); inIdx++) {
		caerFrameEventConst inFrame = caerFrameEventPacketGetEventConst(inputFramePacket, inIdx);
		if (!caerFrameEventIsValid(inFrame)) {
			continue;
		}

		caerFrameEvent outFrame = caerFrameEventPacketGetEvent(outputFramePacket, outIdx);

		// Copy header over. This will also copy validity information, so all copied frames are valid.
		memcpy(outFrame, inFrame, (sizeof(struct caer_frame_event) - sizeof(uint16_t)));

		// Verify requirements for demosaicing operation.
		if (state->doDemosaic && (caerFrameEventGetChannelNumber(inFrame) == GRAYSCALE)
			&& (caerFrameEventGetColorFilter(inFrame) != MONO)) {
#if defined(LIBCAER_HAVE_OPENCV) && LIBCAER_HAVE_OPENCV == 1
			if ((state->demosaicType != DEMOSAIC_TO_GRAY) && (state->demosaicType != DEMOSAIC_OPENCV_TO_GRAY)) {
#else
			if (state->demosaicType != DEMOSAIC_TO_GRAY) {
#endif
				// Demosaicing needs output frame set to RGB. If color requested.
				caerFrameEventSetLengthXLengthYChannelNumber(outFrame, caerFrameEventGetLengthX(inFrame),
					caerFrameEventGetLengthY(inFrame), RGB, outputFramePacket);
			}

			caerFrameUtilsDemosaic(inFrame, outFrame, state->demosaicType);
		}
		else {
			// Just copy data over.
			memcpy(caerFrameEventGetPixelArrayUnsafe(outFrame), caerFrameEventGetPixelArrayUnsafeConst(inFrame),
				caerFrameEventGetPixelsSize(inFrame));
		}

		if (state->doContrast) {
			caerFrameUtilsContrast(outFrame, outFrame, state->contrastType);
		}

		outIdx++;
	}

	// Set number of events in output packet correctly.
	caerEventPacketHeaderSetEventNumber(&outputFramePacket->packetHeader, outIdx);
	caerEventPacketHeaderSetEventValid(&outputFramePacket->packetHeader, outIdx);

	// Make a packet container and return the result.
	*out = caerEventPacketContainerAllocate(1);
	if (*out == NULL) {
		free(outputFramePacket);
		return;
	}

	caerEventPacketContainerSetEventPacket(*out, 0, (caerEventPacketHeader) outputFramePacket);
}

static void caerFrameEnhancerConfig(dvModuleData moduleData) {
	FrameEnhancerState state = moduleData->moduleState;

	state->doDemosaic = dvConfigNodeGetBool(moduleData->moduleNode, "doDemosaic");

	state->doContrast = dvConfigNodeGetBool(moduleData->moduleNode, "doContrast");

	char *demosaicType = dvConfigNodeGetString(moduleData->moduleNode, "demosaicType");

#if defined(LIBCAER_HAVE_OPENCV) && LIBCAER_HAVE_OPENCV == 1
	if (caerStrEquals(demosaicType, "opencv_edge_aware")) {
		state->demosaicType = DEMOSAIC_OPENCV_EDGE_AWARE;
	}
	else if (caerStrEquals(demosaicType, "opencv_to_gray")) {
		state->demosaicType = DEMOSAIC_OPENCV_TO_GRAY;
	}
	else if (caerStrEquals(demosaicType, "opencv_standard")) {
		state->demosaicType = DEMOSAIC_OPENCV_STANDARD;
	}
	else
#endif
		if (caerStrEquals(demosaicType, "to_gray")) {
		state->demosaicType = DEMOSAIC_TO_GRAY;
	}
	else {
		// Standard, non-OpenCV method.
		state->demosaicType = DEMOSAIC_STANDARD;
	}

	free(demosaicType);

	char *contrastType = dvConfigNodeGetString(moduleData->moduleNode, "contrastType");

#if defined(LIBCAER_HAVE_OPENCV) && LIBCAER_HAVE_OPENCV == 1
	if (caerStrEquals(contrastType, "opencv_normalization")) {
		state->contrastType = CONTRAST_OPENCV_NORMALIZATION;
	}
	else if (caerStrEquals(contrastType, "opencv_histogram_equalization")) {
		state->contrastType = CONTRAST_OPENCV_HISTOGRAM_EQUALIZATION;
	}
	else if (caerStrEquals(contrastType, "opencv_clahe")) {
		state->contrastType = CONTRAST_OPENCV_CLAHE;
	}
	else
#endif
	{
		// Standard, non-OpenCV method.
		state->contrastType = CONTRAST_STANDARD;
	}

	free(contrastType);
}

static void caerFrameEnhancerExit(dvModuleData moduleData) {
	// Remove listener, which can reference invalid memory in userData.
	dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &dvModuleDefaultConfigListener);

	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	dvConfigNodeClearSubTree(sourceInfoNode, true);
}
