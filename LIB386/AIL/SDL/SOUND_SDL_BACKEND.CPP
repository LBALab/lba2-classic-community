#include "AIL.H"

#include <SYSTEM/LIMITS.H>

#include <stdlib.h>
#include <SDL3/SDL.h>

// === TIMER ===================================================================
void InitAIL() {
  SDL_Log("DEBUG - InitAIL()");
}

// === COMMON ==================================================================
S32 Midi_Driver_Enabled = FALSE;   // TODO: Remove from public extern, private to MILES
S32 Sample_Driver_Enabled = FALSE; // Exposed by COMMON.H and SAMPLE.H
S32 SamplingRate = 22050;          // TODO: Remove from public extern, private to MILES

char SampleDriverName[ADELINE_MAX_PATH] = ""; // TODO: Remove from public extern, private to MILES
char MidiDriverName[ADELINE_MAX_PATH] = "";   // TODO: Remove from public extern, private to MILES

VOID_FUNC *PauseMidiPtr = NULL; // TODO: Remove from public extern, private to MILES
VOID_FUNC *ResumeMidiPtr = NULL; // TODO: Remove from public extern, private to MILES
VOID_FUNC *PauseSamplesPtr = NULL; // TODO: Remove from public extern, private to MILES
VOID_FUNC *ResumeSamplesPtr = NULL; // TODO: Remove from public extern, private to MILES

void PauseBothMiles() { // TODO: Remove from public extern, private to MILES
  (*PauseSamplesPtr)();
  (*PauseMidiPtr)();
}

void ResumeBothMiles() { // TODO: Remove from public extern, private to MILES
  (*ResumeMidiPtr)();
  (*ResumeSamplesPtr)();
}

// === MIDI ====================================================================
S32 Midi_Driver_Type = MIDI; // TODO: Remove from public extern, private to MILES

void ClearMidiDriver() { Midi_Driver_Enabled = FALSE; } // TODO: Remove from public extern, private to MILES
void PauseMidi() {} // TODO: Remove from public extern, private to MILES
void ResumeMidi() {} // TODO: Remove from public extern, private to MILES
void ResetMidi() {} // TODO: Remove from public extern, private to MILES
U32 PrepareMidi(void *ptrxmi, S32 seqnum, S32 volume) { return TRUE; } // TODO: Remove from public extern, private to MILES
void ClearMidi() {} // TODO: Remove from public extern, private to MILES
void PlayBlockMidi(S32 blocknum) {} // TODO: Remove from public extern, private to MILES
void ChainBlockMidi(S32 blocknum) {} // TODO: Remove from public extern, private to MILES
void ChangeVolumeMidi(S32 volume, S32 delay) {} // TODO: Remove from public extern, private to MILES
void StopChainMidi() {} // TODO: Remove from public extern, private to MILES
void StopMidi() {} // TODO: Remove from public extern, private to MILES
S32 GetBlockPlayingMidi() { return -1; } // TODO: Remove from public extern, private to MILES

