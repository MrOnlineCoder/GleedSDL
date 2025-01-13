# SDL_movie

This is a simple library for playing **.webm** movies with SDL3. It is intended to be mostly used for playing short cinematics in games, but cautious usage for other purposes is also possible.

## Features

- Provides SDL-like C API
- API mostly inspired by RAD's Bink Video, but with focus on open-source formats and codecs
- Supports .webm files with **VP8** or **VP9** for video codecs, and **Vorbis** or **Opus** for audio codecs
- Provides utility functions for playing back video via `SDL_Texture`
- Audio samples may be directly fed to `SDL_AudioStream`

## Building and linking

You can build the library with CMake. It has the following dependencies:

- `libvpx` for V8 and V9 decoding
- `libvorbis` and `libogg` for Vorbis decoding
- `libwebm` for WebM parsing

* `libopus` for Opus decoding

They are downloaded and build automatically via CMake FetchContent module, so in general you don't need to worry about them.

The only problem is that if you are using SDL_mixer for example, it also depends on `libvorbis` and `libogg`, which causes these dependencies to be built/linked twice. I am open to suggestions on how to solve this issue.

Note: you will need a C++ compiler to build this library, as `libwebm` parser is written in C++, which is used internally by SDL_movie.

## Usage

See [basic.cpp](examples/basic.cpp) for a simple example playing Big Buck Bunny short trailer using more low-level `SDL_Movie` object.

See another [player.cpp](examples/player.cpp) with more high-level `SDL_MoviePlayer` object, which handles all the timing and synchronization for you and is the **recommended way** to use the library, unless you have specific needs.

The API is documented in the header file itself: [SDL_movie.h](include/SDL_movie.h).

The general workflow for `SDL_Movie` is the following:

1. Open a .webm file with `SDLMovie_Open(path)` or `SDLMovie_OpenIO(io_stream)`, obtaining a `SDLMovie*` handle.
2. Optionally, select an audio or video track with `SDLMovie_SelectTrack`. If not called, the first video and audio tracks are selected by default.
3. In the application loop, call `SDLMovie_DecodeVideoFrame` to decode video frame, and `SDLMovie_DecodeAudioFrame` to decode audio frame.
4. On success, do useful rendering with video pixels (`SDLMovie_GetVideoFrameSurface`) and audio samples (`SDLMovie_GetAudioSamples`)
5. Call `SDLMovie_NextVideoFrame` and `SDLMovie_NextAudioFrame` to advance to the next frame. Use `SDLMovie_HasNextVideoFrame` and `SDLMovie_HasNextAudioFrame` to check if there are more frames to decode.
6. When done, call `SDLMovie_FreeMovie` to free resources.

**However**, the main problem with that workflow is that it all timing and synchronization is left to the user. Doing all that a frame rate higher than the original will cause inconsistent playback speed and audio desync.

Therefore, it's advised to use `SDL_MoviePlayer`. Aside from handling timing for you, it also supports:

- Directly feeding audio output to your SDL_AudioDevice.
- Pausing
- Disabling audio or video playback, if needed
- Automatic frame rate adjustment
- Automatic calculation of time delta (pass `SDL_MOVIE_PLAYER_TIME_DELTA_AUTO` as second argument to `SDLMovie_UpdatePlayer`)
- _Probably will support seeking in future_

Very quick example with the player:

```cpp
// Initialization

SDL_Movie* movie = SDLMovie_Open("bunny.webm");

SDL_MoviePlayer* player = SDLMovie_CreatePlayer(movie);

SDL_Texture *video_frame = SDLMovie_CreatePlaybackTexture(
        movie, renderer);

SDLMovie_SetPlayerVideoOutputTexture(player, video_frame);
SDLMovie_SetPlayerAudioOutput(player, audio_device); // your SDL_AudioDevice, already opened

// Update loop
SDLMovie_UpdatePlayer(player, SDL_MOVIE_PLAYER_TIME_DELTA_AUTO); //second argument is time delta, you can your own

// Rendering
SDL_RenderCopy(renderer, video_frame, NULL, NULL); // render video frame

// Cleanup
SDLMovie_FreePlayer(player);
SDLMovie_FreeMovie(movie, true);
SDLMovie_DestroyTexture(video_frame);
```

## Why WebM?

I wanted to use a simple, open-source, free and relatively modern container format - although things like AVI, MP4 and MKV are more popular, they are either too old, too proprietary, or too complex for my needs. WebM is a nice subset of Matroska, containing everything needed for replaying short cinematics, as descrbied earlier.

## Why not just use ffmpeg and support all formats?

I have 2 reasons for that:

1. ffmpeg is _big_ and kinda-overkill when you have control over the input format for your video assets, allowing you to choose one.
2. I wanted to learn how low-level codecs APIs and struggle a bit :D

## How can I convert my video to WebM for SDL_Movie?

Use `ffmpeg`:

```bash
ffmpeg -i input.mp4 -c:v libvpx -c:a libvorbis output.webm
```

You may also use Opus as audio codec:

```bash
ffmpeg -i input.mp4 -c:v libvpx -c:a libopus output.webm
```

## Stability note

Please note, that this library was mostly written for educational purposes, and this was my first time working with low-level codecs such as VP8 and Vorbis, therefore, it may contain bugs or memory leaks. Use it at your own risk, and feel free to report any issues.

## Hardware acceleration

For now, it does not support it, only accelerations available are implemented by codecs themselves (like multithreading, maybe). I have not yet researched this topic so far.

## License

[MIT](LICENSE)
