#include <SDL_movie.h>
#include "SDL_movie_internal.h"

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

static vpx_codec_iface_t *vp8 = NULL;
static vpx_codec_ctx_t codec;

bool SDLMovie_Decode_VP8(SDL_Movie *movie)
{
    if (!vp8)
    {
        vp8 = vpx_codec_vp8_dx();

        if (!vp8)
        {
            SDLMovie_SetError("Failed to initialize VP8 decoder");
            return false;
        }

        vpx_codec_err_t init_err = vpx_codec_dec_init(&codec, vp8, NULL, 0);

        if (init_err != VPX_CODEC_OK)
        {
            SDLMovie_SetError("Failed to initialize VP8 decoder: %s", vpx_codec_err_to_string(init_err));
            return false;
        }
    }

    vpx_codec_err_t decode_err = vpx_codec_decode(&codec, movie->encoded_video_frame, movie->encoded_video_frame_size, NULL, 0);

    if (decode_err != VPX_CODEC_OK)
    {
        SDLMovie_SetError("Failed to decode VP8 frame: %s", vpx_codec_err_to_string(decode_err));
        return false;
    }

    vpx_codec_iter_t iter = NULL;

    vpx_image_t *img = NULL;

    img = vpx_codec_get_frame(&codec, &iter);

    if (!img)
    {
        SDLMovie_SetError("Failed to get decoded VP8 frame - received no image");
        return false;
    }

    if (!movie->current_frame_surface)
    {
        movie->current_frame_surface = SDL_CreateSurface(
            img->d_w,
            img->d_h,
            SDL_PIXELFORMAT_YV12);
    }

    SDL_LockSurface(movie->current_frame_surface);

    Uint8 *pixels = (Uint8 *)movie->current_frame_surface->pixels;

    for (int y = 0; y < img->d_h; y++)
    {
        for (int x = 0; x < img->d_w; x++)
        {
            Uint8 *y_plane = img->planes[VPX_PLANE_Y] + y * img->stride[VPX_PLANE_Y] + x;
            Uint8 *u_plane = img->planes[VPX_PLANE_U] + y / 2 * img->stride[VPX_PLANE_U] + x / 2;
            Uint8 *v_plane = img->planes[VPX_PLANE_V] + y / 2 * img->stride[VPX_PLANE_V] + x / 2;

            Uint8 y_val = *y_plane;
            Uint8 u_val = *u_plane;
            Uint8 v_val = *v_plane;

            Uint32 loc = y * img->d_w + x;

            pixels[loc + 0] = y_val;
            pixels[loc + 1] = u_val;
            pixels[loc + 2] = v_val;
        }
    }

    SDL_UnlockSurface(movie->current_frame_surface);

    return true;
}