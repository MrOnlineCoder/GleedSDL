# SDL_movie

This is simple library for playing **.webm** movies with SDL3. It is intended to be mostly used for playing short cinematics in games, but cautious usage for other purposes is also possible.

## Features

- Provides SDL-like C API
- API mostly inspired by RAD's Bink Video, but with open-source formats
- Supports .webm files with **VP8** or **VP9** for video codecs, and **Vorbis** for audio codec.
- Supports simple seeking
- Provides utility functions for playing back video via `SDL_Texture`

## Building and linking

You can build the library with CMake. It has the following dependencies:

- `libvpx` for V8 and V9 decoding
- `libvorbis` and `libogg` for Vorbis decoding
- `libwebm` for WebM parsing

They are downloaded and build automatically via CMake FetchContent module.

Note: you will need a C++ compiler to build this library, as `libwebm` parser is written in C++, which is used internally by SDL_movie.

## Why WebM?

I wanted to use a simple, open-source, free and relatively modern container format - although things like AVI, MP4 and MKV are more popular, they are either too old, too proprietary, or too complex for my needs. WebM is a nice subset of Matroska, containing everything needed for replaying short cinematics, as descrbied earlier.

## Stability note

Please note, that this library was mostly written for educational purposes, and this was my first time working with low-level codecs such as VP8 and Vorbis, therefore, it may contain bugs or memory leaks. Use it at your own risk, and feel free to report any issues.

## Usage

See [example](examples/main.cpp) for a simple example playing Big Buck Bunny short trailer.

## License

[MIT](LICENSE)
