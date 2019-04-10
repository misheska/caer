IF (NOT ENABLE_VISUALIZER)
	SET(ENABLE_VISUALIZER 0 CACHE BOOL "Enable the visualizer module. This is deprecated in favor of dv-gui!")
ENDIF()

IF (ENABLE_VISUALIZER)
	FIND_PACKAGE(SFML 2.3.0 REQUIRED COMPONENTS graphics)
	FIND_PACKAGE(GLEW 1.10.0 REQUIRED)

	ADD_LIBRARY(visualizer SHARED visualizer.cpp visualizer_renderers.cpp)

	SET_TARGET_PROPERTIES(visualizer
		PROPERTIES
		PREFIX "dv_"
	)

	TARGET_LINK_LIBRARIES(visualizer
		PRIVATE
			dvsdk
			sfml-graphics
			GLEW::GLEW)

	INSTALL(TARGETS visualizer DESTINATION ${DV_MODULES_DIR})
ENDIF()
