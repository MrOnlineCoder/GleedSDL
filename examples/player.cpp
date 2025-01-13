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

    SDL_Window *window = SDL_CreateWindow("SDL_movie Player Example", 800, 600, 0);

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
    SDL_Movie *movie = SDLMovie_Open("ocean.webm");

    if (!movie)
    {
        /* Any SDL_Movie error can be retrieved with SDLMovie_GetError() */
        std::cerr << SDLMovie_GetError() << std::endl;
        return 1;
    }

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);

    if (!audio_device)
    {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_MoviePlayer *player = SDLMovie_CreatePlayer(movie);
    SDLMovie_SetPlayerAudioOutput(player, audio_device);
    SDLMovie_PreloadAudioStream(movie);

    SDL_Texture *video_frame = SDLMovie_CreatePlaybackTexture(
        movie, renderer);
    SDLMovie_SetPlayerVideoOutputTexture(player, video_frame);

    SDLMovie_SetPlayerVideoEnabled(player, false);

    bool running = true;

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
                if (ev.key.key == SDLK_SPACE)
                {
                    if (SDLMovie_IsPlayerPaused(player))
                        SDLMovie_ResumePlayer(player);
                    else
                        SDLMovie_PausePlayer(player);
                }
            }
        }

        SDL_MoviePlayerUpdateResult upd = SDLMovie_UpdatePlayer(player, SDL_MOVIE_PLAYER_TIME_DELTA_AUTO);

        if (upd == SDL_MOVIE_PLAYER_UPDATE_ERROR)
        {
            std::cerr << "Error updating player: " << SDLMovie_GetError() << std::endl;
            break;
        }

        if (SDLMovie_HasPlayerFinished(player))
        {
            printf("Move finished, duration = %.2f seconds\n", SDLMovie_GetPlayerCurrentTimeSeconds(player));
            running = false;
        }

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, video_frame, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // 60 FPS
    }

    /* Don't forget to free movie resources after finishing playback */
    SDLMovie_FreePlayer(player);
    SDLMovie_FreeMovie(movie, true);
    SDL_DestroyTexture(video_frame);
    SDL_CloseAudioDevice(audio_device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}