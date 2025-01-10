#ifndef SDL_MOVIE_INTERNAL_H
#define SDL_MOVIE_INTERNAL_H

#include <SDL_movie.h>

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

    typedef struct SDL_Movie
    {
        SDL_IOStream *io;
        SDL_Surface *current_frame_surface;

        Uint32 ntracks;
        SDL_MovieTrack tracks[MAX_SDL_MOVIE_TRACKS];

        Uint64 count_cached_frames[MAX_SDL_MOVIE_TRACKS];
        Uint64 capacity_cached_frames[MAX_SDL_MOVIE_TRACKS];
        CachedMovieFrame *cached_frames[MAX_SDL_MOVIE_TRACKS];

        Uint8 *encoded_video_frame;
        Uint64 encoded_video_frame_size;
        Uint8 *conversion_video_frame_buffer;
        Uint64 conversion_video_frame_buffer_size;
        SDL_MovieCodecType video_codec;

        Uint8 *encoded_audio_frame;
        Uint64 encoded_audio_frame_size;

        Uint8 *encoded_audio_buffer;
        Uint64 encoded_audio_buffer_size;
        Uint64 encoded_audio_buffer_cursor;

        SDL_MovieAudioSample *decoded_audio_frame;
        int decoded_audio_samples;
        Uint64 decoded_audio_frame_size;
        void *vorbis_context;
        SDL_AudioSpec audio_spec;
        SDL_MovieCodecType audio_codec;

        Uint64 timecode_scale;

        Uint64 last_frame_decode_ms;

        Uint64 current_frame;
        Uint64 total_frames;

        Uint64 current_audio_frame;
        Uint64 total_audio_frames;

        Uint64 current_time;
        Uint64 total_time;

        Sint32 current_video_track;
        Sint32 current_audio_track;
    } SDL_Movie;

    extern bool SDLMovie_Parse_WebM(SDL_Movie *movie);

    extern bool SDLMovie_Decode_VP8(SDL_Movie *movie);

    typedef enum
    {
        SDL_MOVIE_VORBIS_DECODE_DONE = 0,
        SDL_MOVIE_VORBIS_DECODE_NEED_MORE_DATA = 1,
        SDL_MOVIE_VORBIS_INIT_ERROR = 2,
        SDL_MOVIE_VORBIS_DECODE_ERROR = 3,
    } VorbisDecodeResult;

    extern VorbisDecodeResult SDLMovie_Decode_Vorbis(SDL_Movie *movie);
    extern void SDLMovie_Close_Vorbis(SDL_Movie *movie);

    extern bool SDLMovie_SetError(const char *fmt, ...);

    extern void SDLMovie_AddCachedFrame(SDL_Movie *movie, Uint32 track, Uint64 timecode, Uint64 offset, Uint64 size);

    extern int SDLMovie_FindTrackByNumber(SDL_Movie *movie, Uint64 track_number);

    extern bool SDLMovie_CanPlaybackVideo(SDL_Movie *movie);
    extern bool SDLMovie_CanPlaybackAudio(SDL_Movie *movie);

    extern SDL_MovieTrack *SDLMovie_GetVideoTrack(SDL_Movie *movie);
    extern SDL_MovieTrack *SDLMovie_GetAudioTrack(SDL_Movie *movie);

    extern void SDLMovie_PreloadAudioStream(SDL_Movie *movie);
    extern void *SDLMovie_ReadEncodedAudioData(SDL_Movie *movie, void *dest, int size);

    extern void SDLMovie_ReadCurrentFrame(SDL_Movie *movie, SDL_MovieTrackType type);

#ifdef __cplusplus
}
#endif

#endif