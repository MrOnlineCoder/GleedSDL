#include <iostream>
#include <fstream>
#include <SDL3/SDL.h>

#include <SDL_movie.h>

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("SDL_movie Example", 800, 600, 0);

    if (!window)
    {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);

    if (!renderer)
    {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Movie *movie = SDLMovie_Open("bunny2.webm");

    if (!movie)
    {
        std::cerr << SDLMovie_GetError() << std::endl;
        return 1;
    }

    const SDL_AudioSpec *movie_audio_spec = SDLMovie_GetAudioSpec(movie);
    SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, movie_audio_spec, NULL, NULL);
    SDL_ResumeAudioStreamDevice(audio_stream);

    if (!audio_stream)
    {
        std::cerr << "Failed to open audio stream: " << SDL_GetError() << std::endl;
        return 1;
    }

    bool running = true;

    SDL_Texture *movieFrameTexture = SDLMovie_CreatePlaybackTexture(
        movie,
        renderer);

    SDL_Event ev;
    bool shouldIterate = false;
    bool test = false;
    while (running)
    {
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_EVENT_QUIT)
            {
                running = false;
                break;
            }

            if (ev.type == SDL_EVENT_KEY_DOWN)
            {
                if (ev.key.key == SDLK_0)
                {
                    SDLMovie_SeekFrame(movie, 0);
                }

                if (ev.key.key == SDLK_SPACE)
                {
                    shouldIterate = true;
                }
            }

            if (ev.type == SDL_EVENT_KEY_UP)
            {
                if (ev.key.key == SDLK_SPACE)
                {
                    shouldIterate = false;
                }
            }
        }

        if (shouldIterate && SDLMovie_HasNextVideoFrame(movie))
        {
            if (!SDLMovie_DecodeVideoFrame(movie))
            {
                std::cerr << "Failed to decode next frame: " << SDLMovie_GetError() << std::endl;
                return 1;
            }

            printf("Frame %llu decoded in %llu ms\n", SDLMovie_GetCurrentFrame(movie), SDLMovie_GetLastFrameDecodeTime(movie));

            if (!SDLMovie_UpdatePlaybackTexture(movie, movieFrameTexture))
            {
                std::cerr << "Failed to update playback texture: " << SDLMovie_GetError() << std::endl;
                return 1;
            }

            SDLMovie_NextVideoFrame(movie);
        }

        if (shouldIterate && !test && SDLMovie_HasNextAudioFrame(movie))
        {
            if (!SDLMovie_DecodeAudioFrame(movie))
            {
                std::cerr << "Failed to decode next audio frame: " << SDLMovie_GetError() << std::endl;
                return 1;
            }

            size_t sz;
            int samples;
            const SDL_MovieAudioSample *audio_buffer = SDLMovie_GetAudioSamples(movie, &sz, &samples);

            if (audio_buffer)
            {
                // printf("Samples: %d\n", samples);
                // for (int i = 0; i < samples; i++)
                // {
                //     printf("%.3f ", audio_buffer[i]);
                // }
                // printf("\n");

                SDL_PutAudioStreamData(audio_stream, audio_buffer, sz);
                printf("Put %d samples\n", samples);
            }

            SDLMovie_NextAudioFrame(movie);
        }

        int iw, ih;
        SDLMovie_GetVideoSize(movie, &iw, &ih);

        SDL_FRect dst = {};
        dst.w = iw;
        dst.h = ih;

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, movieFrameTexture, NULL, &dst);
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // 30 FPS
    }

    SDLMovie_Free(movie, true);
    SDL_DestroyTexture(movieFrameTexture);
    SDL_DestroyRenderer(renderer);
    SDL_FlushAudioStream(audio_stream);
    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}