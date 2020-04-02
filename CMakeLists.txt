cmake_minimum_required(VERSION 3.10)

# build options
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif (NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Werror -mtune=native -march=native")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("\n##########################")
    message("###### DEBUG BUILD #######")
    message("##########################\n")
else ()
    message("\n############################")
    message("###### RELEASE BUILD #######")
    message("############################\n")
endif ()

project(server017 VERSION 0.0.1 DESCRIPTION "Server of team 17 for the Sopra")

find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)

file(GLOB_RECURSE SOURCES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.hpp)

SET(${LIBS} LIBS spdlog::spdlog CLI11::CLI11 stdc++fs)

include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})
