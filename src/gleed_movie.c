#include "gleed_movie_internal.h"

static char gleed_movie_error[1024] = {0};

static int GleedCachedFrameComparator(const void *a, const void *b)
{
    const CachedMovieFrame *frame_a = (const CachedMovieFrame *)a;
    const CachedMovieFrame *frame_b = (const CachedMovieFrame *)b;

    return frame_a->timecode - frame_b->timecode;
}

bool GleedSetError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(gleed_movie_error, sizeof(gleed_movie_error), fmt, ap);
    va_end(ap);

    return false;
}

const char *GleedGetError()
{
    return gleed_movie_error;
}

GleedMovie *GleedOpen(const char *file)
{
    SDL_IOStream *stream = SDL_IOFromFile(file, "rb");

    if (!stream)
    {
        GleedSetError("Failed to open movie file %s:", SDL_GetError());
        return NULL;
    }

    return GleedOpenIO(stream);
}

GleedMovie *GleedOpenIO(SDL_IOStream *io)
{
    if (!io)
    {
        return NULL;
    }

    GleedMovie *movie = SDL_calloc(1, sizeof(GleedMovie));
    if (!movie)
    {
        GleedSetError("Failed to allocate memory for movie");
        return NULL;
    }

    movie->io = io;
    movie->current_audio_track = GLEED_NO_TRACK;
    movie->current_video_track = GLEED_NO_TRACK;

    if (!GleedParseWebM(movie))
    {
        SDL_free(movie);
        return NULL;
    }

    /* Pre-select default tracks if possible */
    if (movie->ntracks > 0)
    {
        for (int i = 0; i < movie->ntracks; i++)
        {
            GleedMovieTrack *tr = &movie->tracks[i];
            if (tr->type == GLEED_TRACK_TYPE_VIDEO && movie->current_video_track == GLEED_NO_TRACK)
            {
                GleedSelectTrack(movie, GLEED_TRACK_TYPE_VIDEO, i);
            }
            else if (tr->type == GLEED_TRACK_TYPE_AUDIO && movie->current_audio_track == GLEED_NO_TRACK)
            {
                GleedSelectTrack(movie, GLEED_TRACK_TYPE_AUDIO, i);
            }

            /* Important step to ensure we have frames ordered chronologically, by timecode */
            SDL_qsort(movie->cached_frames[i], movie->count_cached_frames[i], sizeof(CachedMovieFrame), GleedCachedFrameComparator);
        }
    }

    return movie;
}

void GleedFreeMovie(GleedMovie *movie, bool closeio)
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

    if (movie->decoded_audio_frame)
    {
        SDL_free(movie->decoded_audio_frame);
    }

    if (movie->current_frame_surface)
    {
        SDL_DestroySurface(movie->current_frame_surface);
    }

    if (movie->encoded_audio_buffer)
    {
        SDL_free(movie->encoded_audio_buffer);
    }

    GleedCloseVorbis(movie);
    GleedCloseVPX(movie);

    if (closeio)
    {
        SDL_CloseIO(movie->io);
    }

    SDL_free(movie);
}

SDL_Texture *GleedCreatePlaybackTexture(GleedMovie *movie, SDL_Renderer *renderer)
{
    if (!movie || !renderer)
    {
        GleedSetError("movie and renderer cannot be NULL");
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, /*The texture contents will be updated frequently*/
        GleedGetVideoTrack(movie)->video_width,
        GleedGetVideoTrack(movie)->video_height);

    if (!texture)
    {
        GleedSetError("Failed to create playback texture: %s", SDL_GetError());
        return NULL;
    }

    return texture;
}

