cmake_minimum_required(VERSION 3.10)
set(SOURCES
        main.cpp
        network/MessageRouter.cpp
        Server.cpp
        util/Player.cpp
        OperationHandling.cpp
        OperationHandling.hpp)

include_directories(.)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_CXX_FLAGS})