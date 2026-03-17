#pragma once

// This header contains declarations private to SDL

#include <AIL/SAMPLE.H>

#ifdef __cplusplus
extern "C" {
#endif

extern S32 sampleVolume;

void SfxLogEnable(int enable);
int  SfxLogIsEnabled(void);

#ifdef __cplusplus
}
#endif
