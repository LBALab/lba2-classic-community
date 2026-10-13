#include <SYSTEM/EVENTS.H>

#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/WINDOW.H>

#include <PLATFORM/PLATFORM.H>
#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// Implemented in SOURCES/JOYSTICK.CPP. Forward-declared here so LIB386 does
// not pull in game-side headers; the symbol is resolved at final link time.
// A weak no-op default lets ASM-equivalence test binaries link without
// dragging in the full gamepad translation unit.
void HandleEventsJoystick(const PlatformEvent *event);
__attribute__((weak)) void HandleEventsJoystick(const PlatformEvent *event) {
    (void)event;
}

// Touch / virtual gamepad handler — implemented in SOURCES/TOUCH_INPUT.CPP.
// Weak no-op default so the engine links without the touch module.
S32 HandleEventsTouch(const PlatformEvent *event);
__attribute__((weak)) S32 HandleEventsTouch(const PlatformEvent *event) {
    (void)event;
    return 0;
}

// --- Private state -----------------------------------------------------------
bool eventSystemInitialized = false;
static EventFilterFn s_eventFilter = NULL;

// --- Initialization ----------------------------------------------------------
bool InitEvents() {
    assert(eventSystemInitialized == false);

    if (!g_platform.events.init()) {
        Log_Error("Event system init failed: %s", g_platform.system.get_error());
        eventSystemInitialized = false;
        return false;
    }

    eventSystemInitialized = true;
    return true;
}

void EndEvents() {
    if (eventSystemInitialized) {
        g_platform.events.shutdown();
        eventSystemInitialized = false;
    }
}

// --- Interface ---------------------------------------------------------------
void SetEventFilter(EventFilterFn fn) {
    s_eventFilter = fn;
}

void ManageEvents() {
    assert(eventSystemInitialized == true);

    g_platform.events.pump();
    {
        PlatformEvent event;
        while (g_platform.events.poll(&event)) {
            if (s_eventFilter && s_eventFilter(&event))
                continue;

            HandleEventsTimer(&event);
            HandleEventsMouse(&event);
            HandleEventsKeyboard(&event);
            HandleEventsJoystick(&event);
            if (!HandleEventsTouch(&event)) {
                HandleEventsVideo(&event);
            }
            HandleEventsWindow(&event);
        }
    }

    // TODO: Pump audio events (refer to WINSYS.CPP)
}

// =============================================================================
#ifdef __cplusplus
}
#endif
