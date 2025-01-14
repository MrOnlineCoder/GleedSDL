/*
    SDL_Movie library
    Version 1.0.0

    Copyright (c) 2024-2025 Nikita Kogut

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

*/

#ifndef SDL_MOVIE_H
#define SDL_MOVIE_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Library version, mimics SDL defines*/
#define SDL_MOVIE_MAJOR_VERSION 1
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

    /**
     * Movie track type
     */
    typedef enum
    {
        SDL_MOVIE_TRACK_TYPE_UNKNOWN = 0, /**< Unknown track, should not be used */
        SDL_MOVIE_TRACK_TYPE_VIDEO = 1,   /**< Video track */
        SDL_MOVIE_TRACK_TYPE_AUDIO = 2,   /**< Audio track */
    } SDL_MovieTrackType;

    /**
     * Movie codec type
     */
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
     * It must be freed with SDLMovie_FreeMovie function after no longer needed.
     *
     * SDL_Movie API allows loading WebM file and per-frame decoding, but nothing more.
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
     * Most of fields are from Matroska spec: https://www.matroska.org/technical/elements.html
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

        Uint8 *codec_private_data; /**< Codec private data, if available. Currently used mostly by Vorbis */
        Uint32 codec_private_size; /**< Size of the codec private data */

        Uint64 codec_delay;   /**< Codec delay, if available, in Matroska ticks */
        Uint64 seek_pre_roll; /**< Seek pre-roll, if available, in Matroska ticks */

        Uint32 track_number;     /**< Track number in the WebM file (usually indexed from 1) */
        SDL_MovieTrackType type; /**< Track type (video or audio) */

        Uint32 total_frames; /**< Total number of frames in the track */
        Uint32 total_bytes;  /**< Total number of bytes in the track */

        bool lacing; /**< True if the track uses lacing */

        Uint32 video_width;      /**< Video frame width, non-zero only for video tracks */
        Uint32 video_height;     /**< Video frame height, non-zero only for video tracks */
        double video_frame_rate; /**< Video frame rate, may not be specified in the file */

        double audio_sample_frequency; /**< Audio sample frequency, non-zero only for audio tracks */
        double audio_output_frequency; /**< Audio output frequency, non-zero only for audio tracks */
        Uint32 audio_channels;         /**< Number of audio channels, non-zero only for audio tracks */
        Uint32 audio_bit_depth;        /**< Audio bit depth, non-zero only for audio tracks */
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
     *
     * \returns Pointer to prepared SDL_Movie, or NULL on error. Call SDLMovie_GetError to get the error message.
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
    extern void SDLMovie_FreeMovie(SDL_Movie *movie, bool closeio);

    /**
     * Get a track from movie
     *
     * This function returns a pointer to the track structure of the movie, if you want to query its properties.
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
     * \returns Number of tracks in the movie, or 0 if there are no tracks or movie is invalid
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
     * Please note, that the passed track index is not the same as the track number in the movie file,
     * as some tracks may be skipped (e.g. subtitles or ones which are not supported) - it is zero indexed.
     *
     * You can use SDLMovie_GetTrack and SDLMovie_GetTrackCount to query available tracks.
     *
     * It's recommended to call this function BEFORE decoding any frames, as some codecs are stateful and require
     * continuous decoding.
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
     * This also means that calling this function again will create a new texture, not update the existing one.
     *
     * Texture format is SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING access mode
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
     * It does not perform any strict checks on the texture origin, so you may provide even
     * your custom texture, but it must be compatible with the video format of the movie.
     *
     * During this operation, texture will be locked.
     *
     * A texture of different size may be provided, then default SDL blitting rules will be applied (basically stretching).
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
     * The format of the surface is SDL_PIXELFORMAT_RGB24, and the size is the same as the video frame size.
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
     * (optionally) rendering it's contents, although this is not required.
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
     * Each audio frame may contain multiple encoded audio samples.
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
     * This function returns a pointer to buffer of decoded PCM audio samples for current frame.
     * The buffer is valid until the next call to SDLMovie_DecodeAudioFrame.
     *
     * You can directly queue the samples for playback via SDL_PutAudioStreamData into
     * a SDL_AudioStream that has the correct spec, obtained from SDLMovie_GetAudioSpec - the
     * samples buffer has interleaved format, supported by SDL audio.
     *
     * The size of the buffer in bytes is returned in the size parameter,
     * and the number of per-channel samples is returned in the count parameter.
     *
     * On error, both size and count will be set to 0.
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
     * You may pass this audio spec to SDL Audio functions to create a matching audio stream/device.
     *
     * If there is no audio track selected, it will return NULL.
     *
     * The returned pointer is valid until the movie is freed.
     *
     * \param movie SDL_Movie instance
     */
    extern const SDL_AudioSpec *SDLMovie_GetAudioSpec(SDL_Movie *movie);

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
    extern void SDLMovie_SeekFrame(SDL_Movie *movie, Uint32 frame);

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
    extern Uint32 SDLMovie_GetLastFrameDecodeTime(SDL_Movie *movie);

    /**
     * Get the total number of video frames in the movie
     *
     * \param movie SDL_Movie instance
     * \returns Total number of video frames in the movie, or 0 on error.
     */
    extern Uint32 SDLMovie_GetTotalVideoFrames(SDL_Movie *movie);

    /**
     * Get the current video frame number
     *
     * This number is incremented with each call to SDLMovie_NextVideoFrame.
     *
     * \param movie SDL_Movie instance
     * \returns Current video frame number, or 0 on error.
     */
    extern Uint32 SDLMovie_GetCurrentFrame(SDL_Movie *movie);

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
     * Currently, error is not cleared after retrieval or successful operation.
     *
     * \returns Error message string, or NULL if there was no error.
     */
    extern const char *SDLMovie_GetError();

    /**
     * Preload audio stream
     *
     * This function will load WHOLE audio stream into memory, so it can be played back without any delay.
     * Take care when working with large audio tracks.
     * You may estimate the memory footprint of doing so by looking at track->total_bytes and track->total_frames.
     * It does not perform decoding, only loads the encoded audio data into memory.
     * It will still need to seek and read each frame separately because of the nature of the Matroska/WebM blocks.
     * As audio tracks are usually much smaller than video tracks, this function is usually safe to call,
     * and probably even recommended for smooth playback.
     *
     * \param movie SDL_Movie instance with configured audio track
     * \returns True on success, false on error. Call SDLMovie_GetError to get the error message.
     *
     */
    extern bool SDLMovie_PreloadAudioStream(SDL_Movie *movie);

    /*
        Movie player structure

        SDL_MoviePlayer is a high level abstraction over SDL_Movie, which allows you to play back movies
        with respect to audio and video synchronization, and provides easy to use API for playback control.

        Movie player can be created with SDLMovie_CreatePlayer() and must be freed with SDLMovie_FreePlayer().

        Opaque structure, do not modify its members directly.
    */
    typedef struct SDL_MoviePlayer SDL_MoviePlayer;

