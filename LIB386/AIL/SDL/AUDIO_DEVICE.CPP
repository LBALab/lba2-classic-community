#include "AUDIO_DEVICE.H"

#include <limits.h>
#include <math.h>

bool AILAudio_Initialized(void) {
    return SDL_WasInit(SDL_INIT_AUDIO) != 0;
}

namespace {
bool AudioError() {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "AIL audio output: %s", SDL_GetError());
    return false;
}
} // namespace

#ifdef LBA2_USE_SDL2
struct AILAudioStream {
    SDL_AudioStream *converter;
    AILAudioCallback callback;
    void *userdata;
    int sourceBytesPerFrame;
    int sourceRate;
    float gain;
    double pendingFrames;
    bool flushed;
    bool paused;
    AILAudioStream *next;
};

namespace {
const int OUTPUT_RATE = 44100;
SDL_AudioDeviceID s_device = 0;
SDL_mutex *s_mutex = NULL;
AILAudioStream *s_streams = NULL;

void Read(AILAudioStream *stream, Uint8 *output, int bytes) {
    SDL_memset(output, 0, (size_t)bytes);
    const int available = SDL_AudioStreamAvailable(stream->converter);
    if (available < 0) {
        AudioError();
        return;
    }
    if (stream->callback && available < bytes) {
        const int frames = (int)ceil((double)(bytes - available) / (2 * sizeof(float)) *
                                     stream->sourceRate / OUTPUT_RATE);
        stream->callback(stream->userdata, stream, frames * stream->sourceBytesPerFrame,
                         frames * stream->sourceBytesPerFrame);
    }
    const int received = SDL_AudioStreamGet(stream->converter, output, bytes);
    if (received < 0)
        AudioError();
    else {
        stream->pendingFrames -= (double)received / (2 * sizeof(float)) * stream->sourceRate / OUTPUT_RATE;
        if (stream->pendingFrames < 0 ||
            (stream->flushed && SDL_AudioStreamAvailable(stream->converter) == 0))
            stream->pendingFrames = 0;
        float *samples = (float *)output;
        for (int i = 0; i < received / (int)sizeof(float); ++i)
            samples[i] *= stream->gain;
    }
}
void SDLCALL Mix(void *, Uint8 *output, int bytes) {
    SDL_memset(output, 0, (size_t)bytes);
    SDL_LockMutex(s_mutex);
    float scratch[2048];
    for (int offset = 0; offset < bytes;) {
        const int chunk = bytes - offset < (int)sizeof(scratch) ? bytes - offset : (int)sizeof(scratch);
        float *dest = (float *)(output + offset);
        for (AILAudioStream *stream = s_streams; stream; stream = stream->next) {
            if (stream->paused)
                continue;
            Read(stream, (Uint8 *)scratch, chunk);
            for (int i = 0; i < chunk / (int)sizeof(float); ++i)
                dest[i] += scratch[i];
        }
        for (int i = 0; i < chunk / (int)sizeof(float); ++i) {
            if (dest[i] > 1.0f)
                dest[i] = 1.0f;
            else if (dest[i] < -1.0f)
                dest[i] = -1.0f;
        }
        offset += chunk;
    }
    SDL_UnlockMutex(s_mutex);
}
bool OpenDevice() {
    if (s_device)
        return true;
    s_mutex = SDL_CreateMutex();
    if (!s_mutex)
        return AudioError();
    SDL_AudioSpec desired = {};
    desired.freq = OUTPUT_RATE;
    desired.channels = 2;
    desired.format = AUDIO_F32SYS;
    desired.samples = 1024;
    desired.callback = Mix;
    s_device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
    if (!s_device) {
        AudioError();
        SDL_DestroyMutex(s_mutex);
        s_mutex = NULL;
        return false;
    }
    return true;
}
} // namespace

