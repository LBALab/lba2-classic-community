#include "SAMPLE_SDL.H"

#include <SDL3/SDL.h>

#include <AIL/COMMON.H>

S32 Sample_Driver_Enabled = FALSE; // PUBLIC - From SAMPLE.H
S32 UseWaveMixer = FALSE;          // PUBLIC - From SAMPLE.H
S32 sampleVolume = 0;              // PRIVATE - From SAMPLE_SDL.H

S32 InitSampleDriver(char *driver_name) {
  SDL_Log("DEBUG - InitSampleDriver(%s)", driver_name);

  if (Sample_Driver_Enabled) {
    return TRUE;
  }

  Sample_Driver_Enabled = TRUE;

  return TRUE;
}

void SetMasterVolumeSample(S32 volume) {
  SDL_Log("DEBUG - SetMasterVolumeSample(%d)", volume);
  sampleVolume = volume;
}

S32 FadeOutSamples(S32 delay) {
  SDL_Log("DEBUG - FadeOutSamples(%d)", delay);
  return 0;
}

S32 FadeInSamples(S32 delay) {
  SDL_Log("DEBUG - FadeInSamples(%d)", delay);
  return delay;
}
void InverseStereoSample(S32 inverse) {
  SDL_Log("DEBUG - InverseStereoSample(%d)", inverse);
}

U32 PlaySample(void *buffer, S32 sizeBytes, U32 userhandle, S32 pitchbend,
               S32 repeat, S32 volume, S32 pan) {
  SDL_Log("DEBUG - PlaySample(%p, %d, %u, %d, %d, %d, %d)", buffer, sizeBytes,
          userhandle, pitchbend, repeat, volume, pan);
  return 0xFFFFFFFF;
}

void ChangePitchbendSample(U32 sample, S32 pitchbend) {
  SDL_Log("DEBUG - ChangePitchbendSample(%u, %d)", sample, pitchbend);
}

void ChangeVolumePanSample(U32 sample, S32 volume, S32 pan) {
  SDL_Log("DEBUG - ChangeVolumePanSample(%u, %d, %d)", sample, volume, pan);
}

void StopOneSample(U32 sample) {
  SDL_Log("DEBUG - StopOneSample(%u)", sample);
}

void StopSamples() {
  SDL_Log("DEBUG - StopSamples()");
}

void PauseSamples() {
  SDL_Log("DEBUG - PauseSamples()");
}

void ResumeSamples() {
  SDL_Log("DEBUG - ResumeSamples()");
}

U32 IsSamplePlaying(U32 sample) {
  SDL_Log("DEBUG - IsSamplePlaying(%u)", sample);
  return FALSE;
}
