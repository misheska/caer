#ifndef VISUALIZER_HPP_
#define VISUALIZER_HPP_

#include "dv-sdk/utils.h"

#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <atomic>

struct caer_visualizer_public_state {
	dv::Config::Node visualizerConfigNode;
	uint32_t renderSizeX;
	uint32_t renderSizeY;
	std::atomic<float> renderZoomFactor;
	void *renderState; // Reserved for renderers to put their internal state into.
	sf::RenderWindow *renderWindow;
	sf::Font *font;
};

typedef struct caer_visualizer_public_state *caerVisualizerPublicState;

// This define is for Render State Init functions that allocate no memory, to use as return value.
#define DV_VISUALIZER_RENDER_INIT_NO_MEM static_cast<void *>(0x01)

#endif /* VISUALIZER_HPP_ */
