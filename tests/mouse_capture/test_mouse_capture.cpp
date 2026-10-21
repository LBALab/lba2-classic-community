/* Host test for the mouse capture rule (SOURCES/MOUSE_CAPTURE.CPP).
 *
 * The window, the console and the setting are stubbed, so the test drives
 * MouseCapture_Update the way the present hook does and counts what reaches
 * the platform. Two things matter most: relative mode ends on its own when the
 * camera stops asking (a menu or an interior never asks), and nothing is
 * re-applied on a frame where the mode did not change.
 */
#include "MOUSE_CAPTURE.H"

#include <cstdio>

static bool s_fullscreen = false;
static int s_consoleOpen = 0;
static bool s_relative = false;
static bool s_grab = false;
static int s_relativeCalls = 0;
static int s_grabCalls = 0;

// Stand-ins for the window and the console (C linkage, as declared).
extern "C" {
bool Window_IsFullscreen(void) { return s_fullscreen; }
void SetWindowRelativeMouse(bool enabled) {
    s_relative = enabled;
    s_relativeCalls++;
}
void SetWindowMouseGrab(bool enabled) {
    s_grab = enabled;
    s_grabCalls++;
}
int Console_IsOpen(void) { return s_consoleOpen; }
}

static int fails = 0;

static void expect(bool ok, const char *label) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        fails++;
    }
}

static void expectMode(T_MOUSE_CAPTURE want, const char *label) {
    T_MOUSE_CAPTURE got = MouseCapture_Current();
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s: %s (want %s)\n", label, MouseCapture_Name(got), MouseCapture_Name(want));
        fails++;
    }
    expect(s_relative == (want == MOUSE_CAPTURE_RELATIVE), "relative mode matches the mode");
    expect(s_grab == (want != MOUSE_CAPTURE_FREE), "grab matches the mode");
}

static void testRule(void) {
    expect(MouseCapture_Decide(MOUSE_CONFINE_NEVER, 1, 0, 0) == MOUSE_CAPTURE_FREE, "never: free in fullscreen");
    expect(MouseCapture_Decide(MOUSE_CONFINE_FULLSCREEN, 1, 0, 0) == MOUSE_CAPTURE_CONFINED, "fullscreen: confined");
    expect(MouseCapture_Decide(MOUSE_CONFINE_FULLSCREEN, 0, 0, 0) == MOUSE_CAPTURE_FREE, "fullscreen: free in a window");
    expect(MouseCapture_Decide(MOUSE_CONFINE_ALWAYS, 0, 0, 0) == MOUSE_CAPTURE_CONFINED, "always: confined in a window");
    expect(MouseCapture_Decide(MOUSE_CONFINE_NEVER, 0, 0, 1) == MOUSE_CAPTURE_RELATIVE, "an orbit is relative whatever the setting");
    expect(MouseCapture_Decide(MOUSE_CONFINE_ALWAYS, 1, 1, 1) == MOUSE_CAPTURE_CONFINED, "the console never gets relative");
    expect(MouseCapture_Decide(MOUSE_CONFINE_NEVER, 1, 1, 1) == MOUSE_CAPTURE_FREE, "console with confine off: free");
}

static void testUpdate(void) {
    MouseConfine = MOUSE_CONFINE_FULLSCREEN;
    s_fullscreen = true;

    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_CONFINED, "first present in fullscreen");
    const int relativeCalls = s_relativeCalls, grabCalls = s_grabCalls;
    MouseCapture_Update();
    expect(s_relativeCalls == relativeCalls && s_grabCalls == grabCalls, "an unchanged mode is not re-applied");

    // The camera orbits on each gameplay frame.
    for (int i = 0; i < 5; i++) {
        MouseCapture_RequestOrbit();
        MouseCapture_Update();
        expectMode(MOUSE_CAPTURE_RELATIVE, "orbiting");
    }

    // A present with no gameplay frame in between keeps it...
    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_RELATIVE, "one present without a request keeps relative");
    // ...the second one ends it: a menu or an interior that never asks.
    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_CONFINED, "relative ends when the camera stops asking");

    // Leaving fullscreen frees the cursor on the next present.
    s_fullscreen = false;
    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_FREE, "windowed with confine 1");

    // Above the step rate most gameplay frames skip the simulation, where the
    // camera asks; the loop carries the request over them. Eight frames a step
    // must keep relative mode throughout, with nothing re-applied.
    MouseCapture_RequestOrbit();
    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_RELATIVE, "orbit before the skipped frames");
    {
        const int before = s_relativeCalls;
        for (int step = 0; step < 4; step++) {
            for (int frame = 0; frame < 7; frame++) {
                MouseCapture_CarryOrbit();
                MouseCapture_Update();
                expectMode(MOUSE_CAPTURE_RELATIVE, "a frame that skips the simulation keeps the orbit");
            }
            MouseCapture_RequestOrbit();
            MouseCapture_Update();
        }
        expect(s_relativeCalls == before, "relative mode is not toggled between steps");
    }
    // A menu's own loop does not carry: the second present ends it as before.
    MouseCapture_Update();
    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_FREE, "relative ends once frames stop carrying");
    // Carrying makes no request of its own.
    MouseCapture_CarryOrbit();
    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_FREE, "a carry with no orbit under way does nothing");

    // The console while orbiting gets a cursor back.
    MouseConfine = MOUSE_CONFINE_ALWAYS;
    s_consoleOpen = 1;
    MouseCapture_RequestOrbit();
    MouseCapture_Update();
    expectMode(MOUSE_CAPTURE_CONFINED, "console open while orbiting");
    s_consoleOpen = 0;
}

int main() {
    testRule();
    testUpdate();
    if (fails == 0) {
        std::printf("test_mouse_capture: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_mouse_capture: %d failure(s)\n", fails);
    return 1;
}
