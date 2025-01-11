cmake_minimum_required(VERSION 3.20)

include(FetchContent)

cmake_policy(SET CMP0074 NEW)

set(SDL_REQUIRED_VERSION 3.0.0)
set(C_STANDARD 99)

set(CMAKE_BUILD_TYPE Debug)

set(MAJOR_VERSION 3)
set(MINOR_VERSION 0)
set(MICRO_VERSION 0)
project(SDL3_movie
    LANGUAGES C CXX
    VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}"
)

FetchContent_Declare(
    SDL3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL"
    GIT_TAG "main"
)
FetchContent_MakeAvailable(SDL3)


set(ENABLE_WEBM_PARSER ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    libwebm
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_REPOSITORY "https://github.com/webmproject/libwebm"
    GIT_TAG "libwebm-1.0.0.31"
)
FetchContent_MakeAvailable(libwebm)

FetchContent_Declare(
    libogg
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_REPOSITORY "https://github.com/xiph/ogg"
    GIT_TAG "v1.3.5"
)
FetchContent_MakeAvailable(libogg)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    libvorbis
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_REPOSITORY "https://github.com/xiph/vorbis"
    GIT_TAG "v1.3.7"
)
FetchContent_MakeAvailable(libvorbis)


include(libvpx.cmake)

set(LIB_SOURCES 
    src/SDL_movie_webm.cpp
    src/SDL_movie_vpx.c
    src/SDL_movie.c
    src/SDL_movie_vorbis.c
    src/SDL_movie_player.c
)

add_library(
    SDL_movie
    STATIC
    ${LIB_SOURCES}
)

target_link_libraries(SDL_movie PUBLIC SDL3::SDL3 webm libvpx vorbis)
target_include_directories(SDL_movie PRIVATE ${libwebm_SOURCE_DIR}/webm_parser/include)

target_include_directories(SDL_movie PUBLIC include/)

option(SDL_MOVIE_BUILD_EXAMPLES "Build SDL_Movie examples" ON)

if (SDL_MOVIE_BUILD_EXAMPLES)
    add_subdirectory(examples/)
endif()