#include "SDL_movie_internal.h"

#define SDL_MOVIE_PLAYER_SOUND_PRELOAD_MS 200

static bool check_player(SDL_MoviePlayer *player)
{
    return player && player->mov;
}

SDL_MoviePlayer *SDLMovie_CreatePlayer(SDL_Movie *mov)
{
    if (!mov)
        return NULL;

    SDL_MoviePlayer *player = (SDL_MoviePlayer *)SDL_calloc(1, sizeof(SDL_MoviePlayer));

    if (!player)
    {
        SDLMovie_SetError("Failed to allocate memory for movie player");
        return NULL;
    }

    SDLMovie_SetPlayerMovie(player, mov);

    player->last_frame_at_ticks = SDL_GetTicks();

    return player;
}

SDL_MoviePlayer *SDLMovie_CreatePlayerFromPath(const char *path)
{
    SDL_Movie *mov = SDLMovie_Open(path);

    if (!mov)
    {
        return NULL;
    }

    return SDLMovie_CreatePlayer(mov);
}

SDL_MoviePlayer *SDLMovie_CreatePlayerFromIO(SDL_IOStream *io)
{
    SDL_Movie *mov = SDLMovie_OpenIO(io);

    if (!mov)
    {
        return NULL;
    }

    return SDLMovie_CreatePlayer(mov);
}

void SDLMovie_SetPlayerMovie(SDL_MoviePlayer *player, SDL_Movie *mov)
{
    if (!player || !mov)
        return;

    player->mov = mov;
    player->current_time = 0;
    player->next_video_frame_at = 0;
    player->next_audio_frame_at = 0;
    player->finished = false;
    player->video_playback = SDLMovie_CanPlaybackVideo(mov);
    player->audio_playback = SDLMovie_CanPlaybackAudio(mov);

    SDLMovie_SeekFrame(player->mov, 0);

    SDL_MovieTrack *audio_track = SDLMovie_GetAudioTrack(player->mov);
    SDL_MovieTrack *video_track = SDLMovie_GetVideoTrack(player->mov);

    for (int n = 0; n < mov->ntracks; n++)
    {
        SDL_MovieTrack *track = &mov->tracks[n];
        for (int c = 0; c < mov->count_cached_frames[n]; c++)
        {
            CachedMovieFrame *frame = &mov->cached_frames[n][c];
            printf("Track %d, frame %d, timecode %lld, offset %d, size %d, key_frame %d\n", n, c, frame->timecode, frame->offset, frame->size, frame->key_frame);
        }
    }

    if (audio_track && audio_track->codec_delay > 0)
    {
        player->next_audio_frame_at = SDLMovie_MatroskaTicksToMilliseconds(player->mov, audio_track->codec_delay);
    }

    if (video_track && video_track->codec_delay > 0)
    {
        player->next_video_frame_at = SDLMovie_MatroskaTicksToMilliseconds(player->mov, video_track->codec_delay);
    }
}

void SDLMovie_FreePlayer(SDL_MoviePlayer *player)
{
    if (!player)
        return;

    if (player->audio_buffer)
        SDL_free(player->audio_buffer);

    if (player->output_audio_stream)
    {
        SDL_DestroyAudioStream(player->output_audio_stream);
    }

    if (player->current_video_frame_surface)
    {
        SDL_DestroySurface(player->current_video_frame_surface);
    }

    SDL_free(player);
}

