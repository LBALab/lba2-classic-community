#include "AIL.H"

#include <SYSTEM/LIMITS.H>

#include <stdlib.h>

// === TIMER ===================================================================
void InitAIL() {}

// === COMMON ==================================================================
S32 Midi_Driver_Enabled = FALSE;   // Exposed by COMMON.H and MIDI.H
S32 Sample_Driver_Enabled = FALSE; // Exposed by COMMON.H and SAMPLE.H
S32 SamplingRate = 22050;          // Exposed by COMMON.H and SAMPLE.H

char SampleDriverName[ADELINE_MAX_PATH] = ""; // Exposed by COMMON.H and SAMPLE.H
char MidiDriverName[ADELINE_MAX_PATH] = "";   // Exposed by COMMON.H and MIDI.H

VOID_FUNC *PauseMidiPtr = NULL;
VOID_FUNC *ResumeMidiPtr = NULL;
VOID_FUNC *PauseSamplesPtr = NULL;
VOID_FUNC *ResumeSamplesPtr = NULL;

void PauseBothMiles() {
  (*PauseSamplesPtr)();
  (*PauseMidiPtr)();
}

void ResumeBothMiles() {
  (*ResumeMidiPtr)();
  (*ResumeSamplesPtr)();
}

// === MIDI ====================================================================
S32 Midi_Driver_Type = MIDI; // Exposed by MIDI.H

void ClearMidiDriver() { Midi_Driver_Enabled = FALSE; }
void PauseMidi() {}
void ResumeMidi() {}
void ResetMidi() {}
U32 PrepareMidi(void *ptrxmi, S32 seqnum, S32 volume) { return TRUE; }
void ClearMidi() {}
void PlayBlockMidi(S32 blocknum) {}
void ChainBlockMidi(S32 blocknum) {}
void ChangeVolumeMidi(S32 volume, S32 delay) {}
void StopChainMidi() {}
void StopMidi() {}
S32 GetBlockPlayingMidi() { return -1; }

S32 InitMidiDriver(char *driver_name) {
  if (Midi_Driver_Enabled) {
    return TRUE;
  }

  Midi_Driver_Type = MIDI;
  Midi_Driver_Enabled = TRUE;
  PauseMidiPtr = PauseMidi;
  ResumeMidiPtr = ResumeMidi;

  return TRUE;
}

// === SAMPLE ==================================================================
S32 SampleOverflow = FALSE; // Exposed by SAMPLE.H
S32 sampleVolume = 0;

void SetMasterVolumeSample(S32 volume) { sampleVolume = volume; }
S32 GetMasterVolumeSample() { return sampleVolume; }
S32 FadeOutSamples(S32 delay) { return 0; }
S32 FadeInSamples(S32 delay) { return delay; }
void InverseStereoSample(S32 inverse) {}
U32 PlaySample(void *buffer, U32 userhandle, S32 pitchbend, S32 repeat,
               S32 volume, S32 pan) {
  return 0xFFFFFFFF;
}
U32 TimerPlaySample(void *ptrsample, U32 usernum, S32 pitchbend, S32 nbrepeat,
                    S32 volume, S32 pan, S32 period) {
  return Sample_Driver_Enabled ? TRUE : FALSE;
}
void TimerStopSample() {}
void ChangePitchbendSample(U32 sample, S32 pitchbend) {}
void ChangeVolumePanSample(U32 sample, S32 volume, S32 pan) {}
void *GetSampleMSSHandle(U32 sample);
void StopOneSample(U32 sample) {}
void StopSamples() {}
void PauseOneSample(U32 sample) {}
void ResumeOneSample(U32 sample) {}
void PauseSamples() {}
void ResumeSamples() {}
S32 GetPlayingSamples(SAMPLE_PLAYING tab[], S32 max) { return 0; }
U32 IsSamplePlaying(U32 sample) { return FALSE; }
S32 GetSamplePosition(U32 sample) { return -1; }
S32 GetSamplePoly() { return 0; }
void SetDataSample(U32 sample, S32 index, S32 value) {}
S32 GetDataSample(U32 sample, S32 index) { return 0; }

S32 InitSampleDriver(char *driver_name) {
  if (Sample_Driver_Enabled) {
    return TRUE;
  }

  Sample_Driver_Enabled = TRUE;
  PauseSamplesPtr = PauseSamples;
  ResumeSamplesPtr = ResumeSamples;

  return TRUE;
}

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

// === CD ======================================================================
const char* OpenCD(char *volume_name) { return "D"; }
void CloseCD() {}
void PlayCD(S32 track) {}
void ChangeVolumeCD(S32 volume) {}
S32 GetVolumeCD() { return 0; }
void StopCD() {}
void PauseCD() {}
void ResumeCD() {}
S32 IsCDPlaying() { return FALSE; }

// =============================================================================
