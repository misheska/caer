ADD_LIBRARY(dvsnoisefilter SHARED dvsnoisefilter.cpp)

SET_TARGET_PROPERTIES(dvsnoisefilter
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(dvsnoisefilter
	PRIVATE
		dvsdk
		Boost::boost)

INSTALL(TARGETS dvsnoisefilter DESTINATION ${DV_MODULES_DIR})
