//--------------------------------------------------------------------------
#ifndef LIB_AIL_STREAM
#define LIB_AIL_STREAM

//--------------------------------------------------------------------------
#include <SYSTEM/ADELINE_TYPES.H>

//--------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------
//	OpenStream :			Initialize Stream API
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void OpenStream();

//--------------------------------------------------------------------------
//	PlayStream :			start a stream
//
//			name:		pathname to WAV file to stream
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void PlayStream(char *name);

//--------------------------------------------------------------------------
//	ChangeVolumeStream :		change the volume of the playing
//					stream
//
//			volume:		stream volume (0-127)
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void ChangeVolumeStream(S32 volume);

//--------------------------------------------------------------------------
//	GetVolumeStream :		get the volume of the playing
//					stream
//
//	Returns	:			stream volume (0-127)
//--------------------------------------------------------------------------
S32 GetVolumeStream();

//--------------------------------------------------------------------------
//	StopStream :			stop playing stream
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void StopStream();

//--------------------------------------------------------------------------
//	PauseStream :			pause playing stream
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void PauseStream();

//--------------------------------------------------------------------------
//	ResumeStream :			resume playing stream
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void ResumeStream();

//--------------------------------------------------------------------------
//	SuspendStreamOutput :		pause the stream's audio OUTPUT only,
//					without touching the logical pause state
//					or the parked resume position. Used for
//					window-focus loss (mirrors the original
//					PauseMiles): a nested device pause that must
//					not disturb an outer PauseStream/ResumeStream.
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void SuspendStreamOutput();

//--------------------------------------------------------------------------
//	ResumeStreamOutput :		undo SuspendStreamOutput, but only if the
//					stream is not logically paused (so a focus
//					blip during a dialogue/menu pause does not
//					un-pause it).
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void ResumeStreamOutput();

//--------------------------------------------------------------------------
//	IsStreamPlaying :		return TRUE while a stream is actively
//					playing; FALSE when stopped or paused
//
//	Returns	:			TRUE/FALSE
//--------------------------------------------------------------------------
S32 IsStreamPlaying();

//--------------------------------------------------------------------------
//	StreamName :			return the name of the playing stream
//					(if nothing is playing return "")
//
//	Returns	:			pathname
//--------------------------------------------------------------------------
char *StreamName();

//--------------------------------------------------------------------------
//	MusicLogEnable :		toggle the [MUSIC]/[CD] trace channel
//
//			enable:		non-zero to turn tracing on
//
//	A single runtime flag, flipped from the console ("audio global log 1").
//	When on, the backend emits per-call [MUSIC]/[CD] lines and the game layer
//	(MUSIC.CPP) emits its high-level transitions, all fanning into adeline.log.
//	Backend-agnostic so any backend (SDL stream, redbook, future ISO) honours
//	the same switch.
//
//	Returns	:			nothing
//--------------------------------------------------------------------------
void MusicLogEnable(int enable);

//--------------------------------------------------------------------------
//	MusicLogIsEnabled :		query the [MUSIC]/[CD] trace channel
//
//	Returns	:			non-zero if tracing is on
//--------------------------------------------------------------------------
int MusicLogIsEnabled(void);

//--------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------
#endif //LIB_AIL_STREAM

//--------------------------------------------------------------------------
