#include "CD_SDL.H"

#include <SDL3/SDL.h>

const char* OpenCD(char *volume_name) {
  SDL_Log("DEBUG - OpenCD(%s)", volume_name);
  return "D";
}

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
