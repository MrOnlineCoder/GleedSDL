#include "SDL_movie_internal.h"
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

typedef struct
{
    vorbis_info vi;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    vorbis_block vb;

    int packet_no;
    FILE *file;
} VorbisContext;

static bool SDLMovie_Init_Vorbis(SDL_Movie *movie)
{
    SDL_MovieTrack *audio_track = SDLMovie_GetAudioTrack(movie);
    if (!audio_track->codec_private_data)
    {
        return SDLMovie_SetError("No codec private data available for Vorbis audio track");
    }

    Uint8 vorbis_init_packets_count = audio_track->codec_private_data[0];

    /* According to: https://www.matroska.org/technical/codec_specs.html */
    if (vorbis_init_packets_count != 2)
    {
        return SDLMovie_SetError("Invalid number of Vorbis initialization packets: %d", vorbis_init_packets_count);
    }

    VorbisContext *ctx = (VorbisContext *)SDL_calloc(1, sizeof(VorbisContext));

    vorbis_info_init(&ctx->vi);
    vorbis_comment_init(&ctx->vc);

    Uint8 vorbis_id_header_size = audio_track->codec_private_data[1];
    Uint8 vorbis_comment_header_size = audio_track->codec_private_data[2];

    ogg_packet header;
    header.packet = audio_track->codec_private_data + 3;
    header.packetno = 0;
    header.bytes = vorbis_id_header_size;
    header.b_o_s = 1;
    header.e_o_s = 0;

    int header_in_error = vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &header);

    if (header_in_error != 0)
    {
        SDL_free(ctx);
        return SDLMovie_SetError("Failed to parse Vorbis ID header: %d", header_in_error);
    }

    header.packet = audio_track->codec_private_data + 3 + vorbis_id_header_size;
    header.b_o_s = 0;
    header.bytes = vorbis_comment_header_size;

    header_in_error = vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &header);

    if (header_in_error != 0)
    {
        SDL_free(ctx);
        return SDLMovie_SetError("Failed to parse Vorbis comment header: %d", header_in_error);
    }

    header.packet = audio_track->codec_private_data + 3 + vorbis_id_header_size + vorbis_comment_header_size;
    header.bytes = audio_track->codec_private_size - 3 - vorbis_id_header_size - vorbis_comment_header_size;

    header_in_error = vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &header);

    if (header_in_error != 0)
    {
        SDL_free(ctx);
        return SDLMovie_SetError("Failed to parse Vorbis codebooks header: %d", header_in_error);
    }

    if (vorbis_synthesis_init(&ctx->vd, &ctx->vi) != 0)
    {
        SDL_free(ctx);
        return SDLMovie_SetError("Failed to initialize Vorbis synthesis");
    }

    int block_error = vorbis_block_init(&ctx->vd, &ctx->vb);

    if (block_error != 0)
    {
        SDL_free(ctx);
        return SDLMovie_SetError("Failed to initialize Vorbis block: %d", block_error);
    }

    ctx->file = fopen("audio.pcm", "wb");

    movie->vorbis_context = ctx;

    return true;
}

VorbisDecodeResult SDLMovie_Decode_Vorbis(SDL_Movie *movie)
{
    SDL_MovieTrack *audio_track = SDLMovie_GetAudioTrack(movie);

    if (!movie->vorbis_context)
    {
        if (!SDLMovie_Init_Vorbis(movie))
        {
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }
    }

    VorbisContext *ctx = (VorbisContext *)movie->vorbis_context;

    int current_packet_size = movie->encoded_audio_frame_size;

    ogg_packet packet = {0};
    packet.packetno = ctx->packet_no;
    packet.bytes = current_packet_size;
    packet.packet = movie->encoded_audio_frame;

    if (vorbis_synthesis(&ctx->vb, &packet) != 0)
    {
        SDLMovie_SetError("Failed to synthesize Vorbis packet");
        return SDL_MOVIE_VORBIS_DECODE_ERROR;
    }

    if (vorbis_synthesis_blockin(&ctx->vd, &ctx->vb) != 0)
    {
        SDLMovie_SetError("Failed to synthesize Vorbis block");
        return SDL_MOVIE_VORBIS_DECODE_ERROR;
    }

    ctx->packet_no++;

    float **pcm;

    int samples = vorbis_synthesis_pcmout(&ctx->vd, &pcm);

    if (vorbis_synthesis_read(&ctx->vd, samples) != 0)
    {
        SDLMovie_SetError("Failed to mark read samples in Vorbis synthesis");
        return SDL_MOVIE_VORBIS_DECODE_ERROR;
    }

    if (samples == 0)
        return SDL_MOVIE_VORBIS_DECODE_DONE;

    const int total_samples = samples * ctx->vi.channels;
    const int total_pcm_size = total_samples * sizeof(float);

    if (!movie->decoded_audio_frame || movie->decoded_audio_frame_size < total_pcm_size)
    {
        movie->decoded_audio_frame = (SDL_MovieAudioSample *)SDL_realloc(movie->decoded_audio_frame, total_pcm_size);
        movie->decoded_audio_frame_size = total_pcm_size;
    }
    else
    {
        SDL_memset(movie->decoded_audio_frame, 0, total_pcm_size);
    }

    /* Make interleaved samples for SDL */
    for (int c = 0; c < ctx->vi.channels; c++)
    {
        for (int s = 0; s < samples; s++)
        {
            const float sample = pcm[c][s];
            const int sample_index = s * ctx->vi.channels + c;
            movie->decoded_audio_frame[sample_index] = sample;
            fwrite(&sample, sizeof(float), 1, ctx->file);
        }
    }

    movie->decoded_audio_samples = samples;

    return SDL_MOVIE_VORBIS_DECODE_DONE;
}

void SDLMovie_Close_Vorbis(SDL_Movie *movie)
{
    if (movie->vorbis_context)
    {
        VorbisContext *ctx = (VorbisContext *)movie->vorbis_context;
        fclose(ctx->file);
        SDL_free(movie->vorbis_context);
        movie->vorbis_context = NULL;
    }
}