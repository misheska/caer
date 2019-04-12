#include "visualizer.hpp"

#include "dv-sdk/data/event.hpp"
#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/data/imu.hpp"
#include "dv-sdk/module.hpp"

#include "ext/fonts/LiberationSans-Bold.h"
#include "ext/sfml/helpers.hpp"
#include "ext/sfml/line.hpp"
#include "visualizer_renderers.hpp"

#include <mutex>

#if defined(OS_LINUX) && OS_LINUX == 1
#	include <X11/Xlib.h>
#endif

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

#define VISUALIZER_REFRESH_RATE 60
#define VISUALIZER_ZOOM_DEF 2.0f
#define VISUALIZER_ZOOM_INC 0.25f
#define VISUALIZER_ZOOM_MIN 0.50f
#define VISUALIZER_ZOOM_MAX 50.0f
#define VISUALIZER_POSITION_X_DEF 100
#define VISUALIZER_POSITION_Y_DEF 100

#define GLOBAL_FONT_SIZE 20   // in pixels
#define GLOBAL_FONT_SPACING 5 // in pixels

// Track system init.
static std::once_flag visualizerSystemIsInitialized;

static void caerVisualizerConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void initSystemOnce();
static bool initGraphics(dvModuleData moduleData);
static void exitGraphics(dvModuleData moduleData);
static void updateDisplaySize(caerVisualizerState state);
static void updateDisplayLocation(caerVisualizerState state);
static void saveDisplayLocation(caerVisualizerState state);
static void handleEvents(dvModuleData moduleData);
static void renderScreen(dvModuleData moduleData);

class Visualizer : public dv::ModuleBase {
private:
	int16_t renderSizeX;
	int16_t renderSizeY;
	std::atomic<float> renderZoomFactor;
	void *renderState; // Reserved for renderers to put their internal state into.
	sf::RenderWindow *renderWindow;
	sf::Font *font;
	std::atomic_bool windowResize;
	std::atomic_bool windowMove;
	caerVisualizerRendererInfo renderer;
	uint32_t packetSubsampleCount;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("visualize", "ANYT", false);
	}

	static const char *getDescription() {
		return ("Visualize data in various simple ways. Please use dv-gui instead!");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add("subsampleRendering",
			dv::ConfigOption::intOption(
				"Speed-up rendering by only taking every Nth EventPacketContainer to render.", 1, 1, 10000));

		config.add("windowPositionX", dv::ConfigOption::intOption("Position of window on screen (X coordinate).",
										  VISUALIZER_POSITION_X_DEF, 0, UINT16_MAX));
		config.add("windowPositionY", dv::ConfigOption::intOption("Position of window on screen (Y coordinate).",
										  VISUALIZER_POSITION_Y_DEF, 0, UINT16_MAX));
		config.add("zoomFactor", dv::ConfigOption::floatOption("Content zoom factor.", VISUALIZER_ZOOM_DEF,
									 VISUALIZER_ZOOM_MIN, VISUALIZER_ZOOM_MAX));
	}

	static void initSystemOnce() {
// Call XInitThreads() on Linux.
#if defined(OS_LINUX) && OS_LINUX == 1
		XInitThreads();
#endif
	}

	Visualizer() {
		// Initialize visualizer framework (global font sizes). Do only once per startup!
		std::call_once(visualizerSystemIsInitialized, &initSystemOnce);

		// Initialize visualizer. Needs size information from the source.
		auto info = inputs.getInfoNode("visualize");
		if (!info) {
			throw std::runtime_error("Input not ready, upstream module not running.");
		}

		auto type = info.getParent().get<dvCfgType::STRING>("typeIdentifier");

		if (type == dv::EventPacket::identifier) {
			// Size from output info.
			renderSizeX = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeX"));
			renderSizeY = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeY"));
			renderer    = &caerVisualizerRendererList[0];
		}
		else if (type == dv::Frame::identifier) {
			// Size from output info.
			renderSizeX = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeX"));
			renderSizeY = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeY"));
			renderer    = &caerVisualizerRendererList[1];
		}
		else if (type == dv::IMUPacket::identifier) {
			// Fixed size.
			renderSizeX = 128;
			renderSizeY = 128;
			renderer    = &caerVisualizerRendererList[2];
		}
		else {
			throw std::runtime_error("Type '" + type + "' is not supported by the visualizer.");
		}

		// Initialize graphics on separate thread. Mostly to avoid Windows quirkiness.
		if (!initGraphics(moduleData)) {
			throw std::runtime_error("Failed to initialize rendering window.");
		}

		// Ensure OpenGL context is active, whether it was created in this thread
		// or on the main thread.
		renderWindow->setActive(true);

		// Initialize GLEW. glewInit() should be called after every context change,
		// since we have one context per visualizer, always active only in this one
		// rendering thread, we can just do it here once and always be fine.
		GLenum res = glewInit();
		if (res != GLEW_OK) {
			exitGraphics(moduleData); // Destroy on error.

			throw std::runtime_error("Failed to initialize GLEW, error: "
									 + std::string(reinterpret_cast<const char *>(glewGetErrorString(res))));
		}

		// Initialize renderer state.
		if (renderer->stateInit != nullptr) {
			renderState = (*renderer->stateInit)();
			if (renderState == nullptr) {
				exitGraphics(moduleData); // Destroy on error.

				// Failed at requested state initialization, error out!
				throw std::runtime_error("Failed to initialize renderer state.");
			}
		}

		// Initialize window by clearing it to all black.
		renderWindow->clear(sf::Color::Black);
		renderWindow->display();
	}

	~Visualizer() override {
		// Destroy render state, if it exists.
		if ((renderer->stateExit != nullptr) && (renderState != nullptr)
			&& (renderState != DV_VISUALIZER_RENDER_INIT_NO_MEM)) {
			(*renderer->stateExit)();
		}

		// Destroy graphics objects on same thread that created them.
		exitGraphics(moduleData);
	}

	void run() override {
		// Only render every Nth container (or packet, if using standard visualizer).
		packetSubsampleCount++;

		if (packetSubsampleCount >= config.get<dvCfgType::INT>("subsampleRendering")) {
			packetSubsampleCount = 0;
		}
		else {
			return;
		}

		handleEvents(moduleData);

		renderScreen(moduleData);
	}
};

