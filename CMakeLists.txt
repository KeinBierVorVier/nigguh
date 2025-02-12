cmake_minimum_required(VERSION 3.28)
project(discord_bot VERSION 1.0 DESCRIPTION "Nigger")

set(CMAKE_CXX_STANDARD 20)
add_subdirectory(libs/DPP)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} dpp)

target_include_directories(${PROJECT_NAME} PRIVATE libs/DPP/include)
