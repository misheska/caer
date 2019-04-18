# Compile dv-control (CLI control utility)
ADD_EXECUTABLE(dv-control
	# Linenoise line-editing support.
	../ext/linenoise-ng/ConvertUTF.cpp
	../ext/linenoise-ng/wcwidth.cpp
	../ext/linenoise-ng/linenoise.cpp
	# Executable.
	dv-control.cpp)

TARGET_LINK_LIBRARIES(dv-control
	PRIVATE
		dvsdk
		Boost::boost
		Boost::system
		Boost::filesystem
		Boost::program_options
		${BOOST_ASIO_LIBRARIES})

INSTALL(TARGETS dv-control DESTINATION ${CMAKE_INSTALL_BINDIR})