AILAudioStream *AILAudio_Open(const SDL_AudioSpec *spec, AILAudioCallback callback, void *userdata) {
    if (!spec || spec->freq <= 0 || !spec->channels) {
        SDL_SetError("Invalid audio specification");
        AudioError();
        return NULL;
    }
    AILAudioStream *stream = (AILAudioStream *)SDL_calloc(1, sizeof(*stream));
    if (!stream) {
        AudioError();
        return NULL;
    }
    stream->converter = SDL_NewAudioStream(spec->format, spec->channels, spec->freq,
                                           AUDIO_F32SYS, 2, OUTPUT_RATE);
    stream->sourceRate = spec->freq;
    stream->sourceBytesPerFrame = SDL_AUDIO_BITSIZE(spec->format) / 8 * spec->channels;
    stream->gain = 1.0f;
    stream->callback = callback;
    stream->userdata = userdata;
    stream->paused = true;
    if (!stream->converter || !OpenDevice()) {
        if (!stream->converter)
            AudioError();
        if (stream->converter)
            SDL_FreeAudioStream(stream->converter);
        SDL_free(stream);
        return NULL;
    }
    SDL_LockMutex(s_mutex);
    stream->next = s_streams;
    s_streams = stream;
    SDL_UnlockMutex(s_mutex);
    return stream;
}
void AILAudio_Destroy(AILAudioStream *stream) {
    if (!stream)
        return;
    SDL_LockMutex(s_mutex);
    AILAudioStream **link = &s_streams;
    while (*link && *link != stream)
        link = &(*link)->next;
    if (*link)
        *link = stream->next;
    SDL_FreeAudioStream(stream->converter);
    SDL_free(stream);
    const bool empty = s_streams == NULL;
    SDL_UnlockMutex(s_mutex);
    if (empty) {
        SDL_CloseAudioDevice(s_device);
        s_device = 0;
        SDL_DestroyMutex(s_mutex);
        s_mutex = NULL;
    }
}
void AILAudio_Lock(AILAudioStream *) { SDL_LockMutex(s_mutex); }
void AILAudio_Unlock(AILAudioStream *) { SDL_UnlockMutex(s_mutex); }
bool AILAudio_Put(AILAudioStream *stream, const void *data, int bytes) {
    AILAudio_Lock(stream);
    const int result = SDL_AudioStreamPut(stream->converter, data, bytes);
    if (result == 0) {
        stream->pendingFrames += (double)bytes / stream->sourceBytesPerFrame;
        stream->flushed = false;
    }
    AILAudio_Unlock(stream);
    return result == 0 || AudioError();
}
bool AILAudio_Flush(AILAudioStream *stream) {
    AILAudio_Lock(stream);
    const int result = SDL_AudioStreamFlush(stream->converter);
    if (result == 0)
        stream->flushed = true;
    AILAudio_Unlock(stream);
    return result == 0 || AudioError();
}
int AILAudio_Queued(AILAudioStream *stream) {
    AILAudio_Lock(stream);
    const double pending = ceil(stream->pendingFrames * stream->sourceBytesPerFrame);
    AILAudio_Unlock(stream);
    if (pending > INT_MAX) {
        SDL_SetError("Queued audio exceeds the AIL byte-count range");
        AudioError();
        return -1;
    }
    return (int)pending;
}
bool AILAudio_Gain(AILAudioStream *stream, float gain) {
    if (!(gain >= 0.0f) || gain > 1.0f) {
        SDL_SetError("Invalid audio gain");
        return AudioError();
    }
    AILAudio_Lock(stream);
    stream->gain = gain;
    AILAudio_Unlock(stream);
    return true;
}
bool AILAudio_Resume(AILAudioStream *stream) {
    AILAudio_Lock(stream);
    stream->paused = false;
    AILAudio_Unlock(stream);
#ifndef LBA2_AUDIO_TESTS
    SDL_PauseAudioDevice(s_device, 0);
#endif
    return true;
}
bool AILAudio_Pause(AILAudioStream *stream) {
    AILAudio_Lock(stream);
    stream->paused = true;
    AILAudio_Unlock(stream);
    return true;
}
bool AILAudio_LoadWAV(const char *path, SDL_AudioSpec *spec, Uint8 **data, Uint32 *bytes) {
    return SDL_LoadWAV(path, spec, data, bytes) != NULL;
}
bool AILAudio_LoadWAVMemory(const void *data, int bytes, SDL_AudioSpec *spec, Uint8 **pcm, Uint32 *pcmBytes) {
    SDL_RWops *io = SDL_RWFromConstMem(data, bytes);
    return io && SDL_LoadWAV_RW(io, 1, spec, pcm, pcmBytes) != NULL;
}
#ifdef LBA2_AUDIO_TESTS
void AILAudio_TestRead(AILAudioStream *stream, float *output, int samples) {
    AILAudio_Lock(stream);
    Read(stream, (Uint8 *)output, samples * (int)sizeof(float));
    AILAudio_Unlock(stream);
}
void AILAudio_TestMix(float *output, int samples) {
    Mix(NULL, (Uint8 *)output, samples * (int)sizeof(float));
}
#endif

#else

AILAudioStream *AILAudio_Open(const SDL_AudioSpec *spec, AILAudioCallback callback, void *userdata) {
    return SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, spec, callback, userdata);
}
void AILAudio_Destroy(AILAudioStream *stream) { SDL_DestroyAudioStream(stream); }
void AILAudio_Lock(AILAudioStream *stream) {
    if (!SDL_LockAudioStream(stream))
        AudioError();
}
void AILAudio_Unlock(AILAudioStream *stream) {
    if (!SDL_UnlockAudioStream(stream))
        AudioError();
}
bool AILAudio_Put(AILAudioStream *stream, const void *data, int bytes) {
    return SDL_PutAudioStreamData(stream, data, bytes) || AudioError();
}
bool AILAudio_Flush(AILAudioStream *stream) { return SDL_FlushAudioStream(stream) || AudioError(); }
int AILAudio_Queued(AILAudioStream *stream) {
    const int bytes = SDL_GetAudioStreamQueued(stream);
    if (bytes < 0)
        AudioError();
    return bytes;
}
bool AILAudio_Gain(AILAudioStream *stream, float gain) {
    return SDL_SetAudioStreamGain(stream, gain) || AudioError();
}
bool AILAudio_Resume(AILAudioStream *stream) { return SDL_ResumeAudioStreamDevice(stream) || AudioError(); }
bool AILAudio_Pause(AILAudioStream *stream) { return SDL_PauseAudioStreamDevice(stream) || AudioError(); }
bool AILAudio_LoadWAV(const char *path, SDL_AudioSpec *spec, Uint8 **data, Uint32 *bytes) {
    return SDL_LoadWAV(path, spec, data, bytes);
}
bool AILAudio_LoadWAVMemory(const void *data, int bytes, SDL_AudioSpec *spec, Uint8 **pcm, Uint32 *pcmBytes) {
    SDL_IOStream *io = SDL_IOFromConstMem(data, bytes);
    return io && SDL_LoadWAV_IO(io, true, spec, pcm, pcmBytes);
}
#endif