S32 InitMidiDriver(char *driver_name) {
  SDL_Log("DEBUG - InitMidiDriver()");

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
S32 SampleOverflow = FALSE; // TODO: Remove from public extern, private to MILES
S32 sampleVolume = 0;

void SetMasterVolumeSample(S32 volume) {
  SDL_Log("DEBUG - SetMasterVolumeSample(%d)", volume);
  sampleVolume = volume;
}

S32 GetMasterVolumeSample() { return sampleVolume; } // TODO: Remove from public extern, private to MILES
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

U32 PlaySample(void *buffer, S32 sizeBytes, U32 userhandle, S32 pitchbend, S32 repeat,
               S32 volume, S32 pan) {
  SDL_Log("DEBUG - PlaySample(%p, %d, %u, %d, %d, %d, %d)", buffer, sizeBytes, userhandle, pitchbend, repeat, volume, pan);
  return 0xFFFFFFFF;
}

U32 TimerPlaySample(void *ptrsample, U32 usernum, S32 pitchbend, S32 nbrepeat, // TODO: Remove from public extern, private to MILES
                    S32 volume, S32 pan, S32 period) {
  return Sample_Driver_Enabled ? TRUE : FALSE;
}
void TimerStopSample() {} // TODO: Remove from public extern, private to MILES
void ChangePitchbendSample(U32 sample, S32 pitchbend) {
  SDL_Log("DEBUG - ChangePitchbendSample(%u, %d)", sample, pitchbend);
}

void ChangeVolumePanSample(U32 sample, S32 volume, S32 pan) {
  SDL_Log("DEBUG - ChangeVolumePanSample(%u, %d, %d)", sample, volume, pan);
}

void *GetSampleMSSHandle(U32 sample); // TODO: Remove from public extern, private to MILES
void StopOneSample(U32 sample) {
  SDL_Log("DEBUG - StopOneSample(%u)", sample);
}

void StopSamples() {
  SDL_Log("DEBUG - StopSamples()");
}

void PauseOneSample(U32 sample) {} // TODO: Remove from public extern, private to MILES
void ResumeOneSample(U32 sample) {} // TODO: Remove from public extern, private to MILES
void PauseSamples() {
  SDL_Log("DEBUG - PauseSamples()");
}

void ResumeSamples() {
  SDL_Log("DEBUG - ResumeSamples()");
}

S32 GetPlayingSamples(SAMPLE_PLAYING tab[], S32 max) { return 0; } // TODO: Remove from public extern, private to MILES
U32 IsSamplePlaying(U32 sample) {
  SDL_Log("DEBUG - IsSamplePlaying(%u)", sample);
  return FALSE;
}

S32 GetSamplePosition(U32 sample) { return -1; } // TODO: Remove from public extern, private to MILES
S32 GetSamplePoly() { return 0; } // TODO: Remove from public extern, private to MILES
void SetDataSample(U32 sample, S32 index, S32 value) {} // TODO: Remove from public extern, private to MILES
S32 GetDataSample(U32 sample, S32 index) { return 0; } // TODO: Remove from public extern, private to MILES

S32 InitSampleDriver(char *driver_name) {
  SDL_Log("DEBUG - InitSampleDriver(%s)", driver_name);

  if (Sample_Driver_Enabled) {
    return TRUE;
  }

  Sample_Driver_Enabled = TRUE;
  PauseSamplesPtr = PauseSamples;
  ResumeSamplesPtr = ResumeSamples;

  return TRUE;
}

// === STREAM ==================================================================
S32 streamVolume = 0; // TODO: Remove from public extern, private to MILES

void OpenStream() {
  SDL_Log("DEBUG - OpenStream()");
}

void PlayStream(char *name) {
  SDL_Log("DEBUG - PlayStream(%s)", name);
}

void ChangeVolumeStream(S32 volume) {
  SDL_Log("DEBUG - ChangeVolumeStream(%d)", volume);
  streamVolume = volume;
}

S32 GetVolumeStream() {
  SDL_Log("DEBUG - GetVolumeStream() -> %d", streamVolume);
  return streamVolume;
}

void StopStream() {
  SDL_Log("DEBUG - StopStream()");
}

void PauseStream() {
  SDL_Log("DEBUG - PauseStream()");
}

void ResumeStream() {
  SDL_Log("DEBUG - ResumeStream()");
}

S32 IsStreamPlaying() {
  SDL_Log("DEBUG - IsStreamPlaying()");
  return FALSE;
}

char *StreamName() {
  SDL_Log("DEBUG - StreamName()");
  return "";
}

// === CD ======================================================================
const char* OpenCD(char *volume_name) {
  SDL_Log("DEBUG - OpenCD(%s)", volume_name);
  return "D";
}

void CloseCD() {} // TODO: Remove from public extern, private to MILES
void PlayCD(S32 track) {
  SDL_Log("DEBUG - PlayCD(%d)", track);
}

void ChangeVolumeCD(S32 volume) {
  SDL_Log("DEBUG - ChangeVolumeCD(%d)", volume);
}

S32 GetVolumeCD() {
  SDL_Log("DEBUG - GetVolumeCD()");
  return 0;
}

void StopCD() {
  SDL_Log("DEBUG - StopCD()");
}

void PauseCD() {
  SDL_Log("DEBUG - PauseCD()");
}

void ResumeCD() {
  SDL_Log("DEBUG - ResumeCD()");
}

S32 IsCDPlaying() {
  SDL_Log("DEBUG - IsCDPlaying()");

  return FALSE;
}

// =============================================================================
