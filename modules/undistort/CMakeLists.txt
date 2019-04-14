# OpenCV support with extra components.
FIND_PACKAGE(OpenCV REQUIRED COMPONENTS core imgproc calib3d)
IF (OpenCV_VERSION VERSION_LESS "3.1.0")
	MESSAGE(FATAL_ERROR "Cannot find OpenCV 3.1.0 or newer.")
ENDIF()

ADD_LIBRARY(undistort SHARED undistort.cpp)

SET_TARGET_PROPERTIES(undistort
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(undistort
	PRIVATE
		dvsdk
		${OpenCV_LIBS})

INSTALL(TARGETS undistort DESTINATION ${DV_MODULES_DIR})
