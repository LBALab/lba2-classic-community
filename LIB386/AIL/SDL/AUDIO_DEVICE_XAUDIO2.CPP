/* The AIL output transport on XAudio2, for the UWP build, which has no SDL.

   One stream is one source voice. XAudio2 converts each voice's format and
   rate, mixes the voices into the mastering voice and applies each voice's
   gain: the work SDL3 does for the SDL transport. A stream's bytes, whether
   pushed (AILAudio_Put) or pulled (its callback, asked when the voice is about
   to run dry, as SDL3 asks), wait in a queue and reach the voice one buffer
   per XAudio2 processing pass, on XAudio2's own thread.

   Also here: InitAIL, and the host services SDK.H declares for this build. */

#include "AUDIO_DEVICE.H"
#include "WAV_DECODE.H"

#include <AIL/COMMON.H>
#include <PLATFORM/PLATFORM.H>
#include <SYSTEM/CRASH.H>

#include <windows.h>
#include <xaudio2.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Host services ─────────────────────────────────────────────────────── */

namespace {
/* Per thread, as SDL keeps its error. */
__thread char t_error[256];
} // namespace

void AIL_Log(const char *fmt, ...) {
    char line[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    Platform_Log(PLATFORM_LOG_INFO, "%s", line);
}

bool AIL_SetError(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(t_error, sizeof(t_error), fmt, args);
    va_end(args);
    return false;
}

const char *AIL_GetError(void) { return t_error; }

struct AILThread {
    HANDLE handle;
    AILThreadFunction fn;
    void *data;
};

namespace {
DWORD WINAPI ThreadMain(void *param) {
    AILThread *thread = (AILThread *)param;
    return (DWORD)thread->fn(thread->data);
}
} // namespace

AILThread *AIL_CreateThread(AILThreadFunction fn, const char *name, void *data) {
    (void)name;
    AILThread *thread = (AILThread *)calloc(1, sizeof(*thread));
    if (!thread) {
        AIL_SetError("Unable to allocate a thread");
        return NULL;
    }
    thread->fn = fn;
    thread->data = data;
    thread->handle = CreateThread(NULL, 0, ThreadMain, thread, 0, NULL);
    if (!thread->handle) {
        AIL_SetError("CreateThread failed (%lu)", (unsigned long)GetLastError());
        free(thread);
        return NULL;
    }
    return thread;
}

void AIL_WaitThread(AILThread *thread) {
    if (!thread)
        return;
    WaitForSingleObjectEx(thread->handle, INFINITE, FALSE);
    CloseHandle(thread->handle);
    free(thread);
}

AILSemaphore *AIL_CreateSemaphore(U32 initial) {
    HANDLE semaphore = CreateSemaphoreExW(NULL, (LONG)initial, 0x7fffffff, NULL, 0, SEMAPHORE_ALL_ACCESS);
    if (!semaphore)
        AIL_SetError("CreateSemaphoreEx failed (%lu)", (unsigned long)GetLastError());
    return (AILSemaphore *)semaphore;
}

void AIL_DestroySemaphore(AILSemaphore *semaphore) {
    if (semaphore)
        CloseHandle((HANDLE)semaphore);
}

int AIL_WaitSemaphoreTimeout(AILSemaphore *semaphore, int timeoutMs) {
    const DWORD wait = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
    const DWORD result = WaitForSingleObjectEx((HANDLE)semaphore, wait, FALSE);
    if (result == WAIT_OBJECT_0)
        return 0;
    if (result == WAIT_TIMEOUT)
        return 1;
    AIL_SetError("WaitForSingleObjectEx failed (%lu)", (unsigned long)GetLastError());
    return -1;
}

bool AIL_SignalSemaphore(AILSemaphore *semaphore) { return ReleaseSemaphore((HANDLE)semaphore, 1, NULL) != 0; }

/* Milliseconds, as SDL_GetTicks counts them; GetTickCount64 moves in ~16 ms
   steps, which the fades would show. */
U64 AIL_Ticks() { return g_platform.system.ticks_ms(); }

/* ── Engine ────────────────────────────────────────────────────────────── */

namespace {
class EngineEvents : public IXAudio2EngineCallback {
  public:
    void STDMETHODCALLTYPE OnProcessingPassStart() {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() {}
    /* The output device went away. Every voice is silent from here on. */
    void STDMETHODCALLTYPE OnCriticalError(HRESULT error) {
        AIL_Log("XAudio2 AIL: audio engine stopped (0x%08lx)", (unsigned long)error);
    }
};

IXAudio2 *s_engine = NULL;
IXAudio2MasteringVoice *s_master = NULL;
EngineEvents s_engineEvents;
CRITICAL_SECTION s_streamsLock;
AILAudioStream *s_streams = NULL;

void ClearAIL(void);
} // namespace

bool AILAudio_Initialized(void) { return s_engine != NULL; }

bool AIL_SetHeadless(void) {
    /* --headless also skips InitAIL, so no device is opened. */
    return true;
}

void InitAIL() {
    if (s_engine)
        return;
    HRESULT hr = XAudio2Create(&s_engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        AIL_Log("XAudio2 AIL: Could not init audio: XAudio2Create failed (0x%08lx)", (unsigned long)hr);
        s_engine = NULL;
        return;
    }
    s_engine->RegisterForCallbacks(&s_engineEvents);
    hr = s_engine->CreateMasteringVoice(&s_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, NULL,
                                        NULL, AudioCategory_GameEffects);
    if (FAILED(hr)) {
        AIL_Log("XAudio2 AIL: Could not init audio: no output device (0x%08lx)", (unsigned long)hr);
        s_engine->Release();
        s_engine = NULL;
        return;
    }
    InitializeCriticalSectionEx(&s_streamsLock, 0, 0);
    /* As the SDL transport does: stop the audio thread before the exit
       handlers and static destructors pull its data away. */
    atexit(ClearAIL);
}

/* ── Streams ───────────────────────────────────────────────────────────── */

namespace {
enum { MAX_BUFFERS = 16 };

/* One buffer handed to the voice; freed when the voice is done with it. */
struct Buffer {
    int bytes;
    U8 data[1];
};

class VoiceEvents : public IXAudio2VoiceCallback {
  public:
    AILAudioStream *stream;
    VoiceEvents() : stream(NULL) {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 bytesRequired);
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() {}
    void STDMETHODCALLTYPE OnStreamEnd() {}
    void STDMETHODCALLTYPE OnBufferStart(void *) {}
    void STDMETHODCALLTYPE OnBufferEnd(void *context);
    void STDMETHODCALLTYPE OnLoopEnd(void *) {}
    void STDMETHODCALLTYPE OnVoiceError(void *, HRESULT error) {
        AIL_Log("XAudio2 AIL: voice error (0x%08lx)", (unsigned long)error);
    }
};
} // namespace

struct AILAudioStream {
    VoiceEvents events;
    IXAudio2SourceVoice *voice;
    CRITICAL_SECTION lock;
    AILAudioCallback callback;
    void *userdata;
    int frameBytes;
    int passBytes; // one 10 ms processing pass of this stream's audio, rounded up
    /* Bytes waiting for the voice: queue[head .. head + queued). */
    U8 *queue;
    int head, queued, capacity;
    /* Bytes handed to the voice and not yet played. */
    Buffer *buffers[MAX_BUFFERS];
    int submitted;
    bool submitFailed;
    AILAudioStream *next;
};

namespace {
void Submit(AILAudioStream *stream, int bytes) {
    int slot = 0;
    while (slot < MAX_BUFFERS && stream->buffers[slot])
        ++slot;
    if (slot == MAX_BUFFERS)
        return; // the voice holds enough; the next pass takes the rest
    Buffer *buffer = (Buffer *)malloc(sizeof(Buffer) + (size_t)bytes);
    if (!buffer)
        return;
    buffer->bytes = bytes;
    memcpy(buffer->data, stream->queue + stream->head, (size_t)bytes);
    XAUDIO2_BUFFER submit;
    memset(&submit, 0, sizeof(submit));
    submit.AudioBytes = (UINT32)bytes;
    submit.pAudioData = buffer->data;
    submit.pContext = buffer;
    const HRESULT hr = stream->voice->SubmitSourceBuffer(&submit, NULL);
    if (FAILED(hr)) {
        if (!stream->submitFailed)
            AIL_Log("XAudio2 AIL: SubmitSourceBuffer failed (0x%08lx)", (unsigned long)hr);
        stream->submitFailed = true;
        free(buffer);
        return;
    }
    stream->buffers[slot] = buffer;
    stream->submitted += bytes;
    stream->head += bytes;
    stream->queued -= bytes;
    if (!stream->queued)
        stream->head = 0;
}

/* XAudio2's thread, at the start of each pass: bytesRequired is what the voice
   needs now to avoid running dry. One pass more is handed over as a margin. */
void STDMETHODCALLTYPE VoiceEvents::OnVoiceProcessingPassStart(UINT32 bytesRequired) {
    if (!bytesRequired)
        return;
    Crash_PrepareThread();
    AILAudioStream *s = stream;
    EnterCriticalSection(&s->lock);
    int want = (int)bytesRequired + s->passBytes;
    want -= want % s->frameBytes;
    if (s->callback && s->queued < want)
        s->callback(s->userdata, s, want - s->queued, want);
    int bytes = s->queued < want ? s->queued : want;
    bytes -= bytes % s->frameBytes;
    if (bytes > 0)
        Submit(s, bytes);
    LeaveCriticalSection(&s->lock);
}

void STDMETHODCALLTYPE VoiceEvents::OnBufferEnd(void *context) {
    Buffer *buffer = (Buffer *)context;
    AILAudioStream *s = stream;
    EnterCriticalSection(&s->lock);
    for (int i = 0; i < MAX_BUFFERS; ++i) {
        if (s->buffers[i] == buffer) {
            s->buffers[i] = NULL;
            s->submitted -= buffer->bytes;
            free(buffer);
            break;
        }
    }
    LeaveCriticalSection(&s->lock);
}

void DestroyVoice(AILAudioStream *stream) {
    if (!stream->voice)
        return;
    /* Waits for the voice's callbacks to finish, so no lock may be held. */
    stream->voice->DestroyVoice();
    stream->voice = NULL;
    for (int i = 0; i < MAX_BUFFERS; ++i) {
        free(stream->buffers[i]);
        stream->buffers[i] = NULL;
    }
    stream->submitted = 0;
}

void ClearAIL(void) {
    if (!s_engine)
        return;
    s_engine->StopEngine();
    EnterCriticalSection(&s_streamsLock);
    AILAudioStream *streams = s_streams;
    LeaveCriticalSection(&s_streamsLock);
    for (AILAudioStream *stream = streams; stream; stream = stream->next)
        DestroyVoice(stream);
    s_master->DestroyVoice();
    s_master = NULL;
    s_engine->Release();
    s_engine = NULL;
}
} // namespace

AILAudioStream *AILAudio_Open(const AILAudioSpec *spec, AILAudioCallback callback, void *userdata) {
    if (!spec || spec->freq < XAUDIO2_MIN_SAMPLE_RATE || spec->freq > XAUDIO2_MAX_SAMPLE_RATE ||
        spec->channels < 1 || spec->channels > 2 ||
        (spec->format != AIL_AUDIO_U8 && spec->format != AIL_AUDIO_S16)) {
        AIL_SetError("Invalid audio specification");
        return NULL;
    }
    if (!s_engine) {
        AIL_SetError("Audio is not initialised");
        return NULL;
    }
    AILAudioStream *stream = new AILAudioStream();
    stream->events.stream = stream;
    stream->callback = callback;
    stream->userdata = userdata;
    stream->frameBytes = AIL_AUDIO_BITSIZE(spec->format) / 8 * spec->channels;
    stream->passBytes = (spec->freq / 100 + 1) * stream->frameBytes;
    InitializeCriticalSectionEx(&stream->lock, 0, 0);

    WAVEFORMATEX format;
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = (WORD)spec->channels;
    format.nSamplesPerSec = (DWORD)spec->freq;
    format.wBitsPerSample = (WORD)AIL_AUDIO_BITSIZE(spec->format);
    format.nBlockAlign = (WORD)stream->frameBytes;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    /* Created stopped: a stream starts paused, as SDL3's does. */
    const HRESULT hr =
        s_engine->CreateSourceVoice(&stream->voice, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &stream->events, NULL, NULL);
    if (FAILED(hr)) {
        AIL_SetError("CreateSourceVoice failed (0x%08lx)", (unsigned long)hr);
        DeleteCriticalSection(&stream->lock);
        delete stream;
        return NULL;
    }
    EnterCriticalSection(&s_streamsLock);
    stream->next = s_streams;
    s_streams = stream;
    LeaveCriticalSection(&s_streamsLock);
    return stream;
}

void AILAudio_Destroy(AILAudioStream *stream) {
    if (!stream)
        return;
    if (s_engine) {
        EnterCriticalSection(&s_streamsLock);
        AILAudioStream **link = &s_streams;
        while (*link && *link != stream)
            link = &(*link)->next;
        if (*link)
            *link = stream->next;
        LeaveCriticalSection(&s_streamsLock);
    }
    DestroyVoice(stream);
    DeleteCriticalSection(&stream->lock);
    free(stream->queue);
    delete stream;
}

void AILAudio_Lock(AILAudioStream *stream) { EnterCriticalSection(&stream->lock); }
void AILAudio_Unlock(AILAudioStream *stream) { LeaveCriticalSection(&stream->lock); }

bool AILAudio_Put(AILAudioStream *stream, const void *data, int bytes) {
    if (bytes <= 0)
        return bytes == 0;
    AILAudio_Lock(stream);
    if (stream->head + stream->queued + bytes > stream->capacity) {
        if (stream->head) {
            memmove(stream->queue, stream->queue + stream->head, (size_t)stream->queued);
            stream->head = 0;
        }
        if (stream->queued + bytes > stream->capacity) {
            int capacity = stream->capacity ? stream->capacity : 16384;
            while (capacity < stream->queued + bytes)
                capacity *= 2;
            U8 *queue = (U8 *)realloc(stream->queue, (size_t)capacity);
            if (!queue) {
                AILAudio_Unlock(stream);
                AIL_SetError("Unable to queue %d audio bytes", bytes);
                return false;
            }
            stream->queue = queue;
            stream->capacity = capacity;
        }
    }
    memcpy(stream->queue + stream->head + stream->queued, data, (size_t)bytes);
    stream->queued += bytes;
    AILAudio_Unlock(stream);
    return true;
}

/* Every queued byte reaches the voice in whole frames; there is no
   converter to drain. */
bool AILAudio_Flush(AILAudioStream *) { return true; }

int AILAudio_Queued(AILAudioStream *stream) {
    AILAudio_Lock(stream);
    const int bytes = stream->queued + stream->submitted;
    AILAudio_Unlock(stream);
    return bytes;
}

bool AILAudio_Gain(AILAudioStream *stream, float gain) {
    if (!(gain >= 0.0f) || gain > 1.0f)
        return AIL_SetError("Invalid audio gain");
    return stream->voice && SUCCEEDED(stream->voice->SetVolume(gain, XAUDIO2_COMMIT_NOW));
}

bool AILAudio_Resume(AILAudioStream *stream) {
    return stream->voice && SUCCEEDED(stream->voice->Start(0, XAUDIO2_COMMIT_NOW));
}

bool AILAudio_Pause(AILAudioStream *stream) {
    return stream->voice && SUCCEEDED(stream->voice->Stop(0, XAUDIO2_COMMIT_NOW));
}

/* ── WAV ───────────────────────────────────────────────────────────────── */

/* PCM and IMA ADPCM (WAV_DECODE.CPP): SDL's loader also decodes float, A-law,
   mu-law and MS ADPCM, which no release of the game uses for music. */
bool AILAudio_LoadWAVMemory(const void *data, int bytes, AILAudioSpec *spec, U8 **pcm, U32 *pcmBytes) {
    return AILWav_DecodeMemory(data, bytes, spec, pcm, pcmBytes);
}

bool AILAudio_LoadWAV(const char *path, AILAudioSpec *spec, U8 **data, U32 *bytes) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return AIL_SetError("Unable to open %s", path);
    long size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    U8 *contents = size > 0 && size < 0x7fffffff ? (U8 *)malloc((size_t)size) : NULL;
    const bool read = contents && fseek(file, 0, SEEK_SET) == 0 && fread(contents, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if (!read) {
        free(contents);
        return AIL_SetError("Unable to read %s", path);
    }
    const bool loaded = AILAudio_LoadWAVMemory(contents, (int)size, spec, data, bytes);
    free(contents);
    return loaded;
}
