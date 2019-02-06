ADD_LIBRARY(dvsnoisefilter SHARED dvsnoisefilter.c)

SET_TARGET_PROPERTIES(dvsnoisefilter
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(dvsnoisefilter ${DV_LIBS})

INSTALL(TARGETS dvsnoisefilter DESTINATION ${DV_MODULES_DIR})