SDL_MoviePlayerUpdateResult SDLMovie_UpdatePlayer(SDL_MoviePlayer *player, int time_delta_ms)
{
    if (!check_player(player))
        return SDL_MOVIE_PLAYER_UPDATE_NONE;

    if (time_delta_ms == 0)
        return SDL_MOVIE_PLAYER_UPDATE_NONE;

    if (player->paused)
        return SDL_MOVIE_PLAYER_UPDATE_NONE;

    if (player->finished)
        return SDL_MOVIE_PLAYER_UPDATE_NONE;

    SDL_MoviePlayerUpdateResult result = SDL_MOVIE_PLAYER_UPDATE_NONE;

    const Uint64 time_passed = time_delta_ms < 0 ? SDL_GetTicks() - player->last_frame_at_ticks : time_delta_ms;

    player->current_time += time_passed;

    player->last_frame_at_ticks = SDL_GetTicks();

    if (player->audio_playback && SDLMovie_CanPlaybackAudio(player->mov) && player->current_time >= player->next_audio_frame_at)
    {
        /* Audio output is much more sensitive to delays or interruptions, so we load a bit more samples */
        const Uint64 preload_time = player->current_time + SDL_MOVIE_PLAYER_SOUND_PRELOAD_MS;

        CachedMovieFrame *next_frame_to_play = SDLMovie_GetCurrentCachedFrame(
            player->mov, SDL_MOVIE_TRACK_TYPE_AUDIO);

        while (SDLMovie_HasNextAudioFrame(player->mov) && SDLMovie_TimecodeToMilliseconds(player->mov, next_frame_to_play->timecode) < preload_time)
        {
            if (!SDLMovie_DecodeAudioFrame(player->mov))
            {
                return SDL_MOVIE_PLAYER_UPDATE_ERROR;
            }

            int samples_count;

            const SDL_MovieAudioSample *samples = SDLMovie_GetAudioSamples(player->mov, NULL, &samples_count);

            if (player->audio_buffer_count + samples_count > player->audio_buffer_capacity)
            {
                /* Rollback to start of buffer and start overwriting, user should've read them */
                player->audio_buffer_count = 0;
            }

            if (samples_count > 0)
            {
                SDLMovie_AddAudioSamplesToPlayer(player, samples, samples_count);

                if (player->output_audio_stream)
                {
                    SDL_PutAudioStreamData(player->output_audio_stream, samples, samples_count * sizeof(SDL_MovieAudioSample));
                    player->audio_buffer_count = 0;
                }
            }

            SDLMovie_NextAudioFrame(player->mov);
            next_frame_to_play = SDLMovie_GetCurrentCachedFrame(
                player->mov, SDL_MOVIE_TRACK_TYPE_AUDIO);
        }

        if (next_frame_to_play)
        {
            player->next_audio_frame_at = SDLMovie_TimecodeToMilliseconds(player->mov, next_frame_to_play->timecode);
        }

        result |= SDL_MOVIE_PLAYER_UPDATE_AUDIO;
    }

    if (player->video_playback && SDLMovie_CanPlaybackVideo(player->mov) && player->current_time >= player->next_video_frame_at)
    {
        CachedMovieFrame *next_frame_to_play = SDLMovie_GetCurrentCachedFrame(
            player->mov, SDL_MOVIE_TRACK_TYPE_VIDEO);

        bool frame_changed = false;
        int last_key_frame_index = -1;

        while (SDLMovie_HasNextVideoFrame(player->mov) && next_frame_to_play && SDLMovie_TimecodeToMilliseconds(player->mov, next_frame_to_play->timecode) <= player->current_time)
        {
            if (next_frame_to_play->key_frame)
            {
                last_key_frame_index = player->mov->current_frame;
            }
            SDLMovie_NextVideoFrame(player->mov);
            next_frame_to_play = SDLMovie_GetCurrentCachedFrame(
                player->mov, SDL_MOVIE_TRACK_TYPE_VIDEO);
            frame_changed = true;
        }

        if (frame_changed)
        {
            if (last_key_frame_index >= 0 && player->mov->current_frame != last_key_frame_index)
            {
                printf("Decoding key-frame first: %d\n", last_key_frame_index);
                int back_frame = player->mov->current_frame;
                SDLMovie_SeekFrame(player->mov, last_key_frame_index);

                /* Key frames must be decoded prior to ensure correct rendering */
                if (!SDLMovie_DecodeVideoFrame(player->mov))
                {
                    return SDL_MOVIE_PLAYER_UPDATE_ERROR;
                }

                SDLMovie_SeekFrame(player->mov, back_frame);
            }

            if (!SDLMovie_DecodeVideoFrame(player->mov))
            {
                return SDL_MOVIE_PLAYER_UPDATE_ERROR;
            }

            if (!player->current_video_frame_surface)
            {
                player->current_video_frame_surface = SDL_DuplicateSurface(
                    (SDL_Surface *)SDLMovie_GetVideoFrameSurface(player->mov));
            }
            else
            {
                SDL_BlitSurface(
                    (SDL_Surface *)SDLMovie_GetVideoFrameSurface(player->mov),
                    NULL,
                    player->current_video_frame_surface,
                    NULL);
            }

            if (player->output_video_frame_texture)
            {
                SDLMovie_UpdatePlaybackTexture(
                    player->mov,
                    player->output_video_frame_texture);
            }

            if (next_frame_to_play)
            {
                player->next_video_frame_at = SDLMovie_TimecodeToMilliseconds(player->mov, next_frame_to_play->timecode);
            }

            result |= SDL_MOVIE_PLAYER_UPDATE_VIDEO;
        }

        if (!SDLMovie_HasNextVideoFrame(player->mov))
        {
            player->finished = true;
        }
    }

    return result;
}

void SDLMovie_AddAudioSamplesToPlayer(
    SDL_MoviePlayer *player,
    const SDL_MovieAudioSample *samples,
    int count)
{
    if (!check_player(player))
        return;

    if (!samples || count <= 0)
        return;

    if (!player->audio_buffer)
    {
        player->audio_buffer_capacity = player->mov->audio_spec.freq * player->mov->audio_spec.channels + player->audio_output_samples_buffer_size;
        player->audio_buffer = (SDL_MovieAudioSample *)SDL_calloc(player->audio_buffer_capacity, sizeof(SDL_MovieAudioSample));

        if (!player->audio_buffer)
        {
            SDLMovie_SetError("Failed to allocate memory for audio buffer");
            return;
        }
    }

    for (int s = 0; s < count; s++)
    {
        player->audio_buffer[player->audio_buffer_count + s] = samples[s];
    }

    player->audio_buffer_count += count;
}