static void caerVisualizerConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	caerVisualizerState state = (caerVisualizerState) userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_FLOAT && caerStrEquals(changeKey, "zoomFactor")) {
			// Set resize flag.
			state->windowResize.store(true);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "showStatistics")) {
			// Set resize flag. This will then also update the showStatistics flag, ensuring
			// statistics are never shown without the screen having been properly resized first.
			state->windowResize.store(true);
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "subsampleRendering")) {
			state->packetSubsampleRendering.store(U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "windowPositionX")) {
			// Set move flag.
			state->windowMove.store(true);
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "windowPositionY")) {
			// Set move flag.
			state->windowMove.store(true);
		}
	}
}

static bool initGraphics(dvModuleData moduleData) {
	caerVisualizerState state = (caerVisualizerState) moduleData->moduleState;

	// Create OpenGL context. Depending on flag, either an OpenGL 2.1
	// default (compatibility) context, so it can be used with SFML graphics,
	// or an OpenGL 3.3 context with core profile, so it can do 3D everywhere,
	// even on MacOS X where newer OpenGL's only support the core profile.
	sf::ContextSettings openGLSettings;

	openGLSettings.depthBits   = 24;
	openGLSettings.stencilBits = 8;

	if (state->renderer->needsOpenGL3) {
		openGLSettings.majorVersion   = 3;
		openGLSettings.minorVersion   = 3;
		openGLSettings.attributeFlags = sf::ContextSettings::Core;
	}
	else {
		openGLSettings.majorVersion   = 2;
		openGLSettings.minorVersion   = 1;
		openGLSettings.attributeFlags = sf::ContextSettings::Default;
	}

	// Create display window and set its title.
	state->renderWindow = new sf::RenderWindow(sf::VideoMode(state->renderSizeX, state->renderSizeY),
		moduleData->moduleSubSystemString, sf::Style::Titlebar | sf::Style::Close, openGLSettings);
	if (state->renderWindow == nullptr) {
		dvModuleLog(moduleData, CAER_LOG_ERROR,
			"Failed to create display window with sizeX=%" PRIu32 ", sizeY=%" PRIu32 ".", state->renderSizeX,
			state->renderSizeY);
		return (false);
	}

	// Set frameRate limit to 60 FPS.
	state->renderWindow->setFramerateLimit(60);

	// Default zoom factor for above window would be 1.
	new (&state->renderZoomFactor) std::atomic<float>(1.0f);

	// Set scale transform for display window, update sizes.
	updateDisplaySize(state);

	// Set window position.
	updateDisplayLocation(state);

	// Load font here to have it always available on request.
	state->font = new sf::Font();
	if (state->font == nullptr) {
		dvModuleLog(
			moduleData, CAER_LOG_WARNING, "Failed to create display font. Text rendering will not be possible.");
	}
	else {
		if (!state->font->loadFromMemory(LiberationSans_Bold_ttf, LiberationSans_Bold_ttf_len)) {
			dvModuleLog(
				moduleData, CAER_LOG_WARNING, "Failed to load display font. Text rendering will not be possible.");

			delete state->font;
			state->font = nullptr;
		}
	}

	return (true);
}

