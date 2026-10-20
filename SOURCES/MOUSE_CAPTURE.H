#ifndef MOUSE_CAPTURE_H
#define MOUSE_CAPTURE_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* What the window does with the mouse. One place decides it, once per
 * presented frame, so no screen has to remember to give the mouse back.
 *
 * FREE      the cursor may leave the window
 * CONFINED  the cursor stays inside the window; its position is still tracked
 * RELATIVE  the camera is orbiting: motion only, no position
 *
 * The host lifts both CONFINED and RELATIVE while the window is unfocused and
 * restores them on focus, so Alt-Tab and Cmd-Tab work in every mode. */
typedef enum {
    MOUSE_CAPTURE_FREE,
    MOUSE_CAPTURE_CONFINED,
    MOUSE_CAPTURE_RELATIVE,
} T_MOUSE_CAPTURE;

/* mouse_confine: when to keep the cursor inside the window. */
#define MOUSE_CONFINE_NEVER 0
#define MOUSE_CONFINE_FULLSCREEN 1
#define MOUSE_CONFINE_ALWAYS 2
#define MOUSE_CONFINE_DEFAULT MOUSE_CONFINE_FULLSCREEN

extern S32 MouseConfine;

/* The rule, with no state: what the mouse should be doing for these inputs.
 * `orbit` is whether the camera asked for relative motion this frame. The
 * console needs a cursor to select text, so it never gets RELATIVE. */
T_MOUSE_CAPTURE MouseCapture_Decide(S32 confine, S32 fullscreen, S32 consoleOpen, S32 orbit);

/* Called by the mouse camera on each gameplay frame it orbits. Only a frame
 * that asks keeps relative mode, so a menu, a cutscene with its own loop or
 * an interior ends it without having to know about it. */
void MouseCapture_RequestOrbit(void);

/* Once per presented frame: decide and apply what changed. */
void MouseCapture_Update(void);

T_MOUSE_CAPTURE MouseCapture_Current(void);
const char *MouseCapture_Name(T_MOUSE_CAPTURE mode);

#ifdef __cplusplus
}
#endif

#endif /* MOUSE_CAPTURE_H */