void GleedAddCachedFrame(GleedMovie *movie, Uint32 track, Uint64 timecode, Uint32 offset, Uint32 size, bool key_frame)
{
    if (!movie)
        return;

    if (movie->ntracks <= track)
        return;
    if (track >= MAX_GLEED_TRACKS)
        return;

    if (movie->count_cached_frames[track] >= movie->capacity_cached_frames[track])
    {
        movie->capacity_cached_frames[track] = movie->capacity_cached_frames[track] ? movie->capacity_cached_frames[track] * 2 : 1;
        movie->cached_frames[track] = SDL_realloc(movie->cached_frames[track], movie->capacity_cached_frames[track] * sizeof(CachedMovieFrame));
    }

    const int new_frame_index = movie->count_cached_frames[track];

    CachedMovieFrame *frame = &movie->cached_frames[track][new_frame_index];

    /* Up to discussion if this is correct*/
    const Sint64 final_timecode = timecode - GleedMatroskaTicksToMilliseconds(movie, movie->tracks[track].codec_delay);

    if (final_timecode < 0)
    {
        return;
    }

    frame->timecode = final_timecode;
    frame->offset = offset;
    frame->size = size;
    frame->key_frame = key_frame;

    /* We record the actual memory offset for each frame (accumulating the sizes of previous frames) */
    if (new_frame_index == 0)
    {
        frame->mem_offset = 0;
    }
    else
    {
        const int last_frame_index = new_frame_index - 1;

        const CachedMovieFrame *last_frame = &movie->cached_frames[track][last_frame_index];

        frame->mem_offset = last_frame->mem_offset + last_frame->size;
    }

    movie->count_cached_frames[track]++;

    movie->tracks[track].total_frames++;
    movie->tracks[track].total_bytes += size;
}

int GleedFindTrackByNumber(GleedMovie *movie, Uint32 track_number)
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

bool GleedCanPlaybackVideo(GleedMovie *movie)
{
    return movie && movie->ntracks > 0 && movie->total_frames > 0 && movie->current_video_track != GLEED_NO_TRACK;
}

bool GleedCanPlaybackAudio(GleedMovie *movie)
{
    return movie && movie->ntracks > 0 && movie->total_audio_frames > 0 && movie->current_audio_track != GLEED_NO_TRACK;
}

static GleedMovieCodecType GleedGetTrackCodec(GleedMovieTrack *track)
{
    if (SDL_strncmp(track->codec_id, "V_VP8", 32) == 0)
    {
        return GLEED_CODEC_TYPE_VP8;
    }
    else if (SDL_strncmp(track->codec_id, "V_VP9", 32) == 0)
    {
        return GLEED_CODEC_TYPE_VP9;
    }
    else if (SDL_strncmp(track->codec_id, "A_VORBIS", 32) == 0)
    {
        return GLEED_CODEC_TYPE_VORBIS;
    }
    else if (SDL_strncmp(track->codec_id, "A_OPUS", 32) == 0)
    {
        return GLEED_CODEC_TYPE_OPUS;
    }
    else
    {
        return GLEED_CODEC_TYPE_UNKNOWN;
    }
}

