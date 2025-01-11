#include <SDL_movie.h>
#include "SDL_movie_internal.h"

static char sdl_movie_error[1024] = {0};

bool SDLMovie_SetError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(sdl_movie_error, sizeof(sdl_movie_error), fmt, ap);
    va_end(ap);

    return false;
}

const char *SDLMovie_GetError()
{
    return sdl_movie_error;
}

SDL_Movie *SDLMovie_Open(const char *file)
{
    SDL_IOStream *stream = SDL_IOFromFile(file, "rb");

    if (!stream)
    {
        SDLMovie_SetError("Failed to open movie file %s:", SDL_GetError());
        return NULL;
    }

    return SDLMovie_OpenIO(stream);
}

SDL_Movie *SDLMovie_OpenIO(SDL_IOStream *io)
{
    if (!io)
    {
        return NULL;
    }

    SDL_Movie *movie = SDL_calloc(1, sizeof(SDL_Movie));
    if (!movie)
    {
        SDLMovie_SetError("Failed to allocate memory for movie");
        return NULL;
    }

    movie->io = io;
    movie->current_audio_track = SDL_MOVIE_NO_TRACK;
    movie->current_video_track = SDL_MOVIE_NO_TRACK;

    if (!SDLMovie_Parse_WebM(movie))
    {
        SDL_free(movie);
        return NULL;
    }

    /* Pre-select default tracks if possible */
    if (movie->ntracks > 0)
    {
        for (int i = 0; i < movie->ntracks; i++)
        {
            SDL_MovieTrack *tr = &movie->tracks[i];
            if (tr->type == SDL_MOVIE_TRACK_TYPE_VIDEO && movie->current_video_track == SDL_MOVIE_NO_TRACK)
            {
                SDLMovie_SelectTrack(movie, SDL_MOVIE_TRACK_TYPE_VIDEO, i);
            }
            else if (tr->type == SDL_MOVIE_TRACK_TYPE_AUDIO && movie->current_audio_track == SDL_MOVIE_NO_TRACK)
            {
                SDLMovie_SelectTrack(movie, SDL_MOVIE_TRACK_TYPE_AUDIO, i);
            }
        }
    }

    return movie;
}

void SDLMovie_FreeMovie(SDL_Movie *movie, bool closeio)
{
    if (!movie)
        return;

    for (int i = 0; i < movie->ntracks; i++)
    {
        SDL_free(movie->cached_frames[i]);

        if (movie->tracks[i].codec_private_data)
        {
            SDL_free(movie->tracks[i].codec_private_data);
        }
    }

    if (movie->conversion_video_frame_buffer)
    {
        SDL_free(movie->conversion_video_frame_buffer);
    }

    if (movie->encoded_video_frame)
    {
        SDL_free(movie->encoded_video_frame);
    }

    if (movie->current_frame_surface)
    {
        SDL_DestroySurface(movie->current_frame_surface);
    }

    if (movie->encoded_audio_buffer)
    {
        SDL_free(movie->encoded_audio_buffer);
    }

    SDLMovie_Close_Vorbis(movie);
    SDLMovie_Close_VPX(movie);

    if (closeio)
        SDL_CloseIO(movie->io);
    SDL_free(movie);
}

SDL_Texture *SDLMovie_CreatePlaybackTexture(SDL_Movie *movie, SDL_Renderer *renderer)
{
    if (!movie || !renderer)
    {
        SDLMovie_SetError("movie and renderer cannot be NULL");
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        SDLMovie_GetVideoTrack(movie)->video_width,
        SDLMovie_GetVideoTrack(movie)->video_height);

    if (!texture)
    {
        SDLMovie_SetError("Failed to create playback texture: %s", SDL_GetError());
        return NULL;
    }

    return texture;
}

void SDLMovie_AddCachedFrame(SDL_Movie *movie, Uint32 track, Uint64 timecode, Uint64 offset, Uint64 size)
{
    if (!movie)
        return;

    if (movie->ntracks <= track)
        return;
    if (track >= MAX_SDL_MOVIE_TRACKS)
        return;

    if (movie->count_cached_frames[track] >= movie->capacity_cached_frames[track])
    {
        movie->capacity_cached_frames[track] = movie->capacity_cached_frames[track] ? movie->capacity_cached_frames[track] * 2 : 1;
        movie->cached_frames[track] = SDL_realloc(movie->cached_frames[track], movie->capacity_cached_frames[track] * sizeof(CachedMovieFrame));
    }

    Uint64 last_frame_index = movie->count_cached_frames[track];

    CachedMovieFrame *frame = &movie->cached_frames[track][last_frame_index];

    frame->timecode = timecode;
    frame->offset = offset;
    frame->size = size;

    movie->count_cached_frames[track]++;

    movie->tracks[track].total_frames++;
    movie->tracks[track].total_bytes += size;
}

