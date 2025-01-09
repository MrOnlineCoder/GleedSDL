#ifndef SDL_MOVIE_H
#define SDL_MOVIE_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SDL_MOVIE_MAJOR_VERSION 3
#define SDL_MOVIE_MINOR_VERSION 0
#define SDL_MOVIE_MICRO_VERSION 0

#define SDL_MOVIE_VERSION \
    SDL_VERSIONNUM(SDL_IMAGE_MAJOR_VERSION, SDL_IMAGE_MINOR_VERSION, SDL_IMAGE_MICRO_VERSION)

#define MAX_SDL_MOVIE_TRACKS 8

    typedef enum
    {
        SDL_MOVIE_TRACK_TYPE_UNKNOWN = 0,
        SDL_MOVIE_TRACK_TYPE_VIDEO = 1,
        SDL_MOVIE_TRACK_TYPE_AUDIO = 2,
    } SDL_MovieTrackType;

    typedef enum
    {
        SDL_MOVIE_CODEC_TYPE_UNKNOWN = 0,
        SDL_MOVIE_CODEC_TYPE_VP8 = 1,
        SDL_MOVIE_CODEC_TYPE_VP9 = 2,
        SDL_MOVIE_CODEC_TYPE_VORBIS = 3,
        SDL_MOVIE_CODEC_TYPE_OPUS = 4,
    } SDL_MovieCodecType;

    typedef struct SDL_Movie SDL_Movie;

    extern SDL_Movie *SDLMovie_Open(const char *file);
    extern SDL_Movie *SDLMovie_OpenIO(SDL_IOStream *io);

    extern SDL_Texture *SDLMovie_CreatePlaybackTexture(SDL_Movie *movie, SDL_Renderer *renderer);
    extern bool SDLMovie_HasNextFrame(SDL_Movie *movie);
    extern bool SDLMovie_DecodeNextFrame(SDL_Movie *movie);
    extern bool SDLMovie_UpdatePlaybackTexture(SDL_Movie *movie, SDL_Texture *texture);
    extern SDL_Surface *SDLMovie_GetFrameSurface(SDL_Movie *movie);
    extern void SDLMovie_SeekSeconds(SDL_Movie *movie, float time);
    extern void SDLMovie_SeekFrame(SDL_Movie *movie, int frame);
    extern void SDLMovie_SelectTrack(SDL_Movie *movie, SDL_MovieTrackType type, int track);

    extern void SDLMovie_Free(SDL_Movie *movie);

    extern const char *SDLMovie_GetError();

#ifdef __cplusplus
}
#endif

#endif