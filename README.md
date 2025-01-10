# SDL_movie

This is a simple library for playing **.webm** movies with SDL3. It is intended to be mostly used for playing short cinematics in games, but cautious usage for other purposes is also possible.

## Features

- Provides SDL-like C API
- API mostly inspired by RAD's Bink Video, but with focus on open-source formats and codecs
- Supports .webm files with **VP8** or **VP9** for video codecs, and **Vorbis** for audio codec
- Supports simple seeking
- Provides utility functions for playing back video via `SDL_Texture`
- Audio samples may be directly fed to `SDL_AudioStream`

## Building and linking

You can build the library with CMake. It has the following dependencies:

- `libvpx` for V8 and V9 decoding
- `libvorbis` and `libogg` for Vorbis decoding
- `libwebm` for WebM parsing

They are downloaded and build automatically via CMake FetchContent module, so in general you don't need to worry about them.

The only problem is that if you are using SDL_mixer for example, it also depends on `libvorbis` and `libogg`, which causes these dependencies to be built/linked twice. I am open to suggestions on how to solve this issue.

Note: you will need a C++ compiler to build this library, as `libwebm` parser is written in C++, which is used internally by SDL_movie.

## Usage

See [example](examples/main.cpp) for a simple example playing Big Buck Bunny short trailer.

The API is document in the header file itself: [SDL_movie.h](include/SDL_movie.h).

The general workflow is the following:

1. Open a .webm file with `SDLMovie_Open(path)` or `SDLMovie_OpenIO(io_stream)`, obtaining a `SDLMovie*` handle.
2. Optionally, select an audio or video track with `SDLMovie_SelectTrack`. If not called, the first video and audio tracks are selected by default.
3. In the application loop, call `SDLMovie_DecodeVideoFrame` to decode video frame, and `SDLMovie_DecodeAudioFrame` to decode audio frame.
4. On success, do useful rendering with video pixels (`SDLMovie_GetVideoFrameSurface`) and audio samples (`SDLMovie_GetAudioSamples`)
5. Call `SDLMovie_NextVideoFrame` and `SDLMovie_NextAudioFrame` to advance to the next frame. Use `SDLMovie_HasNextVideoFrame` and `SDLMovie_HasNextAudioFrame` to check if there are more frames to decode.
6. When done, call `SDLMovie_Free` to free resources.

## Why WebM?

I wanted to use a simple, open-source, free and relatively modern container format - although things like AVI, MP4 and MKV are more popular, they are either too old, too proprietary, or too complex for my needs. WebM is a nice subset of Matroska, containing everything needed for replaying short cinematics, as descrbied earlier.

## Stability note

Please note, that this library was mostly written for educational purposes, and this was my first time working with low-level codecs such as VP8 and Vorbis, therefore, it may contain bugs or memory leaks. Use it at your own risk, and feel free to report any issues.

## License

[MIT](LICENSE)
