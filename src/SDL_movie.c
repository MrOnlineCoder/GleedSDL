#include <SDL_movie.h>
#include "SDL_movie_internal.h"

static char sdl_movie_error[1024] = {0};

void SDLMovie_SetError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(sdl_movie_error, sizeof(sdl_movie_error), fmt, ap);
    va_end(ap);
}

const char *SDLMovie_GetError()
{
    return sdl_movie_error;
}

SDL_Movie *SDLMovie_Open(const char *file)
{
    return SDLMovie_OpenIO(
        SDL_IOFromFile(file, "rb"));
}

SDL_Movie *SDLMovie_OpenIO(SDL_IOStream *io)
{
    if (!io)
        return NULL;

    SDL_Movie *movie = SDL_calloc(1, sizeof(SDL_Movie));
    if (!movie)
        return NULL;

    movie->io = io;

    if (!SDLMovie_Parse_WebM(movie))
    {
        SDL_free(movie);
        return NULL;
    }

    if (SDLMovie_CanPlayback(movie))
    {
        for (int i = 0; i < movie->ntracks; i++)
        {
            MovieTrack *tr = &movie->tracks[i];
            if (tr->type == SDL_MOVIE_TRACK_TYPE_VIDEO)
            {
                movie->current_video_track = i;
            }
            else if (tr->type == SDL_MOVIE_TRACK_TYPE_AUDIO)
            {
                movie->current_audio_track = i;
            }
        }
    }

    return movie;
}

void SDLMovie_Free(SDL_Movie *movie)
{
    if (!movie)
        return;

    for (int i = 0; i < movie->ntracks; i++)
    {
        SDL_free(movie->cached_frames[i]);
    }

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
        SDL_PIXELFORMAT_XBGR8888,
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

    if (timecode > movie->total_time)
    {
        movie->total_time = timecode;
    }

    if (last_frame_index > movie->tracks[track].total_frames)
    {
        movie->tracks[track].total_frames = last_frame_index;
    }
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

bool SDLMovie_CanPlayback(SDL_Movie *movie)
{
    return movie && movie->ntracks > 0 && movie->count_cached_frames[0] > 0;
}

void SDLMovie_SelectTrack(SDL_Movie *movie, SDL_MovieTrackType type, int track)
{
    if (!movie)
        return;

    if (type == SDL_MOVIE_TRACK_TYPE_VIDEO)
    {
        movie->current_video_track = track;
    }
    else if (type == SDL_MOVIE_TRACK_TYPE_AUDIO)
    {
        movie->current_audio_track = track;
    }

    movie->video_codec = SDL_MOVIE_CODEC_TYPE_VP8;
    movie->audio_codec = SDL_MOVIE_CODEC_TYPE_VORBIS;

    movie->total_frames = movie->tracks[movie->current_video_track].total_frames;
}

MovieTrack *SDLMovie_GetVideoTrack(SDL_Movie *movie)
{
    if (!movie)
        return NULL;

    return &movie->tracks[movie->current_video_track];
}
MovieTrack *SDLMovie_GetAudioTrack(SDL_Movie *movie)
{
    if (!movie)
        return NULL;

    return &movie->tracks[movie->current_audio_track];
}

bool SDLMovie_HasNextFrame(SDL_Movie *movie)
{
    return movie && SDLMovie_CanPlayback(movie);
}

bool SDLMovie_DecodeFrame(SDL_Movie *movie)
{
    if (!movie)
        return false;

    if (!SDLMovie_CanPlayback(movie))
    {
        SDLMovie_SetError("No tracks or playback data available");
        return false;
    }

    MovieTrack *video_track = SDLMovie_GetVideoTrack(movie);

    if (!movie->current_frame_surface)
    {
        movie->current_frame_surface = SDL_CreateSurface(
            video_track->video_width,
            video_track->video_height,
            SDL_PIXELFORMAT_RGB24);
    }

    SDLMovie_ReadCurrentFrame(movie);

    SDLMovie_Decode_VP8(movie);

    movie->current_frame++;

    return true;
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

    SDL_Surface *target;
    SDL_LockTextureToSurface(texture, NULL, &target);
    SDL_BlitSurface(movie->current_frame_surface, NULL, target, NULL);
    SDL_UnlockTexture(texture);

    return true;
}

void SDLMovie_ReadCurrentFrame(SDL_Movie *movie)
{
    if (!movie)
        return;

    CachedMovieFrame *frame = &movie->cached_frames[movie->current_video_track][movie->current_frame];

    movie->encoded_video_frame = SDL_realloc(movie->encoded_video_frame, frame->size);

    SDL_SeekIO(movie->io, frame->offset, SDL_IO_SEEK_SET);

    SDL_ReadIO(movie->io, movie->encoded_video_frame, frame->size);

    movie->encoded_video_frame_size = frame->size;
}