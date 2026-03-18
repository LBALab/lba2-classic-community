#include <AIL/VIDEO_AUDIO_RESAMPLE.H>

void ResampleAndAddToMix(unsigned long srcRate, unsigned long targetRate,
    const void *srcPtr, unsigned long srcBytes,
    void *mixPtr, unsigned long mixSamples, int is16)
{
  if (srcRate == 0 || targetRate == 0 || mixSamples == 0) return;
  unsigned long srcSamples = srcBytes / (is16 ? 2u : 1u);
  if (srcSamples == 0) return;

  if (srcRate == targetRate) {
    unsigned long n = (srcSamples < mixSamples) ? srcSamples : mixSamples;
    if (is16) {
      const short *src = (const short *)srcPtr;
      short *mix = (short *)mixPtr;
      for (unsigned long j = 0; j < n; j++) {
        int s = (int)mix[j] + (int)src[j];
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        mix[j] = (short)s;
      }
    } else {
      const U8 *src = (const U8 *)srcPtr;
      U8 *mix = (U8 *)mixPtr;
      for (unsigned long j = 0; j < n; j++) {
        int s = (int)(mix[j]) - 128 + (int)(src[j]) - 128;
        if (s > 127) s = 127;
        if (s < -128) s = -128;
        mix[j] = (U8)(s + 128);
      }
    }
    return;
  }

  /* Resample to exactly mixSamples with linear interpolation to reduce grain. */
  if (is16) {
    const short *src = (const short *)srcPtr;
    short *mix = (short *)mixPtr;
    for (unsigned long k = 0; k < mixSamples; k++) {
      double idx = (double)k * (double)srcSamples / (double)mixSamples;
      unsigned long idx0 = (unsigned long)idx;
      if (idx0 >= srcSamples) idx0 = srcSamples - 1;
      unsigned long idx1 = idx0 + 1;
      if (idx1 >= srcSamples) idx1 = srcSamples - 1;
      double frac = idx - (double)idx0;
      int sample = (int)((1.0 - frac) * (double)src[idx0] + frac * (double)src[idx1]);
      int s = (int)mix[k] + sample;
      if (s > 32767) s = 32767;
      if (s < -32768) s = -32768;
      mix[k] = (short)s;
    }
  } else {
    const U8 *src = (const U8 *)srcPtr;
    U8 *mix = (U8 *)mixPtr;
    for (unsigned long k = 0; k < mixSamples; k++) {
      double idx = (double)k * (double)srcSamples / (double)mixSamples;
      unsigned long idx0 = (unsigned long)idx;
      if (idx0 >= srcSamples) idx0 = srcSamples - 1;
      unsigned long idx1 = idx0 + 1;
      if (idx1 >= srcSamples) idx1 = srcSamples - 1;
      double frac = idx - (double)idx0;
      int sample = (int)((1.0 - frac) * ((int)(src[idx0]) - 128) + frac * ((int)(src[idx1]) - 128));
      int s = (int)(mix[k]) - 128 + sample;
      if (s > 127) s = 127;
      if (s < -128) s = -128;
      mix[k] = (U8)(s + 128);
    }
  }
}

unsigned long GetEffectiveTrackRate(int t, const unsigned long *a_r,
    const U8 *a_c, unsigned long targetRate)
{
  unsigned long r = a_r[t];
  if (t >= 1 && t <= 3 && r == 22050 && (a_c[t] == 1) && targetRate == 22050)
    return 11025u;
  return r;
}
