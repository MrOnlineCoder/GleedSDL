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

        const int decoderSize = opus_decoder_get_size(movie->audio_spec.channels);

        MovieOpusContext *ctx = (MovieOpusContext *)movie->opus_context;

        ctx->decoder = SDL_calloc(1, decoderSize);

        int decoderInitError = opus_decoder_init(ctx->decoder, movie->audio_spec.freq, movie->audio_spec.channels);

        if (decoderInitError != OPUS_OK)
        {
            SDL_free(movie->opus_context);
            movie->opus_context = NULL;
            return SDLMovie_SetError("Failed to initialize Opus decoder: %s", opus_strerror(decoderInitError));
        }

        /* Allocate one second of buffer for all channels */
        ctx->pcm_buffer_size_per_channel = movie->audio_spec.freq * sizeof(float);
        ctx->pcm_buffer_size = ctx->pcm_buffer_size_per_channel * movie->audio_spec.channels;
        ctx->pcm_buffer = SDL_calloc(1, ctx->pcm_buffer_size);
    }

    MovieOpusContext *ctx = (MovieOpusContext *)movie->opus_context;

    int samplesCount = opus_decode_float(ctx->decoder, movie->encoded_audio_frame, movie->encoded_audio_frame_size, &ctx->pcm_buffer[0], movie->audio_spec.freq, 0);

    if (samplesCount < OPUS_OK)
    {
        return SDLMovie_SetError("Failed to decode Opus frame: %s", opus_strerror(samplesCount));
    }

    if (!movie->decoded_audio_frame || movie->decoded_audio_frame_size < ctx->pcm_buffer_size)
    {
        movie->decoded_audio_frame = (SDL_MovieAudioSample *)SDL_realloc(movie->decoded_audio_frame, ctx->pcm_buffer_size);
        movie->decoded_audio_frame_size = ctx->pcm_buffer_size;
    }
    else
    {
        SDL_memset(movie->decoded_audio_frame, 0, ctx->pcm_buffer_size);
    }

    for (int s = 0; s < samplesCount; s++)
    {
        const float sample = ctx->pcm_buffer[s];
        movie->decoded_audio_frame[s] = sample;
    }

    movie->decoded_audio_samples = samplesCount;

    return true;
}

void SDLMovie_CloseOpus(SDL_Movie *movie)
{
    if (movie->opus_context)
    {
        SDL_free(movie->opus_context);
        movie->opus_context = NULL;
    }
}