int SDLMovie_FindTrackByNumber(SDL_Movie *movie, Uint64 track_number)
{
    for (int i = 0; i < movie->ntracks; i++)
    {
        if (movie->tracks[i].track_number == track_number)
        {
            return i;
        }
    }
    return -1;
}

bool SDLMovie_CanPlaybackVideo(SDL_Movie *movie)
{
    return movie && movie->ntracks > 0 && movie->total_frames > 0 && movie->current_video_track != SDL_MOVIE_NO_TRACK;
}

bool SDLMovie_CanPlaybackAudio(SDL_Movie *movie)
{
    return movie && movie->ntracks > 0 && movie->total_audio_frames > 0 && movie->current_audio_track != SDL_MOVIE_NO_TRACK;
}

static SDL_MovieCodecType SDLMovie_GetTrackCodec(SDL_MovieTrack *track)
{
    if (SDL_strncmp(track->codec_id, "V_VP8", 32) == 0)
    {
        return SDL_MOVIE_CODEC_TYPE_VP8;
    }
    else if (SDL_strncmp(track->codec_id, "V_VP9", 32) == 0)
    {
        return SDL_MOVIE_CODEC_TYPE_VP9;
    }
    else if (SDL_strncmp(track->codec_id, "A_VORBIS", 32) == 0)
    {
        return SDL_MOVIE_CODEC_TYPE_VORBIS;
    }
    else if (SDL_strncmp(track->codec_id, "A_OPUS", 32) == 0)
    {
        return SDL_MOVIE_CODEC_TYPE_OPUS;
    }
    else
    {
        return SDL_MOVIE_CODEC_TYPE_UNKNOWN;
    }
}

void SDLMovie_SelectTrack(SDL_Movie *movie, SDL_MovieTrackType type, int track)
{
    if (!movie)
        return;

    if (track < 0)
        return;
    if (track >= movie->ntracks)
        return;

    if (movie->tracks[track].type != type)
        return;

    if (type == SDL_MOVIE_TRACK_TYPE_VIDEO)
    {
        movie->current_video_track = track;

        SDL_MovieTrack *new_video_track = SDLMovie_GetVideoTrack(movie);

        movie->video_codec = SDLMovie_GetTrackCodec(new_video_track);
        movie->total_frames = new_video_track->total_frames;

        if (movie->current_frame_surface)
        {
            SDL_DestroySurface(movie->current_frame_surface);
        }

        movie->current_frame_surface = SDL_CreateSurface(
            new_video_track->video_width,
            new_video_track->video_height,
            SDL_PIXELFORMAT_RGB24);
    }
    else if (type == SDL_MOVIE_TRACK_TYPE_AUDIO)
    {
        movie->current_audio_track = track;

        SDL_MovieTrack *new_audio_track = SDLMovie_GetAudioTrack(movie);
        movie->audio_codec = SDLMovie_GetTrackCodec(new_audio_track);
        movie->total_audio_frames = new_audio_track->total_frames;
        movie->audio_spec.channels = new_audio_track->audio_channels;
        movie->audio_spec.freq = new_audio_track->audio_sample_frequency;
        movie->audio_spec.format = SDL_AUDIO_F32;
    }
}

SDL_MovieTrack *SDLMovie_GetVideoTrack(SDL_Movie *movie)
{
    if (!movie)
        return NULL;

    return &movie->tracks[movie->current_video_track];
}
SDL_MovieTrack *SDLMovie_GetAudioTrack(SDL_Movie *movie)
{
    if (!movie)
        return NULL;

    return &movie->tracks[movie->current_audio_track];
}

bool SDLMovie_HasNextVideoFrame(SDL_Movie *movie)
{
    return movie && SDLMovie_CanPlaybackVideo(movie) && movie->current_frame < movie->total_frames;
}

bool SDLMovie_DecodeVideoFrame(SDL_Movie *movie)
{
    if (!movie)
        return false;

    if (!SDLMovie_CanPlaybackVideo(movie))
    {
        SDLMovie_SetError("No tracks or playback data available");
        return false;
    }

    SDL_MovieTrack *video_track = SDLMovie_GetVideoTrack(movie);

    SDLMovie_ReadCurrentFrame(movie, SDL_MOVIE_TRACK_TYPE_VIDEO);

    if (movie->video_codec == SDL_MOVIE_CODEC_TYPE_VP8 || movie->video_codec == SDL_MOVIE_CODEC_TYPE_VP9)
    {
        return SDLMovie_Decode_VPX(movie);
    }

    SDLMovie_SetError("Unsupported video codec, frame not decoded");

    return false;
}

