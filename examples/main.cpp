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

    SDL_Movie *movie = SDLMovie_Open("bunny.webm");

    if (!movie)
    {
        std::cerr << "Failed to open movie: " << SDLMovie_GetError() << std::endl;
        return 1;
    }

    bool running = true;

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
        }

        if (!SDLMovie_DecodeNextFrame(movie))
        {
            std::cerr << "Failed to decode next frame: " << SDLMovie_GetError() << std::endl;
            return 1;
        }

        if (!SDLMovie_UpdatePlaybackTexture(movie, movieFrameTexture))
        {
            std::cerr << "Failed to update playback texture: " << SDLMovie_GetError() << std::endl;
            return 1;
        }

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, movieFrameTexture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(30); // 30 FPS
    }

    SDLMovie_Free(movie);
    SDL_DestroyTexture(movieFrameTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}