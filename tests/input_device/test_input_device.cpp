/* Host-only test for the save-menu input-device classification.
 *
 * The save menu (ChoosePlayerName, SOURCES/GAMEMENU.CPP) offers keyboard text
 * entry only when the last input came from a physical keyboard; every other
 * device (mouse, gamepad, touch) routes to the auto-name "controller" flow.
 * That decision reads one last-input-wins flag, LastInputWasKeyboard, kept up
 * to date by the SDL event handlers. This test drives the real
 * HandleEventsKeyboard / HandleEventsMouse with synthesized SDL events and
 * asserts the flag lands where the save menu expects.
 *
 * Gamepad (SOURCES/JOYSTICK.CPP) and touch (SOURCES/TOUCH_INPUT.CPP) clear the
 * same flag the same way the mouse handler does here; their end-to-end save
 * flow is verified on-device. The Android-TV guard (IsAndroidTVDevice) is a
 * no-op stub off Android, so it does not affect this host classification.
 *
 * It also covers the sibling flag LastInputWasTouch, which the touch overlay
 * reads to decide whether to draw itself. The case worth a test is the one that
 * looks like a bug: the mouse handler must NOT clear it. SDL synthesises mouse
 * events from touch by default, so a mouse-side clear would cancel every touch
 * the instant it arrived and the overlay would never appear on a phone.
 */
#include <SYSTEM/EVENTS.H>   // ManageEvents (stubbed below)
#include <SYSTEM/KEYBOARD.H> // HandleEventsKeyboard
#include <SYSTEM/MOUSE.H>    // HandleEventsMouse
#include <SYSTEM/WINDOW.H>   // WindowToSurfaceCoords (stubbed below)

#include <SDL3/SDL.h>

#include <cstdio>

extern "C" S32 LastInputWasKeyboard; // defined in LIB386/SYSTEM/KEYBOARD.CPP
                                     // (LastInputWasTouch comes from KEYBOARD.H)

// --- Link stubs: symbols the handler TUs reference on paths this test never
// drives, provided so the two real handlers link standalone. ---
void ManageEvents() {}
void WindowToSurfaceCoords(S32 wx, S32 wy, S32 *sx, S32 *sy) {
    *sx = wx;
    *sy = wy;
}
extern "C" {
U8 BinGphMouse[4096] = {0};
}
U32 ModeDesiredX = 640; // MOUSE.CPP SetMousePos clamp (unreached path here)
U32 ModeDesiredY = 480;

static int fails = 0;

static void expect(S32 got, S32 want, const char *label) {
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s: LastInputWasKeyboard=%d (want %d)\n", label, got, want);
        fails++;
    }
}

static void expectTouch(S32 want, const char *label) {
    if (LastInputWasTouch != want) {
        std::fprintf(stderr, "FAIL: %s: LastInputWasTouch=%d (want %d)\n", label,
                     LastInputWasTouch, want);
        fails++;
    }
}

static SDL_Event mkEvent(Uint32 type) {
    SDL_Event e;
    SDL_memset(&e, 0, sizeof e);
    e.type = type;
    return e;
}

int main() {
    SDL_Event keyDown = mkEvent(SDL_EVENT_KEY_DOWN);
    SDL_Event keyUp = mkEvent(SDL_EVENT_KEY_UP);
    SDL_Event mouseBtn = mkEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
    SDL_Event mouseWheel = mkEvent(SDL_EVENT_MOUSE_WHEEL);

    // Default: not a keyboard, so a touch-only phone / fresh boot routes to the
    // auto-name flow rather than a keyboard prompt nothing can fill.
    expect(LastInputWasKeyboard, 0, "default is 0 (not keyboard)");

    // Physical key-down => keyboard path.
    HandleEventsKeyboard(&keyDown);
    expect(LastInputWasKeyboard, 1, "key-down -> keyboard");

    // Mouse activity => not keyboard. This is the deliberate desktop behavior
    // change: a mouse no longer offers typed save names.
    HandleEventsMouse(&mouseBtn);
    expect(LastInputWasKeyboard, 0, "mouse button -> not keyboard");

    HandleEventsKeyboard(&keyDown);
    HandleEventsMouse(&mouseWheel);
    expect(LastInputWasKeyboard, 0, "mouse wheel -> not keyboard");

    // Last-input-wins: a key-down reclaims the keyboard path after the mouse.
    HandleEventsKeyboard(&keyDown);
    expect(LastInputWasKeyboard, 1, "key-down after mouse -> keyboard");

    // Only a KEY_DOWN flips it; a key-up (or any non-down event) leaves it.
    HandleEventsMouse(&mouseBtn); // -> 0
    HandleEventsKeyboard(&keyUp); // not a down event
    expect(LastInputWasKeyboard, 0, "key-up does not set keyboard");

    // --- LastInputWasTouch -------------------------------------------------
    // The overlay draws only while touch is the last device used, so what
    // clears this flag decides whether it is ever on screen.
    expectTouch(0, "touch flag default is 0");

    // A real key means the player moved to a keyboard: the overlay stands down.
    LastInputWasTouch = 1;
    HandleEventsKeyboard(&keyDown);
    expectTouch(0, "key-down -> not touch");

    // ...but only a key-DOWN, on the same terms as the flag above.
    LastInputWasTouch = 1;
    HandleEventsKeyboard(&keyUp);
    expectTouch(1, "key-up leaves touch alone");

    // The one that would look like a tidy-up and is not. SDL turns touches into
    // mouse events, so clearing here would undo the touch that produced them and
    // the overlay would never appear. Both mouse paths must leave it set.
    LastInputWasTouch = 1;
    HandleEventsMouse(&mouseBtn);
    expectTouch(1, "mouse button does NOT clear touch (SDL synthesises it from touch)");

    LastInputWasTouch = 1;
    HandleEventsMouse(&mouseWheel);
    expectTouch(1, "mouse wheel does NOT clear touch");

    LastInputWasTouch = 0;

    if (fails == 0) {
        std::printf("test_input_device: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_device: %d failure(s)\n", fails);
    return 1;
}
