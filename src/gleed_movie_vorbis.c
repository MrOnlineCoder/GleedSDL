#include "gleed_movie_internal.h"
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

typedef struct
{
    vorbis_info vi;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    vorbis_block vb;

    int packet_no;
} VorbisContext;

static bool GleedInitVorbis(GleedMovie *movie)
{
    GleedMovieTrack *audio_track = GleedGetAudioTrack(movie);
    if (!audio_track->codec_private_data)
    {
        return GleedSetError("No codec private data available for Vorbis audio track");
    }

    Uint8 vorbis_init_packets_count = audio_track->codec_private_data[0];

    /* According to: https://www.matroska.org/technical/codec_specs.html */
    if (vorbis_init_packets_count != 2)
    {
        return GleedSetError("Invalid number of Vorbis initialization packets: %d", vorbis_init_packets_count);
    }

    VorbisContext *ctx = (VorbisContext *)SDL_calloc(1, sizeof(VorbisContext));

    vorbis_info_init(&ctx->vi);
    vorbis_comment_init(&ctx->vc);

    Uint8 vorbis_id_header_size = audio_track->codec_private_data[1];
    Uint8 vorbis_comment_header_size = audio_track->codec_private_data[2];

    /**
     * For some reason, libvorbis has a hard dependency on libogg and expects packets to be in ogg_packet format.
     *
     * WebM audio tracks are not Ogg streams, they are pure Vorbis encoded data, so we don't use any of ogg_sync/ogg_stream functions.
     *
     * Therefore, we "fake" it by creating the packet manually.
     *
     * The first 3 bytes are the number of packets, the size of the Vorbis ID header, and the size of the Vorbis comment header.
     */
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
        return GleedSetError("Failed to parse Vorbis ID header: %d", header_in_error);
    }

    header.packet = audio_track->codec_private_data + 3 + vorbis_id_header_size;
    header.b_o_s = 0;
    header.bytes = vorbis_comment_header_size;

    header_in_error = vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &header);

    if (header_in_error != 0)
    {
        SDL_free(ctx);
        return GleedSetError("Failed to parse Vorbis comment header: %d", header_in_error);
    }

    header.packet = audio_track->codec_private_data + 3 + vorbis_id_header_size + vorbis_comment_header_size;
    header.bytes = audio_track->codec_private_size - 3 - vorbis_id_header_size - vorbis_comment_header_size;

    header_in_error = vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &header);

    if (header_in_error != 0)
    {
        SDL_free(ctx);
        return GleedSetError("Failed to parse Vorbis codebooks header: %d", header_in_error);
    }

    if (vorbis_synthesis_init(&ctx->vd, &ctx->vi) != 0)
    {
        SDL_free(ctx);
        return GleedSetError("Failed to initialize Vorbis synthesis");
    }

    int block_error = vorbis_block_init(&ctx->vd, &ctx->vb);

    if (block_error != 0)
    {
        SDL_free(ctx);
        return GleedSetError("Failed to initialize Vorbis block: %d", block_error);
    }

    movie->vorbis_context = ctx;

    return true;
}

VorbisDecodeResult GleedDecode_Vorbis(GleedMovie *movie)
{
    GleedMovieTrack *audio_track = GleedGetAudioTrack(movie);

    if (!movie->vorbis_context)
    {
        if (!GleedInitVorbis(movie))
        {
            return GLEED_VORBIS_INIT_ERROR;
        }
    }

    VorbisContext *ctx = (VorbisContext *)movie->vorbis_context;

    int current_packet_size = movie->encoded_audio_frame_size;

    /*Another fake ogg packet to feed into libvorbis */
    ogg_packet packet = {0};
    packet.packetno = ctx->packet_no;
    packet.bytes = current_packet_size;
    packet.packet = movie->encoded_audio_frame;

    if (vorbis_synthesis(&ctx->vb, &packet) != 0)
    {
        GleedSetError("Failed to synthesize Vorbis packet");
        return GLEED_VORBIS_DECODE_ERROR;
    }

    if (vorbis_synthesis_blockin(&ctx->vd, &ctx->vb) != 0)
    {
        GleedSetError("Failed to synthesize Vorbis block");
        return GLEED_VORBIS_DECODE_ERROR;
    }

    ctx->packet_no++;

    float **pcm;

    int samples = vorbis_synthesis_pcmout(&ctx->vd, &pcm);

    if (vorbis_synthesis_read(&ctx->vd, samples) != 0)
    {
        GleedSetError("Failed to mark read samples in Vorbis synthesis");
        return GLEED_VORBIS_DECODE_ERROR;
    }

    if (samples == 0)
        return GLEED_VORBIS_DECODE_DONE;

    const int total_samples = samples * ctx->vi.channels;
    const int total_pcm_size = total_samples * sizeof(float);

    if (!movie->decoded_audio_frame || movie->decoded_audio_frame_size < total_pcm_size)
    {
        movie->decoded_audio_frame = (GleedMovieAudioSample *)SDL_realloc(movie->decoded_audio_frame, total_pcm_size);
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
            const int sample_index = ctx->vi.channels * s + c;
            movie->decoded_audio_frame[sample_index] = sample;
        }
    }

    movie->decoded_audio_samples = samples;

    return GLEED_VORBIS_DECODE_DONE;
}

void GleedCloseVorbis(GleedMovie *movie)
{
    if (movie->vorbis_context)
    {
        VorbisContext *ctx = (VorbisContext *)movie->vorbis_context;
        SDL_free(movie->vorbis_context);
        movie->vorbis_context = NULL;
    }
}