# OpenCV support with extra components.
FIND_PACKAGE(OpenCV REQUIRED COMPONENTS core imgproc calib3d)
IF (OpenCV_VERSION VERSION_LESS "3.1.0")
	MESSAGE(FATAL_ERROR "Cannot find OpenCV 3.1.0 or newer.")
ENDIF()

ADD_LIBRARY(lenscalibration SHARED lenscalibration.cpp)

SET_TARGET_PROPERTIES(lenscalibration
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(lenscalibration
	PRIVATE
		dvsdk
		Boost::boost
		${OpenCV_LIBS})

INSTALL(TARGETS lenscalibration DESTINATION ${DV_MODULES_DIR})
