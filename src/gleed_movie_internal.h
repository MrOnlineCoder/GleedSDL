/**
 * Internal functions and structures for SDL_movie library.
 *
 * Should not be used directly by the user, as it's not part of the public API.
 */

#ifndef GLEED_INTERNAL_H
#define GLEED_INTERNAL_H

#include <gleed.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * This structure represents metadata of a single encoded frame of a movie track (video or audio).
     *
     * All frames metadata for all supported tracks are loaded from Matroska/WebM Block elements during parsing,
     * as they contain crucial information about when to play each frame and how big the frame is.
     */
    typedef struct
    {
        Uint64 timecode;   /**< Time code of frame, in Matroska ticks */
        Uint32 mem_offset; /**< Offset in memory, IF frame data was stored continuously. This is crucial when you preload an audio stream for example  */
        Uint32 offset;     /**< Offset of the frame in WebM file */
        Uint32 size;       /**< Size of frame in WebM in bytes */
        bool key_frame;    /**< Is given frame a keyframe; needed for seeking and maintaining codecs state */
    } CachedMovieFrame;
    typedef struct GleedMovie
    {
        SDL_IOStream *io; /**< IO stream to read movie data */

        Uint32 ntracks;                           /**< Number of tracks in the movie */
        GleedMovieTrack tracks[MAX_GLEED_TRACKS]; /**< Array of tracks */

        Uint32 count_cached_frames[MAX_GLEED_TRACKS];      /**< Number of cached frames for each track */
        Uint32 capacity_cached_frames[MAX_GLEED_TRACKS];   /**< Capacity of cached frames for each track (vector-like allocation) */
        CachedMovieFrame *cached_frames[MAX_GLEED_TRACKS]; /**< Cached frames for each track */

        Uint8 *encoded_video_frame;                /**< Current encoded video frame data */
        Uint32 encoded_video_frame_size;           /**< Size of the encoded video frame data */
        Uint8 *conversion_video_frame_buffer;      /**< Buffer for decoded video frame data, can be used by decoder to reduce allocations */
        Uint32 conversion_video_frame_buffer_size; /**< Size of the buffer for decoded video frame data */
        void *vpx_context;                         /**< VPX decoder context (both VP8 and VP9) */
        SDL_PixelFormat video_pixel_format;        /**< Pixel format for the video track */
        SDL_Surface *current_frame_surface;        /**< Current video frame surface, containing decoded frame pixels */
        GleedMovieCodecType video_codec;           /**< Video codec type */

        Uint8 *encoded_audio_frame;      /**< Current encoded audio frame data */
        Uint32 encoded_audio_frame_size; /**< Size of the encoded audio frame data */

        Uint8 *encoded_audio_buffer;      /**< Encoded audio buffer, containing ALL audio at once (for preload) */
        Uint32 encoded_audio_buffer_size; /**< Encoded audio buffer size */

        GleedMovieAudioSample *decoded_audio_frame; /**< Decoded audio frame data */
        int decoded_audio_samples;                  /**< Number of decoded audio samples combined for all channels */
        Uint32 decoded_audio_frame_size;            /**< Size of the decoded audio frame
                                                     (can be LARGER than decoded_audio_samples * sizeof(float),
                                                     it's basically serving as buffer capacity) */
        void *vorbis_context;                       /**< Vorbis decoder context, NULL if vorbis not used */
        void *opus_context;                         /**< Opus decoder context, NULL if opus not used */
        SDL_AudioSpec audio_spec;                   /**< Audio spec for the audio track */
        GleedMovieCodecType audio_codec;            /**< Audio codec type */

        Uint64 timecode_scale; /**< Timecode scale from WebM file */

        Uint32 last_frame_decode_ms; /**< Time in milliseconds spent to decode last frame */

        Uint32 current_frame; /**< Current frame number */
        Uint32 total_frames;  /**< Total number of frames in the movie */

        Uint32 current_audio_frame; /**< Current audio frame number */
        Uint32 total_audio_frames;  /**< Total number of audio frames in the movie */

        Sint32 current_video_track; /**< Current video track index or GLEED_NO_TRACK if not set */
        Sint32 current_audio_track; /**< Current audio track index or GLEED_NO_TRACK if not set  */
    } GleedMovie;

    extern bool GleedParseWebM(GleedMovie *movie);

    extern bool GleedDecodeVPX(GleedMovie *movie);

    extern void GleedCloseVPX(GleedMovie *movie);

    typedef enum
    {
        GLEED_VORBIS_DECODE_DONE = 0,
        GLEED_VORBIS_DECODE_NEED_MORE_DATA = 1,
        GLEED_VORBIS_INIT_ERROR = 2,
        GLEED_VORBIS_DECODE_ERROR = 3,
    } VorbisDecodeResult;

    extern VorbisDecodeResult GleedDecode_Vorbis(GleedMovie *movie);

    extern void GleedCloseVorbis(GleedMovie *movie);

    extern bool GleedDecodeOpus(GleedMovie *movie);

    extern void GleedCloseOpus(GleedMovie *movie);

    extern bool GleedSetError(const char *fmt, ...);

    extern void GleedAddCachedFrame(GleedMovie *movie, Uint32 track, Uint64 timecode, Uint32 offset, Uint32 size, bool key_frame);

    extern int GleedFindTrackByNumber(GleedMovie *movie, Uint32 track_number);

    extern bool GleedCanPlaybackVideo(GleedMovie *movie);

    extern bool GleedCanPlaybackAudio(GleedMovie *movie);

    extern GleedMovieTrack *GleedGetVideoTrack(GleedMovie *movie);

    extern GleedMovieTrack *GleedGetAudioTrack(GleedMovie *movie);

    extern void *GleedReadEncodedAudioData(GleedMovie *movie, void *dest, int size);

    extern void GleedReadCurrentFrame(GleedMovie *movie, GleedMovieTrackType type);

    extern CachedMovieFrame *GleedGetCurrentCachedFrame(GleedMovie *movie, GleedMovieTrackType type);

    extern Uint64 GleedTimecodeToMilliseconds(GleedMovie *movie, Uint64 timecode);

    extern Uint64 GleedMatroskaTicksToMilliseconds(GleedMovie *movie, Uint64 ticks);

    extern Uint64 GleedMillisecondsToTimecode(GleedMovie *movie, Uint64 ms);

    typedef struct GleedMoviePlayer
    {
        bool paused;         /**< Is player paused */
        bool finished;       /**< Has player finished playback (no more frames left) */
        bool video_playback; /**< Is video playback enabled */
        bool audio_playback; /**< Is audio playback enabled */
        GleedMovie *mov;     /**< Movie instance */

        Uint64 last_frame_at_ticks; /**< Last frame time in ticks, used for synchronization */

        Uint64 current_time; /**< Current playback (movie) time in milliseconds */

        Uint64 next_audio_frame_at;           /**< Time in milliseconds when next audio frame should be played (in movie time) */
        GleedMovieAudioSample *audio_buffer;  /**< Audio buffer for samples */
        Uint32 audio_buffer_count;            /**< Number of samples in the audio buffer */
        Uint32 audio_buffer_capacity;         /**< Capacity of the audio buffer */
        SDL_AudioDeviceID bound_audio_device; /**< Audio device bound to the audio stream, or 0 if none */
        SDL_AudioStream *output_audio_stream; /**< Audio stream for output, may be NULL */
        int audio_output_samples_buffer_size; /**< Size of the audio output buffer in samples (hardware-specific) */
        int audio_output_samples_buffer_ms;   /**< Audio output frame size in ms (hardware-specific) */

        Uint64 next_video_frame_at;               /**< Time in milliseconds when next video frame should be played (in movie time) */
        SDL_Surface *current_video_frame_surface; /**< Current video frame surface */
        SDL_Texture *output_video_frame_texture;  /**< Output video frame texture, may be NULL */
    } GleedMoviePlayer;

    extern void GleedAddAudioSamplesToPlayer(
        GleedMoviePlayer *player,
        const GleedMovieAudioSample *samples,
        int count);

    /* Not currently ready to be exposed as public API, as switching movies can be tricky*/
    extern void GleedSetPlayerMovie(GleedMoviePlayer *player, GleedMovie *mov);

#ifdef __cplusplus
}
#endif

#endif