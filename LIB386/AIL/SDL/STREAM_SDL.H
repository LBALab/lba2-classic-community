#pragma once

// This header contains declarations private to SDL

#include <AIL/STREAM.H>

#ifdef __cplusplus
extern "C" {
#endif

extern S32 streamVolume;

void MusicLogEnable(int enable);
int  MusicLogIsEnabled(void);

#ifdef __cplusplus
}
#endif
