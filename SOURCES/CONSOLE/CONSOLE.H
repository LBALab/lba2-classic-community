#ifndef CONSOLE_H
#define CONSOLE_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Other TUs that call this API: INPUT (UpdateGameInput: open-state update, suppress flag),
 * PLAYACF (ACF loop: stall while open), PERSO (SDL event filter + pre-present + PNG save),
 * GAMEMENU (slide-show gate). Command table and cheats: CONSOLE_CMD.CPP. Scene/mode strings
 * shared with commands: CONSOLE_STATE.H / CONSOLE_STATE.CPP. Host tests: tests/console/.
 */

/* Public API */

int Console_IsOpen(void);
void Console_Toggle(void);
/** Close the console if open (e.g. before playvideo/cube so the game takes over). Sets suppress-input-for-one-frame. */
void Console_Close(void);
/** Process one frame of input (drains key queue filled by Console_FeedEvent). Call when console is open. */
void Console_Update(void);
void Console_Print(const char *fmt, ...);

/** Called from video layer pre-present callback. Overlays console onto ARGB frameBuffer if open. */
void Console_PrePresent(U32 *frameBuffer, U32 pitch, U32 width, U32 height, const U8 *palette_rgb);

/** Event filter: call from ManageEvents. Returns 1 if event was consumed (toggle or console input). */
int Console_FeedEvent(const void *sdlEvent);

/** Set console toggle scancode (SDL scancode value). Invalid values fall back to F12. */
void Console_SetToggleScancode(int scancode);
/** Current console toggle scancode used by Console_FeedEvent / input reservation. */
int Console_GetToggleScancode(void);

/** Returns 1 for one frame after closing the console; call from input path and suppress game input when 1. Clears the flag. */
int Console_ShouldSuppressInputThisFrame(void);

/** Set to 1 when entering demo slide show, 0 when leaving. Prevents opening console during slide (avoids segfault). */
void Console_SetSlideShowActive(int active);

/** Request a screenshot without the console. Closes the console. If \a use_png, game will save PNG (via SavePNG); else next frame is saved as PPM in PrePresent. */
void Console_RequestScreenshot(const char *path, int use_png);
/** If a screenshot is pending, copy path into \a path_out (max \a path_max) and set \a out_is_png. Return 1 if pending, 0 otherwise. */
int Console_HasPendingScreenshot(char *path_out, unsigned int path_max, int *out_is_png);
/** Clear pending screenshot request. */
void Console_ClearPendingScreenshot(void);

/* Extensibility - used only inside the console module */
typedef void (*Console_CmdFn)(int argc, const char *argv[]);
typedef void (*Console_LineSinkFn)(const char *line);

typedef enum {
    CONSOLE_CVAR_INT,
    CONSOLE_CVAR_BOOL,
    CONSOLE_CVAR_BOOL_U8
} Console_CvarType;

void Console_RegisterCommand(const char *name, Console_CmdFn callback, const char *help);
/** Register with optional one-line usage and context (for `help <cmd>`); pass NULL to omit. */
void Console_RegisterCommandEx(const char *name, Console_CmdFn callback, const char *help, const char *usage, const char *context);
void Console_RegisterCvar(const char *name, void *ptr, Console_CvarType type, const char *help);

/* Called from CONSOLE_CMD.CPP to register all commands/cvars */
void Console_RegisterAll(void);

/* Internal command execution entry point; used by host-only parser tests. */
void Console_Execute(const char *line);
/* Test hook: receives each console output line after it is pushed to scrollback. */
void Console_SetLineSinkForTests(Console_LineSinkFn sink);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_H */
