#include "visualizer_renderers.hpp"

#include <libcaercpp/events/frame.hpp>
#include <libcaercpp/events/imu6.hpp>
#include <libcaercpp/events/polarity.hpp>

#include <libcaercpp/devices/davis.hpp> // Only for constants.

#include "ext/sfml/helpers.hpp"
#include "ext/sfml/line.hpp"

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static void *caerVisualizerRendererPolarityEventsStateInit(caerVisualizerPublicState state);
static bool caerVisualizerRendererPolarityEvents(caerVisualizerPublicState state, caerEventPacketContainer container);
static const struct caer_visualizer_renderer_info rendererPolarityEvents(
	"Polarity", &caerVisualizerRendererPolarityEvents, false, &caerVisualizerRendererPolarityEventsStateInit, nullptr);

static void *caerVisualizerRendererFrameEventsStateInit(caerVisualizerPublicState state);
static void caerVisualizerRendererFrameEventsStateExit(caerVisualizerPublicState state);
static bool caerVisualizerRendererFrameEvents(caerVisualizerPublicState state, caerEventPacketContainer container);
static const struct caer_visualizer_renderer_info rendererFrameEvents("Frame", &caerVisualizerRendererFrameEvents,
	false, &caerVisualizerRendererFrameEventsStateInit, &caerVisualizerRendererFrameEventsStateExit);

static bool caerVisualizerRendererIMU6Events(caerVisualizerPublicState state, caerEventPacketContainer container);
static const struct caer_visualizer_renderer_info rendererIMU6Events("IMU_6-axes", &caerVisualizerRendererIMU6Events);

static void *caerVisualizerRendererPolarityAndFrameEventsStateInit(caerVisualizerPublicState state);
static void caerVisualizerRendererPolarityAndFrameEventsStateExit(caerVisualizerPublicState state);
static bool caerVisualizerRendererPolarityAndFrameEvents(
	caerVisualizerPublicState state, caerEventPacketContainer container);
static const struct caer_visualizer_renderer_info rendererPolarityAndFrameEvents("Polarity_and_Frames",
	&caerVisualizerRendererPolarityAndFrameEvents, false, &caerVisualizerRendererPolarityAndFrameEventsStateInit,
	&caerVisualizerRendererPolarityAndFrameEventsStateExit);

const std::string caerVisualizerRendererListOptionsString = "Polarity,Frame,IMU_6-axes,Polarity_and_Frames";

const struct caer_visualizer_renderer_info caerVisualizerRendererList[] = {
	{"None", nullptr}, rendererPolarityEvents, rendererFrameEvents, rendererIMU6Events, rendererPolarityAndFrameEvents};

const size_t caerVisualizerRendererListLength
	= (sizeof(caerVisualizerRendererList) / sizeof(struct caer_visualizer_renderer_info));

static void *caerVisualizerRendererPolarityEventsStateInit(caerVisualizerPublicState state) {
	state->visualizerConfigNode.create<dvCfgType::BOOL>("DoubleSpacedAddresses", false, {}, dvCfgFlags::NORMAL,
		"Space DVS addresses apart by doubling them, this is useful for the CDAVIS sensor to put them as they are in "
		"the pixel array.");

	return (DV_VISUALIZER_RENDER_INIT_NO_MEM); // No allocated memory.
}

static bool caerVisualizerRendererPolarityEvents(caerVisualizerPublicState state, caerEventPacketContainer container) {
	UNUSED_ARGUMENT(state);

	caerEventPacketHeader polarityPacketHeader
		= caerEventPacketContainerFindEventPacketByType(container, POLARITY_EVENT);

	// No packet of requested type or empty packet (no valid events).
	if (polarityPacketHeader == nullptr || caerEventPacketHeaderGetEventValid(polarityPacketHeader) == 0) {
		return (false);
	}

	bool doubleSpacedAddresses = state->visualizerConfigNode.get<dvCfgType::BOOL>("DoubleSpacedAddresses");

	const libcaer::events::PolarityEventPacket polarityPacket(polarityPacketHeader, false);

	std::vector<sf::Vertex> vertices;
	vertices.reserve((size_t) polarityPacket.getEventValid() * 4);

	// Render all valid events.
	for (const auto &polarityEvent : polarityPacket) {
		if (!polarityEvent.isValid()) {
			continue; // Skip invalid events.
		}

		uint16_t x = polarityEvent.getX();
		uint16_t y = polarityEvent.getY();

		if (doubleSpacedAddresses) {
			x = U16T(x << 1);
			y = U16T(y << 1);
		}

		// ON polarity (green), OFF polarity (red).
		sfml::Helpers::addPixelVertices(vertices, x, y, state->renderZoomFactor.load(std::memory_order_relaxed),
			(polarityEvent.getPolarity()) ? (sf::Color::Green) : (sf::Color::Red));
	}

	state->renderWindow->draw(vertices.data(), vertices.size(), sf::Quads);

	return (true);
}

