#include <AIL/VIDEO_AUDIO.H>

S32 StartVideoAudio(S32 freq, S32 channels, S32 is16bit) {
  (void)freq;
  (void)channels;
  (void)is16bit;
  return 1;
}

void ResumeVideoAudio(void) {}

void PushVideoAudio(const void *data, U32 sizeBytes) {
  (void)data;
  (void)sizeBytes;
}

void StopVideoAudio(void) {}
