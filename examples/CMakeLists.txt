cmake_minimum_required(VERSION 3.16)

add_executable(gleed_basic basic.cpp)
add_executable(gleed_player player.cpp)

target_link_libraries(gleed_basic PRIVATE SDL3::SDL3 Gleed)
target_link_libraries(gleed_player PRIVATE SDL3::SDL3 Gleed)