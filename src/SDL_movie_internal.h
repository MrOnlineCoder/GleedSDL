#ifndef SDL_MOVIE_INTERNAL_H
#define SDL_MOVIE_INTERNAL_H

#include <SDL_movie.h>
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        Uint64 timecode;
        Uint64 offset;
        Uint64 size;
    } CachedMovieFrame;

    typedef struct
    {
        char name[256];
        char language[32];
        char codec_id[32];

        Uint64 track_number;
        SDL_MovieTrackType type;

        Uint64 total_frames;

        bool lacing;

        Uint64 video_width;
        Uint64 video_height;
        double video_frame_rate;

        double audio_sample_frequency;
        double audio_output_frequency;
        Uint64 audio_channels;
        Uint64 audio_bit_depth;
    } MovieTrack;

    typedef struct SDL_Movie
    {
        SDL_IOStream *io;
        SDL_Surface *current_frame_surface;

        Uint32 ntracks;
        MovieTrack tracks[MAX_SDL_MOVIE_TRACKS];

        Uint64 count_cached_frames[MAX_SDL_MOVIE_TRACKS];
        Uint64 capacity_cached_frames[MAX_SDL_MOVIE_TRACKS];
        CachedMovieFrame *cached_frames[MAX_SDL_MOVIE_TRACKS];

        Uint8 *encoded_video_frame;
        Uint64 encoded_video_frame_size;

        SDL_MovieCodecType video_codec;
        SDL_MovieCodecType audio_codec;

        Uint64 timecode_scale;

        Uint64 current_frame;
        Uint64 total_frames;

        Uint64 current_time;
        Uint64 total_time;

        Uint32 current_video_track;
        Uint32 current_audio_track;
    } SDL_Movie;

    extern bool SDLMovie_Parse_WebM(SDL_Movie *movie);

    extern bool SDLMovie_Decode_VP8(SDL_Movie *movie);

    extern void SDLMovie_SetError(const char *fmt, ...);

    extern void SDLMovie_AddCachedFrame(SDL_Movie *movie, Uint32 track, Uint64 timecode, Uint64 offset, Uint64 size);

    extern int SDLMovie_FindTrackByNumber(SDL_Movie *movie, Uint64 track_number);

    extern bool SDLMovie_CanPlayback(SDL_Movie *movie);

    extern MovieTrack *SDLMovie_GetVideoTrack(SDL_Movie *movie);
    extern MovieTrack *SDLMovie_GetAudioTrack(SDL_Movie *movie);

    extern void SDLMovie_ReadCurrentFrame(SDL_Movie *movie);

#ifdef __cplusplus
}
#endif

#endif