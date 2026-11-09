#pragma once

#include "SDK.H"

/* Decodes a whole WAV held in memory to PCM, for a transport without SDL's
   loader (the XAudio2 one). Reads 8 and 16-bit PCM and 4-bit IMA ADPCM, the
   encoding of the CD releases' and the demo's music, giving the bytes SDL's
   loader gives with no WAV hints set: whole ADPCM blocks only, the fact chunk
   ignored. The PCM is malloc'd; AIL_Free releases it on that build. */
bool AILWav_DecodeMemory(const void *data, int bytes, AILAudioSpec *spec, U8 **pcm, U32 *pcmBytes);
