#pragma once

#include <PLATFORM/PLATFORM.H>
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

bool SDL3_Init(void);
bool SDL3_SetHeadless(void);
void SDL3_ClearError(void);
uint64_t SDL3_Ticks(void);
uint64_t SDL3_Counter(void);
uint64_t SDL3_Frequency(void);
uint64_t SDL3_ThreadID(void);
int32_t SDL3_CPUCount(void);
int32_t SDL3_RAM(void);
bool SDL3_InitVideo(void);
bool SDL3_VideoInitialized(void);
void SDL3_SetLogOutput(PlatformLogFn callback, void *userdata);
void *SDL3_AndroidEnvironment(void);
void *SDL3_AndroidActivity(void);

bool SDL3_InitEvents(void);
void SDL3_EndEvents(void);
bool SDL3_PollEvent(PlatformEvent *event);
bool SDL3_TranslateEvent(const SDL_Event *source, PlatformEvent *event);

bool SDL3_InitWindow(const char *title);
void SDL3_EndWindow(void);
bool SDL3_IsWindowInitialized(void);
bool SDL3_CreateWindow(uint32_t width, uint32_t height, bool fullscreen);
void SDL3_DestroyWindow(void);
bool SDL3_ResizeWindow(uint32_t width, uint32_t height, bool keepWindowSize);
bool SDL3_WindowSize(int32_t *width, int32_t *height);
bool SDL3_DisplaySize(uint32_t *width, uint32_t *height);
bool SDL3_PrimaryDisplaySize(uint32_t *width, uint32_t *height);
void SDL3_SurfaceCoords(int32_t x, int32_t y, int32_t *sx, int32_t *sy);
void SDL3_SetFullscreen(bool fullscreen);
bool SDL3_GetFullscreen(void);
bool SDL3_IsFullscreen(void);
void SDL3_SetVSync(bool enabled);
void SDL3_SetRelativeMouse(bool enabled);
void SDL3_HandleWindowEvent(const PlatformEvent *event);
bool SDL3_Present(const PlatformFrame *frame);
#ifdef LBA2_SDL3_TESTS
void SDL3_SetPresentationProbe(void (*probe)(SDL_Renderer *renderer));
#endif

const uint8_t *SDL3_KeyboardState(void);
uint32_t SDL3_Keycode(int32_t scancode, uint16_t modifiers);
const char *SDL3_ScancodeName(int32_t scancode);
uint16_t SDL3_Modifiers(void);
void SDL3_TextInput(bool enabled);
void SDL3_CursorVisible(bool visible);
bool SDL3_InitGamepads(void);
void SDL3_EndGamepads(void);
uint32_t *SDL3_Gamepads(int32_t *count);
PlatformGamepad *SDL3_OpenGamepad(uint32_t id);
void SDL3_CloseGamepad(PlatformGamepad *pad);
const char *SDL3_GamepadName(PlatformGamepad *pad);
bool SDL3_GamepadButton(PlatformGamepad *pad, PlatformGamepadButton button);
int16_t SDL3_GamepadAxis(PlatformGamepad *pad, PlatformGamepadAxis axis);

bool SDL3_MessageBox(PlatformMessageKind kind, const char *title, const char *message);
PlatformFolderDialog *SDL3_BeginFolderDialog(const char *initialPath);
PlatformFolderDialog *SDL3_AllocateFolderDialog(void);
void SDLCALL SDL3_FolderChosen(void *userdata, const char *const *files, int filter);
PlatformDialogResult SDL3_PollFolderDialog(PlatformFolderDialog *dialog, const char **path);
void SDL3_EndFolderDialog(PlatformFolderDialog *dialog);
const char *SDL3_HomePath(void);
int32_t SDL3_Strcasecmp(const char *a, const char *b);
int32_t SDL3_Strncasecmp(const char *a, const char *b, size_t length);

#ifdef __cplusplus
}
#endif