bool SDLMovie_SetPlayerAudioOutput(SDL_MoviePlayer *player, SDL_AudioDeviceID dev)
{
    if (!check_player(player))
        return SDL_SetError("Invalid player");

    if (!SDLMovie_CanPlaybackAudio(player->mov))
    {
        return SDLMovie_SetError("No audio track selected");
    }

    if (player->output_audio_stream)
    {
        SDL_DestroyAudioStream(player->output_audio_stream);
        player->output_audio_stream = NULL;
        player->bound_audio_device = 0;
    }

    /* If zero was provided for device id - user wants to stop audio output */
    if (!dev)
    {
        return true;
    }

    SDL_AudioSpec dst_audio_spec;
    if (!SDL_GetAudioDeviceFormat(dev, &dst_audio_spec, &player->audio_output_samples_buffer_size))
    {
        return SDLMovie_SetError("Failed to get audio device format: %s", SDL_GetError());
    }

    if (!player->audio_output_samples_buffer_size)
    {
        player->audio_output_samples_buffer_size = 1024;
    }

    player->audio_output_samples_buffer_ms = ((Sint64)player->audio_output_samples_buffer_size * 1000) / dst_audio_spec.freq;

    player->output_audio_stream = SDL_CreateAudioStream(
        &player->mov->audio_spec, &dst_audio_spec);

    if (!player->output_audio_stream)
    {
        return SDLMovie_SetError("Failed to create audio stream: %s", SDL_GetError());
    }

    if (!SDL_BindAudioStream(dev, player->output_audio_stream))
    {
        SDL_DestroyAudioStream(player->output_audio_stream);
        return SDLMovie_SetError("Failed to bind audio stream: %s", SDL_GetError());
    }

    player->bound_audio_device = dev;

    return true;
}

const SDL_MovieAudioSample *SDLMovie_GetPlayerAvailableAudioSamples(
    SDL_MoviePlayer *player,
    int *count)
{
    if (!check_player(player))
        return NULL;

    if (!player->audio_buffer)
        return NULL;

    if (count)
        *count = player->audio_buffer_count;

    return player->audio_buffer;
}

void SDLMovie_PausePlayer(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return;

    player->paused = true;

    if (player->output_audio_stream)
    {
        SDL_UnbindAudioStream(player->output_audio_stream);
    }
}

void SDLMovie_ResumePlayer(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return;

    player->paused = false;
    player->last_frame_at_ticks = SDL_GetTicks();

    if (player->output_audio_stream)
    {
        SDL_BindAudioStream(player->bound_audio_device, player->output_audio_stream);
    }
}

bool SDLMovie_IsPlayerPaused(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->paused;
}

float SDLMovie_GetPlayerCurrentTimeSeconds(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return 0;

    return (float)player->current_time / 1000.0f;
}

Uint64 SDLMovie_GetPlayerCurrentTime(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return 0;

    return player->current_time;
}

bool SDLMovie_SetPlayerVideoOutputTexture(
    SDL_MoviePlayer *player,
    SDL_Texture *texture)
{
    if (!check_player(player))
        return SDL_SetError("Invalid player");

    if (!player->mov->current_frame_surface)
    {
        return SDL_SetError("No video playback available, check if video track is selected");
    }

    if (texture->format != player->mov->current_frame_surface->format)
    {
        return SDL_SetError("Texture format does not match the video frame format");
    }

    player->output_video_frame_texture = texture;

    return true;
}

const SDL_Surface *SDLMovie_GetPlayerCurrentVideoFrameSurface(
    SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return NULL;

    return player->current_video_frame_surface;
}

bool SDLMovie_HasPlayerFinished(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->finished;
}

bool SDLMovie_IsPlayerAudioEnabled(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->audio_playback;
}

bool SDLMovie_IsPlayerVideoEnabled(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->video_playback;
}

void SDLMovie_SetPlayerAudioEnabled(SDL_MoviePlayer *player, bool enabled)
{
    if (!check_player(player))
        return;

    if (!SDLMovie_CanPlaybackAudio(player->mov))
    {
        return;
    }

    player->audio_playback = enabled;
}

void SDLMovie_SetPlayerVideoEnabled(SDL_MoviePlayer *player, bool enabled)
{
    if (!check_player(player))
        return;

    if (!SDLMovie_CanPlaybackVideo(player->mov))
    {
        return;
    }

    player->video_playback = enabled;
}

/*

Kinda complex to implement efficiently, so left as TODO.

void SDLMovie_SeekPlayer(SDL_MoviePlayer *player, Uint64 time_ms)
{
    if (!check_player(player))
        return;

    player->current_time = time_ms;

    player->next_video_frame_at = 0;
    player->next_audio_frame_at = 0;
    player->finished = false;
}

void SDLMovie_SeekPlayerSeconds(SDL_MoviePlayer *player, float time_s)
{
    return SDLMovie_SeekPlayer(player, (Uint64)(time_s * 1000));
}*/