bool SDLMovie_UpdatePlaybackTexture(SDL_Movie *movie, SDL_Texture *texture)
{
    if (!movie || !texture)
    {
        SDLMovie_SetError("movie and texture cannot be NULL");
        return false;
    }

    if (!movie->current_frame_surface)
    {
        SDLMovie_SetError("No frame available, you must decode a frame first");
        return false;
    }

    if (texture->format != movie->current_frame_surface->format)
    {
        SDLMovie_SetError("Texture format does not match video frame format, provided = %d, required = %d",
                          texture->format, movie->current_frame_surface->format);
        return false;
    }

    SDL_Surface *target;
    SDL_LockTextureToSurface(texture, NULL, &target);
    SDL_BlitSurface(movie->current_frame_surface, NULL, target, NULL);
    SDL_UnlockTexture(texture);

    return true;
}

void SDLMovie_ReadCurrentFrame(SDL_Movie *movie, SDL_MovieTrackType type)
{
    if (!movie)
        return;

    int target_track_index = type == SDL_MOVIE_TRACK_TYPE_VIDEO ? movie->current_video_track : movie->current_audio_track;

    if (target_track_index == SDL_MOVIE_NO_TRACK)
    {
        return;
    }

    if (type == SDL_MOVIE_TRACK_TYPE_VIDEO)
    {
        CachedMovieFrame *frame = &movie->cached_frames[target_track_index][movie->current_frame];

        if (!movie->encoded_video_frame || movie->encoded_video_frame_size < frame->size)
        {
            movie->encoded_video_frame = SDL_realloc(movie->encoded_video_frame, frame->size);
        }

        SDL_SeekIO(movie->io, frame->offset, SDL_IO_SEEK_SET);

        SDL_ReadIO(movie->io, movie->encoded_video_frame, frame->size);

        movie->encoded_video_frame_size = frame->size;
    }
    else
    {
        CachedMovieFrame *frame = &movie->cached_frames[target_track_index][movie->current_audio_frame];

        if (!movie->encoded_audio_frame || movie->encoded_audio_frame_size < frame->size)
        {
            movie->encoded_audio_frame = SDL_realloc(movie->encoded_audio_frame, frame->size);
        }

        SDL_SeekIO(movie->io, frame->offset, SDL_IO_SEEK_SET);

        SDL_ReadIO(movie->io, movie->encoded_audio_frame, frame->size);

        movie->encoded_audio_frame_size = frame->size;
    }
}

Uint64 SDLMovie_GetLastFrameDecodeTime(SDL_Movie *movie)
{
    if (!movie)
        return 0;
    return movie->last_frame_decode_ms;
}

Uint64 SDLMovie_GetTotalFrames(SDL_Movie *movie)
{
    if (!movie)
        return 0;
    return movie->total_frames;
}

Uint64 SDLMovie_GetCurrentFrame(SDL_Movie *movie)
{
    if (!movie)
        return 0;
    return movie->current_frame;
}

void SDLMovie_NextVideoFrame(SDL_Movie *movie)
{
    if (!movie)
        return;

    if (!SDLMovie_CanPlaybackVideo(movie))
    {
        SDLMovie_SetError("No tracks or playback data available");
        return;
    }

    if (movie->current_frame < movie->total_frames)
    {
        movie->current_frame++;
    }
}

void SDLMovie_GetVideoSize(SDL_Movie *movie, int *w, int *h)
{
    if (!movie)
        return;

    if (movie->current_video_track == SDL_MOVIE_NO_TRACK)
        return;

    SDL_MovieTrack *video_track = SDLMovie_GetVideoTrack(movie);

    if (w)
        *w = video_track->video_width;
    if (h)
        *h = video_track->video_height;
}

const SDL_Surface *SDLMovie_GetVideoFrameSurface(SDL_Movie *movie)
{
    if (!movie || !movie->current_frame_surface)
    {
        return NULL;
    }

    return movie->current_frame_surface;
}

void SDLMovie_SeekFrame(SDL_Movie *movie, Uint64 frame)
{
    if (!movie)
        return;

    if (frame >= movie->total_frames)
        return;

    movie->current_frame = frame;
}

