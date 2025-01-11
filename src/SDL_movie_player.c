#include "SDL_movie_internal.h"

#define SDL_MOVIE_PLAYER_DEFAULT_PREFETCH_TIME_MS 1000

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

    player->last_frame_at_ticks = SDL_GetTicks();

    SDLMovie_SetPlayerMovie(player, mov);

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

    SDL_free(player);
}

void SDLMovie_UpdatePlayer(SDL_MoviePlayer *player, int time_delta_ms)
{
    if (!check_player(player))
        return;

    if (time_delta_ms == 0)
        return;

    const Uint64 time_passed = time_delta_ms < 0 ? SDL_GetTicks() - player->last_frame_at_ticks : time_delta_ms;

    player->current_time += time_passed;

    player->last_frame_at_ticks = SDL_GetTicks();

    if (SDLMovie_CanPlaybackAudio(player->mov))
    {
        const Uint32 audio_samples_to_prefetch = ((time_passed * player->mov->audio_spec.freq) / 1000) * player->mov->audio_spec.channels;

        while (player->audio_buffer_count < audio_samples_to_prefetch && SDLMovie_HasNextAudioFrame(player->mov))
        {
            if (!SDLMovie_DecodeAudioFrame(player->mov))
            {
                return;
            }

            int samples_count;

            const SDL_MovieAudioSample *samples = SDLMovie_GetAudioSamples(player->mov, NULL, &samples_count);

            if (samples_count > 0)
            {
                SDLMovie_AddAudioSamplesToPlayer(player, samples, samples_count);
            }

            SDLMovie_NextAudioFrame(player->mov);
        }

        if (player->output_audio_stream && player->audio_buffer_count > 0)
        {
            printf("Put audio stream data %d\n", player->audio_buffer_count);
            SDL_PutAudioStreamData(player->output_audio_stream, player->audio_buffer, player->audio_buffer_count * sizeof(SDL_MovieAudioSample));
            player->audio_buffer_count = 0;
        }
    }
}

void SDLMovie_SyncPlayer(SDL_MoviePlayer *player)
{
    if (!check_player(player))
        return;
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

    if (!player->audio_buffer || player->audio_buffer_count + count > player->audio_buffer_capacity)
    {
        player->audio_buffer_capacity = player->mov->audio_spec.freq * 2;
        player->audio_buffer = (SDL_MovieAudioSample *)SDL_calloc(player->audio_buffer_capacity, sizeof(SDL_MovieAudioSample));
        // player->audio_buffer_capacity = player->audio_buffer_capacity ? player->audio_buffer_capacity * 2 : count;
        // player->audio_buffer = SDL_realloc(player->audio_buffer, player->audio_buffer_capacity * sizeof(SDL_MovieAudioSample));

        if (!player->audio_buffer)
        {
            SDLMovie_SetError("Failed to allocate memory for audio buffer");
            return;
        }
    }

    for (int s = 0; s < count; s++)
    {
        player->audio_buffer[player->audio_buffer_count + s] = samples[s];
        // printf("%d > %d\n", player->audio_buffer_count + s, player->audio_buffer_capacity);
    }

    // SDL_memcpy(player->audio_buffer + player->audio_buffer_count, samples, count * sizeof(SDL_MovieAudioSample));

    player->audio_buffer_count += count;
}

bool SDLMovie_SetAudioOutput(SDL_MoviePlayer *player, SDL_AudioDeviceID dev)
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

    player->audio_output_sample_buffer_ms = ((Sint64)player->audio_output_samples_buffer_size * 1000) / dst_audio_spec.freq;

    player->output_audio_stream = SDL_CreateAudioStream(
        &player->mov->audio_spec, &dst_audio_spec);

    if (!player->output_audio_stream)
    {
        return SDLMovie_SetError("Failed to create audio stream: %s", SDL_GetError());
    }

    if (!SDL_BindAudioStream(dev, player->output_audio_stream))
    {
        return SDLMovie_SetError("Failed to bind audio stream: %s", SDL_GetError());
    }

    return true;
}