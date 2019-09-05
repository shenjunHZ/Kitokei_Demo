set(MODULE_NAME logger)
project(${MODULE_NAME} CXX)

message(STATUS "Configuring ${MODULE_NAME}")

#find_package(spdlog REQUIRED CONFIG ONLY_CMAKE_FIND_ROOT_PATH)

set(SOURCES
        src/Logger.cpp
        src/LoggerConfiguration2stdout.cpp
        )

set(HEADERS
        include/logger/Logger.hpp
        include/logger/LoggerConfiguration.hpp
        include/logger/LoggerEnabled.hpp
        include/logger/LoggerFwd.hpp
        include/logger/LoggerImpl.hpp
        include/logger/LoggerPriorityTags.hpp
        )

# create lib
add_library(logger STATIC ${SOURCES} ${HEADERS})


target_include_directories(logger PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src
        )

option(ENABLE_LOGGING "ENABLE_LOGGING defaults to 'ON'" ON)

if(ENABLE_LOGGING)
    target_compile_definitions(logger PUBLIC -DENABLE_LOGGING)
endif()

target_link_libraries(logger PUBLIC
        -lpthread
#        spdlog::spdlog
        )
