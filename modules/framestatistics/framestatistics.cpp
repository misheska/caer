#include <libcaercpp/events/frame.hpp>

#include "dv-sdk/mainloop.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

struct caer_frame_statistics_state {
	int numBins;
	int roiRegion;
};

typedef struct caer_frame_statistics_state *caerFrameStatisticsState;

static void caerFrameStatisticsConfigInit(dvConfigNode moduleNode);
static bool caerFrameStatisticsInit(dvModuleData moduleData);
static void caerFrameStatisticsRun(
	dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out);
static void caerFrameStatisticsExit(dvModuleData moduleData);
static void caerFrameStatisticsConfig(dvModuleData moduleData);

static const struct dvModuleFunctionsS FrameStatisticsFunctions
	= {.moduleConfigInit = &caerFrameStatisticsConfigInit,
		.moduleInit      = &caerFrameStatisticsInit,
		.moduleRun       = &caerFrameStatisticsRun,
		.moduleConfig    = &caerFrameStatisticsConfig,
		.moduleExit      = &caerFrameStatisticsExit,
		.moduleReset     = NULL};

static const struct caer_event_stream_in FrameStatisticsInputs[]
	= {{.type = FRAME_EVENT, .number = 1, .readOnly = true}};

static const struct dvModuleInfoS FrameStatisticsInfo = {.version = 1,
	.name                                                            = "FrameStatistics",
	.description                                                     = "Display statistics on frames (histogram).",
	.type                                                            = DV_MODULE_OUTPUT,
	.memSize                                                         = sizeof(struct caer_frame_statistics_state),
	.functions                                                       = &FrameStatisticsFunctions,
	.inputStreamsSize                                                = CAER_EVENT_STREAM_IN_SIZE(FrameStatisticsInputs),
	.inputStreams                                                    = FrameStatisticsInputs,
	.outputStreamsSize                                               = 0,
	.outputStreams                                                   = NULL};

dvModuleInfo dvModuleGetInfo(void) {
	return (&FrameStatisticsInfo);
}

static void caerFrameStatisticsConfigInit(dvConfigNode moduleNode) {
	dvCfg::Node cfg(moduleNode);

	cfg.create<dvCfgType::INT>(
		"numBins", 1024, {4, UINT16_MAX + 1}, dvCfgFlags::NORMAL, "Number of bins in which to divide values up.");
	cfg.create<dvCfgType::INT>("roiRegion", 0, {0, 7}, dvCfgFlags::NORMAL, "Selects which ROI region to display.");
	cfg.create<dvCfgType::INT>(
		"windowPositionX", 20, {0, UINT16_MAX}, dvCfgFlags::NORMAL, "Position of window on screen (X coordinate).");
	cfg.create<dvCfgType::INT>(
		"windowPositionY", 20, {0, UINT16_MAX}, dvCfgFlags::NORMAL, "Position of window on screen (Y coordinate).");
}

static bool caerFrameStatisticsInit(dvModuleData moduleData) {
	// Get configuration.
	caerFrameStatisticsConfig(moduleData);

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData, &dvModuleDefaultConfigListener);

	cv::namedWindow(moduleData->moduleSubSystemString,
		cv::WindowFlags::WINDOW_AUTOSIZE | cv::WindowFlags::WINDOW_KEEPRATIO | cv::WindowFlags::WINDOW_GUI_EXPANDED);

	return (true);
}

static void caerFrameStatisticsRun(
	dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
	UNUSED_ARGUMENT(out);

	caerFrameEventPacket inPacket
		= reinterpret_cast<caerFrameEventPacket>(caerEventPacketContainerGetEventPacket(in, 0));

	// Only process packets with content.
	if (inPacket == nullptr) {
		return;
	}

	const libcaer::events::FrameEventPacket frames(inPacket, false);

	caerFrameStatisticsState state = static_cast<caerFrameStatisticsState>(moduleData->moduleState);

	for (const auto &frame : frames) {
		if ((!frame.isValid()) || (frame.getROIIdentifier() != state->roiRegion)) {
			continue;
		}

		const cv::Mat frameOpenCV = frame.getOpenCVMat(false);

		// Calculate histogram, full uint16 range.
		const float range[]    = {0, UINT16_MAX + 1};
		const float *histRange = {range};

		cv::Mat hist;
		cv::calcHist(&frameOpenCV, 1, nullptr, cv::Mat(), hist, 1, &state->numBins, &histRange, true, false);

		// Generate histogram image, with N x N/3 pixels.
		int hist_w = state->numBins;
		int hist_h = state->numBins / 3;

		cv::Mat histImage(hist_h, hist_w, CV_8UC1, cv::Scalar(0));

		// Normalize the result to [0, histImage.rows].
		cv::normalize(hist, hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());

		// Draw the histogram.
		for (int i = 1; i < state->numBins; i++) {
			cv::line(histImage, cv::Point(i - 1, hist_h - cvRound(hist.at<float>(i - 1))),
				cv::Point(i, hist_h - cvRound(hist.at<float>(i))), cv::Scalar(255, 255, 255), 2, 8, 0);
		}

		// Simple display, just use OpenCV GUI.
		cv::imshow(moduleData->moduleSubSystemString, histImage);
		cv::waitKey(1);
	}
}

static void caerFrameStatisticsExit(dvModuleData moduleData) {
	cv::destroyWindow(moduleData->moduleSubSystemString);

	// Remove listener, which can reference invalid memory in userData.
	dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &dvModuleDefaultConfigListener);
}

static void caerFrameStatisticsConfig(dvModuleData moduleData) {
	caerFrameStatisticsState state = (caerFrameStatisticsState) moduleData->moduleState;
	dvCfg::Node cfg(moduleData->moduleNode);

	state->numBins   = cfg.get<dvCfgType::INT>("numBins");
	state->roiRegion = cfg.get<dvCfgType::INT>("roiRegion");

	int posX = cfg.get<dvCfgType::INT>("windowPositionX");
	int posY = cfg.get<dvCfgType::INT>("windowPositionY");

	cv::moveWindow(moduleData->moduleSubSystemString, posX, posY);
}
