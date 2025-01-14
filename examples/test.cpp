#include <SDL3/SDL.h>

#include <opus.h>

int main()
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_Window *wnd = SDL_CreateWindow("test", 800, 600, 0);

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);

    SDL_AudioSpec spec;
    spec.freq = 48000;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;

    SDL_AudioSpec dst;
    SDL_GetAudioDeviceFormat(dev, &dst, NULL);

    SDL_AudioStream *output = SDL_CreateAudioStream(&spec, &dst);
    SDL_BindAudioStream(dev, output);

    SDL_ResumeAudioDevice(dev);

    FILE *file = fopen("ocean.opus", "rb");

    OpusDecoder *decoder = opus_decoder_create(48000, 2, NULL);

    int fsize = fseek(file, 0, SEEK_END);

    fseek(file, 0, SEEK_SET);

    unsigned char *fbuf = new unsigned char[fsize];

    float *pcm = new float[48000 * 2 * 50];

    fread(fbuf, 1, fsize, file);

    int samples = opus_decode_float(decoder, fbuf, fsize, pcm, 48000 * 50, 0);

    if (samples < 0)
    {
        printf("Failed to decode frame: %s\n", opus_strerror(samples));
    }

    delete[] fbuf;
    delete[] pcm;

    SDL_PutAudioStreamData(output, pcm, samples);
    printf("Placed %d samples\n", samples);

    SDL_FlushAudioStream(output);

    fclose(file);

    bool run = true;

    while (run)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_EVENT_QUIT)
            {
                run = false;
            }
        }

        SDL_Delay(16);
    }

    SDL_DestroyWindow(wnd);

    opus_decoder_destroy(decoder);

    SDL_UnbindAudioStream(output);
    SDL_DestroyAudioStream(output);
    SDL_CloseAudioDevice(dev);

    return 0;
}