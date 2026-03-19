//--------------------------------------------------------------------------
#ifndef LIB_AIL_VIDEO_AUDIO
#define LIB_AIL_VIDEO_AUDIO

//--------------------------------------------------------------------------
#include <SYSTEM/ADELINE_TYPES.H>

//--------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// Start a dedicated audio stream for video playback (e.g. Smacker track).
// Stream is created paused; call ResumeVideoAudio() after pre-filling so playback doesn't stutter.
// freq: sample rate (e.g. 22050), channels: 1 or 2, is16bit: 1 for 16-bit PCM, 0 for 8-bit.
// Returns 1 on success, 0 on failure.
S32 StartVideoAudio(S32 freq, S32 channels, S32 is16bit);

// Start playback (call after pushing initial buffer so device doesn't underrun).
void ResumeVideoAudio(void);

// Pause playback (e.g. when console is open). Call ResumeVideoAudio() to resume.
void PauseVideoAudio(void);

// Push PCM data to the video audio stream. data is sizeBytes bytes.
void PushVideoAudio(const void *data, U32 sizeBytes);

// Stop and destroy the video audio stream.
void StopVideoAudio(void);

//--------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

#endif
