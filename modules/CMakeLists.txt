# Set full RPATH, modules are libraries for dv-runtime
SET(CMAKE_INSTALL_RPATH ${USER_LOCAL_PREFIX}/${DV_MODULES_DIR})

#ADD_SUBDIRECTORY(cameracalibration)
#ADD_SUBDIRECTORY(cameras)
ADD_SUBDIRECTORY(dvsnoisefilter)
ADD_SUBDIRECTORY(frameenhancer)
ADD_SUBDIRECTORY(framestatistics)
#ADD_SUBDIRECTORY(inout)
#ADD_SUBDIRECTORY(visualizer)
