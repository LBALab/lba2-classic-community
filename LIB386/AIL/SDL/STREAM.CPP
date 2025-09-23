#include "STREAM_SDL.H"

#include <SDL3/SDL.h>

S32 streamVolume = 0; // PRIVATE - From STREAM_SDL.H

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