void GleedSelectTrack(GleedMovie *movie, GleedMovieTrackType type, int track)
{
    if (!movie)
        return;

    if (track < 0)
        return;
    if (track >= movie->ntracks)
        return;

    if (movie->tracks[track].type != type)
        return;

    if (type == GLEED_TRACK_TYPE_VIDEO)
    {
        movie->current_video_track = track;

        GleedMovieTrack *new_video_track = GleedGetVideoTrack(movie);

        movie->video_codec = GleedGetTrackCodec(new_video_track);
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
    else if (type == GLEED_TRACK_TYPE_AUDIO)
    {
        movie->current_audio_track = track;

        GleedMovieTrack *new_audio_track = GleedGetAudioTrack(movie);
        movie->audio_codec = GleedGetTrackCodec(new_audio_track);
        movie->total_audio_frames = new_audio_track->total_frames;
        movie->audio_spec.channels = new_audio_track->audio_channels;
        movie->audio_spec.freq = new_audio_track->audio_sample_frequency;
        movie->audio_spec.format = SDL_AUDIO_F32;
    }
}

GleedMovieTrack *GleedGetVideoTrack(GleedMovie *movie)
{
    if (!movie)
        return NULL;

    return &movie->tracks[movie->current_video_track];
}
GleedMovieTrack *GleedGetAudioTrack(GleedMovie *movie)
{
    if (!movie)
        return NULL;

    return &movie->tracks[movie->current_audio_track];
}

bool GleedHasNextVideoFrame(GleedMovie *movie)
{
    return movie && GleedCanPlaybackVideo(movie) && movie->current_frame < movie->total_frames;
}

bool GleedDecodeVideoFrame(GleedMovie *movie)
{
    if (!movie)
        return false;

    if (!GleedCanPlaybackVideo(movie))
    {
        GleedSetError("No tracks or playback data available");
        return false;
    }

    GleedMovieTrack *video_track = GleedGetVideoTrack(movie);

    GleedReadCurrentFrame(movie, GLEED_TRACK_TYPE_VIDEO);

    if (movie->video_codec == GLEED_CODEC_TYPE_VP8 || movie->video_codec == GLEED_CODEC_TYPE_VP9)
    {
        return GleedDecodeVPX(movie);
    }

    GleedSetError("Unsupported video codec, frame not decoded");

    return false;
}

bool GleedUpdatePlaybackTexture(GleedMovie *movie, SDL_Texture *texture)
{
    if (!movie || !texture)
    {
        GleedSetError("movie and texture cannot be NULL");
        return false;
    }

    if (!movie->current_frame_surface)
    {
        GleedSetError("No frame available, you must decode a frame first");
        return false;
    }

    if (texture->format != movie->current_frame_surface->format)
    {
        GleedSetError("Texture format does not match video frame format, provided = %d, required = %d",
                      texture->format, movie->current_frame_surface->format);
        return false;
    }

    SDL_Surface *target;
    SDL_LockTextureToSurface(texture, NULL, &target);
    SDL_BlitSurface(movie->current_frame_surface, NULL, target, NULL);
    SDL_UnlockTexture(texture);

    return true;
}

void GleedReadCurrentFrame(GleedMovie *movie, GleedMovieTrackType type)
{
    if (!movie)
        return;

    int target_track_index = type == GLEED_TRACK_TYPE_VIDEO ? movie->current_video_track : movie->current_audio_track;

    if (target_track_index == GLEED_NO_TRACK)
    {
        return;
    }

    if (type == GLEED_TRACK_TYPE_VIDEO)
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

        /* If we have preloaded all our encoded audio data into one big buffer, just point inside it*/
        if (movie->encoded_audio_buffer && movie->encoded_audio_buffer_size > 0)
        {
            movie->encoded_audio_frame = movie->encoded_audio_buffer + frame->mem_offset;
        }
        else
        {
            /* Otherwise, perform an IO read */
            if (!movie->encoded_audio_frame || movie->encoded_audio_frame_size < frame->size)
            {
                movie->encoded_audio_frame = SDL_realloc(movie->encoded_audio_frame, frame->size);
            }

            SDL_SeekIO(movie->io, frame->offset, SDL_IO_SEEK_SET);

            SDL_ReadIO(movie->io, movie->encoded_audio_frame, frame->size);
        }

        movie->encoded_audio_frame_size = frame->size;
    }
}

Uint32 GleedGetLastFrameDecodeTime(GleedMovie *movie)
{
    if (!movie)
        return 0;
    return movie->last_frame_decode_ms;
}

Uint32 GleedGetTotalVideoFrames(GleedMovie *movie)
{
    if (!movie)
        return 0;
    return movie->total_frames;
}

Uint32 GleedGetCurrentFrame(GleedMovie *movie)
{
    if (!movie)
        return 0;
    return movie->current_frame;
}

void GleedNextVideoFrame(GleedMovie *movie)
{
    if (!movie)
        return;

    if (!GleedCanPlaybackVideo(movie))
    {
        GleedSetError("No tracks or playback data available");
        return;
    }

    if (movie->current_frame < movie->total_frames)
    {
        movie->current_frame++;
    }
}

void GleedGetVideoSize(GleedMovie *movie, int *w, int *h)
{
    if (!movie)
        return;

    if (movie->current_video_track == GLEED_NO_TRACK)
        return;

    GleedMovieTrack *video_track = GleedGetVideoTrack(movie);

    if (w)
        *w = video_track->video_width;
    if (h)
        *h = video_track->video_height;
}

const SDL_Surface *GleedGetVideoFrameSurface(GleedMovie *movie)
{
    if (!movie || !movie->current_frame_surface)
    {
        return NULL;
    }

    return movie->current_frame_surface;
}

void GleedSeekFrame(GleedMovie *movie, Uint32 frame)
{
    if (!movie)
        return;

    if (frame >= movie->total_frames)
        return;

    movie->current_frame = frame;
}

bool GleedHasNextAudioFrame(GleedMovie *movie)
{
    if (!movie || movie->current_audio_track == GLEED_NO_TRACK)
    {
        return false;
    }

    return movie->current_audio_frame < movie->total_audio_frames;
}