static void exitGraphics(dvModuleData moduleData) {
	caerVisualizerState state = (caerVisualizerState) moduleData->moduleState;

	// Save visualizer window location in config.
	saveDisplayLocation(state);

	// Close rendering window and free memory.
	state->renderWindow->close();

	delete state->font;
	delete state->renderWindow;
}

static void updateDisplaySize(caerVisualizerState state) {
	state->showStatistics = state->visualizerConfigNode.get<dvCfgType::BOOL>("showStatistics");
	float zoomFactor      = state->visualizerConfigNode.get<dvCfgType::FLOAT>("zoomFactor");

	sf::Vector2u newRenderWindowSize(state->renderSizeX, state->renderSizeY);

	// Apply zoom to rendered content only, not statistics.
	newRenderWindowSize.x *= zoomFactor;
	newRenderWindowSize.y *= zoomFactor;

	// When statistics are turned on, we need to add some space to the
	// X axis for displaying the whole line and the Y axis for spacing.
	if (state->showStatistics) {
		if (STATISTICS_WIDTH > newRenderWindowSize.x) {
			newRenderWindowSize.x = STATISTICS_WIDTH;
		}

		newRenderWindowSize.y += STATISTICS_HEIGHT;
	}

	// Set window size to zoomed area (only if value changed!).
	sf::Vector2u oldSize = state->renderWindow->getSize();

	if ((newRenderWindowSize.x != oldSize.x) || (newRenderWindowSize.y != oldSize.y)) {
		state->renderWindow->setSize(newRenderWindowSize);

		// Update zoom factor.
		state->renderZoomFactor.store(zoomFactor);

		// Set view size to render area.
		state->renderWindow->setView(sf::View(sf::FloatRect(0, 0, newRenderWindowSize.x, newRenderWindowSize.y)));
	}
}

static void updateDisplayLocation(caerVisualizerState state) {
	// Set current position to what is in configuration storage.
	const sf::Vector2i newPos(state->visualizerConfigNode.get<dvCfgType::INT>("windowPositionX"),
		state->visualizerConfigNode.get<dvCfgType::INT>("windowPositionY"));

	state->renderWindow->setPosition(newPos);
}

static void saveDisplayLocation(caerVisualizerState state) {
	const sf::Vector2i currPos = state->renderWindow->getPosition();

	// Update current position in configuration storage.
	state->visualizerConfigNode.put<dvCfgType::INT>("windowPositionX", currPos.x);
	state->visualizerConfigNode.put<dvCfgType::INT>("windowPositionY", currPos.y);
}

