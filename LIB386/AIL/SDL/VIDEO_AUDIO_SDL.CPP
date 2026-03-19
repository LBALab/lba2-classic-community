#include <AIL/VIDEO_AUDIO.H>
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

static SDL_AudioStream *videoStream = NULL;

S32 StartVideoAudio(S32 freq, S32 channels, S32 is16bit) {
    StopVideoAudio();
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = freq;
    spec.format = is16bit ? SDL_AUDIO_S16 : SDL_AUDIO_U8;
    spec.channels = (Uint8)(channels >= 2 ? 2 : 1);
    videoStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!videoStream) {
        SDL_Log("SDL AIL: Could not open video audio stream: %s", SDL_GetError());
        return 0;
    }
    /* Leave paused until ResumeVideoAudio() after pre-fill (avoids stutter like lba2-classic-ida's SoLoud buffer) */
    return 1;
}

void ResumeVideoAudio(void) {
    if (videoStream)
        SDL_ResumeAudioStreamDevice(videoStream);
}

void PauseVideoAudio(void) {
    if (videoStream)
        SDL_PauseAudioStreamDevice(videoStream);
}

void PushVideoAudio(const void *data, U32 sizeBytes) {
    if (!videoStream || !data || sizeBytes == 0)
        return;
    /* If stream is 16-bit we push as-is; if 8-bit we push as-is (SDL_AUDIO_U8). */
    SDL_PutAudioStreamData(videoStream, data, (int)sizeBytes);
}

void StopVideoAudio(void) {
    if (videoStream) {
        SDL_DestroyAudioStream(videoStream);
        videoStream = NULL;
    }
}