struct renderer_frame_events_state {
	sf::Sprite sprite;
	sf::Texture texture;
	std::vector<uint8_t> pixels;
};

typedef struct renderer_frame_events_state *rendererFrameEventsState;

static void *caerVisualizerRendererFrameEventsStateInit(caerVisualizerPublicState state) {
	// Add configuration for ROI region.
	state->visualizerConfigNode.create<dvCfgType::INT>("ROIRegion", 0, {0, 2}, dvCfgFlags::NORMAL,
		"Selects which ROI region to display. 0 is the standard image, 1 is for debug (reset read), 2 is for debug "
		"(signal read).");

	// Allocate memory via C++ for renderer state, since we use C++ objects directly.
	rendererFrameEventsState renderState = new renderer_frame_events_state();

	// Create texture representing frame, set smoothing.
	renderState->texture.create(state->renderSizeX, state->renderSizeY);
	renderState->texture.setSmooth(false);

	// Assign texture to sprite.
	renderState->sprite.setTexture(renderState->texture);

	// 32-bit RGBA pixels (8-bit per channel), standard CG layout.
	renderState->pixels.reserve(state->renderSizeX * state->renderSizeY * 4);

	return (renderState);
}

static void caerVisualizerRendererFrameEventsStateExit(caerVisualizerPublicState state) {
	rendererFrameEventsState renderState = (rendererFrameEventsState) state->renderState;

	delete renderState;
}

static bool caerVisualizerRendererFrameEvents(caerVisualizerPublicState state, caerEventPacketContainer container) {
	rendererFrameEventsState renderState = (rendererFrameEventsState) state->renderState;

	caerEventPacketHeader framePacketHeader = caerEventPacketContainerFindEventPacketByType(container, FRAME_EVENT);

	// No packet of requested type or empty packet (no valid events).
	if ((framePacketHeader == nullptr) || (caerEventPacketHeaderGetEventValid(framePacketHeader) == 0)) {
		return (false);
	}

	int roiRegionSelect = state->visualizerConfigNode.get<dvCfgType::INT>("ROIRegion");

	const libcaer::events::FrameEventPacket framePacket(framePacketHeader, false);

	// Only operate on the last, valid frame for the selected ROI region.
	const libcaer::events::FrameEvent *frame = {nullptr};

	for (const auto &f : framePacket) {
		if (f.isValid() && (f.getROIIdentifier() == roiRegionSelect)) {
			frame = &f;
		}
	}

	if (frame == nullptr) {
		return (false);
	}

	// 32-bit RGBA pixels (8-bit per channel), standard CG layout.
	switch (frame->getChannelNumber()) {
		case libcaer::events::FrameEvent::colorChannels::GRAYSCALE: {
			for (size_t srcIdx = 0, dstIdx = 0; srcIdx < frame->getPixelsMaxIndex();) {
				uint8_t greyValue             = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8);
				renderState->pixels[dstIdx++] = greyValue; // R
				renderState->pixels[dstIdx++] = greyValue; // G
				renderState->pixels[dstIdx++] = greyValue; // B
				renderState->pixels[dstIdx++] = UINT8_MAX; // A
			}
			break;
		}

		case libcaer::events::FrameEvent::colorChannels::RGB: {
			for (size_t srcIdx = 0, dstIdx = 0; srcIdx < frame->getPixelsMaxIndex();) {
				renderState->pixels[dstIdx++] = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8); // R
				renderState->pixels[dstIdx++] = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8); // G
				renderState->pixels[dstIdx++] = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8); // B
				renderState->pixels[dstIdx++] = UINT8_MAX;                                        // A
			}
			break;
		}

		case libcaer::events::FrameEvent::colorChannels::RGBA: {
			for (size_t srcIdx = 0, dstIdx = 0; srcIdx < frame->getPixelsMaxIndex();) {
				renderState->pixels[dstIdx++] = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8); // R
				renderState->pixels[dstIdx++] = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8); // G
				renderState->pixels[dstIdx++] = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8); // B
				renderState->pixels[dstIdx++] = U8T(frame->getPixelArrayUnsafe()[srcIdx++] >> 8); // A
			}
			break;
		}
	}

	renderState->texture.update(renderState->pixels.data(), U32T(frame->getLengthX()), U32T(frame->getLengthY()),
		U32T(frame->getPositionX()), U32T(frame->getPositionY()));

	renderState->sprite.setTextureRect(
		sf::IntRect(frame->getPositionX(), frame->getPositionY(), frame->getLengthX(), frame->getLengthY()));

	float zoomFactor = state->renderZoomFactor.load(std::memory_order_relaxed);

	renderState->sprite.setPosition(
		(float) frame->getPositionX() * zoomFactor, (float) frame->getPositionY() * zoomFactor);

	renderState->sprite.setScale(zoomFactor, zoomFactor);

	state->renderWindow->draw(renderState->sprite);

	return (true);
}

