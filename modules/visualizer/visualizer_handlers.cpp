#include "visualizer_handlers.hpp"

#include "dv-sdk/mainloop.h"

#include <boost/algorithm/string.hpp>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

// Default event handlers.
static void caerVisualizerEventHandlerInput(caerVisualizerPublicState state, const sf::Event &event);

const std::string caerVisualizerEventHandlerListOptionsString = "Input";

const struct caer_visualizer_event_handler_info caerVisualizerEventHandlerList[]
	= {{"None", nullptr}, {"Input", &caerVisualizerEventHandlerInput}};

const size_t caerVisualizerEventHandlerListLength
	= (sizeof(caerVisualizerEventHandlerList) / sizeof(struct caer_visualizer_event_handler_info));

static void caerVisualizerEventHandlerInput(caerVisualizerPublicState state, const sf::Event &event) {
	// This only works with an input module.
	const std::string moduleLibrary = state->eventSourceConfigNode.get<dvCfgType::STRING>("moduleLibrary");
	if (!boost::algorithm::starts_with(moduleLibrary, "caer_input_")) {
		return;
	}

	// PAUSE.
	if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::Space) {
		bool pause = state->eventSourceConfigNode.get<dvCfgType::BOOL>("pause");

		state->eventSourceConfigNode.put<dvCfgType::BOOL>("pause", !pause);
	}
	// SLOW DOWN.
	else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::S) {
		int timeSlice = state->eventSourceConfigNode.get<dvCfgType::INT>("PacketContainerInterval");

		state->eventSourceConfigNode.put<dvCfgType::BOOL>("PacketContainerInterval", timeSlice / 2);
	}
	// SPEED UP.
	else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Key::F) {
		int timeSlice = state->eventSourceConfigNode.get<dvCfgType::BOOL>("PacketContainerInterval");

		state->eventSourceConfigNode.put<dvCfgType::BOOL>("PacketContainerInterval", timeSlice * 2);
	}
}
