# DVS128
ADD_LIBRARY(dvs128 SHARED dvs128.c aedat4_convert.cpp)

SET_TARGET_PROPERTIES(dvs128
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(dvs128
	PRIVATE
		libcaer::caer
		dvsdk)

INSTALL(TARGETS dvs128 DESTINATION ${DV_MODULES_DIR})

# eDVS4337
ADD_LIBRARY(edvs SHARED edvs.c aedat4_convert.cpp)

SET_TARGET_PROPERTIES(edvs
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(edvs
	PRIVATE
		libcaer::caer
		dvsdk)

INSTALL(TARGETS edvs DESTINATION ${DV_MODULES_DIR})

# DAVIS
ADD_LIBRARY(davis SHARED davis.c aedat4_convert.cpp)

SET_TARGET_PROPERTIES(davis
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(davis
	PRIVATE
		libcaer::caer
		dvsdk)

INSTALL(TARGETS davis DESTINATION ${DV_MODULES_DIR})

IF (OS_LINUX)
	# Raspberry-Pi camera module support (Linux-only).
	ADD_LIBRARY(davis_rpi SHARED davis_rpi.c aedat4_convert.cpp)

	SET_TARGET_PROPERTIES(davis_rpi
		PROPERTIES
		PREFIX "dv_"
	)

	TARGET_LINK_LIBRARIES(davis_rpi
		PRIVATE
			libcaer::caer
			dvsdk)

	INSTALL(TARGETS davis_rpi DESTINATION ${DV_MODULES_DIR})
ENDIF()

# DVS132S
ADD_LIBRARY(dvs132s SHARED dvs132s.c aedat4_convert.cpp)

SET_TARGET_PROPERTIES(dvs132s
    PROPERTIES
    PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(dvs132s
	PRIVATE
		libcaer::caer
		dvsdk)

INSTALL(TARGETS dvs132s DESTINATION ${DV_MODULES_DIR})
