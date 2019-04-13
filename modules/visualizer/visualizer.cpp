#include "dv-sdk/data/event.hpp"
#include "dv-sdk/data/frame.hpp"
#include "dv-sdk/data/imu.hpp"
#include "dv-sdk/module.hpp"

#include "ext/fonts/LiberationSans-Bold.h"
#include "ext/sfml/helpers.hpp"
#include "ext/sfml/line.hpp"
#include "visualizer_renderers.hpp"

#include <SFML/Graphics.hpp>
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
#define VISUALIZER_SUBSAMPLE_MIN 1
#define VISUALIZER_SUBSAMPLE_MAX 10000

class Visualizer : public dv::ModuleBase {
private:
	int16_t renderSizeX;
	int16_t renderSizeY;
	sf::RenderWindow renderWindow;
	sf::Font renderFont;
	std::unique_ptr<Renderer> renderer;
	bool windowResize;
	bool windowMove;
	int32_t packetSubsampleCount;

	// Track system init.
	static std::once_flag visualizerSystemIsInitialized;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("visualize", "ANYT", false);
	}

	static const char *getDescription() {
		return ("Visualize data in various simple ways. Please use dv-gui instead!");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add("subsampleRendering",
			dv::ConfigOption::intOption("Speed-up rendering by only taking every Nth EventPacketContainer to render.",
				1, VISUALIZER_SUBSAMPLE_MIN, VISUALIZER_SUBSAMPLE_MAX));

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

	Visualizer() : windowResize(false), windowMove(false), packetSubsampleCount(0) {
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
			renderer    = std::make_unique<EventRenderer>(renderSizeX, renderSizeY, &renderWindow, &renderFont);
		}
		else if (type == dv::Frame::identifier) {
			// Size from output info.
			renderSizeX = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeX"));
			renderSizeY = static_cast<int16_t>(info.get<dvCfgType::INT>("sizeY"));
			renderer    = std::make_unique<FrameRenderer>(renderSizeX, renderSizeY, &renderWindow, &renderFont);
		}
		else if (type == dv::IMUPacket::identifier) {
			// Fixed size.
			renderSizeX = 256;
			renderSizeY = 256;
			renderer    = std::make_unique<IMURenderer>(renderSizeX, renderSizeY, &renderWindow, &renderFont);
		}
		else {
			throw std::runtime_error("Type '" + type + "' is not supported by the visualizer.");
		}

		// Load font to have it always available.
		if (!renderFont.loadFromMemory(LiberationSans_Bold_ttf, LiberationSans_Bold_ttf_len)) {
			throw std::runtime_error("Failed to load display font.");
		}

		// Initialize graphics on this thread.
		// Create OpenGL 2.1 default (compatibility) context, so it can be used with SFML graphics.e.
		sf::ContextSettings openGLSettings;

		openGLSettings.depthBits      = 24;
		openGLSettings.stencilBits    = 8;
		openGLSettings.majorVersion   = 2;
		openGLSettings.minorVersion   = 1;
		openGLSettings.attributeFlags = sf::ContextSettings::Default;

		// Create display window and set its title.
		new (&renderWindow) sf::RenderWindow(
			sf::VideoMode(static_cast<unsigned int>(renderSizeX), static_cast<unsigned int>(renderSizeY)),
			moduleNode.getName(), sf::Style::Titlebar | sf::Style::Close, openGLSettings);

		// Set frame rate limit.
		renderWindow.setFramerateLimit(VISUALIZER_REFRESH_RATE);

		// Set scale transform for display window, update sizes.
		updateDisplaySize();

		// Set window position.
		updateDisplayLocation();

		// Ensure OpenGL context is active.
		renderWindow.setActive(true);

		// Initialize window by clearing it to all black.
		renderWindow.clear(sf::Color::Black);
		renderWindow.display();
	}

	~Visualizer() override {
		// Save visualizer window location in config.
		saveDisplayLocation();

		// Close rendering window and free memory.
		renderWindow.close();
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

		handleEvents();

		renderScreen();
	}

	void advancedConfigUpdate() override {
		// Here we don't know what changed, just that something changed,
		// so we force both resize and move updates, if nothing really
		// changed, they're both pretty cheap.
		windowResize = true;
		windowMove   = true;
	}

	void updateDisplaySize() {
		float zoomFactor = config.get<dvCfgType::FLOAT>("zoomFactor");

		// Update zoom factor.
		renderer->setZoomFactor(zoomFactor);

		sf::Vector2u newRenderWindowSize(
			static_cast<unsigned int>(renderSizeX), static_cast<unsigned int>(renderSizeY));

		// Apply zoom to rendered content only, not statistics.
		newRenderWindowSize.x = static_cast<unsigned int>(newRenderWindowSize.x * zoomFactor);
		newRenderWindowSize.y = static_cast<unsigned int>(newRenderWindowSize.y * zoomFactor);

		// Set window size to zoomed area (only if value changed!).
		sf::Vector2u oldSize = renderWindow.getSize();

		if ((newRenderWindowSize.x != oldSize.x) || (newRenderWindowSize.y != oldSize.y)) {
			renderWindow.setSize(newRenderWindowSize);

			// Set view size to render area.
			renderWindow.setView(sf::View(sf::FloatRect(0, 0, newRenderWindowSize.x, newRenderWindowSize.y)));
		}
	}

	void updateDisplayLocation() {
		// Set current position to what is in configuration storage.
		const sf::Vector2i newPos(
			config.get<dvCfgType::INT>("windowPositionX"), config.get<dvCfgType::INT>("windowPositionY"));

		renderWindow.setPosition(newPos);
	}

	void saveDisplayLocation() {
		const sf::Vector2i currPos = renderWindow.getPosition();

		// Update current position in configuration storage.
		config.set<dvCfgType::INT>("windowPositionX", currPos.x);
		config.set<dvCfgType::INT>("windowPositionY", currPos.y);
	}

	void handleEvents() {
		sf::Event event;

		while (renderWindow.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				// Stop visualizer module on window close.
				config.set<dvCfgType::BOOL>("running", false);
			}
			else if (event.type == sf::Event::Resized) {
				// Handle resize events, the window is non-resizeable, so in theory all
				// resize events should come from our zoom resizes, and thus we can just
				// ignore them. But in reality we can also get resize events from things
				// like maximizing a window, especially with tiling window managers.
				// So we always just set the resize flag to true; on the next graphics
				// pass it will then go and set the size again to the correctly zoomed
				// value. If the size is the same, nothing happens.
				windowResize = true;
			}
			else if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased
					 || event.type == sf::Event::TextEntered) {
				// React to key presses, but only if they came from the corresponding display.
				if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::PageUp) {
					float currentZoomFactor = config.get<dvCfgType::FLOAT>("zoomFactor");

					currentZoomFactor += VISUALIZER_ZOOM_INC;

					// Clip zoom factor.
					if (currentZoomFactor > VISUALIZER_ZOOM_MAX) {
						currentZoomFactor = VISUALIZER_ZOOM_MAX;
					}

					config.set<dvCfgType::FLOAT>("zoomFactor", currentZoomFactor);
				}
				else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::PageDown) {
					float currentZoomFactor = config.get<dvCfgType::FLOAT>("zoomFactor");

					currentZoomFactor -= VISUALIZER_ZOOM_INC;

					// Clip zoom factor.
					if (currentZoomFactor < VISUALIZER_ZOOM_MIN) {
						currentZoomFactor = VISUALIZER_ZOOM_MIN;
					}

					config.set<dvCfgType::FLOAT>("zoomFactor", currentZoomFactor);
				}
				else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::Home) {
					// Reset zoom factor to default value.
					config.set<dvCfgType::FLOAT>("zoomFactor", VISUALIZER_ZOOM_DEF);
				}
				else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::W) {
					int32_t currentSubsampling = config.get<dvCfgType::INT>("subsampleRendering");

					currentSubsampling--;

					// Clip subsampling factor.
					if (currentSubsampling < VISUALIZER_SUBSAMPLE_MIN) {
						currentSubsampling = VISUALIZER_SUBSAMPLE_MIN;
					}

					config.set<dvCfgType::INT>("subsampleRendering", currentSubsampling);
				}
				else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::E) {
					int32_t currentSubsampling = config.get<dvCfgType::INT>("subsampleRendering");

					currentSubsampling++;

					// Clip subsampling factor.
					if (currentSubsampling > VISUALIZER_SUBSAMPLE_MAX) {
						currentSubsampling = VISUALIZER_SUBSAMPLE_MAX;
					}

					config.set<dvCfgType::INT>("subsampleRendering", currentSubsampling);
				}
			}
			else if (event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::MouseButtonReleased
					 || event.type == sf::Event::MouseWheelScrolled || event.type == sf::Event::MouseEntered
					 || event.type == sf::Event::MouseLeft || event.type == sf::Event::MouseMoved) {
				if (event.type == sf::Event::MouseWheelScrolled && event.mouseWheelScroll.delta > 0) {
					float currentZoomFactor = config.get<dvCfgType::FLOAT>("zoomFactor");

					currentZoomFactor += (VISUALIZER_ZOOM_INC * static_cast<float>(event.mouseWheelScroll.delta));

					// Clip zoom factor.
					if (currentZoomFactor > VISUALIZER_ZOOM_MAX) {
						currentZoomFactor = VISUALIZER_ZOOM_MAX;
					}

					config.set<dvCfgType::FLOAT>("zoomFactor", currentZoomFactor);
				}
				else if (event.type == sf::Event::MouseWheelScrolled && event.mouseWheelScroll.delta < 0) {
					float currentZoomFactor = config.get<dvCfgType::FLOAT>("zoomFactor");

					// Add because delta is negative for scroll-down.
					currentZoomFactor += (VISUALIZER_ZOOM_INC * static_cast<float>(event.mouseWheelScroll.delta));

					// Clip zoom factor.
					if (currentZoomFactor < VISUALIZER_ZOOM_MIN) {
						currentZoomFactor = VISUALIZER_ZOOM_MIN;
					}

					config.set<dvCfgType::FLOAT>("zoomFactor", currentZoomFactor);
				}
			}
		}
	}

	void renderScreen() {
		// Handle resize and move first, so that when drawing the window is up-to-date.
		// Handle display resize due to zoom.
		if (windowResize) {
			// Update statistics flag and resize display appropriately.
			updateDisplaySize();
		}

		// Handle display move.
		if (windowMove) {
			// Move display location appropriately.
			updateDisplayLocation();
		}

		// Get data container to visualize.
		auto typedObject = dvModuleInputGet(moduleData, "visualize");

		if (typedObject != nullptr) {
		repeat:
			// Are there others? Only render last one, to avoid getting backed up!
			auto typedObject2 = dvModuleInputGet(moduleData, "visualize");

			if (typedObject2 != nullptr) {
				dvModuleInputDismiss(moduleData, "visualize", typedObject);
				typedObject = typedObject2;
				goto repeat;
			}
		}

		if (typedObject != nullptr) {
			// Update render window with new content. (0, 0) is upper left corner.
			renderer->render(typedObject->obj);

			// Done with data container.
			dvModuleInputDismiss(moduleData, "visualize", typedObject);
		}

		// Render content to display.
		// Render visual area border.
		float renderZoomFactor = config.get<dvCfgType::FLOAT>("zoomFactor");

		sfml::Line borderX(sf::Vector2f(0, renderSizeY * renderZoomFactor),
			sf::Vector2f(renderSizeX * renderZoomFactor, renderSizeY * renderZoomFactor), 2.0f, sf::Color::Blue);
		sfml::Line borderY(sf::Vector2f(renderSizeX * renderZoomFactor, 0),
			sf::Vector2f(renderSizeX * renderZoomFactor, renderSizeY * renderZoomFactor), 2.0f, sf::Color::Blue);
		renderWindow.draw(borderX);
		renderWindow.draw(borderY);

		// Draw to screen.
		renderWindow.display();

		// Reset window to all black for next rendering pass.
		renderWindow.clear(sf::Color::Black);
	}
};

registerModuleClass(Visualizer)
