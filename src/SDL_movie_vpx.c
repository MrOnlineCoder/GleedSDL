#include <SDL_movie.h>
#include "SDL_movie_internal.h"

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

typedef struct
{
    vpx_codec_iface_t *vp8;
    vpx_codec_iface_t *vp9;
    vpx_codec_ctx_t codec8;
    vpx_codec_ctx_t codec9;
} VPXContext;

/* Stolen from libvpx/tools_common.c */
static int vpx_img_plane_width(const vpx_image_t *img, int plane)
{
    if (plane > 0 && img->x_chroma_shift > 0)
        return (img->d_w + 1) >> img->x_chroma_shift;
    else
        return img->d_w;
}

static int vpx_img_plane_height(const vpx_image_t *img, int plane)
{
    if (plane > 0 && img->y_chroma_shift > 0)
        return (img->d_h + 1) >> img->y_chroma_shift;
    else
        return img->d_h;
}

static SDL_PixelFormat vpx_format_to_sdl_format(vpx_img_fmt_t fmt)
{
    switch (fmt)
    {
    case VPX_IMG_FMT_NONE:
        return SDL_PIXELFORMAT_UNKNOWN;
    case VPX_IMG_FMT_YV12:
        return SDL_PIXELFORMAT_YV12;
    case VPX_IMG_FMT_I420:
        return SDL_PIXELFORMAT_IYUV;
    case VPX_IMG_FMT_I422:
        return SDL_PIXELFORMAT_YVYU;
    default:
        return SDL_PIXELFORMAT_YV12;
    }
}

static SDL_Colorspace vpx_cs_to_sdl_cs(vpx_color_space_t cs)
{
    switch (cs)
    {
    case VPX_CS_BT_2020:
        return SDL_COLORSPACE_BT2020_FULL;
    case VPX_CS_BT_601:
        return SDL_COLORSPACE_BT601_FULL;
    case VPX_CS_BT_709:
        return SDL_COLORSPACE_BT709_FULL;
    case VPX_CS_SRGB:
        return SDL_COLORSPACE_SRGB;
    default:
        return SDL_COLORSPACE_YUV_DEFAULT;
    }
}

