cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

project(server017)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif (NOT CMAKE_BUILD_TYPE)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("\n##########################")
    message("###### DEBUG BUILD #######")
    message("##########################\n")
else ()
    message("\n############################")
    message("###### RELEASE BUILD #######")
    message("############################\n")
endif ()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Werror -mtune=native -march=native")
set(CMAKE_CXX_FLAGS "-Wall")

file(GLOB_RECURSE SOURCES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.hpp
)

include_directories(externalLibs/CLI11/include/)

add_executable(${PROJECT_NAME} ${SOURCES})