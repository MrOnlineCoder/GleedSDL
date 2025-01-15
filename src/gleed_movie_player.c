#include "gleed_movie_internal.h"

#define GLEED_PLAYER_SOUND_PRELOAD_MS 50

static bool check_player(GleedMoviePlayer *player)
{
    return player && player->mov;
}

GleedMoviePlayer *GleedCreatePlayer(GleedMovie *mov)
{
    if (!mov)
        return NULL;

    GleedMoviePlayer *player = (GleedMoviePlayer *)SDL_calloc(1, sizeof(GleedMoviePlayer));

    if (!player)
    {
        GleedSetError("Failed to allocate memory for movie player");
        return NULL;
    }

    GleedSetPlayerMovie(player, mov);

    player->last_frame_at_ticks = SDL_GetTicks();

    return player;
}

GleedMoviePlayer *GleedCreatePlayerFromPath(const char *path)
{
    GleedMovie *mov = GleedOpen(path);

    if (!mov)
    {
        return NULL;
    }

    return GleedCreatePlayer(mov);
}

GleedMoviePlayer *GleedCreatePlayerFromIO(SDL_IOStream *io)
{
    GleedMovie *mov = GleedOpenIO(io);

    if (!mov)
    {
        return NULL;
    }

    return GleedCreatePlayer(mov);
}

void GleedSetPlayerMovie(GleedMoviePlayer *player, GleedMovie *mov)
{
    if (!player || !mov)
        return;

    player->mov = mov;
    player->current_time = 0;
    player->next_video_frame_at = 0;
    player->next_audio_frame_at = 0;
    player->finished = false;
    player->video_playback = GleedCanPlaybackVideo(mov);
    player->audio_playback = GleedCanPlaybackAudio(mov);

    /*Ideally, we should not do this, but for now let's assume player always plays movie from start*/
    GleedSeekFrame(player->mov, 0);

    GleedMovieTrack *audio_track = GleedGetAudioTrack(player->mov);
    GleedMovieTrack *video_track = GleedGetVideoTrack(player->mov);

    /*TODO: not sure if this is correct, the description in Matroska spec is not clear for me*/
    if (audio_track && audio_track->codec_delay > 0)
    {
        player->next_audio_frame_at = GleedMatroskaTicksToMilliseconds(player->mov, audio_track->codec_delay);
    }

    if (video_track && video_track->codec_delay > 0)
    {
        player->next_video_frame_at = GleedMatroskaTicksToMilliseconds(player->mov, video_track->codec_delay);
    }
}

void GleedFreePlayer(GleedMoviePlayer *player)
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