bool SDLMovie_HasNextAudioFrame(SDL_Movie *movie)
{
    if (!movie || movie->current_audio_track == SDL_MOVIE_NO_TRACK)
    {
        return false;
    }

    return movie->current_audio_frame < movie->total_audio_frames;
}

bool SDLMovie_DecodeAudioFrame(SDL_Movie *movie)
{
    if (!movie || movie->current_audio_track == SDL_MOVIE_NO_TRACK)
    {
        return false;
    }

    SDLMovie_ReadCurrentFrame(movie, SDL_MOVIE_TRACK_TYPE_AUDIO);

    if (movie->audio_codec == SDL_MOVIE_CODEC_TYPE_VORBIS)
    {
        VorbisDecodeResult res = SDLMovie_Decode_Vorbis(movie);

        return res == SDL_MOVIE_VORBIS_DECODE_DONE;
    }

    SDLMovie_SetError("Unsupported audio codec, frame not decoded");

    return false;
}

const SDL_MovieAudioSample *SDLMovie_GetAudioSamples(SDL_Movie *movie, size_t *size, int *count)
{
    if (!movie || !movie->decoded_audio_frame)
    {
        if (size)
            *size = 0;
        if (count)
            *count = 0;
        return NULL;
    }

    if (size)
        *size = movie->decoded_audio_samples * sizeof(SDL_MovieAudioSample) * movie->audio_spec.channels;

    if (count)
        *count = movie->decoded_audio_samples;

    return movie->decoded_audio_frame;
}

void SDLMovie_NextAudioFrame(SDL_Movie *movie)
{
    if (!movie)
        return;

    if (!SDLMovie_CanPlaybackAudio(movie))
    {
        SDLMovie_SetError("No tracks or playback data available");
        return;
    }

    if (movie->current_audio_frame < movie->total_audio_frames)
    {
        movie->current_audio_frame++;
    }
}

const SDL_AudioSpec *SDLMovie_GetAudioSpec(SDL_Movie *movie)
{
    if (!movie)
        return NULL;
    if (movie->current_audio_track == SDL_MOVIE_NO_TRACK)
        return NULL;
    return &movie->audio_spec;
}

bool SDLMovie_PreloadAudioStream(SDL_Movie *movie)
{
    if (!movie)
        return false;

    if (movie->current_audio_track == SDL_MOVIE_NO_TRACK)
        return false;

    SDL_MovieTrack *audio_track = SDLMovie_GetAudioTrack(movie);

    Uint64 buffer_size = audio_track->total_bytes;

    if (!movie->encoded_audio_buffer || movie->encoded_audio_buffer_size < buffer_size)
    {
        movie->encoded_audio_buffer = SDL_realloc(movie->encoded_audio_buffer, buffer_size);
        movie->encoded_audio_buffer_size = buffer_size;
    }

    Uint64 offset = 0;

    for (Uint64 frame = 0; frame < audio_track->total_frames; frame++)
    {
        CachedMovieFrame *frame_data = &movie->cached_frames[movie->current_audio_track][frame];

        SDL_SeekIO(movie->io, frame_data->offset, SDL_IO_SEEK_SET);

        SDL_ReadIO(movie->io, movie->encoded_audio_buffer + offset, frame_data->size);

        offset += frame_data->size;

        SDL_assert(offset <= buffer_size);
    }

    movie->encoded_audio_buffer_cursor = 0;

    return true;
}

void *SDLMovie_ReadEncodedAudioData(SDL_Movie *movie, void *dest, int size)
{
    if (!movie || !movie->encoded_audio_buffer || movie->encoded_audio_buffer_cursor + size > movie->encoded_audio_buffer_size)
    {
        return NULL;
    }

    Uint8 *data = movie->encoded_audio_buffer + movie->encoded_audio_buffer_cursor;

    if (dest)
        SDL_memcpy(dest, data, size);

    return data;
}

const SDL_MovieTrack *SDLMovie_GetTrack(const SDL_Movie *movie, int index)
{
    if (!movie || index < 0 || index >= movie->ntracks)
    {
        return NULL;
    }

    return &movie->tracks[index];
}

int SDLMovie_GetTrackCount(const SDL_Movie *movie)
{
    if (!movie)
        return 0;
    return movie->ntracks;
}

Uint64 SDLMovie_TimecodeToMilliseconds(SDL_Movie *movie, Uint64 timecode)
{
    if (!movie)
        return 0;
    return timecode * movie->timecode_scale / 1000000;
}
Uint64 SDLMovie_MillisecondsToTimecode(SDL_Movie *movie, Uint64 ms)
{
    if (!movie)
        return 0;
    return ms * 1000000 / movie->timecode_scale;
}