#define RESET_LIMIT_POS(VAL, LIMIT) \
	if ((VAL) > (LIMIT)) {          \
		(VAL) = (LIMIT);            \
	}
#define RESET_LIMIT_NEG(VAL, LIMIT) \
	if ((VAL) < (LIMIT)) {          \
		(VAL) = (LIMIT);            \
	}

static bool caerVisualizerRendererIMU6Events(caerVisualizerPublicState state, caerEventPacketContainer container) {
	caerEventPacketHeader imu6PacketHeader = caerEventPacketContainerFindEventPacketByType(container, IMU6_EVENT);

	if (imu6PacketHeader == nullptr || caerEventPacketHeaderGetEventValid(imu6PacketHeader) == 0) {
		return (false);
	}

	const libcaer::events::IMU6EventPacket imu6Packet(imu6PacketHeader, false);

	float zoomFactor = state->renderZoomFactor.load(std::memory_order_relaxed);

	float scaleFactorAccel = 30 * zoomFactor;
	float scaleFactorGyro  = 15 * zoomFactor;
	float lineThickness    = 4 * zoomFactor;
	float maxSizeX         = (float) state->renderSizeX * zoomFactor;
	float maxSizeY         = (float) state->renderSizeY * zoomFactor;

	sf::Color accelColor = sf::Color::Green;
	sf::Color gyroColor  = sf::Color::Magenta;

	float centerPointX = maxSizeX / 2;
	float centerPointY = maxSizeY / 2;

	float accelX = 0, accelY = 0, accelZ = 0;
	float gyroX = 0, gyroY = 0, gyroZ = 0;
	float temp = 0;

	// Iterate over valid IMU events and average them.
	// This somewhat smoothes out the rendering.
	for (const auto &imu6Event : imu6Packet) {
		accelX += imu6Event.getAccelX();
		accelY += imu6Event.getAccelY();
		accelZ += imu6Event.getAccelZ();

		gyroX += imu6Event.getGyroX();
		gyroY += imu6Event.getGyroY();
		gyroZ += imu6Event.getGyroZ();

		temp += imu6Event.getTemp();
	}

	// Normalize values.
	int32_t validEvents = imu6Packet.getEventValid();

	accelX /= (float) validEvents;
	accelY /= (float) validEvents;
	accelZ /= (float) validEvents;

	gyroX /= (float) validEvents;
	gyroY /= (float) validEvents;
	gyroZ /= (float) validEvents;

	temp /= (float) validEvents;

	// Acceleration X, Y as lines. Z as a circle.
	float accelXScaled = centerPointX - accelX * scaleFactorAccel;
	RESET_LIMIT_POS(accelXScaled, maxSizeX - 2 - lineThickness);
	RESET_LIMIT_NEG(accelXScaled, 1 + lineThickness);
	float accelYScaled = centerPointY - accelY * scaleFactorAccel;
	RESET_LIMIT_POS(accelYScaled, maxSizeY - 2 - lineThickness);
	RESET_LIMIT_NEG(accelYScaled, 1 + lineThickness);
	float accelZScaled = fabsf(accelZ * scaleFactorAccel);
	RESET_LIMIT_POS(accelZScaled, centerPointY - 2 - lineThickness); // Circle max.
	RESET_LIMIT_NEG(accelZScaled, 1);                                // Circle min.

	sfml::Line accelLine(
		sf::Vector2f(centerPointX, centerPointY), sf::Vector2f(accelXScaled, accelYScaled), lineThickness, accelColor);
	state->renderWindow->draw(accelLine);

	sf::CircleShape accelCircle(accelZScaled);
	sfml::Helpers::setOriginToCenter(accelCircle);
	accelCircle.setFillColor(sf::Color::Transparent);
	accelCircle.setOutlineColor(accelColor);
	accelCircle.setOutlineThickness(-lineThickness);
	accelCircle.setPosition(sf::Vector2f(centerPointX, centerPointY));

	state->renderWindow->draw(accelCircle);

	// Gyroscope pitch(X), yaw(Y), roll(Z) as lines.
	float gyroXScaled = centerPointY + gyroX * scaleFactorGyro;
	RESET_LIMIT_POS(gyroXScaled, maxSizeY - 2 - lineThickness);
	RESET_LIMIT_NEG(gyroXScaled, 1 + lineThickness);
	float gyroYScaled = centerPointX + gyroY * scaleFactorGyro;
	RESET_LIMIT_POS(gyroYScaled, maxSizeX - 2 - lineThickness);
	RESET_LIMIT_NEG(gyroYScaled, 1 + lineThickness);
	float gyroZScaled = centerPointX - gyroZ * scaleFactorGyro;
	RESET_LIMIT_POS(gyroZScaled, maxSizeX - 2 - lineThickness);
	RESET_LIMIT_NEG(gyroZScaled, 1 + lineThickness);

	sfml::Line gyroLine1(
		sf::Vector2f(centerPointX, centerPointY), sf::Vector2f(gyroYScaled, gyroXScaled), lineThickness, gyroColor);
	state->renderWindow->draw(gyroLine1);

	sfml::Line gyroLine2(sf::Vector2f(centerPointX, centerPointY - 20), sf::Vector2f(gyroZScaled, centerPointY - 20),
		lineThickness, gyroColor);
	state->renderWindow->draw(gyroLine2);

	// TODO: enhance IMU renderer with more text info.
	if (state->font != nullptr) {
		char valStr[128];

		// Acceleration X/Y.
		snprintf(valStr, 128, "%.2f,%.2f g", (double) accelX, (double) accelY);

		sf::Text accelText(valStr, *state->font, 30);
		sfml::Helpers::setTextColor(accelText, accelColor);
		accelText.setPosition(sf::Vector2f(accelXScaled, accelYScaled));

		state->renderWindow->draw(accelText);

		// Acceleration Z.
		snprintf(valStr, 128, "%.2f g", (double) accelZ);

		sf::Text accelTextZ(valStr, *state->font, 30);
		sfml::Helpers::setTextColor(accelTextZ, accelColor);
		accelTextZ.setPosition(sf::Vector2f(centerPointX, centerPointY + accelZScaled + lineThickness));

		state->renderWindow->draw(accelTextZ);

		// Temperature.
		snprintf(valStr, 128, "Temp: %.2f C", (double) temp);

		sf::Text tempText(valStr, *state->font, 30);
		sfml::Helpers::setTextColor(tempText, sf::Color::White);
		tempText.setPosition(sf::Vector2f(0, 0));

		state->renderWindow->draw(tempText);
	}

	return (true);
}

static void *caerVisualizerRendererPolarityAndFrameEventsStateInit(caerVisualizerPublicState state) {
	caerVisualizerRendererPolarityEventsStateInit(state);
	return (caerVisualizerRendererFrameEventsStateInit(state));
}

static void caerVisualizerRendererPolarityAndFrameEventsStateExit(caerVisualizerPublicState state) {
	caerVisualizerRendererFrameEventsStateExit(state);
}

static bool caerVisualizerRendererPolarityAndFrameEvents(
	caerVisualizerPublicState state, caerEventPacketContainer container) {
	bool drewFrameEvents = caerVisualizerRendererFrameEvents(state, container);

	bool drewPolarityEvents = caerVisualizerRendererPolarityEvents(state, container);

	return (drewFrameEvents || drewPolarityEvents);
}
