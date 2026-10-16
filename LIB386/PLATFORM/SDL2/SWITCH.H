#pragma once

#include <PLATFORM/PLATFORM.H>

#ifdef __cplusplus
extern "C" {
#endif

const char *SDL2_SwitchBasePath(void);
const char *SDL2_SwitchUserPath(void);
void SDL2_SwitchWindowSize(int *width, int *height);
bool SDL2_SwitchMessageBox(const char *title, const char *message);
bool SDL2_SwitchInitGameData(void);
bool SDL2_SwitchInitNetloader(void);
void SDL2_SwitchEndNetloader(void);

#ifdef __cplusplus
}
#endif
