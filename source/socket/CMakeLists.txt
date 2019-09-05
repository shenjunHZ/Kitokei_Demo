set(MODULE_NAME socket)
project(${MODULE_NAME} CXX)

message(STATUS "Configuring ${MODULE_NAME}")

set(SOURCES
        src/ClientSocket.cpp
        src/SocketSysCall.cpp
    )

set(HEADERS
        include/socket/ISocketSysCall.hpp
        include/socket/IDataListener.hpp
        include/socket/SocketSysCall.hpp
        include/socket/ClientSocket.hpp
    )

# create lib
add_library(${MODULE_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(socket 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE 
        src
    )

target_link_libraries(${MODULE_NAME}
    PRIVATE
        logger
    )