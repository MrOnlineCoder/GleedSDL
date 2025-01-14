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
    char file_to_play[32] = {0};

    printf("Select the file number you would like to play: \n");
    printf("(1) bunny.webm (VP8 video, Vorbis audio)\n");
    printf("(2) hl2.webm (VP8 video, Opus audio)\n");
    printf("(3) beach.webm (VP9 video only)\n");
    printf("(4) ocean.webm (VP9 video, Opus audio)\n");
    printf("> ");

    int file_sel = 0;
    scanf("%d", &file_sel);

    if (file_sel == 1)
    {
        SDL_strlcpy(file_to_play, "bunny.webm", sizeof(file_to_play));
    }
    else if (file_sel == 2)
    {
        SDL_strlcpy(file_to_play, "hl2.webm", sizeof(file_to_play));
    }
    else if (file_sel == 3)
    {
        SDL_strlcpy(file_to_play, "beach.webm", sizeof(file_to_play));
    }
    else if (file_sel == 4)
    {
        SDL_strlcpy(file_to_play, "ocean.webm", sizeof(file_to_play));
    }
    else
    {
        printf("Invalid file selection\n");
        return 1;
    }

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

    /*
        Open a WebM movie from file

        This will parse the WebM file, and pre-select first available video and audio tracks.
    */
    SDL_Movie *movie = SDLMovie_Open(file_to_play);

    if (!movie)
    {
        /* Any SDL_Movie error can be retrieved with SDLMovie_GetError() */
        std::cerr << SDLMovie_GetError() << std::endl;
        return 1;
    }

    int w, h;

    SDLMovie_GetVideoSize(movie, &w, &h);

    /*
        Resize the window to the video size, but not larger than 1920x1080
    */
    if (w <= 1920)
    {
        SDL_SetWindowSize(window, w, h);
    }

    /*
        We will use that renderer to draw video frames
    */
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);

    if (!renderer)
    {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return 1;
    }

    /*
        And this device will serve as an output for movie audio
    */
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);

    if (!audio_device)
    {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return 1;
    }

    /*
        Create a player instance from a movie
    */
    SDL_MoviePlayer *player = SDLMovie_CreatePlayer(movie);

    /*
        Set the audio output device for the player

        This will automatically bind the audio stream to the device
        and player will put audio samples during update
    */
    SDLMovie_SetPlayerAudioOutput(player, audio_device);

    /*
        For a better user experience, we will preload the audio stream
    */
    SDLMovie_PreloadAudioStream(movie);

    /*
        This texture will hold our video frame data

        You can pass your own texture, but it must have same format and it's recommended to use a streaming texture
    */
    SDL_Texture *video_frame = SDLMovie_CreatePlaybackTexture(
        movie, renderer);
    SDLMovie_SetPlayerVideoOutputTexture(player, video_frame);

    bool running = true;

    char title[128];

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
                    /* Very simple pausing control */
                    if (SDLMovie_IsPlayerPaused(player))
                        SDLMovie_ResumePlayer(player);
                    else
                        SDLMovie_PausePlayer(player);
                }
            }
        }

        /*
            Update the player, you should call it each application's frame

            Second argument expects a delta time, but here we let player decide it
        */
        SDL_MoviePlayerUpdateResult upd = SDLMovie_UpdatePlayer(player, SDL_MOVIE_PLAYER_TIME_DELTA_AUTO);

        /*
            Crash on error
        */
        if (upd == SDL_MOVIE_PLAYER_UPDATE_ERROR)
        {
            std::cerr << "Error updating player: " << SDLMovie_GetError() << std::endl;
            break;
        }

        /*
            Exit when movie is finished
        */
        if (SDLMovie_HasPlayerFinished(player))
        {
            printf("Move finished, duration = %.2f seconds\n", SDLMovie_GetPlayerCurrentTimeSeconds(player));
            running = false;
        }

        SDL_RenderClear(renderer);
        /*
            Render the current video frame

            Note that depending on movie frame rate, this texture may not be updated after each player update
        */
        SDL_RenderTexture(renderer, video_frame, NULL, NULL);
        SDL_RenderPresent(renderer);

        /*
            Some debug info in the window title
        */
        snprintf(title, 128, "SDL_movie Player (movie %s, time %.2f)",
                 file_to_play,
                 SDLMovie_GetPlayerCurrentTimeSeconds(player));

        SDL_SetWindowTitle(window, title);

        SDL_Delay(8); // 120 fps kinda of
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