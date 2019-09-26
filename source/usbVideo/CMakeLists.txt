set(MODULE_NAME usbVideo)
project(${MODULE_NAME} CXX)

message(STATUS "Configuring ${MODULE_NAME}")

set(SOURCES
        src/CameraControl.cpp
        src/CameraProcess.cpp
        src/CameraImage.cpp
        src/EncodeCameraStream.cpp
        src/StreamProcess.cpp
        src/VideoManagement.cpp
    )

set(HEADERS
        include/usbVideo/ICameraControl.hpp
        include/usbVideo/CameraControl.hpp
        include/usbVideo/CameraProcess.hpp
        include/usbVideo/CameraImage.hpp
        include/usbVideo/IEncodeCameraStream.hpp
        include/usbVideo/EncodeCameraStream.hpp
        include/usbVideo/StreamProcess.hpp
        include/usbVideo/IVideoManagement.hpp
        include/usbVideo/VideoManagement.hpp
    )

MESSAGE(STATUS ${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# create lib
add_library(${MODULE_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(usbVideo 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE 
        src
    )

target_link_libraries(${MODULE_NAME}
    PRIVATE
        logger
        timer
        avformat
        avdevice
        avcodec
        avutil
        avfilter
        postproc
        swresample
        swscale
        -lpthread
        -ldl
        -lbz2
        -lm -lz
        -llzma
        -lgnutls
        -ldrm
        -lrockchip_mpp
    )
 #libz.a
 #libm.a