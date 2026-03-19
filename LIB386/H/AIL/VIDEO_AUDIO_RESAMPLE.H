#ifndef VIDEO_AUDIO_RESAMPLE_H
#define VIDEO_AUDIO_RESAMPLE_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Resample chunk to exactly mixSamples (one frame at stream rate) and add into mix.
 * Uses fractional indexing so pitch/duration stay correct. */
void ResampleAndAddToMix(unsigned long srcRate, unsigned long targetRate,
    const void *srcPtr, unsigned long srcBytes,
    void *mixPtr, unsigned long mixSamples, int is16);

/* LBA2 intro: voice tracks 1/2/3 often have header rate=22050 but are actually 11025 Hz.
 * Return effective rate for resampling. */
unsigned long GetEffectiveTrackRate(int t, const unsigned long *a_r,
    const U8 *a_c, unsigned long targetRate);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_AUDIO_RESAMPLE_H */
