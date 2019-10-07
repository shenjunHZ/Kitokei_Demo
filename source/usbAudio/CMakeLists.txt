set(MODULE_NAME usbAudio)
project(${MODULE_NAME} CXX)

message(STATUS "Configuring ${MODULE_NAME}")

set(SOURCES
        src/LinuxRec.cpp
        src/AudioRecordService.cpp
    )

set(HEADERS
        include/usbAudio/ISysRec.hpp
        include/usbAudio/LinuxRec.hpp
        include/usbAudio/IAudioRecordService.hpp
        include/usbAudio/AudioRecordService.hpp
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
        asound
    )