#include "SDL_movie_internal.h"

#include <opus.h>

typedef struct
{
    OpusDecoder *decoder;
    float *pcm_buffer;
    int pcm_buffer_size;
    int pcm_buffer_size_per_channel;
} MovieOpusContext;

bool SDLMovie_DecodeOpus(SDL_Movie *movie)
{
    if (!movie->opus_context)
    {
        movie->opus_context = SDL_calloc(1, sizeof(MovieOpusContext));

        MovieOpusContext *ctx = (MovieOpusContext *)movie->opus_context;

        int decoderInitError;

        ctx->decoder = opus_decoder_create(movie->audio_spec.freq, movie->audio_spec.channels, &decoderInitError);

        if (decoderInitError != OPUS_OK)
        {
            SDL_free(movie->opus_context);
            movie->opus_context = NULL;
            return SDLMovie_SetError("Failed to initialize Opus decoder: %s", opus_strerror(decoderInitError));
        }

        /*
            Allocate 1 second of buffer for all channels, this should be enough for any Opus frame size

            Takes about 384 KB of memory for 48 kHz stereo audio
        */
        ctx->pcm_buffer_size_per_channel = movie->audio_spec.freq * sizeof(float);
        ctx->pcm_buffer_size = ctx->pcm_buffer_size_per_channel * movie->audio_spec.channels;
        ctx->pcm_buffer = SDL_calloc(1, ctx->pcm_buffer_size);
    }

    MovieOpusContext *ctx = (MovieOpusContext *)movie->opus_context;

    int per_channel_samples_decoded = opus_decode_float(ctx->decoder, movie->encoded_audio_frame, movie->encoded_audio_frame_size, &ctx->pcm_buffer[0], movie->audio_spec.freq, 0);

    if (per_channel_samples_decoded < OPUS_OK)
    {
        return SDLMovie_SetError("Failed to decode Opus frame: %s", opus_strerror(per_channel_samples_decoded));
    }

    int samples_count = per_channel_samples_decoded * movie->audio_spec.channels;

    if (!movie->decoded_audio_frame || movie->decoded_audio_frame_size < ctx->pcm_buffer_size)
    {
        movie->decoded_audio_frame = (SDL_MovieAudioSample *)SDL_realloc(movie->decoded_audio_frame, ctx->pcm_buffer_size);
        movie->decoded_audio_frame_size = ctx->pcm_buffer_size;
    }
    else
    {
        SDL_memset(movie->decoded_audio_frame, 0, ctx->pcm_buffer_size);
    }

    for (int s = 0; s < samples_count; s++)
    {
        const float sample = ctx->pcm_buffer[s];
        movie->decoded_audio_frame[s] = sample;
    }

    movie->decoded_audio_samples = samples_count;

    return true;
}

void SDLMovie_CloseOpus(SDL_Movie *movie)
{
    if (movie->opus_context)
    {
        MovieOpusContext *ctx = (MovieOpusContext *)movie->opus_context;
        opus_decoder_destroy(ctx->decoder);
        SDL_free(movie->opus_context);
        movie->opus_context = NULL;
    }
}