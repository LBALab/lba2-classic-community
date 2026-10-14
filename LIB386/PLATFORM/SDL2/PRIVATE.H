#pragma once

#include <PLATFORM/PLATFORM.H>
#include <SDL.h>

bool SDL2_InitVideo(void);
bool SDL2_InitWindow(const char *title);
void SDL2_EndWindow(void);
bool SDL2_WindowInitialized(void);
bool SDL2_CreateWindow(uint32_t width, uint32_t height, bool fullscreen);
void SDL2_DestroyWindow(void);
bool SDL2_ResizeWindow(uint32_t width, uint32_t height, bool keepSize);
bool SDL2_WindowSize(int32_t *width, int32_t *height);
bool SDL2_DisplaySize(uint32_t *width, uint32_t *height);
bool SDL2_PrimaryDisplaySize(uint32_t *width, uint32_t *height);
void SDL2_SurfaceCoords(int32_t x, int32_t y, int32_t *sx, int32_t *sy);
void SDL2_SetFullscreen(bool enabled);
bool SDL2_GetFullscreen(void);
bool SDL2_IsFullscreen(void);
void SDL2_SetVSync(bool enabled);
bool SDL2_SetPresentFilter(PlatformPresentFilter mode);
void SDL2_SetRelativeMouse(bool enabled);
bool SDL2_Present(const PlatformFrame *frame);
void SDL2_HandleWindowEvent(const SDL_Event *event);
bool SDL2_TranslateEvent(const SDL_Event *source, PlatformEvent *event);
bool SDL2_PollEvent(PlatformEvent *event);
char *SDL2_CurrentDirectory(void);
const char *SDL2_HomePath(void);
bool SDL2_CreateDirectory(const char *path);
bool SDL2_RemovePath(const char *path);
bool SDL2_MessageBox(PlatformMessageKind kind, const char *title, const char *message);
PlatformFolderDialog *SDL2_BeginFolderDialog(const char *initial);
PlatformDialogResult SDL2_PollFolderDialog(PlatformFolderDialog *dialog, const char **path);
void SDL2_EndFolderDialog(PlatformFolderDialog *dialog);

#ifdef LBA2_SDL2_TESTS
SDL_Window *SDL2_TestWindow(void);
void SDL2_SetPresentationProbe(void (*probe)(SDL_Renderer *));
void SDL2_SetFullscreenProbe(int (*probe)(SDL_Window *, Uint32));
void SDL2_SetMessageProbe(int (*probe)(Uint32, const char *, const char *, SDL_Window *));
#endif