static void handleEvents(dvModuleData moduleData) {
	caerVisualizerState state = (caerVisualizerState) moduleData->moduleState;

	sf::Event event;

	while (state->renderWindow->pollEvent(event)) {
		if (event.type == sf::Event::Closed) {
			// Stop visualizer module on window close.
			dvConfigNodePutBool(moduleData->moduleNode, "running", false);
		}
		else if (event.type == sf::Event::Resized) {
			// Handle resize events, the window is non-resizeable, so in theory all
			// resize events should come from our zoom resizes, and thus we can just
			// ignore them. But in reality we can also get resize events from things
			// like maximizing a window, especially with tiling window managers.
			// So we always just set the resize flag to true; on the next graphics
			// pass it will then go and set the size again to the correctly zoomed
			// value. If the size is the same, nothing happens.
			state->windowResize.store(true);
		}
		else if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased
				 || event.type == sf::Event::TextEntered) {
			// React to key presses, but only if they came from the corresponding display.
			if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::PageUp) {
				float currentZoomFactor = dvConfigNodeGetFloat(moduleData->moduleNode, "zoomFactor");

				currentZoomFactor += VISUALIZER_ZOOM_INC;

				// Clip zoom factor.
				if (currentZoomFactor > VISUALIZER_ZOOM_MAX) {
					currentZoomFactor = VISUALIZER_ZOOM_MAX;
				}

				dvConfigNodePutFloat(moduleData->moduleNode, "zoomFactor", currentZoomFactor);
			}
			else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::PageDown) {
				float currentZoomFactor = dvConfigNodeGetFloat(moduleData->moduleNode, "zoomFactor");

				currentZoomFactor -= VISUALIZER_ZOOM_INC;

				// Clip zoom factor.
				if (currentZoomFactor < VISUALIZER_ZOOM_MIN) {
					currentZoomFactor = VISUALIZER_ZOOM_MIN;
				}

				dvConfigNodePutFloat(moduleData->moduleNode, "zoomFactor", currentZoomFactor);
			}
			else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::Home) {
				// Reset zoom factor to default value.
				dvConfigNodePutFloat(moduleData->moduleNode, "zoomFactor", VISUALIZER_ZOOM_DEF);
			}
			else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::W) {
				int32_t currentSubsampling = dvConfigNodeGetInt(moduleData->moduleNode, "subsampleRendering");

				currentSubsampling--;

				// Clip subsampling factor.
				if (currentSubsampling < 1) {
					currentSubsampling = 1;
				}

				dvConfigNodePutInt(moduleData->moduleNode, "subsampleRendering", currentSubsampling);
			}
			else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::E) {
				int32_t currentSubsampling = dvConfigNodeGetInt(moduleData->moduleNode, "subsampleRendering");

				currentSubsampling++;

				// Clip subsampling factor.
				if (currentSubsampling > 100000) {
					currentSubsampling = 100000;
				}

				dvConfigNodePutInt(moduleData->moduleNode, "subsampleRendering", currentSubsampling);
			}
			else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::Q) {
				bool currentShowStatistics = dvConfigNodeGetBool(moduleData->moduleNode, "showStatistics");

				dvConfigNodePutBool(moduleData->moduleNode, "showStatistics", !currentShowStatistics);
			}
			else {
				// Forward event to user-defined event handler.
				if (state->eventHandler->eventHandler != nullptr) {
					(*state->eventHandler->eventHandler)((caerVisualizerPublicState) state, event);
				}
			}
		}
		else if (event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::MouseButtonReleased
				 || event.type == sf::Event::MouseWheelScrolled || event.type == sf::Event::MouseEntered
				 || event.type == sf::Event::MouseLeft || event.type == sf::Event::MouseMoved) {
			if (event.type == sf::Event::MouseWheelScrolled && event.mouseWheelScroll.delta > 0) {
				float currentZoomFactor = dvConfigNodeGetFloat(moduleData->moduleNode, "zoomFactor");

				currentZoomFactor += (VISUALIZER_ZOOM_INC * (float) event.mouseWheelScroll.delta);

				// Clip zoom factor.
				if (currentZoomFactor > VISUALIZER_ZOOM_MAX) {
					currentZoomFactor = VISUALIZER_ZOOM_MAX;
				}

				dvConfigNodePutFloat(moduleData->moduleNode, "zoomFactor", currentZoomFactor);
			}
			else if (event.type == sf::Event::MouseWheelScrolled && event.mouseWheelScroll.delta < 0) {
				float currentZoomFactor = dvConfigNodeGetFloat(moduleData->moduleNode, "zoomFactor");

				// Add because delta is negative for scroll-down.
				currentZoomFactor += (VISUALIZER_ZOOM_INC * (float) event.mouseWheelScroll.delta);

				// Clip zoom factor.
				if (currentZoomFactor < VISUALIZER_ZOOM_MIN) {
					currentZoomFactor = VISUALIZER_ZOOM_MIN;
				}

				dvConfigNodePutFloat(moduleData->moduleNode, "zoomFactor", currentZoomFactor);
			}
			else {
				// Forward event to user-defined event handler.
				if (state->eventHandler->eventHandler != nullptr) {
					(*state->eventHandler->eventHandler)((caerVisualizerPublicState) state, event);
				}
			}
		}
	}
}

