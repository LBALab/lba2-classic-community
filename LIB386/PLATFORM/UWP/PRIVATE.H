#pragma once

/* The UWP backend's internal API, shared by its files. */

#include "COM.H"

#include <PLATFORM/PLATFORM.H>

#include <windows.applicationmodel.core.h>
#include <windows.ui.core.h>

/* Reports through the shared error slot (HOST_OS.H), so get_error sees it. */
bool Uwp_Fail(const char *format, ...);

/* The app's writable folder with a trailing backslash, in the ANSI code page
   the narrow file calls use; empty when unavailable. */
const char *Uwp_LocalState(void);

/* BACKEND.CPP: an INFO line for the game's log. Lines from before the log is
   open (the arguments, the launch) are kept and written by Uwp_FlushNotes,
   which the first pump calls; stderr goes nowhere on the console. */
void Uwp_Note(const char *format, ...);
void Uwp_FlushNotes(void);

/* EVENTS.CPP: the window's events, set up once the CoreWindow exists. */
void Uwp_AttachWindow(ABI::Windows::UI::Core::ICoreWindow *window);
void Uwp_DetachWindow(void);
void Uwp_PushEvent(const PlatformEvent *event);

/* INPUT.CPP: subscribed at startup (see Uwp_WatchGamepads); polled from pump. */
void Uwp_WatchGamepads(void);
void Uwp_PollGamepads(void);

/* WINDOW.CPP */
ABI::Windows::UI::Core::ICoreWindow *Uwp_CoreWindow(void);
void Uwp_WindowSizeChanged(void);
void Uwp_WindowTrim(void);

/* Contract callbacks, gathered into the table by BACKEND.CPP. */
bool Uwp_EventsInit(void);
void Uwp_EventsShutdown(void);
void Uwp_Pump(void);
bool Uwp_Poll(PlatformEvent *event);
const uint8_t *Uwp_KeyboardState(void);
uint32_t Uwp_Keycode(int32_t scancode, uint16_t modifiers);
const char *Uwp_ScancodeName(int32_t scancode);
uint16_t Uwp_Modifiers(void);
void Uwp_TextInput(bool enabled);

bool Uwp_InitGamepads(void);
void Uwp_ShutdownGamepads(void);
uint32_t *Uwp_Gamepads(int32_t *count);
PlatformGamepad *Uwp_OpenGamepad(uint32_t id);
void Uwp_CloseGamepad(PlatformGamepad *pad);
const char *Uwp_GamepadName(PlatformGamepad *pad);
bool Uwp_GamepadButton(PlatformGamepad *pad, PlatformGamepadButton button);
int16_t Uwp_GamepadAxis(PlatformGamepad *pad, PlatformGamepadAxis axis);

bool Uwp_WindowInit(const char *title);
void Uwp_WindowShutdown(void);
bool Uwp_WindowInitialized(void);
bool Uwp_WindowCreate(uint32_t width, uint32_t height, bool fullscreen);
void Uwp_WindowDestroy(void);
bool Uwp_WindowResize(uint32_t width, uint32_t height, bool keepWindowSize);
bool Uwp_WindowSize(int32_t *width, int32_t *height);
bool Uwp_DisplaySize(uint32_t *width, uint32_t *height);
void Uwp_SurfaceCoords(int32_t x, int32_t y, int32_t *sx, int32_t *sy);
void Uwp_SetVSync(bool enabled);
bool Uwp_SetPresentFilter(PlatformPresentFilter mode);
bool Uwp_Present(const PlatformFrame *frame);
