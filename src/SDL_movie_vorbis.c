#include "SDL_movie_internal.h"
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

typedef struct
{
    vorbis_info vi;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    vorbis_block vb;
    ogg_stream_state os;
    ogg_sync_state oy;
} VorbisContext;

static const int OGG_BUFFER_SIZE = 4096;

VorbisDecodeResult SDLMovie_Decode_Vorbis(SDL_Movie *movie)
{
    MovieTrack *audio_track = SDLMovie_GetAudioTrack(movie);

    if (!movie->vorbis_context)
    {
        if (!audio_track->codec_private_data)
        {
            SDLMovie_SetError("No codec private data available for Vorbis audio track");
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        Uint8 vorbis_init_packets_count = audio_track->codec_private_data[0];

        /* According to: https://www.matroska.org/technical/codec_specs.html */
        if (vorbis_init_packets_count != 2)
        {
            SDLMovie_SetError("Invalid number of Vorbis initialization packets: %d", vorbis_init_packets_count);
            return SDL_MOVIE_VORBIS_INIT_ERROR;
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
            SDLMovie_SetError("Failed to parse Vorbis ID header: %d", header_in_error);
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        header.packet = audio_track->codec_private_data + 3 + vorbis_id_header_size;
        header.b_o_s = 0;
        header.bytes = vorbis_comment_header_size;

        header_in_error = vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &header);

        if (header_in_error != 0)
        {
            SDL_free(ctx);
            SDLMovie_SetError("Failed to parse Vorbis comment header: %d", header_in_error);
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        header.packet = audio_track->codec_private_data + 3 + vorbis_id_header_size + vorbis_comment_header_size;
        header.bytes = audio_track->codec_private_size - 3 - vorbis_id_header_size - vorbis_comment_header_size;

        header_in_error = vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &header);

        if (header_in_error != 0)
        {
            SDL_free(ctx);
            SDLMovie_SetError("Failed to parse Vorbis setup header: %d", header_in_error);
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        if (vorbis_synthesis_init(&ctx->vd, &ctx->vi) != 0)
        {
            SDL_free(ctx);
            SDLMovie_SetError("Failed to initialize Vorbis synthesis");
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        int block_error = vorbis_block_init(&ctx->vd, &ctx->vb);

        if (block_error != 0)
        {
            SDL_free(ctx);
            SDLMovie_SetError("Failed to initialize Vorbis block: %d", block_error);
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        if (ogg_stream_init(&ctx->os, 0) != 0)
        {
            SDL_free(ctx);
            SDLMovie_SetError("Failed to initialize Ogg stream");
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        if (ogg_sync_init(&ctx->oy) != 0)
        {
            SDL_free(ctx);
            SDLMovie_SetError("Failed to initialize Ogg sync");
            return SDL_MOVIE_VORBIS_INIT_ERROR;
        }

        movie->vorbis_context = ctx;
    }

    VorbisContext *ctx = (VorbisContext *)movie->vorbis_context;

    char *sync_buffer = ogg_sync_buffer(&ctx->oy, OGG_BUFFER_SIZE);

    if (!sync_buffer)
    {
        SDLMovie_SetError("Failed to get Ogg sync buffer");
        return SDL_MOVIE_VORBIS_DECODE_ERROR;
    }

    SDLMovie_ReadEncodedAudioData(movie, sync_buffer, OGG_BUFFER_SIZE);

    ogg_page page = {0};

    if (ogg_sync_wrote(&ctx->oy, OGG_BUFFER_SIZE) != 0)
    {
        SDLMovie_SetError("Failed to write to Ogg sync buffer");
        return SDL_MOVIE_VORBIS_DECODE_ERROR;
    }

    int pageout_code = ogg_sync_pageout(&ctx->oy, &page);

    // if (pageout_code == 0)
    // {
    //     /* more data needed*/
    //     SDLMovie_NextAudioFrame(movie);
    //     SDLMovie_ReadCurrentFrame(movie, SDL_MOVIE_TRACK_TYPE_AUDIO);

    //     enc_frame_size = movie->encoded_audio_frame_size;
    //     enc_frame = movie->encoded_audio_frame;

    //     sync_buffer = ogg_sync_buffer(&ctx->oy, OGG_BUFFER_SIZE);
    //     SDL_memcpy(sync_buffer, enc_frame, enc_frame_size);
    //     ogg_sync_wrote(&ctx->oy, enc_frame_size);

    //     pageout_code = ogg_sync_pageout(&ctx->oy, &page);
    // }

    // if (pageout_code != 1)
    // {
    //     SDLMovie_SetError("Failed to build Ogg page");
    //     return SDL_MOVIE_VORBIS_DECODE_ERROR;
    // }

    if (ogg_stream_pagein(&ctx->os, &page) != 0)
    {
        SDLMovie_SetError("Failed to stream Ogg page");
        return SDL_MOVIE_VORBIS_DECODE_ERROR;
    }

    ogg_packet packet;

    if (ogg_stream_packetout(&ctx->os, &packet) != 1)
    {
        SDLMovie_SetError("Failed to get Ogg packet");
        return SDL_MOVIE_VORBIS_DECODE_ERROR;
    }

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

    float **pcm;

    int samples = vorbis_synthesis_pcmout(&ctx->vd, &pcm);

    if (samples == 0)
        return SDL_MOVIE_VORBIS_DECODE_DONE;

    movie->decoded_audio_frame = pcm;
    movie->decoded_audio_frame_size = samples * ctx->vi.channels * sizeof(float);

    return SDL_MOVIE_VORBIS_DECODE_DONE;
}

void SDLMovie_Close_Vorbis(SDL_Movie *movie)
{
    if (movie->vorbis_context)
    {
        VorbisContext *ctx = (VorbisContext *)movie->vorbis_context;
        ogg_stream_destroy(&ctx->os);
        ogg_sync_destroy(&ctx->oy);
        SDL_free(movie->vorbis_context);
        movie->vorbis_context = NULL;
    }
}