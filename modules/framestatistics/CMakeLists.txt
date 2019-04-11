ADD_LIBRARY(framestatistics SHARED framestatistics.cpp)

SET_TARGET_PROPERTIES(framestatistics
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(framestatistics
	PRIVATE
		dvsdk
		Boost::boost
		${OpenCV_LIBS})

INSTALL(TARGETS framestatistics DESTINATION ${DV_MODULES_DIR})
