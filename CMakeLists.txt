# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
#Copyright (c) 2019 junshen
cmake_minimum_required (VERSION 3.8)

#must be added before use project() or enable_language()
#set(CMAKE_SYSTEM_NAME Linsee)
#set(CMAKE_C_COMPILER "/opt/gcc/x86_64/8.3.0a/bin/gcc")
#set(CMAKE_CXX_COMPILER "/opt/gcc/x86_64/8.3.0a/bin/g++")

# useless before set, need export environment variable
#CXX="/opt/gcc/x86_64/6.3.1a/bin/g++"
#CC="/opt/gcc/x86_64/6.3.1a/bin/gcc"

project ("Kitokei_Demo")
set(EXECUTABLE_NAME "Kitokei_Demo")

MESSAGE(STATUS "${CMAKE_CXX_COMPILER_ID}")
MESSAGE(STATUS "${CMAKE_SYSTEM_VERSION}")
MESSAGE(STATUS "${CMAKE_CXX_COMPILER}")

if("${CMAKE_SYSTEM_VERSION}" MATCHES "^.NSN.$")
    set(NSN_LINUX TRUE)
    MESSAGE(STATUS "NSN LINUX env.")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(ARM true)
    add_definitions(-DRUN_IN_ARM)
    MESSAGE(STATUS "ARM env.")
endif()

# according to the compile env

if(${ARM})
    INCLUDE_DIRECTORIES(
        "/usr/local/include"
    )

    LINK_DIRECTORIES(
        "/usr/local/lib"
    )

    #boost library
    set(Boost_MIN_VERSION "1.58.0")
    set(Boost_USE_MULTITHREADED ON)
    find_package(Boost 1.58.0 REQUIRED program_options filesystem)

    # include
    MESSAGE(STATUS "Boost Include Path: ${Boost_INCLUDE_DIRS}")
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
    # lib
    MESSAGE(STATUS "Boost Library Path: ${Boost_LIBRARY_DIRS}")
    LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

    #jrtp Library
    find_package(JRTPLIB REQUIRED)

    # include
    MESSAGE(STATUS "JRTP Include Path: ${JRTPLIB_INCLUDE_DIRS}")
    INCLUDE_DIRECTORIES(${JRTPLIB_INCLUDE_DIRS})
    # lib
    MESSAGE(STATUS "JRTP Library Path: ${JRTPLIB_LIBRARIES}")
    LINK_DIRECTORIES(${JRTPLIB_LIBRARIES})
endif()

 set(COMMON_FLAGS ${CMAKE_CXX_FLAGS})
 set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=c++14")

# 包含子项目。
add_subdirectory ("source")
