# Set full RPATH, modules are libraries for dv-runtime
SET(CMAKE_INSTALL_RPATH ${USER_LOCAL_PREFIX}/${DV_MODULES_DIR})

# Add SDK library to default libs
SET(DV_LIBS ${DV_LIBS} dvsdk)

ADD_SUBDIRECTORY(cameracalibration)
ADD_SUBDIRECTORY(cameras)
ADD_SUBDIRECTORY(dvsnoisefilter)
ADD_SUBDIRECTORY(frameenhancer)
ADD_SUBDIRECTORY(framestatistics)
ADD_SUBDIRECTORY(inout)
ADD_SUBDIRECTORY(statistics)
ADD_SUBDIRECTORY(visualizer)
