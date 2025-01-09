cmake_minimum_required(VERSION 3.16)

add_executable(sdl_movie_example main.cpp)

target_link_libraries(sdl_movie_example PRIVATE SDL3::SDL3 SDL_movie)