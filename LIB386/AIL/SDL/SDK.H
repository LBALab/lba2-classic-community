#pragma once

/* What the AIL sound rules (samples, music, CD, video audio, WAV) need from the
   host, under AIL_ names: the output transport's spec, log and error, a thread,
   a semaphore, an atomic, the clock and the WAV hints. The rules files reach the
   host only through this header and AUDIO_DEVICE.H, so another transport can
   take SDL's place: SDL2 and SDL3 below, or XAudio2 (AUDIO_DEVICE_XAUDIO2.CPP)
   for a UWP build without SDL. */

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef LBA2_AIL_XAUDIO2

#include <stdlib.h>
#include <string.h>

typedef struct {
    volatile int value;
} AILAtomicInt;
/* Set and Add return the previous value, as SDL's do. */
inline int AIL_AtomicGet(AILAtomicInt *a) { return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST); }
inline int AIL_AtomicSet(AILAtomicInt *a, int v) { return __atomic_exchange_n(&a->value, v, __ATOMIC_SEQ_CST); }
inline int AIL_AtomicAdd(AILAtomicInt *a, int v) { return __atomic_fetch_add(&a->value, v, __ATOMIC_SEQ_CST); }

/* SDL3's values: the low byte is the sample size in bits. */
#define AIL_AUDIO_U8 0x0008
#define AIL_AUDIO_S16LE 0x8010
#define AIL_AUDIO_S16 AIL_AUDIO_S16LE
#define AIL_AUDIO_BITSIZE(format) ((format) & 0xFF)

typedef struct {
    int format;
    int channels;
    int freq;
} AILAudioSpec;
#define AILCALL __cdecl

void AIL_Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
bool AIL_SetError(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
const char *AIL_GetError(void);
#define AIL_Free free
#define AIL_Zero(x) memset(&(x), 0, sizeof(x))

/* No hints are set on this build, so the WAV reader keeps its defaults. */
#define AIL_HINT_WAVE_RIFF_CHUNK_SIZE "SDL_WAVE_RIFF_CHUNK_SIZE"
#define AIL_HINT_WAVE_TRUNCATION "SDL_WAVE_TRUNCATION"
#define AIL_HINT_WAVE_FACT_CHUNK "SDL_WAVE_FACT_CHUNK"
inline const char *AIL_GetHint(const char *) { return NULL; }

typedef struct AILThread AILThread;
typedef struct AILSemaphore AILSemaphore;
typedef int(AILCALL *AILThreadFunction)(void *data);

AILThread *AIL_CreateThread(AILThreadFunction fn, const char *name, void *data);
void AIL_WaitThread(AILThread *thread);
AILSemaphore *AIL_CreateSemaphore(U32 initial);
void AIL_DestroySemaphore(AILSemaphore *semaphore);
/* Zero means signaled, positive means timeout. */
int AIL_WaitSemaphoreTimeout(AILSemaphore *semaphore, int timeoutMs);
bool AIL_SignalSemaphore(AILSemaphore *semaphore);
U64 AIL_Ticks();

#else

#ifdef LBA2_USE_SDL2
#include <SDL.h>
typedef SDL_atomic_t AILAtomicInt;
typedef SDL_sem AILSemaphore;
#define AIL_AtomicGet SDL_AtomicGet
#define AIL_AtomicSet SDL_AtomicSet
#define AIL_AtomicAdd SDL_AtomicAdd
#define AIL_AUDIO_S16 AUDIO_S16SYS
#define AIL_AUDIO_S16LE AUDIO_S16LSB
#define AIL_AUDIO_U8 AUDIO_U8
#define AIL_AUDIO_BITSIZE SDL_AUDIO_BITSIZE
#define AIL_AUDIO_DRIVER_HINT SDL_HINT_AUDIODRIVER
#else
#include <SDL3/SDL.h>
typedef SDL_AtomicInt AILAtomicInt;
typedef SDL_Semaphore AILSemaphore;
#define AIL_AtomicGet SDL_GetAtomicInt
#define AIL_AtomicSet SDL_SetAtomicInt
#define AIL_AtomicAdd SDL_AddAtomicInt
#define AIL_AUDIO_S16 SDL_AUDIO_S16
#define AIL_AUDIO_S16LE SDL_AUDIO_S16LE
#define AIL_AUDIO_U8 SDL_AUDIO_U8
#define AIL_AUDIO_BITSIZE(format) (SDL_AUDIO_BYTESIZE(format) * 8)
#define AIL_AUDIO_DRIVER_HINT SDL_HINT_AUDIO_DRIVER
#endif

/* Format, channels and rate of one stream; the rules set those three only. */
typedef SDL_AudioSpec AILAudioSpec;
#define AILCALL SDLCALL

#define AIL_Log SDL_Log
#define AIL_SetError SDL_SetError
#define AIL_GetError SDL_GetError
/* Frees PCM the transport's WAV loader returned. */
#define AIL_Free SDL_free
#define AIL_Zero SDL_zero

/* The WAV hints the music reader honours, as SDL's own loader does. */
#define AIL_HINT_WAVE_RIFF_CHUNK_SIZE SDL_HINT_WAVE_RIFF_CHUNK_SIZE
#define AIL_HINT_WAVE_TRUNCATION SDL_HINT_WAVE_TRUNCATION
#define AIL_HINT_WAVE_FACT_CHUNK SDL_HINT_WAVE_FACT_CHUNK
#define AIL_GetHint SDL_GetHint

typedef SDL_Thread AILThread;
typedef int(AILCALL *AILThreadFunction)(void *data);

inline AILThread *AIL_CreateThread(AILThreadFunction fn, const char *name, void *data) {
    return SDL_CreateThread(fn, name, data);
}

inline void AIL_WaitThread(AILThread *thread) { SDL_WaitThread(thread, NULL); }

inline AILSemaphore *AIL_CreateSemaphore(U32 initial) { return SDL_CreateSemaphore(initial); }

inline void AIL_DestroySemaphore(AILSemaphore *semaphore) { SDL_DestroySemaphore(semaphore); }

/* Zero means signaled, positive means timeout, negative means SDL2 error. */
inline int AIL_WaitSemaphoreTimeout(AILSemaphore *semaphore, int timeoutMs) {
#ifdef LBA2_USE_SDL2
    return SDL_SemWaitTimeout(semaphore, (Uint32)timeoutMs);
#else
    return SDL_WaitSemaphoreTimeout(semaphore, timeoutMs) ? 0 : 1;
#endif
}

inline bool AIL_SignalSemaphore(AILSemaphore *semaphore) {
#ifdef LBA2_USE_SDL2
    return SDL_SemPost(semaphore) == 0;
#else
    SDL_SignalSemaphore(semaphore);
    return true;
#endif
}

inline U64 AIL_Ticks() {
#ifdef LBA2_USE_SDL2
    return SDL_GetTicks64();
#else
    return SDL_GetTicks();
#endif
}

#endif
