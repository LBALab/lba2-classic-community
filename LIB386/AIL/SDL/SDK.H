#pragma once

/* What the AIL sound rules (samples, music, CD, video audio, WAV) need from the
   host, under AIL_ names: the output transport's spec, log and error, a thread,
   a semaphore, an atomic, the clock and the WAV hints. The rules files reach the
   host only through this header and AUDIO_DEVICE.H, so another transport can
   take SDL's place. */

#include <SYSTEM/ADELINE_TYPES.H>

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
