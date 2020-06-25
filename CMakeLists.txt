cmake_minimum_required(VERSION 3.10)
project(server017 VERSION 0.0.1 DESCRIPTION "Server of team 17 for the Sopra")

# build options
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif (NOT CMAKE_BUILD_TYPE)

set(COMMON_CXX_FLAGS -Wall -Wextra -Wpedantic -Werror -mtune=native -march=native)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("\n##########################")
    message("###### DEBUG BUILD #######")
    message("##########################\n")
    list(APPEND COMMON_CXX_FLAGS -Og --coverage)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
else ()
    message("\n############################")
    message("###### RELEASE BUILD #######")
    message("############################\n")
    list(APPEND COMMON_CXX_FLAGS -O3)
endif ()

# Libraries
# spdlog
find_package(spdlog REQUIRED)
list(APPEND LIBS spdlog::spdlog)

# CLI11
find_package(CLI11 REQUIRED)
add_definitions(-DCLI11_HAS_FILESYSTEM=1)
list(APPEND LIBS CLI11::CLI11 stdc++fs)

# LibCommon
add_subdirectory(external/LibCommon)
list(APPEND LIBS SopraCommon)

# WebsocketCPP
add_subdirectory(external/WebsocketCPP)
list(APPEND LIBS SopraNetwork)

# afsm
find_file(AFSM_FOUND afsm/fsm.hpp HINTS /usr/local/include)
if (AFSM_FOUND)
    message("afsm found at ${AFSM_FOUND}")
else ()
    message(FATAL_ERROR "afsm not found")
endif ()

# gtest for LibCommon
# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if (result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif ()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if (result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif ()
# Add googletest directly to the build
add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/googletest-src"
        "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")


add_subdirectory(src)
add_subdirectory(test)