#ifndef CONSOLE_H
#define CONSOLE_H

#include <SYSTEM/ADELINE_TYPES.H>
#include <stddef.h> /* size_t */

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

/* Console text colours for Console_PrintColored. DEFAULT is a palette index
 * (the scene's white, as plain Console_Print uses); the rest are
 * palette-independent fixed-RGB inks resolved in the compositor, so "warn is
 * yellow" / "banner is cyan" holds in every scene regardless of the loaded
 * palette. BANNER matches the terminal sink's cyan for the boot bookends. */
#define CONSOLE_COL_DEFAULT 15
#define CONSOLE_COL_BANNER 250
#define CONSOLE_COL_DEBUG 251
#define CONSOLE_COL_WARN 252
#define CONSOLE_COL_ERROR 253

/** Like Console_Print, but draws the line in the given CONSOLE_COL_* colour. */
void Console_PrintColored(int colour, const char *fmt, ...);

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

/** Cleanroom UI-capture mode. When set, surfaces that normally pre-fill the Screen
 *  buffer with the live 3D scene (so the UI overlay composites over what a player
 *  would see mid-game — see e.g. INVENT.CPP MenuInventory pre-pass) clear it to
 *  black instead. Used by `ui --black-bg <verb>` so the committed UI goldens are
 *  scene-free and byte-identity actually means "the UI rendered correctly", not
 *  "the UI + scene happened to be deterministic at this resolution".
 *
 *  Only meaningful while a capture is armed (Inv_/Dial_/Menu_RequestCapture);
 *  cmd_ui clears the flag at the end of the verb so non-harness flows are
 *  unaffected. */
void Console_SetCaptureBlackBg(int active);
int Console_GetCaptureBlackBg(void);

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

/* Populate the command/cvar table once (idempotent). The console calls this on first
   open; the CLI control harness calls it before Console_Execute. */
void Console_EnsureRegistered(void);

/* Internal command execution entry point; used by host-only parser tests. */
void Console_Execute(const char *line);
/* Test hook: receives each console output line after it is pushed to scrollback. */
void Console_SetLineSinkForTests(Console_LineSinkFn sink);

/* Copy up to max recent scrollback line pointers (chronological, newest last) into
   out[]; returns the count. Pointers are valid until the next console output. */
int Console_GetScrollback(const char **out, int max);

/* Tab-completion core (also used by host tests): match \a prefix against the
   registered commands, cvars and built-ins. \a out receives the longest common
   prefix of all matches (the full name when unique); \a matches receives up to
   \a max candidate name pointers. Returns the number of matches. */
int Console_CompletePrefix(const char *prefix, char *out, size_t outsz, const char **matches, int max);

/* Host-test hooks for the input/selection state machine. They drive the same code
   the live console runs after the event queue, without needing SDL scancodes. */
typedef enum {
    CONSOLE_TKEY_LEFT,
    CONSOLE_TKEY_RIGHT,
    CONSOLE_TKEY_HOME,
    CONSOLE_TKEY_END,
    CONSOLE_TKEY_BACKSPACE,
    CONSOLE_TKEY_DELETE,
    CONSOLE_TKEY_ENTER,
    CONSOLE_TKEY_ESC,
    CONSOLE_TKEY_TAB,
    CONSOLE_TKEY_UP,
    CONSOLE_TKEY_DOWN
} Console_TestKey;
void Console_TestType(const char *s);                     /* type printable chars */
void Console_TestSpecial(Console_TestKey key, int shift); /* a nav/edit key, optional Shift */
void Console_TestCtrl(char letter);                       /* Ctrl+<letter> */
const char *Console_TestInput(void);                      /* current input line */
int Console_TestCursor(void);                             /* cursor position */
int Console_TestSelection(int *lo, int *hi);              /* 1 + [lo,hi) when a selection exists */

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_H */
