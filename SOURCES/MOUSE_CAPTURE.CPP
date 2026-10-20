#include "MOUSE_CAPTURE.H"

#include "CONSOLE/CONSOLE.H" /* Console_IsOpen */
#include "GAMEMENU_MOUSE.H"  /* GamePointer_HiddenBySetting */
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/WINDOW.H>

S32 MouseConfine = MOUSE_CONFINE_DEFAULT;

/* Presents since the camera last asked to orbit. One present without a
 * request is allowed, so a gameplay frame that presents twice does not drop
 * relative mode in between; a screen with its own loop never asks, and ends it
 * on its second frame. */
#define ORBIT_GRACE_PRESENTS 1
static S32 s_presentsSinceOrbit = ORBIT_GRACE_PRESENTS + 1;

static T_MOUSE_CAPTURE s_current = MOUSE_CAPTURE_FREE;
static S32 s_applied = 0;

T_MOUSE_CAPTURE MouseCapture_Decide(S32 confine, S32 fullscreen, S32 consoleOpen, S32 orbit) {
    if (orbit && !consoleOpen)
        return MOUSE_CAPTURE_RELATIVE;
    if (confine == MOUSE_CONFINE_ALWAYS || (confine == MOUSE_CONFINE_FULLSCREEN && fullscreen))
        return MOUSE_CAPTURE_CONFINED;
    return MOUSE_CAPTURE_FREE;
}

void MouseCapture_RequestOrbit(void) { s_presentsSinceOrbit = 0; }

void MouseCapture_Update(void) {
    const S32 orbit = s_presentsSinceOrbit <= ORBIT_GRACE_PRESENTS;
    if (s_presentsSinceOrbit <= ORBIT_GRACE_PRESENTS)
        s_presentsSinceOrbit++;

    const T_MOUSE_CAPTURE want =
        MouseCapture_Decide(MouseConfine, Window_IsFullscreen() ? 1 : 0, Console_IsOpen(), orbit);

    /* No sprite while orbiting (relative mode has no position to draw it at), while the
       console shows the host's cursor, or in play with the setting off. */
    SetMouseSpriteSuppressed(want == MOUSE_CAPTURE_RELATIVE || Console_IsOpen() || GamePointer_HiddenBySetting());
    if (s_applied && want == s_current)
        return;

    const bool relative = want == MOUSE_CAPTURE_RELATIVE;
    const bool grab = want != MOUSE_CAPTURE_FREE;
    if (!s_applied || relative != (s_current == MOUSE_CAPTURE_RELATIVE))
        SetWindowRelativeMouse(relative);
    if (!s_applied || grab != (s_current != MOUSE_CAPTURE_FREE))
        SetWindowMouseGrab(grab);
    s_current = want;
    s_applied = 1;
}

T_MOUSE_CAPTURE MouseCapture_Current(void) { return s_current; }

const char *MouseCapture_Name(T_MOUSE_CAPTURE mode) {
    switch (mode) {
    case MOUSE_CAPTURE_CONFINED:
        return "confined";
    case MOUSE_CAPTURE_RELATIVE:
        return "relative";
    default:
        return "free";
    }
}