static void renderScreen(dvModuleData moduleData) {
	caerVisualizerState state = (caerVisualizerState) moduleData->moduleState;

	// Handle resize and move first, so that when drawing the window is up-to-date.
	// Handle display resize (zoom and statistics).
	if (state->windowResize.exchange(false, std::memory_order_relaxed)) {
		// Update statistics flag and resize display appropriately.
		updateDisplaySize(state);
	}

	// Handle display move.
	if (state->windowMove.exchange(false, std::memory_order_relaxed)) {
		// Move display location appropriately.
		updateDisplayLocation(state);
	}

	// TODO: rethink this, implement max FPS control, FPS count,
	// and multiple render passes per displayed frame.
	caerEventPacketContainer container = (caerEventPacketContainer) caerRingBufferGet(state->dataTransfer);

repeat:
	if (container != nullptr) {
		// Are there others? Only render last one, to avoid getting backed up!
		caerEventPacketContainer container2 = (caerEventPacketContainer) caerRingBufferGet(state->dataTransfer);

		if (container2 != nullptr) {
			caerEventPacketContainerFree(container);
			container = container2;
			goto repeat;
		}
	}

	bool drewSomething = false;

	if (container != nullptr) {
		// Update render window with new content. (0, 0) is upper left corner.
		// NULL renderer is supported and simply does nothing (black screen).
		if (state->renderer->renderer != nullptr) {
			drewSomething = (*state->renderer->renderer)((caerVisualizerPublicState) state, container);
		}

		// Free packet container copy.
		caerEventPacketContainerFree(container);
	}

	// Render content to display.
	if (drewSomething) {
		// Render visual area border.
		sfml::Line borderX(
			sf::Vector2f(0, state->renderSizeY * state->renderZoomFactor.load(std::memory_order_relaxed)),
			sf::Vector2f(state->renderSizeX * state->renderZoomFactor.load(std::memory_order_relaxed),
				state->renderSizeY * state->renderZoomFactor.load(std::memory_order_relaxed)),
			2.0f, sf::Color::Blue);
		sfml::Line borderY(
			sf::Vector2f(state->renderSizeX * state->renderZoomFactor.load(std::memory_order_relaxed), 0),
			sf::Vector2f(state->renderSizeX * state->renderZoomFactor.load(std::memory_order_relaxed),
				state->renderSizeY * state->renderZoomFactor.load(std::memory_order_relaxed)),
			2.0f, sf::Color::Blue);
		state->renderWindow->draw(borderX);
		state->renderWindow->draw(borderY);

		// Render statistics string.
		// TODO: implement for OpenGL 3.3 too, using some text rendering library.
		bool doStatistics = (state->showStatistics && state->font != nullptr && !state->renderer->needsOpenGL3);

		if (doStatistics) {
			// Split statistics string in two to use less horizontal space.
			// Put it below the normal render region, so people can access from
			// (0,0) to (x-1,y-1) normally without fear of overwriting statistics.
			sf::Text totalEventsText(
				state->packetStatistics.currentStatisticsStringTotal, *state->font, GLOBAL_FONT_SIZE);
			sfml::Helpers::setTextColor(totalEventsText, sf::Color::White);
			totalEventsText.setPosition(
				GLOBAL_FONT_SPACING, state->renderSizeY * state->renderZoomFactor.load(std::memory_order_relaxed));
			state->renderWindow->draw(totalEventsText);

			sf::Text validEventsText(
				state->packetStatistics.currentStatisticsStringValid, *state->font, GLOBAL_FONT_SIZE);
			sfml::Helpers::setTextColor(validEventsText, sf::Color::White);
			validEventsText.setPosition(GLOBAL_FONT_SPACING,
				(state->renderSizeY * state->renderZoomFactor.load(std::memory_order_relaxed)) + GLOBAL_FONT_SIZE);
			state->renderWindow->draw(validEventsText);

			sf::Text GapEventsText(
				state->packetStatistics.currentStatisticsStringTSDiff, *state->font, GLOBAL_FONT_SIZE);
			sfml::Helpers::setTextColor(GapEventsText, sf::Color::White);
			GapEventsText.setPosition(
				GLOBAL_FONT_SPACING, (state->renderSizeY * state->renderZoomFactor.load(std::memory_order_relaxed))
										 + (2 * GLOBAL_FONT_SIZE));
			state->renderWindow->draw(GapEventsText);
		}

		// Draw to screen.
		state->renderWindow->display();

		// Reset window to all black for next rendering pass.
		state->renderWindow->clear(sf::Color::Black);
	}
}

registerModuleClass(Visualizer)
