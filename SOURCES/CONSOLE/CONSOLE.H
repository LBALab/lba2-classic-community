#ifndef CONSOLE_H
#define CONSOLE_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Public API */

int  Console_IsOpen(void);
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

/** Returns 1 for one frame after closing the console; call from input path and suppress game input when 1. Clears the flag. */
int Console_ShouldSuppressInputThisFrame(void);

/** Set to 1 when entering demo slide show, 0 when leaving. Prevents opening console during slide (avoids segfault). */
void Console_SetSlideShowActive(int active);

/* Extensibility - used only inside the console module */
typedef void (*Console_CmdFn)(int argc, const char *argv[]);

typedef enum {
	CONSOLE_CVAR_INT,
	CONSOLE_CVAR_BOOL,
	CONSOLE_CVAR_BOOL_U8
} Console_CvarType;

void Console_RegisterCommand(const char *name, Console_CmdFn callback, const char *help);
void Console_RegisterCvar(const char *name, void *ptr, Console_CvarType type, const char *help);

/* Called from CONSOLE_CMD.CPP to register all commands/cvars */
void Console_RegisterAll(void);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_H */