GleedMoviePlayerUpdateResult GleedUpdatePlayer(GleedMoviePlayer *player, int time_delta_ms)
{
    if (!check_player(player))
        return GLEED_PLAYER_UPDATE_NONE;

    /*0 is treated as no time has passed*/
    if (time_delta_ms == 0)
        return GLEED_PLAYER_UPDATE_NONE;

    if (player->paused || player->finished)
        return GLEED_PLAYER_UPDATE_NONE;

    GleedMoviePlayerUpdateResult result = GLEED_PLAYER_UPDATE_NONE;

    /* Decide how much time passed since last update based on second argument*/
    const Uint64 time_passed = time_delta_ms < 0 ? SDL_GetTicks() - player->last_frame_at_ticks : time_delta_ms;

    player->current_time += time_passed;

    /*
        Intuitively, we should record that at end of update,
        but decoding can take quite a lot of time,
        so the next update must account for it too
    */
    player->last_frame_at_ticks = SDL_GetTicks();

    if (player->video_playback && GleedCanPlaybackVideo(player->mov) && player->current_time >= player->next_video_frame_at)
    {
        CachedMovieFrame *next_frame_to_play = GleedGetCurrentCachedFrame(
            player->mov, GLEED_TRACK_TYPE_VIDEO);

        /*
            This function does not account for seeks, so we decode EACH frame until we reach the current time
            assuming that really given time has passed since last update.

            Moreover, for some reasons quite a few .webm files I have tested do not have
            keyframes set correctly, and even seeking back to them before decoding does not help.

            This probably SHOULD be optimized, but I am not quite sure how for now.
        */
        while (GleedHasNextVideoFrame(player->mov) && next_frame_to_play && GleedTimecodeToMilliseconds(player->mov, next_frame_to_play->timecode) <= player->current_time)
        {
            if (!GleedDecodeVideoFrame(player->mov))
            {
                return GLEED_PLAYER_UPDATE_ERROR;
            }
            GleedNextVideoFrame(player->mov);
            next_frame_to_play = GleedGetCurrentCachedFrame(
                player->mov, GLEED_TRACK_TYPE_VIDEO);
        }

        /* Either create a surface or just blit it*/
        if (!player->current_video_frame_surface)
        {
            player->current_video_frame_surface = SDL_DuplicateSurface(
                (SDL_Surface *)GleedGetVideoFrameSurface(player->mov));
        }
        else
        {
            SDL_BlitSurface(
                (SDL_Surface *)GleedGetVideoFrameSurface(player->mov),
                NULL,
                player->current_video_frame_surface,
                NULL);
        }

        /* If user set a target texture, update it's contents*/
        if (player->output_video_frame_texture)
        {
            GleedUpdatePlaybackTexture(
                player->mov,
                player->output_video_frame_texture);
        }

        if (next_frame_to_play)
        {
            player->next_video_frame_at = GleedTimecodeToMilliseconds(player->mov, next_frame_to_play->timecode);
        }

        result |= GLEED_PLAYER_UPDATE_VIDEO;

        /* Currently video is used as determining factor if movie has ended */
        if (!GleedHasNextVideoFrame(player->mov))
        {
            player->finished = true;
        }
    }

    /*
       Only advance audio if we
       1) have it enabled
       2) have audio track
       3) it's time to play next frame
   */
    if (player->audio_playback && GleedCanPlaybackAudio(player->mov) && player->current_time >= player->next_audio_frame_at)
    {
        /* Audio output is much more sensitive to delays or interruptions, so we load a bit more samples */
        const Uint64 preload_time = player->current_time + GLEED_PLAYER_SOUND_PRELOAD_MS;

        CachedMovieFrame *next_frame_to_play = GleedGetCurrentCachedFrame(
            player->mov, GLEED_TRACK_TYPE_AUDIO);

        /*
            This function does not account for seeks, so we decode EACH frame until we reach the current time
            assuming that really given time has passed since last update
        */
        while (GleedHasNextAudioFrame(player->mov) && GleedTimecodeToMilliseconds(player->mov, next_frame_to_play->timecode) < preload_time)
        {
            /*TODO: provide any recovery from such errors? maybe reset codec state */
            if (!GleedDecodeAudioFrame(player->mov))
            {
                return GLEED_PLAYER_UPDATE_ERROR;
            }

            int samples_count;

            const GleedMovieAudioSample *samples = GleedGetAudioSamples(player->mov, NULL, &samples_count);

            if (samples_count > 0)
            {
                GleedAddAudioSamplesToPlayer(player, samples, samples_count);

                /* If output is set up, add samples to stream right away and forget about them*/
                if (player->output_audio_stream)
                {
                    SDL_PutAudioStreamData(player->output_audio_stream, samples, samples_count * sizeof(GleedMovieAudioSample));
                    player->audio_buffer_count = 0;
                }
            }

            GleedNextAudioFrame(player->mov);
            next_frame_to_play = GleedGetCurrentCachedFrame(
                player->mov, GLEED_TRACK_TYPE_AUDIO);
        }

        /* We will play next frame only after this timecode*/
        if (next_frame_to_play)
        {
            player->next_audio_frame_at = GleedTimecodeToMilliseconds(player->mov, next_frame_to_play->timecode);
        }

        result |= GLEED_PLAYER_UPDATE_AUDIO;
    }

    return result;
}

void GleedAddAudioSamplesToPlayer(
    GleedMoviePlayer *player,
    const GleedMovieAudioSample *samples,
    int count)
{
    if (!check_player(player))
        return;

    if (!samples || count <= 0)
        return;

    if (!player->audio_buffer)
    {
        /*Allocate a buffer for at least hardware-provided samples count + plus amount of samples for 1 second at given frequency */
        player->audio_buffer_capacity = player->mov->audio_spec.freq * player->mov->audio_spec.channels + player->audio_output_samples_buffer_size;
        player->audio_buffer = (GleedMovieAudioSample *)SDL_calloc(player->audio_buffer_capacity, sizeof(GleedMovieAudioSample));

        if (!player->audio_buffer)
        {
            GleedSetError("Failed to allocate memory for audio buffer");
            return;
        }
    }

    if (player->audio_buffer_count + count > player->audio_buffer_capacity)
    {
        /* Rollback to start of buffer and start overwriting, user should've read them at this point */
        player->audio_buffer_count = 0;
    }

    /* Copy the samples manually */
    for (int s = 0; s < count; s++)
    {
        player->audio_buffer[player->audio_buffer_count + s] = samples[s];
    }

    player->audio_buffer_count += count;
}

