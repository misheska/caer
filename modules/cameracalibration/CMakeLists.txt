ADD_LIBRARY(cameracalibration SHARED cameracalibration.c calibration.cpp calibration_wrapper.cpp)

SET_TARGET_PROPERTIES(cameracalibration
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(cameracalibration ${DV_LIBS})

INSTALL(TARGETS cameracalibration DESTINATION ${DV_MODULES_DIR})
