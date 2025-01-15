/*
    GleedMovie Example

    It plays a classical Big Buck Bunny trailer in WebM format (bunny.webm)

    You can press '0' on the keyboard to restart the movie from the beginning.

    This example only show usage of lower-level GleedMovie API
    and does not handle time synchronization between audio and video tracks.

    For more advanced features and the recommended way to play movies, see GleedMoviePlayer example (player.cpp)
*/

#include <iostream>
#include <fstream>
#include <SDL3/SDL.h>

#include <gleed.h>

int main()
{
    /* Standard SDL initialization */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Gleed Example", 800, 600, 0);

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
    GleedMovie *movie = GleedOpen("bunny.webm");

    if (!movie)
    {
        /* Any GleedMovie error can be retrieved with GleedGetError() */
        std::cerr << GleedGetError() << std::endl;
        return 1;
    }

    const SDL_AudioSpec *movie_audio_spec = GleedGetAudioSpec(movie);

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
    SDL_Texture *movieFrameTexture = GleedCreatePlaybackTexture(
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
                    GleedSeekFrame(movie, 0);
                }
            }
        }

        /* Video decoding*/
        if (GleedHasNextVideoFrame(movie))
        {
            /* Decode current frame */
            if (!GleedDecodeVideoFrame(movie))
            {
                std::cerr << "Failed to decode next frame: " << GleedGetError() << std::endl;
                return 1;
            }

            printf("Frame %d decoded in %d ms\n", GleedGetCurrentFrame(movie), GleedGetLastFrameDecodeTime(movie));

            /* Update playback texture */
            if (!GleedUpdatePlaybackTexture(movie, movieFrameTexture))
            {
                std::cerr << "Failed to update playback texture: " << GleedGetError() << std::endl;
                return 1;
            }

            /* Advance to next frame */
            GleedNextVideoFrame(movie);
        }

        /* Audio decoding */
        if (GleedHasNextAudioFrame(movie))
        {
            /* Decode current audio frame*/
            if (!GleedDecodeAudioFrame(movie))
            {
                std::cerr << "Failed to decode next audio frame: " << GleedGetError() << std::endl;
                return 1;
            }

            size_t sz;
            int samples_count;

            /*
                Obtain audio samples from the movie and send them to audio stream.
            */
            const GleedMovieAudioSample *samples = GleedGetAudioSamples(movie, &sz, &samples_count);

            if (samples)
            {
                SDL_PutAudioStreamData(audio_stream, samples, sz);
            }

            /* Advance to next audio frame */
            GleedNextAudioFrame(movie);
        }

        SDL_RenderClear(renderer);

        /* Render the movie video frame, contained in our playback texture */
        SDL_RenderTexture(renderer, movieFrameTexture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // 60 FPS
    }

    /* Don't forget to free movie resources after finishing playback */
    GleedFreeMovie(movie, true);
    SDL_DestroyTexture(movieFrameTexture);
    SDL_DestroyRenderer(renderer);
    SDL_FlushAudioStream(audio_stream);
    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}