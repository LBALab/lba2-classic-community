#include "AIL.H"

// === CD ======================================================================
const char* OpenCD(char *volume_name) { return "D"; }
void PlayCD(S32 track) {}
void ChangeVolumeCD(S32 volume) {}
S32 GetVolumeCD() { return 0; }
void StopCD() {}
void PauseCD() {}
void ResumeCD() {}
S32 IsCDPlaying() { return FALSE; }

// === COMMON ==================================================================
void InitAIL() {}

// === MIDI ====================================================================
S32 InitMidiDriver(char *driver_name) {
  return TRUE;
}

// === SAMPLE ==================================================================
S32 Sample_Driver_Enabled = FALSE;
S32 sampleVolume = 0;

S32 InitSampleDriver(char *driver_name) {
  if (Sample_Driver_Enabled) {
    return TRUE;
  }

  Sample_Driver_Enabled = TRUE;

  return TRUE;
}

void SetMasterVolumeSample(S32 volume) { sampleVolume = volume; }
S32 FadeOutSamples(S32 delay) { return 0; }
S32 FadeInSamples(S32 delay) { return delay; }
void InverseStereoSample(S32 inverse) {}
U32 PlaySample(void *buffer, S32 sizeBytes, U32 userhandle, S32 pitchbend,
               S32 repeat, S32 volume, S32 pan) {
  return 0xFFFFFFFF;
}
void ChangePitchbendSample(U32 sample, S32 pitchbend) {}
void ChangeVolumePanSample(U32 sample, S32 volume, S32 pan) {}
void StopOneSample(U32 sample) {}
void StopSamples() {}
void PauseSamples() {}
void ResumeSamples() {}
U32 IsSamplePlaying(U32 sample) { return FALSE; }

// === STREAM ==================================================================
S32 streamVolume = 0;

void OpenStream() {}
void PlayStream(char *name) {}
void ChangeVolumeStream(S32 volume) { streamVolume = volume; }
S32 GetVolumeStream() { return streamVolume; }
void StopStream() {}
void PauseStream() {}
void ResumeStream() {}
S32 IsStreamPlaying() { return FALSE; }
char *StreamName() { return ""; }
