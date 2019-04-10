ADD_LIBRARY(frameenhancer SHARED frameenhancer.cpp)

SET_TARGET_PROPERTIES(frameenhancer
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(frameenhancer
	PRIVATE
		dvsdk
		Boost::boost
		${OpenCV_LIBS})
TARGET_INCLUDE_DIRECTORIES(frameenhancer
	PRIVATE
		${OpenCV_INCLUDE_DIRS})

INSTALL(TARGETS frameenhancer DESTINATION ${DV_MODULES_DIR})
