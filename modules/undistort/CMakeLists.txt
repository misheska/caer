ADD_LIBRARY(undistort SHARED undistort.cpp)

SET_TARGET_PROPERTIES(undistort
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(undistort
	PRIVATE
		dvsdk
		Boost::boost
		${OpenCV_LIBS})

INSTALL(TARGETS undistort DESTINATION ${DV_MODULES_DIR})
