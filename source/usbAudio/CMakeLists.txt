set(MODULE_NAME usbAudio)
project(${MODULE_NAME} CXX)

message(STATUS "Configuring ${MODULE_NAME}")

set(SOURCES
        src/LinuxAlsa.cpp
        src/AudioRecordService.cpp
        src/AudioPlaybackService.cpp
    )

set(HEADERS
        include/usbAudio/ISysAlsa.hpp
        include/usbAudio/LinuxAlsa.hpp
        include/usbAudio/IAudioRecordService.hpp
        include/usbAudio/AudioRecordService.hpp
        include/usbAudio/IAudioPlaybackService.hpp
        include/usbAudio/AudioPlaybackService.hpp
    )

MESSAGE(STATUS ${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# create lib
add_library(${MODULE_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(usbAudio 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE 
        src
    )

target_link_libraries(${MODULE_NAME}
    PRIVATE
        logger
        socket
        -lasound
    )