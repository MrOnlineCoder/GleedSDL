#ifndef SDL_MOVIE_INTERNAL_H
#define SDL_MOVIE_INTERNAL_H

#include <SDL_movie.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        int num;
        Uint64 timecode;
        Uint64 offset;
        Uint64 size;
    } CachedMovieFrame;
    typedef struct SDL_Movie
    {
        SDL_IOStream *io; /**< IO stream to read movie data */

        Uint32 ntracks;                              /**< Number of tracks in the movie */
        SDL_MovieTrack tracks[MAX_SDL_MOVIE_TRACKS]; /**< Array of tracks */

        Uint64 count_cached_frames[MAX_SDL_MOVIE_TRACKS];      /**< Number of cached frames for each track */
        Uint64 capacity_cached_frames[MAX_SDL_MOVIE_TRACKS];   /**< Capacity of cached frames for each track (vector-like allocation) */
        CachedMovieFrame *cached_frames[MAX_SDL_MOVIE_TRACKS]; /**< Cached frames for each track */

        Uint8 *encoded_video_frame;                /**< Current encoded video frame data */
        Uint64 encoded_video_frame_size;           /**< Size of the encoded video frame data */
        Uint8 *conversion_video_frame_buffer;      /**< Buffer for decoded video frame data, can be used by decoder to reduce allocations */
        Uint64 conversion_video_frame_buffer_size; /**< Size of the buffer for decoded video frame data */
        void *vpx_context;                         /**< VPX decoder context */
        SDL_Surface *current_frame_surface;        /**< Current video frame surface, containing decoded frame pixels */
        SDL_MovieCodecType video_codec;            /**< Video codec type */

        Uint8 *encoded_audio_frame;      /**< Current encoded audio frame data */
        Uint64 encoded_audio_frame_size; /**< Size of the encoded audio frame data */

        Uint8 *encoded_audio_buffer;        /**< Encoded audio buffer, containing ALL audio at once (for preload) */
        Uint64 encoded_audio_buffer_size;   /**< Encoded audio buffer size */
        Uint64 encoded_audio_buffer_cursor; /**< Encoded audio buffer cursor */

        SDL_MovieAudioSample *decoded_audio_frame; /**< Decoded audio frame data */
        int decoded_audio_samples;                 /**< Number of decoded audio samples */
        Uint64 decoded_audio_frame_size;           /**< Size of the decoded audio frame data */
        void *vorbis_context;                      /**< Vorbis decoder context */
        SDL_AudioSpec audio_spec;                  /**< Audio spec for the audio track */
        SDL_MovieCodecType audio_codec;            /**< Audio codec type */

        Uint64 timecode_scale; /**< Timecode scale from WebM file */

        Uint64 last_frame_decode_ms; /**< Time in milliseconds to decode last frame */

        Uint64 current_frame; /**< Current frame number */
        Uint64 total_frames;  /**< Total number of frames in the movie */

        Uint64 current_audio_frame; /**< Current audio frame number */
        Uint64 total_audio_frames;  /**< Total number of audio frames in the movie */

        Sint32 current_video_track; /**< Current video track index or SDL_MOVIE_NO_TRACK if not set */
        Sint32 current_audio_track; /**< Current audio track index or SDL_MOVIE_NO_TRACK if not set  */
    } SDL_Movie;

    extern bool SDLMovie_Parse_WebM(SDL_Movie *movie);

    extern bool SDLMovie_Decode_VPX(SDL_Movie *movie);

    extern void SDLMovie_Close_VPX(SDL_Movie *movie);

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

    extern void *SDLMovie_ReadEncodedAudioData(SDL_Movie *movie, void *dest, int size);

    extern void SDLMovie_ReadCurrentFrame(SDL_Movie *movie, SDL_MovieTrackType type);

    extern Uint64 SDLMovie_TimecodeToMilliseconds(SDL_Movie *movie, Uint64 timecode);

    extern Uint64 SDLMovie_MillisecondsToTimecode(SDL_Movie *movie, Uint64 ms);

    typedef struct SDL_MoviePlayer
    {
        SDL_Movie *mov;

        Uint64 last_frame_at_ticks;

        Uint64 current_time;

        SDL_MovieAudioSample *audio_buffer;
        Uint32 audio_buffer_count;
        Uint32 audio_buffer_capacity;

        SDL_AudioStream *output_audio_stream;
        int audio_output_samples_buffer_size;
        int audio_output_sample_buffer_ms;
    } SDL_MoviePlayer;

    extern void SDLMovie_SyncPlayer(SDL_MoviePlayer *player);

    extern void SDLMovie_AddAudioSamplesToPlayer(
        SDL_MoviePlayer *player,
        const SDL_MovieAudioSample *samples,
        int count);

#ifdef __cplusplus
}
#endif

#endif