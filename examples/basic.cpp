/*
    SDL_Movie Example

    It plays a classical Big Buck Bunny trailer in WebM format (bunny.webm)

    You can press '0' on the keyboard to restart the movie from the beginning.
*/

#include <iostream>
#include <fstream>
#include <SDL3/SDL.h>

#include <SDL_movie.h>

int main()
{
    /* Standard SDL initialization */
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

    /*
        Open a WebM movie from file

        This will parse the WebM file, and pre-select first available video and audio tracks.
    */
    SDL_Movie *movie = SDLMovie_Open("bunny.webm");

    if (!movie)
    {
        /* Any SDL_Movie error can be retrieved with SDLMovie_GetError() */
        std::cerr << SDLMovie_GetError() << std::endl;
        return 1;
    }

    const SDL_AudioSpec *movie_audio_spec = SDLMovie_GetAudioSpec(movie);

    /*
        Here we open an audio stream and device, matching the audio spec of the movie.

        Please make sure to check if movie_audio_spec is not NULL in case you are not sure if the movie has audio.
    */
    SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, movie_audio_spec, NULL, NULL);

    if (!audio_stream)
    {
        std::cerr << "Failed to open audio stream: " << SDL_GetError() << std::endl;
        return 1;
    }

    /* SDL creates audio stream device in paused mode by default, so we must resume it */
    SDL_ResumeAudioStreamDevice(audio_stream);

    bool running = true;

    /*
        This is a helper method that allows creating a texture for rendering video frames with SDL_Renderer.

        Note that it's users responsibility to destroy the texture when it's no longer needed.
    */
    SDL_Texture *movieFrameTexture = SDLMovie_CreatePlaybackTexture(
        movie,
        renderer);

    SDL_Event ev;
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
                    /* Seek to start */
                    SDLMovie_SeekFrame(movie, 0);
                }
            }
        }

        /* Video decoding*/
        if (SDLMovie_HasNextVideoFrame(movie))
        {
            /* Decode current frame */
            if (!SDLMovie_DecodeVideoFrame(movie))
            {
                std::cerr << "Failed to decode next frame: " << SDLMovie_GetError() << std::endl;
                return 1;
            }

            printf("Frame %d decoded in %d ms\n", SDLMovie_GetCurrentFrame(movie), SDLMovie_GetLastFrameDecodeTime(movie));

            /* Update playback texture */
            if (!SDLMovie_UpdatePlaybackTexture(movie, movieFrameTexture))
            {
                std::cerr << "Failed to update playback texture: " << SDLMovie_GetError() << std::endl;
                return 1;
            }

            /* Advance to next frame */
            SDLMovie_NextVideoFrame(movie);
        }

        /* Audio decoding */
        if (SDLMovie_HasNextAudioFrame(movie))
        {
            /* Decode current audio frame*/
            if (!SDLMovie_DecodeAudioFrame(movie))
            {
                std::cerr << "Failed to decode next audio frame: " << SDLMovie_GetError() << std::endl;
                return 1;
            }

            size_t sz;
            int samples_count;

            /*
                Obtain audio samples from the movie and send them to audio stream.
            */
            const SDL_MovieAudioSample *samples = SDLMovie_GetAudioSamples(movie, &sz, &samples_count);

            if (samples)
            {
                SDL_PutAudioStreamData(audio_stream, samples, sz);
            }

            /* Advance to next audio frame */
            SDLMovie_NextAudioFrame(movie);
        }

        SDL_RenderClear(renderer);

        /* Render the movie video frame, contained in our playback texture */
        SDL_RenderTexture(renderer, movieFrameTexture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // 60 FPS
    }

    /* Don't forget to free movie resources after finishing playback */
    SDLMovie_FreeMovie(movie, true);
    SDL_DestroyTexture(movieFrameTexture);
    SDL_DestroyRenderer(renderer);
    SDL_FlushAudioStream(audio_stream);
    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}