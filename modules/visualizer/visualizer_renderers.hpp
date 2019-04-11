#ifndef VISUALIZER_RENDERERS_HPP_
#define VISUALIZER_RENDERERS_HPP_

#include "visualizer.hpp"

typedef bool (*caerVisualizerRenderer)(caerVisualizerPublicState state, void *packets);

typedef void *(*caerVisualizerRendererStateInit)(caerVisualizerPublicState state);
typedef void (*caerVisualizerRendererStateExit)(caerVisualizerPublicState state);

struct caer_visualizer_renderer_info {
	const std::string name;
	caerVisualizerRenderer renderer;
	caerVisualizerRendererStateInit stateInit;
	caerVisualizerRendererStateExit stateExit;

	caer_visualizer_renderer_info(const std::string &n, caerVisualizerRenderer r,
		caerVisualizerRendererStateInit stInit = nullptr, caerVisualizerRendererStateExit stExit = nullptr) :
		name(n),
		renderer(r),
		stateInit(stInit),
		stateExit(stExit) {
	}
};

typedef const struct caer_visualizer_renderer_info *caerVisualizerRendererInfo;

extern const std::string caerVisualizerRendererListOptionsString;
extern const struct caer_visualizer_renderer_info caerVisualizerRendererList[];
extern const size_t caerVisualizerRendererListLength;

#endif /* VISUALIZER_RENDERERS_HPP_ */
