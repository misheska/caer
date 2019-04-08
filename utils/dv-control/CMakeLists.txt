# Compile dv-control (CLI control utility)
ADD_EXECUTABLE(dv-control
	# Linenoise line-editing support.
	../ext/linenoise-ng/ConvertUTF.cpp
	../ext/linenoise-ng/wcwidth.cpp
	../ext/linenoise-ng/linenoise.cpp
	# Standalone library part.
	../../src/sdk_lib/sdk_cross.cpp
	../../src/config_tree/tree.cpp
	../../src/config_tree/helper.cpp
	../../src/config_tree/node.cpp
	# Executable.
	dv-control.cpp)
TARGET_LINK_LIBRARIES(dv-control ${DV_LIBS})
INSTALL(TARGETS dv-control DESTINATION ${CMAKE_INSTALL_BINDIR})