bool GleedDecodeAudioFrame(GleedMovie *movie)
{
    if (!movie || movie->current_audio_track == GLEED_NO_TRACK)
    {
        return false;
    }

    GleedReadCurrentFrame(movie, GLEED_TRACK_TYPE_AUDIO);

    if (movie->audio_codec == GLEED_CODEC_TYPE_VORBIS)
    {
        VorbisDecodeResult res = GleedDecode_Vorbis(movie);

        return res == GLEED_VORBIS_DECODE_DONE;
    }
    else if (movie->audio_codec == GLEED_CODEC_TYPE_OPUS)
    {
        return GleedDecodeOpus(movie);
    }

    GleedSetError("Unsupported audio codec, frame not decoded");

    return false;
}

const GleedMovieAudioSample *GleedGetAudioSamples(GleedMovie *movie, size_t *size, int *count)
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
        *size = movie->decoded_audio_samples * sizeof(GleedMovieAudioSample) * movie->audio_spec.channels;

    if (count)
        *count = movie->decoded_audio_samples;

    return movie->decoded_audio_frame;
}

void GleedNextAudioFrame(GleedMovie *movie)
{
    if (!movie)
        return;

    if (!GleedCanPlaybackAudio(movie))
    {
        GleedSetError("No tracks or playback data available");
        return;
    }

    if (movie->current_audio_frame < movie->total_audio_frames)
    {
        movie->current_audio_frame++;
    }
}

const SDL_AudioSpec *GleedGetAudioSpec(GleedMovie *movie)
{
    if (!movie)
        return NULL;
    if (movie->current_audio_track == GLEED_NO_TRACK)
        return NULL;
    return &movie->audio_spec;
}

bool GleedPreloadAudioStream(GleedMovie *movie)
{
    if (!movie)
    {
        return GleedSetError("movie is NULL");
    }

    if (movie->current_audio_track == GLEED_NO_TRACK)
    {
        return GleedSetError("No audio track selected for preload");
    }

    GleedMovieTrack *audio_track = GleedGetAudioTrack(movie);

    const Uint32 buffer_size = audio_track->total_bytes;

    if (!movie->encoded_audio_buffer || movie->encoded_audio_buffer_size < buffer_size)
    {
        movie->encoded_audio_buffer = SDL_realloc(movie->encoded_audio_buffer, buffer_size);
        movie->encoded_audio_buffer_size = buffer_size;
    }

    Uint32 offset = 0;

    for (Uint32 frame = 0; frame < audio_track->total_frames; frame++)
    {
        CachedMovieFrame *frame_data = &movie->cached_frames[movie->current_audio_track][frame];

        SDL_SeekIO(movie->io, frame_data->offset, SDL_IO_SEEK_SET);

        SDL_ReadIO(movie->io, movie->encoded_audio_buffer + offset, frame_data->size);

        offset += frame_data->size;

        SDL_assert(offset <= buffer_size);
    }

    return true;
}

const GleedMovieTrack *GleedGetTrack(const GleedMovie *movie, int index)
{
    if (!movie || index < 0 || index >= movie->ntracks)
    {
        return NULL;
    }

    return &movie->tracks[index];
}

int GleedGetTrackCount(const GleedMovie *movie)
{
    if (!movie)
        return 0;
    return movie->ntracks;
}

Uint64 GleedTimecodeToMilliseconds(GleedMovie *movie, Uint64 timecode)
{
    if (!movie)
        return 0;
    return timecode * movie->timecode_scale / 1000000;
}
Uint64 GleedMillisecondsToTimecode(GleedMovie *movie, Uint64 ms)
{
    if (!movie)
        return 0;
    return ms * 1000000 / movie->timecode_scale;
}

CachedMovieFrame *GleedGetCurrentCachedFrame(GleedMovie *movie, GleedMovieTrackType type)
{
    if (!movie)
        return NULL;

    int target_track_index = type == GLEED_TRACK_TYPE_VIDEO ? movie->current_video_track : movie->current_audio_track;

    if (target_track_index == GLEED_NO_TRACK)
    {
        return NULL;
    }

    if (type == GLEED_TRACK_TYPE_VIDEO)
    {
        return &movie->cached_frames[target_track_index][movie->current_frame];
    }
    else
    {
        return &movie->cached_frames[target_track_index][movie->current_audio_frame];
    }
}

Uint64 GleedMatroskaTicksToMilliseconds(GleedMovie *movie, Uint64 ticks)
{
    /*Matroska ticks are nanoseconds, convert to ms*/
    return ticks / 1000000;
}