ADD_LIBRARY(frameenhancer SHARED frameenhancer.cpp)

SET_TARGET_PROPERTIES(frameenhancer
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(frameenhancer
	PRIVATE
		dvsdk
		${OpenCV_LIBS})

INSTALL(TARGETS frameenhancer DESTINATION ${DV_MODULES_DIR})