/**
 * This constant can be used to as second argument to SDLMovie_UpdatePlayer to
 * let player decide the time delta automatically.
 */
#define SDL_MOVIE_PLAYER_TIME_DELTA_AUTO -1

    /**
     * Create a player from an existing movie
     *
     * This function creates a player from an existing movie instance.
     * It's strongly recommended to NOT modify the movie instance while the player is active,
     * and instead use the player API.
     *
     * In order to advance the playback, you must call SDLMovie_UpdatePlayer with the time delta in milliseconds.
     *
     * Player must be freed with SDLMovie_FreePlayer before freeing the movie itself.
     *
     * \param mov SDL_Movie instance
     *
     * \returns Pointer to the player instance, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern SDL_MoviePlayer *SDLMovie_CreatePlayer(SDL_Movie *mov);

    /**
     * Create a player from path
     *
     * Helper method to create a player from a file path. See SDLMovie_CreatePlayer for more information.
     *
     * \param path Path to the .webm file
     *
     * \returns Pointer to the player instance, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern SDL_MoviePlayer *SDLMovie_CreatePlayerFromPath(const char *path);

    /**
     * Create a player from SDL IO stream
     *
     * Helper method to create a player from an SDL IO stream. See SDLMovie_CreatePlayer for more information.
     *
     * \param io SDL IO stream for the .webm file
     *
     * \returns Pointer to the player instance, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern SDL_MoviePlayer *SDLMovie_CreatePlayerFromIO(SDL_IOStream *io);

    /**
     * Set player audio output device
     *
     * You may set an audio device as an output for the player.
     * This will automatically create an SDL_AudioStream under the hood with needed spec,
     * attach it to the device,
     * and all decoded audio samples will be queued into this stream.
     *
     * Please note, this function DOES NOT open the audio device for the stream,
     * you must open it by yourself - therefore you cannot pass SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK
     * to this function, it will result in an error.
     *
     * The frequency of the audio device does not matter, as all resampling will be handed by audio stream.
     *
     * If you want to stop audio output, you may pass 0 as the device id.
     * This will destroy the audio stream (if it was present) and stop automatic audio output.
     *
     * \param player SDL_MoviePlayer instance
     * \param dev SDL_AudioDeviceID of the opened audio device to output audio to
     *
     * \returns True on success, false on error. Call SDLMovie_GetError to get the error message.
     */
    extern bool SDLMovie_SetPlayerAudioOutput(SDL_MoviePlayer *player, SDL_AudioDeviceID dev);

    /**
     * Set player video output texture
     *
     * This function will set the output texture for the player,
     * which will be automatically updated with decoded frame pixels during player update.
     *
     * Here apply the sames rules as with SDLMovie_UpdatePlaybackTexture - the texture must be compatible with the video format.
     *
     * So it's strongly recommended to pass the texture created with SDLMovie_CreatePlaybackTexture here.
     *
     * You may pass NULL to disable automatic texture update.
     *
     * \param player SDL_MoviePlayer instance
     * \param texture SDL_Texture instance to update with video frame
     *
     * \returns True on success, false on error. Call SDLMovie_GetError to get the error message.
     */
    extern bool SDLMovie_SetPlayerVideoOutputTexture(
        SDL_MoviePlayer *player,
        SDL_Texture *texture);

    /*
        Enum for player update result
    */
    typedef enum
    {
        SDL_MOVIE_PLAYER_UPDATE_NONE = 0,       /**< No update was performed */
        SDL_MOVIE_PLAYER_UPDATE_AUDIO = 1 << 1, /**< Audio samples were updated */
        SDL_MOVIE_PLAYER_UPDATE_VIDEO = 1 << 2, /**< Video frame was updated */
        SDL_MOVIE_PLAYER_UPDATE_ERROR = 1 << 3, /**< An error occurred during update */
    } SDL_MoviePlayerUpdateResult;

    /**
     * Update the player
     *
     * This function updates the player with the time delta in milliseconds and advances playback when possible.
     *
     * This means that it will:
     *
     * 1) Advance movie frames counters
     * 2) Decode needed video and audio frames
     * 3) If an output texture was set with SDLMovie_SetPlayerVideoOutputTexture - the texture will be updated with new video frame pixels.
     * 4) If an audio output device was set with SDLMovie_SetPlayerAudioOutput - the audio samples will be queued into the audio stream.
     *
     * It's usually should be called in your application loop once per frame (your app's frame, not a movie one).
     *
     * If the player is paused or the movie ended, no updates will be performed.
     *
     * If you pass SDL_MOVIE_PLAYER_TIME_DELTA_AUTO as the time delta, the player will decide the time delta automatically -
     * by using SDL_GetTicks() and the last frame time it remembers.
     *
     * Player may queue more audio samples than needed for the current frame in order to have a buffer for smoother experience.
     *
     * Returned value is a bitmask of SDL_MoviePlayerUpdateResult values. On error, only SDL_MOVIE_PLAYER_UPDATE_ERROR will be set.
     *
     * \param player SDL_MoviePlayer instance
     * \param time_delta_ms Time delta in milliseconds since last frame, or SDL_MOVIE_PLAYER_TIME_DELTA_AUTO to let player decide automatically
     *
     * \returns SDL_MoviePlayerUpdateResult bitmask of the update result, or SDL_MOVIE_PLAYER_UPDATE_ERROR on error. Call SDLMovie_GetError to get the error message.
     */
    extern SDL_MoviePlayerUpdateResult SDLMovie_UpdatePlayer(SDL_MoviePlayer *player, int time_delta_ms);

    /**
     * Get the player audio samples
     *
     * This function is similar to SDLMovie_GetAudioSamples, returning a pointer to decoded interleaved PCM audio samples for the current frame.
     *
     * Please note, the buffer is only valid until the next call to SDLMovie_UpdatePlayer - the values may be overwritten after that.
     *
     * Samples can be directly passed to SDL_PutAudioStreamData.
     *
     * If you have set an audio output with SDLMovie_SetPlayerAudioOutput,
     * the samples will be queued automatically and this function should not be called, as it will return 0 samples.
     *
     * \param player SDL_MoviePlayer instance
     * \param count Pointer to store the number of samples in the buffer, or NULL if not needed
     *
     * \returns Pointer to the buffer with audio samples, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern const SDL_MovieAudioSample *SDLMovie_GetPlayerAvailableAudioSamples(
        SDL_MoviePlayer *player,
        int *count);

    /**
     * Get the player current video frame surface
     *
     * This function is similar to SDLMovie_GetVideoFrameSurface, returning the current video frame surface.
     *
     * Please note, the surface is only valid until the next call to SDLMovie_UpdatePlayer - the values may be overwritten after that.
     *
     * If you have set a video output with SDLMovie_SetPlayerVideoOutputTexture, the texture will be updated automatically with
     * the contents of this surface and there is no need to call this function.
     *
     * The size of the surface is equal to movie's video track dimensions.
     *
     * You may use this function to get current video frame to render if you are not using SDL_Renderer directly.
     *
     * \param player SDL_MoviePlayer instance
     *
     * \returns SDL_Surface instance with the video frame pixels, or NULL on error. Call SDLMovie_GetError to get the error message.
     */
    extern const SDL_Surface *SDLMovie_GetPlayerCurrentVideoFrameSurface(
        SDL_MoviePlayer *player);

    /**
     * Pause the player
     *
     * This function will pause the player, stopping the playback until it is resumed.
     *
     * Calls to SDLMovie_UpdatePlayer will be basically no-op.
     *
     * \param player SDL_MoviePlayer instance
     */
    extern void SDLMovie_PausePlayer(SDL_MoviePlayer *player);

    /**
     * Resume the player
     *
     * This function will resume the player, continuing the playback.
     *
     * \param player SDL_MoviePlayer instance
     */
    extern void SDLMovie_ResumePlayer(SDL_MoviePlayer *player);

    /**
     * Check if the player is paused
     *
     * \param player SDL_MoviePlayer instance
     *
     * \returns True if the player is paused, false otherwise
     */
    extern bool SDLMovie_IsPlayerPaused(SDL_MoviePlayer *player);

    /**
     * Check if the player has finished playback
     *
     * Player will automatically stop when it reaches the end of the movie and there no more frames to decode.
     *
     * \param player SDL_MoviePlayer instance
     *
     * \returns True if the player has finished playback, false otherwise
     */
    extern bool SDLMovie_HasPlayerFinished(SDL_MoviePlayer *player);

    /**
     * Get the player current time in seconds
     *
     * This function returns the current time of the player in seconds - amount of time since movie start.
     *
     * \param player SDL_MoviePlayer instance
     *
     * \returns Current time in seconds, or 0.0 on error.
     */
    extern float SDLMovie_GetPlayerCurrentTimeSeconds(SDL_MoviePlayer *player);

    /**
     * Get the player current time in milliseconds
     *
     * This function returns the current time of the player in milliseconds - amount of time since movie start.
     *
     * \param player SDL_MoviePlayer instance
     *
     * \returns Current time in milliseconds, or 0 on error.
     */
    extern Uint64 SDLMovie_GetPlayerCurrentTime(SDL_MoviePlayer *player);

    /**
     * Check if the player has audio enabled
     *
     * You can modify this with SDLMovie_SetPlayerAudioEnabled
     *
     * \param player SDL_MoviePlayer instance
     *
     * \returns True if the player has audio enabled, false otherwise
     */
    extern bool SDLMovie_IsPlayerAudioEnabled(SDL_MoviePlayer *player);

    /**
     * Check if the player has video enabled
     *
     * You can modify this with SDLMovie_SetPlayerVideoEnabled
     *
     * \param player SDL_MoviePlayer instance
     *
     * \returns True if the player has video enabled, false otherwise
     */
    extern bool SDLMovie_IsPlayerVideoEnabled(SDL_MoviePlayer *player);

    /**
     * Set player audio enabled
     *
     * This function allows you to enable or disable audio playback in the player.
     *
     * It's strongly recommended to call this function before first call to SDLMovie_UpdatePlayer,
     * as this function disabled audio decoding and output completely.
     *
     * If the player has no audio track, this function will have no effect.
     *
     * \param player SDL_MoviePlayer instance
     * \param enabled True to enable audio playback, false to disable
     */
    extern void SDLMovie_SetPlayerAudioEnabled(SDL_MoviePlayer *player, bool enabled);

    /**
     * Set player video enabled
     *
     * This function allows you to enable or disable video playback in the player.
     *
     * It's strongly recommended to call this function before first call to SDLMovie_UpdatePlayer,
     * as this function disabled video decoding and output completely.
     *
     * If the player has no video track, this function will have no effect.
     *
     * \param player SDL_MoviePlayer instance
     * \param enabled True to enable video playback, false to disable
     */
    extern void SDLMovie_SetPlayerVideoEnabled(SDL_MoviePlayer *player, bool enabled);

    /**
     * Free the player
     *
     * This function must be called when you no longer need the player instance.
     *
     * It will free all resources associated with the player, but NOT the movie instance attached to it.
     *
     * \param player SDL_MoviePlayer instance
     */
    extern void SDLMovie_FreePlayer(SDL_MoviePlayer *player);

#ifdef __cplusplus
}
#endif

#endif