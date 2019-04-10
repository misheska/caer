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

TARGET_LINK_LIBRARIES(dv-control
	PRIVATE
		libcaer::caer
		Boost::boost
		Boost::system
		Boost::filesystem
		Boost::iostreams
		Boost::program_options
		${BOOST_ASIO_LIBS})
TARGET_INCLUDE_DIRECTORIES(dv-control
	PRIVATE
		${BOOST_ASIO_INCLUDE_DIRS})

INSTALL(TARGETS dv-control DESTINATION ${CMAKE_INSTALL_BINDIR})
