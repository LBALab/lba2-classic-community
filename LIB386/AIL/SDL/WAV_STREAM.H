#pragma once

#include "AUDIO_DEVICE.H"
#include <SYSTEM/ADELINE_TYPES.H>

struct AILWavSource;

/* Missing/non-WAV sources return NULL with recognized=false, for OGG fallback.
   A recognized but invalid WAV returns NULL with an AIL error. */
AILWavSource *AILWav_Open(const char *path, bool image, S32 frame, bool *recognized);
/* One independently decoded batch. Caller frees PCM with AIL_Free. 1=data, 0=EOF, -1=error. */
int AILWav_Read(AILWavSource *source, AILAudioSpec *spec, U8 **pcm, U32 *bytes);
void AILWav_Close(AILWavSource *source);
#ifdef LBA2_MUSIC_TESTS
U32 AILWav_TestBytesRead(const AILWavSource *source);
#endif
