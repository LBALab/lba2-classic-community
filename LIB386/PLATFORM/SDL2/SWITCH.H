#pragma once

#include <PLATFORM/PLATFORM.H>

#ifdef __cplusplus
extern "C" {
#endif

const char *SDL2_SwitchBasePath(void);
const char *SDL2_SwitchUserPath(void);
void SDL2_SwitchWindowSize(int *width, int *height);
bool SDL2_SwitchMessageBox(const char *title, const char *message);

#ifdef __cplusplus
}
#endif
