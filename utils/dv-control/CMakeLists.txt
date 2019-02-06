# Compile dv-control (CLI control utility)
ADD_EXECUTABLE(dv-control
	../ext/linenoise-ng/ConvertUTF.cpp
	../ext/linenoise-ng/wcwidth.cpp
	../ext/linenoise-ng/linenoise.cpp
	dv-control.cpp)
TARGET_LINK_LIBRARIES(dv-control ${DV_LIBS} dvsdk)
INSTALL(TARGETS dv-control DESTINATION ${CMAKE_INSTALL_BINDIR})
