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

set(SOURCES
        src/main.cpp
        src/network/MessageRouter.cpp)

include_directories(src)
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_CXX_FLAGS})

add_subdirectory(test)