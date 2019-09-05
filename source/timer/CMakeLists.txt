set(MODULE_NAME timer)
project(${MODULE_NAME} CXX)

message(STATUS "Configuring ${MODULE_NAME}")

set(SOURCES
        src/DefaultTimerService.cpp
        src/IOService.cpp
)

set(HEADERS
        include/timer/DefaultTimerService.hpp
        include/timer/IOService.hpp
        include/timer/Timer.hpp
        include/timer/TimerIdGenerator.hpp
        include/timer/TimerService.hpp
)

add_library(${MODULE_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${MODULE_NAME} 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${MODULE_NAME}
    PUBLIC
        logger

        #PRIVATE
        #${Boost_LIBRARIES}
)