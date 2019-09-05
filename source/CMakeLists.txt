set(SOURCES
        Main.cpp
        Configurations/ParseConfigFile.cpp
        common/CommonFunction.cpp
        Applications/AppInstance.cpp
)

set(HEADERS
        Configurations/Configurations.hpp
        Configurations/ParseConfigFile.hpp
        common/CommonFunction.hpp
        Applications/AppInstance.hpp
)

INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

add_executable(${EXECUTABLE_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${EXECUTABLE_NAME}
    PRIVATE
        boost_program_options
        logger
        usbVideo
)

add_subdirectory ("logger")
add_subdirectory ("usbVideo")
add_subdirectory ("socket")