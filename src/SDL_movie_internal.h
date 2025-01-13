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
        Uint32 mem_offset;
        Uint32 offset;
        Uint32 size;
        bool key_frame;
    } CachedMovieFrame;
    typedef struct SDL_Movie
    {
        SDL_IOStream *io; /**< IO stream to read movie data */

        Uint32 ntracks;                              /**< Number of tracks in the movie */
        SDL_MovieTrack tracks[MAX_SDL_MOVIE_TRACKS]; /**< Array of tracks */

        Uint32 count_cached_frames[MAX_SDL_MOVIE_TRACKS];      /**< Number of cached frames for each track */
        Uint32 capacity_cached_frames[MAX_SDL_MOVIE_TRACKS];   /**< Capacity of cached frames for each track (vector-like allocation) */
        CachedMovieFrame *cached_frames[MAX_SDL_MOVIE_TRACKS]; /**< Cached frames for each track */

        Uint8 *encoded_video_frame;                /**< Current encoded video frame data */
        Uint32 encoded_video_frame_size;           /**< Size of the encoded video frame data */
        Uint8 *conversion_video_frame_buffer;      /**< Buffer for decoded video frame data, can be used by decoder to reduce allocations */
        Uint32 conversion_video_frame_buffer_size; /**< Size of the buffer for decoded video frame data */
        void *vpx_context;                         /**< VPX decoder context */
        SDL_PixelFormat video_pixel_format;        /**< Pixel format for the video track */
        SDL_Surface *current_frame_surface;        /**< Current video frame surface, containing decoded frame pixels */
        SDL_MovieCodecType video_codec;            /**< Video codec type */

        Uint8 *encoded_audio_frame;      /**< Current encoded audio frame data */
        Uint32 encoded_audio_frame_size; /**< Size of the encoded audio frame data */

        Uint8 *encoded_audio_buffer;      /**< Encoded audio buffer, containing ALL audio at once (for preload) */
        Uint32 encoded_audio_buffer_size; /**< Encoded audio buffer size */

        SDL_MovieAudioSample *decoded_audio_frame; /**< Decoded audio frame data */
        int decoded_audio_samples;                 /**< Number of decoded audio samples */
        Uint32 decoded_audio_frame_size;           /**< Size of the decoded audio frame
                                                    (can be LARGER than decoded_audio_samples * sizeof(float),
                                                    it's basically serving as buffer capacity) */
        void *vorbis_context;                      /**< Vorbis decoder context */
        void *opus_context;                        /**< Opus decoder context */
        SDL_AudioSpec audio_spec;                  /**< Audio spec for the audio track */
        SDL_MovieCodecType audio_codec;            /**< Audio codec type */

        Uint64 timecode_scale; /**< Timecode scale from WebM file */

        Uint32 last_frame_decode_ms; /**< Time in milliseconds to decode last frame */

        Uint32 current_frame; /**< Current frame number */
        Uint32 total_frames;  /**< Total number of frames in the movie */

        Uint32 current_audio_frame; /**< Current audio frame number */
        Uint32 total_audio_frames;  /**< Total number of audio frames in the movie */

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

    extern bool SDLMovie_DecodeOpus(SDL_Movie *movie);
    extern void SDLMovie_CloseOpus(SDL_Movie *movie);

    extern bool SDLMovie_SetError(const char *fmt, ...);

    extern void SDLMovie_AddCachedFrame(SDL_Movie *movie, Uint32 track, Uint64 timecode, Uint32 offset, Uint32 size, bool key_frame);

    extern int SDLMovie_FindTrackByNumber(SDL_Movie *movie, Uint32 track_number);

    extern bool SDLMovie_CanPlaybackVideo(SDL_Movie *movie);

    extern bool SDLMovie_CanPlaybackAudio(SDL_Movie *movie);

    extern SDL_MovieTrack *SDLMovie_GetVideoTrack(SDL_Movie *movie);

    extern SDL_MovieTrack *SDLMovie_GetAudioTrack(SDL_Movie *movie);

    extern void *SDLMovie_ReadEncodedAudioData(SDL_Movie *movie, void *dest, int size);

    extern void SDLMovie_ReadCurrentFrame(SDL_Movie *movie, SDL_MovieTrackType type);

    extern CachedMovieFrame *SDLMovie_GetCurrentCachedFrame(SDL_Movie *movie, SDL_MovieTrackType type);

    extern Uint64 SDLMovie_TimecodeToMilliseconds(SDL_Movie *movie, Uint64 timecode);

    extern Uint64 SDLMovie_MillisecondsToTimecode(SDL_Movie *movie, Uint64 ms);

    typedef struct SDL_MoviePlayer
    {
        bool paused;         /**< Is player pause */
        bool finished;       /**< Is player finished playback (no more frames left) */
        bool video_playback; /**< Is video playback enabled */
        bool audio_playback; /**< Is audio playback enabled */
        SDL_Movie *mov;      /**< Movie instance */

        Uint64 last_frame_at_ticks; /**< Last frame time in ticks, used for syncronization */

        Uint64 current_time; /**< Current playback (movie) time in milliseconds */

        Uint64 next_audio_frame_at;           /**< Time in milliseconds when next audio frame should be played (in movie time) */
        SDL_MovieAudioSample *audio_buffer;   /**< Audio buffer for samples */
        Uint32 audio_buffer_count;            /**< Number of samples in the audio buffer */
        Uint32 audio_buffer_capacity;         /**< Capacity of the audio buffer */
        SDL_AudioDeviceID bound_audio_device; /**< Audio device bound to the audio stream, or 0 if none */
        SDL_AudioStream *output_audio_stream; /**< Audio stream for output, may be NULL */
        int audio_output_samples_buffer_size; /**< Size of the audio output buffer in samples (hardware-specific) */
        int audio_output_samples_buffer_ms;   /**< Audio output frame size in ms (hardware-specific) */

        Uint64 next_video_frame_at;               /**< Time in milliseconds when next video frame should be played (in movie time) */
        SDL_Surface *current_video_frame_surface; /**< Current video frame surface */
        SDL_Texture *output_video_frame_texture;  /**< Output video frame texture, may be NULL */
    } SDL_MoviePlayer;

    extern void SDLMovie_AddAudioSamplesToPlayer(
        SDL_MoviePlayer *player,
        const SDL_MovieAudioSample *samples,
        int count);

#ifdef __cplusplus
}
#endif

#endif