#pragma once

// This header contains declarations private to SDL

#include <AIL/STREAM.H>

#ifdef __cplusplus
extern "C" {
#endif

extern S32 streamVolume;

/* MusicLogEnable / MusicLogIsEnabled are part of the abstract AIL API
   (see <AIL/STREAM.H>); every backend provides them. */

#ifdef __cplusplus
}
#endif
