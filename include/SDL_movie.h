/*
    SDL_Movie library
*/

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

/**
 * Maximum number of tracks in a single movie that SDL_Movie can load
 */
#define MAX_SDL_MOVIE_TRACKS 8

/**
 * Constant to represent no track number
 */
#define SDL_MOVIE_NO_TRACK -1

    typedef enum
    {
        SDL_MOVIE_TRACK_TYPE_UNKNOWN = 0, /**< Unknown track, should not be used */
        SDL_MOVIE_TRACK_TYPE_VIDEO = 1,   /**< Video track */
        SDL_MOVIE_TRACK_TYPE_AUDIO = 2,   /**< Audio track */
    } SDL_MovieTrackType;

    typedef enum
    {
        SDL_MOVIE_CODEC_TYPE_UNKNOWN = 0, /**< Unknown codec, should not be used */
        SDL_MOVIE_CODEC_TYPE_VP8 = 1,     /**< VP8 video codec */
        SDL_MOVIE_CODEC_TYPE_VP9 = 2,     /**< VP9 video codec */
        SDL_MOVIE_CODEC_TYPE_VORBIS = 3,  /**< Vorbis audio codec */
        SDL_MOVIE_CODEC_TYPE_OPUS = 4,    /**< Opus audio codec */
    } SDL_MovieCodecType;

    /**
     * Movie structure
     *
     * Represents single opened and parsed .webm file.
     *
     * Movie can be created via SDLMovie_Open or SDLMovie_OpenIO functions.
     * It must be freed with SDLMovie_Free function after no longer needed.
     *
     * Opaque structure, do not modify its members directly.
     */
    typedef struct SDL_Movie SDL_Movie;

    /**
     * Movie track structure
     *
     * WebM files are subset of Matroska files, which may contain any number of tracks,
     * each representing different stream of data (video, audio, subtitles, etc.).
     *
     * Tracks are built of individual frames, which can be decoded and rendered (or played back).
     * Tracks can be selected for playback with SDLMovie_SelectTrack function.
     *
     * This structure represents single track in the movie file.
     * Please note, video_ members are only valid for video tracks,
     * and audio_ members are only valid for audio tracks respectively.
     *
     * Currently, only video and audio tracks are supported by the library - others are ignored.
     *
     * You may read the track properties, but do not modify them.
     */
    typedef struct
    {
        char name[256];    /**< Track name, or 'Unknown' if it was not specified in the file */
        char language[32]; /**< Track language, or 'eng' if it was not specified in the file */
        char codec_id[32]; /**< Matroska Codec ID of the track */

        Uint8 *codec_private_data; /**< Codec private data, if available */
        Uint64 codec_private_size; /**< Size of the codec private data */

        Uint64 track_number;     /**< Track number in the file */
        SDL_MovieTrackType type; /**< Track type (video or audio) */

        Uint64 total_frames; /**< Total number of frames in the track */
        Uint64 total_bytes;  /**< Total number of bytes in the track */

        bool lacing; /**< True if the track uses lacing */

        Uint64 video_width;      /**< Video frame width, non-zero only for video tracks */
        Uint64 video_height;     /**< Video frame height, non-zero only for video tracks */
        double video_frame_rate; /**< Video frame rate, may not be specified in the file */

        double audio_sample_frequency; /**< Audio sample frequency, non-zero only for audio tracks */
        double audio_output_frequency; /**< Audio output frequency, non-zero only for audio tracks */
        Uint64 audio_channels;         /**< Number of audio channels, non-zero only for audio tracks */
        Uint64 audio_bit_depth;        /**< Audio bit depth, non-zero only for audio tracks */
    } SDL_MovieTrack;

    /**
     * Audio sample type
     */
    typedef float SDL_MovieAudioSample;

    /**
     * Open movie (.webm) file
     *
     * This function opens and parses single .webm file.
     * After successful parse, it will automatically select first available video and/or audio tracks,
     * allowing you to playback movie right away.
     *
     * \param file Path to .webm file
     *
     * \returns Pointer to prepared SDL_Movie, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern SDL_Movie *SDLMovie_Open(const char *file);

    /**
     * Open movie (.webm) file from SDL IO stream
     *
     * Follows the same rules as SDLMovie_Open, but reads the file from an SDL IO stream.
     * Under the hood, actually SDLMovie_Open is a wrapper around this function.
     *
     * If you provide custom IO stream, make sure it is seekable and readable.
     *
     * \param io SDL IO stream for the .webm file
     */
    extern SDL_Movie *SDLMovie_OpenIO(SDL_IOStream *io);

    /**
     * Free (release) a movie instance
     *
     * Must be called after you no longer need the movie instance to cleanup its resources, including all dynamic memory allocations.
     *
     * SDL_Movie pointer is no longer valid after this call.
     *
     * \param movie SDL_Movie instance to free
     * \param closeio If true, will close the SDL IO stream associated with the movie
     */
    extern void SDLMovie_Free(SDL_Movie *movie, bool closeio);

    /**
     * Get a track from movie
     *
     * This function returns a pointer to the track structure of the movie.
     *
     * \param movie SDL_Movie instance
     * \param index Track index
     *
     * \returns Pointer to the track structure, or NULL if track does not exist or index is out of bounds
     */
    extern const SDL_MovieTrack *SDLMovie_GetTrack(const SDL_Movie *movie, int index);

    /**
     * Get the number of tracks in the movie (both video and audio)
     *
     * \param movie SDL_Movie instance
     *
     * \returns Number of tracks in the movie
     */
    extern int SDLMovie_GetTrackCount(const SDL_Movie *movie);

    /**
     * Select a movie track
     *
     * This function allows you to select a video or audio track to be used for playback on the movie.
     * All further decoding will be performed on the selected track.
     *
     * At once, only one video and one audio track can be selected.
     *
     * Usually you won't need this to call this manually,
     * as the first available video and audio tracks are selected automatically after opening the movie.
     *
     * Please note, that the passed track number is not the same as the track index in the movie file,
     * as some tracks may be skipped (e.g. subtitles or ones which are not supported).
     *
     * \param movie SDL_Movie instance
     * \param type Track type (video or audio)
     * \param track Track index to select
     */
    extern void SDLMovie_SelectTrack(SDL_Movie *movie, SDL_MovieTrackType type, int track);

    /**
     * Create a playback texture
     *
     * This is a helper function that creates a streaming texture for playback, capable of being drawn with SDL_Renderer.
     * The texture must be freed by user with SDL_DestroyTexture when no longer needed.
     * Playback texture is not attached or bound in anyway to the movie instance, so it can be used freely
     * and independently from the movie.
     * This also means that calling this function again will create a new texture, not updating the existing one.
     *
     * Texture format is SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING access mode
     * and the size is the same as the video frame size.
     *
     * Contents of the texture can be easily updated with SDLMovie_UpdatePlaybackTexture function.
     *
     * \param movie SDL_Movie instance with configured video track
     * \param renderer SDL_Renderer instance to create the texture for
     *
     * \returns SDL_Texture instance for playback, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern SDL_Texture *SDLMovie_CreatePlaybackTexture(SDL_Movie *movie, SDL_Renderer *renderer);

    /**
     * Update playback texture with the current video frame
     *
     * This function will upload current video frame pixel data to the playback texture.
     * It does not any strict checks on the texture, origin, so you may provide even
     * your custom texture, but it must be compatible with video format.
     *
     * During this operation, texture will be locked.
     *
     * A texture of different size may be provided, then default SDL blitting rules will be applied.
     *
     * This function will result in error if there is no decoded video frame available.
     *
     * \param movie SDL_Movie instance with configured video track and decoded video frame
     * \param texture SDL_Texture instance to update with video frame
     *
     * \returns True on success, false on error. Call SDLMovie_GetError to get the error message.
     */
    extern bool SDLMovie_UpdatePlaybackTexture(SDL_Movie *movie, SDL_Texture *texture);

    /**
     * Check if there is a next video frame available
     *
     * If there is, you may call SDLMovie_NextVideoFrame.
     *
     * \returns true if there is a next video frame available, false otherwise or if there is an error.
     */
    extern bool SDLMovie_HasNextVideoFrame(SDL_Movie *movie);

    /**
     * Decodes current video frame of the movie.
     *
     * You should call this function before getting the video frame surface or updating playback textures.
     *
     * After successful decode, you can get the video frame surface with SDLMovie_GetVideoFrameSurface.
     * You should also call SDLMovie_NextVideoFrame to move to the next frame.
     *
     * \param movie SDL_Movie instance with configured video track
     * \return True on success, false on error. Call SDLMovie_GetError to get the error message.
     */
    extern bool SDLMovie_DecodeVideoFrame(SDL_Movie *movie);

    /**
     * Get the current video frame surface
     *
     * This function returns the current video frame surface, which contains the decoded video frame pixels,
     * that you can use for rendering the video frame.
     *
     * Of course, you must decode a frame with SDLMovie_DecodeVideoFrame before calling this function.
     *
     * If you are using SDL_Renderer, you may use SDLMovie_CreatePlaybackTexture and SDLMovie_UpdatePlaybackTexture functions
     * respectively to create and update a SDL_Texture for playback.
     *
     * The format of the surface is SDL_PIXELFORMAT_XBGR8888, and the size is the same as the video frame size.
     *
     * The surface will be modified by the next call to SDLMovie_DecodeVideoFrame.
     *
     * \param movie SDL_Movie instance with configured video track and decoded video frame
     * \return SDL_Surface instance with the video frame pixels, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern const SDL_Surface *SDLMovie_GetVideoFrameSurface(SDL_Movie *movie);

    /**
     * Move to the next video frame
     *
     * This function should be called after decoding the current video frame with SDLMovie_DecodeVideoFrame, and
     * (optionally) rendering it's contents, although the decoding is optional in that context.
     *
     * After calling this function, you can check if there is a next video frame available with SDLMovie_HasNextVideoFrame.
     *
     * Calling this function when there are no more video frames left will have no effect.
     *
     * \param movie SDL_Movie instance with configured video track
     */
    extern void SDLMovie_NextVideoFrame(SDL_Movie *movie);

    /**
     * Check if there is a next audio frame available
     *
     * Each audio frame may contain multiple audio samples.
     *
     * If there is, you may call SDLMovie_NextAudioFrame.
     *
     * \returns true if there is a next audio frame available, false otherwise or if there is an error.
     */
    extern bool SDLMovie_HasNextAudioFrame(SDL_Movie *movie);

    /**
     * Decodes current audio frame of the movie.
     *
     * You should call this function before getting the audio samples.
     * Movie should have an audio track selected.
     *
     * After successful decode, you can get the audio samples with SDLMovie_GetAudioSamples.
     *
     * You should also call SDLMovie_NextAudioFrame to move to the next frame.
     *
     * \param movie SDL_Movie instance with configured audio track
     * \return True on success, false on error. Call SDLMovie_GetError to get the error message.
     */
    extern bool SDLMovie_DecodeAudioFrame(SDL_Movie *movie);

    /**
     * Get the audio samples of the current audio frame
     *
     * This function returns a pointer to buffer of decoded audio samples for current frame.
     * The buffer is valid until the next call to SDLMovie_DecodeAudioFrame.
     *
     * You can directly queue the samples for playback via SDL_PutAudioStreamData into
     * a SDL_AudioStream that has the correct spec, obtained from SDLMovie_GetAudioSpec - the
     * samples buffer has interleaved format, supported by SDL audio.
     *
     * The size of the buffer in bytes is returned in the size parameter,
     * and the number of per-channel samples is returned in the count parameter.
     *
     * \param movie SDL_Movie instance with configured audio track and decoded audio frame
     * \param size Pointer to store the size of the buffer in bytes, or NULL if not needed
     * \param count Pointer to store the number of samples in the buffer, or NULL if not needed
     *
     * \returns Pointer to the buffer with audio samples, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern const SDL_MovieAudioSample *SDLMovie_GetAudioSamples(SDL_Movie *movie, size_t *size, int *count);

    /**
     * Move to the next audio frame
     *
     * This function should be called after decoding the current audio frame with SDLMovie_DecodeAudioFrame.
     *
     * After calling this function, you can check if there is a next audio frame available with SDLMovie_HasNextAudioFrame.
     *
     * Calling this function when there are no more audio frames left will have no effect.
     *
     * \param movie SDL_Movie instance with configured audio track
     */
    extern void SDLMovie_NextAudioFrame(SDL_Movie *movie);

    /**
     * Get audio specification of the movie
     *
     * This function returns the audio spec for the currently selected audio track in the movie.
     * The resulting samples from SDLMovie_GetAudioSamples will follow this spec.
     *
     * You may use pass this audio spec to SDL Audio functions to create a matching audio stream/device.
     *
     * If there is no audio track selected, it will return NULL.
     *
     * The returned pointer is valid until the movie is freed.
     *
     * \param movie SDL_Movie instance
     */
    extern const SDL_AudioSpec *SDLMovie_GetAudioSpec(SDL_Movie *movie);

    /**
     * Seek to a specific time in the movie
     *
     * This function allows you to seek to a specific time (in seconds) in the movie.
     * The seek may be not precise, as it will seek to the nearest keyframe.
     *
     * If both audio and video tracks are present, it will seek to the nearest keyframe of the video track
     * and sync the audio track to the video track.
     *
     * \param movie SDL_Movie instance
     * \param time Time in seconds to seek to
     */
    extern void SDLMovie_SeekSeconds(SDL_Movie *movie, float time);

    /**
     * Seek to a specific frame in the movie
     *
     * This function allows you to seek to a specific video frame in the movie.
     * The seek may be not precise, as it will seek to the nearest keyframe.
     *
     * If both audio and video tracks are present, it will seek to the nearest keyframe of the video track
     * and sync the audio track to the video track.
     *
     * \param movie SDL_Movie instance
     * \param frame Frame number to seek to
     */
    extern void SDLMovie_SeekFrame(SDL_Movie *movie, Uint64 frame);

    /**
     * Get the last frame decode time in milliseconds
     *
     * This function returns the time in milliseconds it took to decode the last video frame,
     * which you can use for benchmarking or performance monitoring.
     *
     * \param movie SDL_Movie instance
     *
     * \returns Time in milliseconds, 0 if no frame was decoded yet or on error.
     */
    extern Uint64 SDLMovie_GetLastFrameDecodeTime(SDL_Movie *movie);

    /**
     * Get the total number of video frames in the movie
     *
     * \param movie SDL_Movie instance
     * \returns Total number of video frames in the movie, or 0 on error.
     */
    extern Uint64 SDLMovie_GetTotalFrames(SDL_Movie *movie);

    /**
     * Get the current video frame number
     *
     * This number is increment with each call to SDLMovie_NextVideoFrame.
     *
     * \param movie SDL_Movie instance
     * \returns Current video frame number, or 0 on error.
     */
    extern Uint64 SDLMovie_GetCurrentFrame(SDL_Movie *movie);

    /**
     * Get the video size of the movie
     *
     * This function returns the width and height of the video frames in the movie, in pixels.
     * Movie must have a video track selected.
     *
     * If there is an error, width and height parameters will not be changed.
     *
     * \param movie SDL_Movie instance
     * \param w Pointer to store the width of the video frames, or NULL if not needed
     * \param h Pointer to store the height of the video frames, or NULL if not needed
     */
    extern void SDLMovie_GetVideoSize(SDL_Movie *movie, int *w, int *h);

    /**
     * Get the error message
     *
     * This function returns the last error message that occurred during the last operation.
     *
     * \returns Error message string, or NULL if there was no error.
     */
    extern const char *SDLMovie_GetError();

#ifdef __cplusplus
}
#endif

#endif