bool SDLMovie_Decode_VPX(SDL_Movie *movie)
{
    Uint64 decode_start = SDL_GetTicks();

    if (!movie->vpx_context)
    {
        movie->vpx_context = SDL_calloc(1, sizeof(VPXContext));
    }

    vpx_codec_iface_t *vpi = NULL;
    vpx_codec_ctx_t *codec = NULL;

    VPXContext *ctx = (VPXContext *)movie->vpx_context;

    if (movie->video_codec == SDL_MOVIE_CODEC_TYPE_VP8)
    {
        if (!ctx->vp8)
        {
            ctx->vp8 = vpx_codec_vp8_dx();

            int vp8_err = vpx_codec_dec_init(&ctx->codec8, ctx->vp8, NULL, 0);

            if (vp8_err != VPX_CODEC_OK)
            {
                return SDLMovie_SetError("Failed to initialize VP8 decoder: %s", vpx_codec_err_to_string(vp8_err));
            }
        }

        vpi = ctx->vp8;
        codec = &ctx->codec8;
    }
    else if (movie->video_codec == SDL_MOVIE_CODEC_TYPE_VP9)
    {
        if (!ctx->vp9)
        {
            ctx->vp9 = vpx_codec_vp9_dx();

            int vp9_err = vpx_codec_dec_init(&ctx->codec9, ctx->vp9, NULL, 0);

            if (vp9_err != VPX_CODEC_OK)
            {
                return SDLMovie_SetError("Failed to initialize VP9 decoder: %s", vpx_codec_err_to_string(vp9_err));
            }
        }

        vpi = ctx->vp9;
        codec = &ctx->codec9;
    }

    if (!vpi)
    {
        return SDLMovie_SetError("Failed to initialize VPX decoder");
    }

    vpx_codec_err_t decode_err = vpx_codec_decode(codec, movie->encoded_video_frame, movie->encoded_video_frame_size, NULL, 0);

    if (decode_err != VPX_CODEC_OK)
    {
        return SDLMovie_SetError("Failed to decode VPX frame: %s, %s", vpx_codec_err_to_string(decode_err), vpx_codec_error_detail(codec));
    }

    vpx_codec_iter_t iter = NULL;

    vpx_image_t *img = NULL;

    /*
        Boom! We do not query for more frames here, although we have a damn iterator!
        TODO: Implement a way to query for more frames and queue them up for rendering.
    */
    img = vpx_codec_get_frame(codec, &iter);

    if (!img)
    {
        return SDLMovie_SetError("Failed to get decoded VPX frame - received no image");
    }

    if (!movie->current_frame_surface)
    {
        movie->current_frame_surface = SDL_CreateSurface(
            img->d_w,
            img->d_h,
            SDL_PIXELFORMAT_RGB24);
    }

    const SDL_PixelFormatDetails *rgb_format_details = SDL_GetPixelFormatDetails(
        SDL_PIXELFORMAT_RGB24);

    const SDL_PixelFormatDetails *vpx_format_details = SDL_GetPixelFormatDetails(
        vpx_format_to_sdl_format(img->fmt));

    SDL_Colorspace vpx_colorspace = vpx_cs_to_sdl_cs(img->cs);

    size_t buffer_size = 0;

    for (int plane = 0; plane < 3; plane++)
    {
        buffer_size += vpx_img_plane_height(img, plane) * img->stride[plane];
    }

    if (!movie->conversion_video_frame_buffer)
    {
        movie->conversion_video_frame_buffer = (Uint8 *)SDL_calloc(
            1, buffer_size);
        movie->conversion_video_frame_buffer_size = buffer_size;
    }
    else if (movie->conversion_video_frame_buffer_size < buffer_size)
    {
        movie->conversion_video_frame_buffer = (Uint8 *)SDL_realloc(
            movie->conversion_video_frame_buffer, buffer_size);
        movie->conversion_video_frame_buffer_size = buffer_size;
    }

    Uint8 *convert_buffer = movie->conversion_video_frame_buffer;

    Uint8 *convert_buffer_write_ptr = convert_buffer;

    size_t bytes_copied = 0;

    /* Some hacky conversion from VPX img planes format to SDL continuous buffer with 3 planes*/
    for (int plane = 0; plane < 3; plane++)
    {
        const int plane_height = vpx_img_plane_height(img, plane);
        const int plane_width = vpx_img_plane_width(img, plane);
        const int row_size_bytes = img->stride[plane];

        for (int y = 0; y < plane_height; y++)
        {
            SDL_memcpy(
                convert_buffer_write_ptr,
                img->planes[plane] + y * img->stride[plane],
                row_size_bytes);
            convert_buffer_write_ptr += row_size_bytes;
            bytes_copied += row_size_bytes;
        }
    }

    SDL_assert(convert_buffer_write_ptr - convert_buffer == buffer_size);
    SDL_assert(bytes_copied == buffer_size);

    SDL_LockSurface(movie->current_frame_surface);

    /* Thank you SDL for this monster helper! */
    if (!SDL_ConvertPixelsAndColorspace(
            img->d_w,
            img->d_h,
            vpx_format_details->format,
            vpx_colorspace,
            0,
            convert_buffer,
            img->stride[0],
            rgb_format_details->format,
            SDL_GetSurfaceColorspace(movie->current_frame_surface),
            0,
            movie->current_frame_surface->pixels,
            movie->current_frame_surface->pitch))
    {
        SDLMovie_SetError("Failed to convert VPX frame to RGB: %s", SDL_GetError());
        SDL_free(convert_buffer);

        return false;
    }

    SDL_UnlockSurface(movie->current_frame_surface);

    movie->last_frame_decode_ms = SDL_GetTicks() - decode_start;

    return true;
}

void SDLMovie_Close_VPX(SDL_Movie *movie)
{
    if (movie->vpx_context)
    {
        VPXContext *ctx = (VPXContext *)movie->vpx_context;

        if (ctx->vp8)
        {
            vpx_codec_destroy(&ctx->codec8);
            ctx->vp8 = NULL;
        }

        if (ctx->vp9)
        {
            vpx_codec_destroy(&ctx->codec9);
            ctx->vp9 = NULL;
        }

        SDL_free(ctx);

        movie->vpx_context = NULL;
    }
}