bool GleedSetPlayerAudioOutput(GleedMoviePlayer *player, SDL_AudioDeviceID dev)
{
    if (!check_player(player))
        return SDL_SetError("Invalid player");

    if (dev == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)
    {
        return SDL_SetError("Audio output device must be already opened or 0 to disable");
    }

    if (!GleedCanPlaybackAudio(player->mov))
    {
        return GleedSetError("No audio track selected");
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
        return GleedSetError("Failed to get audio device format: %s", SDL_GetError());
    }

    /* 1024 is the reasonable default?*/
    if (!player->audio_output_samples_buffer_size)
    {
        player->audio_output_samples_buffer_size = 1024;
    }

    player->audio_output_samples_buffer_ms = ((Sint64)player->audio_output_samples_buffer_size * 1000) / dst_audio_spec.freq;

    player->output_audio_stream = SDL_CreateAudioStream(
        &player->mov->audio_spec, &dst_audio_spec);

    if (!player->output_audio_stream)
    {
        return GleedSetError("Failed to create audio stream: %s", SDL_GetError());
    }

    if (!SDL_BindAudioStream(dev, player->output_audio_stream))
    {
        SDL_DestroyAudioStream(player->output_audio_stream);
        return GleedSetError("Failed to bind audio stream: %s", SDL_GetError());
    }

    player->bound_audio_device = dev;

    return true;
}

const GleedMovieAudioSample *GleedGetPlayerAvailableAudioSamples(
    GleedMoviePlayer *player,
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

void GleedPausePlayer(GleedMoviePlayer *player)
{
    if (!check_player(player))
        return;

    player->paused = true;

    if (player->output_audio_stream)
    {
        SDL_UnbindAudioStream(player->output_audio_stream);
    }
}

void GleedResumePlayer(GleedMoviePlayer *player)
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

bool GleedIsPlayerPaused(GleedMoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->paused;
}

float GleedGetPlayerCurrentTimeSeconds(GleedMoviePlayer *player)
{
    if (!check_player(player))
        return 0;

    return (float)player->current_time / 1000.0f;
}

Uint64 GleedGetPlayerCurrentTime(GleedMoviePlayer *player)
{
    if (!check_player(player))
        return 0;

    return player->current_time;
}

bool GleedSetPlayerVideoOutputTexture(
    GleedMoviePlayer *player,
    SDL_Texture *texture)
{
    if (!check_player(player))
        return SDL_SetError("Invalid player");

    if (texture == NULL)
    {
        player->output_video_frame_texture = NULL;
        return true;
    }

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

const SDL_Surface *GleedGetPlayerCurrentVideoFrameSurface(
    GleedMoviePlayer *player)
{
    if (!check_player(player))
        return NULL;

    return player->current_video_frame_surface;
}

bool GleedHasPlayerFinished(GleedMoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->finished;
}

bool GleedIsPlayerAudioEnabled(GleedMoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->audio_playback;
}

bool GleedIsPlayerVideoEnabled(GleedMoviePlayer *player)
{
    if (!check_player(player))
        return false;

    return player->video_playback;
}

void GleedSetPlayerAudioEnabled(GleedMoviePlayer *player, bool enabled)
{
    if (!check_player(player))
        return;

    if (!GleedCanPlaybackAudio(player->mov))
    {
        return;
    }

    player->audio_playback = enabled;
}

void GleedSetPlayerVideoEnabled(GleedMoviePlayer *player, bool enabled)
{
    if (!check_player(player))
        return;

    if (!GleedCanPlaybackVideo(player->mov))
    {
        return;
    }

    player->video_playback = enabled;
}

/*

Kinda complex to implement efficiently, so left as TODO.

void GleedSeekPlayer(GleedMoviePlayer *player, Uint64 time_ms)
{
    if (!check_player(player))
        return;

    player->current_time = time_ms;

    player->next_video_frame_at = 0;
    player->next_audio_frame_at = 0;
    player->finished = false;
}

void GleedSeekPlayerSeconds(GleedMoviePlayer *player, float time_s)
{
    return GleedSeekPlayer(player, (Uint64)(time_s * 1000));
}*/