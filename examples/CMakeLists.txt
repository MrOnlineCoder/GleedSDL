cmake_minimum_required(VERSION 3.16)

add_executable(sdl_movie_basic basic.cpp)
add_executable(sdl_movie_player player.cpp)
add_executable(mtest test.cpp)

target_link_libraries(sdl_movie_basic PRIVATE SDL3::SDL3 SDL_movie)
target_link_libraries(sdl_movie_player PRIVATE SDL3::SDL3 SDL_movie)
target_link_libraries(mtest PRIVATE SDL3::SDL3 SDL_movie)