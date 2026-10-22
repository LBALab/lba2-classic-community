#pragma once

#ifdef LBA2_USE_SDL2
#include <SDL.h>
typedef SDL_atomic_t AILAtomicInt;
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
#define AIL_AtomicGet SDL_GetAtomicInt
#define AIL_AtomicSet SDL_SetAtomicInt
#define AIL_AtomicAdd SDL_AddAtomicInt
#define AIL_AUDIO_S16 SDL_AUDIO_S16
#define AIL_AUDIO_S16LE SDL_AUDIO_S16LE
#define AIL_AUDIO_U8 SDL_AUDIO_U8
#define AIL_AUDIO_BITSIZE(format) (SDL_AUDIO_BYTESIZE(format) * 8)
#define AIL_AUDIO_DRIVER_HINT SDL_HINT_AUDIO_DRIVER
#endif

inline Uint64 AIL_Ticks() {
#ifdef LBA2_USE_SDL2
    return SDL_GetTicks64();
#else
    return SDL_GetTicks();
#endif
}
