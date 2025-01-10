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

#define SDL_MOVIE_NO_TRACK -1

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

    typedef float SDL_MovieAudioSample;

    extern SDL_Movie *SDLMovie_Open(const char *file);
    extern SDL_Movie *SDLMovie_OpenIO(SDL_IOStream *io);

    extern void SDLMovie_Free(SDL_Movie *movie, bool closeio);

    extern void SDLMovie_SelectTrack(SDL_Movie *movie, SDL_MovieTrackType type, int track);

    extern SDL_Texture *SDLMovie_CreatePlaybackTexture(SDL_Movie *movie, SDL_Renderer *renderer);
    extern bool SDLMovie_UpdatePlaybackTexture(SDL_Movie *movie, SDL_Texture *texture);

    extern bool SDLMovie_HasNextVideoFrame(SDL_Movie *movie);
    extern bool SDLMovie_DecodeVideoFrame(SDL_Movie *movie);
    extern const SDL_Surface *SDLMovie_GetVideoFrameSurface(SDL_Movie *movie);
    extern void SDLMovie_NextVideoFrame(SDL_Movie *movie);

    extern bool SDLMovie_HasNextAudioFrame(SDL_Movie *movie);
    extern bool SDLMovie_DecodeAudioFrame(SDL_Movie *movie);
    extern const SDL_MovieAudioSample *SDLMovie_GetAudioBuffer(SDL_Movie *movie, size_t *size);
    extern void SDLMovie_NextAudioFrame(SDL_Movie *movie);
    extern const SDL_AudioSpec *SDLMovie_GetAudioSpec(SDL_Movie *movie);

    extern void SDLMovie_SeekSeconds(SDL_Movie *movie, float time);
    extern void SDLMovie_SeekFrame(SDL_Movie *movie, Uint64 frame);

    extern Uint64 SDLMovie_GetLastFrameDecodeTime(SDL_Movie *movie);
    extern Uint64 SDLMovie_GetTotalFrames(SDL_Movie *movie);
    extern Uint64 SDLMovie_GetCurrentFrame(SDL_Movie *movie);

    extern void SDLMovie_GetVideoSize(SDL_Movie *movie, int *w, int *h);

    extern const char *SDLMovie_GetError();

#ifdef __cplusplus
}
#endif

#endif