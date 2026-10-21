#include "C_EXTERN.H"

#include "FOLLOWCAM.H" /* FollowCamera: the right stick only drives the Auto camera */
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOG.H>

#include <PLATFORM/PLATFORM.H>

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stick-feel tunables. Cardinal-snap widens the band where the stick emits a
// pure cardinal (vs. an 8-way diagonal); hysteresis prevents frame-to-frame
// flicker when the stick angle sits on an octant boundary (which otherwise
// causes zig-zag under auto-camera).
static const float CARDINAL_SNAP_DEG = 25.0f;
static const float STICK_HYSTERESIS_DEG = 4.0f;

enum StickDir {
    DIR_NEUTRAL = 0,
    DIR_N,
    DIR_NE,
    DIR_E,
    DIR_SE,
    DIR_S,
    DIR_SW,
    DIR_W,
    DIR_NW,
};
static StickDir s_prevLeftDir = DIR_NEUTRAL;
static StickDir s_prevRightDir = DIR_NEUTRAL;

// Raw right-stick axes from the last UpdateJoystick poll, exposed via
// JoystickGetRightStick for the analog camera. 0,0 when no pad is open.
static S16 s_rstickX = 0;
static S16 s_rstickY = 0;
/* A pad the session is standing in for: a replayed one, or the `stick` command. */
static S32 s_virtualPad = 0;

//***************************************************************************
S32 JoystickDeadzone = LBA2_GAMEPAD_DEADZONE_DEFAULT;

static PlatformGamepad *s_gamepad = NULL;
static U32 s_gamepadId = 0;
static bool s_subsystemInit = false;

// Scancode of the first gamepad button/stick/trigger newly pressed this frame,
// or 0 if none. Refreshed by UpdateJoystick; consumed by MyGetInput to feed
// MyKey so "press any key" wait loops pick up gamepad input without any
// per-callsite changes.
static U32 s_firstPressedThisFrame = 0;

//***************************************************************************
// Mirror of KEYBOARD.CPP's bit-packing for virtual scancodes (>= 256).
// Writing into TabKeys here lets CheckKey() / GetInput() consume gamepad
// state with no special-casing in LIB386.
#define PAD_KEY_INDEX(x) (256 - 32 + ((x) >> 3))
#define PAD_KEY_BIT(x) (1 << ((x) & 7))

static inline void SetVirtualKeyDown(U32 scancode) {
    TabKeys[PAD_KEY_INDEX(scancode)] |= (U8)PAD_KEY_BIT(scancode);
}

// Clear the TabKeys bytes that hold gamepad bits. Called each frame before
// writing fresh state, because keyboard polling only clears TabKeys[0..255].
static inline void ClearGamepadBits(void) {
    U32 first = PAD_KEY_INDEX(K_GAMEPAD_BASE);
    U32 last = PAD_KEY_INDEX(K_GAMEPAD_BASE + K_GAMEPAD_COUNT - 1);
    for (U32 i = first; i <= last; i++) {
        TabKeys[i] = 0;
    }
}

//***************************************************************************
// Open the first available pad. Silent: at boot the structured "Joystick" line
// (InitAdeline) reports the device, and the runtime hot-plug handlers below log
// connect events — so this helper would only duplicate them.
static void OpenFirstAvailableGamepad(void) {
    if (s_gamepad != NULL) {
        return;
    }
    S32 count = 0;
    U32 *ids = g_platform.input.gamepads(&count);
    if (ids != NULL) {
        for (S32 i = 0; i < count; i++) {
            PlatformGamepad *pad = g_platform.input.open_gamepad(ids[i]);
            if (pad != NULL) {
                s_gamepad = pad;
                s_gamepadId = ids[i];
                break;
            }
        }
        g_platform.host.free_memory(ids);
    }
}

//***************************************************************************
void InitJoystick(void) {
    if (!s_subsystemInit) {
        if (!g_platform.input.init_gamepads()) {
            Log_Warn("Gamepad subsystem init failed: %s", g_platform.system.get_error());
            return;
        }
        s_subsystemInit = true;
    }
    OpenFirstAvailableGamepad();
}

//***************************************************************************
void EndJoystick(void) {
    if (s_gamepad != NULL) {
        g_platform.input.close_gamepad(s_gamepad);
        s_gamepad = NULL;
        s_gamepadId = 0;
    }
    if (s_subsystemInit) {
        g_platform.input.shutdown_gamepads();
        s_subsystemInit = false;
    }
}

//***************************************************************************
// Legacy entry point kept for existing call-sites (SCAN.CPP). Ensures the
// subsystem is up and the first available pad is opened.
void DetectJoys(void) {
    InitJoystick();
}

//***************************************************************************
void HandleEventsJoystick(const PlatformEvent *event) {
    const PlatformEvent *ev = event;
    switch (ev->type) {
    case PLATFORM_EVENT_GAMEPAD_ADDED:
        if (s_gamepad == NULL) {
            PlatformGamepad *pad = g_platform.input.open_gamepad(ev->gdevice.which);
            if (pad != NULL) {
                s_gamepad = pad;
                s_gamepadId = ev->gdevice.which;
                Log_Info("Gamepad connected: %s", g_platform.input.gamepad_name(pad));
            }
        }
        break;
    case PLATFORM_EVENT_GAMEPAD_REMOVED:
        if (s_gamepad != NULL && s_gamepadId == ev->gdevice.which) {
            g_platform.input.close_gamepad(s_gamepad);
            s_gamepad = NULL;
            s_gamepadId = 0;
            ClearGamepadBits();
            s_prevLeftDir = DIR_NEUTRAL;
            s_prevRightDir = DIR_NEUTRAL;
            // A device removal is not an input, so it leaves LastInputWasKeyboard
            // alone: it is already 0 from the pad's own activity, and the next
            // real key/mouse/pad event sets it correctly.
            OpenFirstAvailableGamepad();
            if (s_gamepad != NULL)
                Log_Info("Gamepad connected: %s", g_platform.input.gamepad_name(s_gamepad));
        }
        break;
    case PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN:
        LastInputWasKeyboard = 0;
        // The pad is in the player's hands: the touch overlay stands down until
        // a finger says otherwise.
        LastInputWasTouch = 0;
        break;
    case PLATFORM_EVENT_GAMEPAD_AXIS_MOTION:
        // Past-deadzone test: rest-state jitter on the sticks fires constant
        // axis events and would otherwise pin the flag forever.
        if (ev->gaxis.value > JoystickDeadzone || ev->gaxis.value < -JoystickDeadzone) {
            LastInputWasKeyboard = 0;
            LastInputWasTouch = 0;
        }
        break;
    default:
        break;
    }
}

//***************************************************************************
// Convert an analog stick position (SDL axis units, Y+ = down) into at most
// two virtual direction keys. Three-layer conversion:
//  1. Radial deadzone: magnitude < JoystickDeadzone → neutral.
//  2. Cardinal snap: within CARDINAL_SNAP_DEG of a cardinal, emit only that
//     cardinal (prevents accidental diagonals from slight off-axis tilt).
//  3. Hysteresis: the previous sector's band is widened by STICK_HYSTERESIS_DEG
//     so single-frame oscillation at an octant boundary doesn't flicker the
//     emitted direction (which otherwise causes zig-zag under auto-camera).
static void ApplyStickDirection(S16 x, S16 y, U32 keyLeft, U32 keyRight,
                                U32 keyUp, U32 keyDown, StickDir *prevState) {
    U32 mag2 = (U32)((S32)x * x) + (U32)((S32)y * y);
    U32 dz = (U32)JoystickDeadzone;
    if (mag2 < dz * dz) {
        *prevState = DIR_NEUTRAL;
        return;
    }

    float fx = (float)x;
    float fy = (float)y;
    // atan2(-y, x): 0° = east, 90° = north (SDL Y+ = down, so negate).
    float deg = atan2f(-fy, fx) * (180.0f / (float)M_PI);
    while (deg < 0.0f)
        deg += 360.0f;

    // Sectors: 0=E, 1=NE, 2=N, 3=NW, 4=W, 5=SW, 6=S, 7=SE. Center = i*45°.
    // Cardinals (even i) get CARDINAL_SNAP_DEG half-width; diagonals get the
    // remainder (45 - CARDINAL_SNAP_DEG). Sum = 360°.
    int sector = -1;

    // Check previous sector first with hysteresis widening.
    if (*prevState != DIR_NEUTRAL) {
        int prev = (int)(*prevState) - 1;
        bool isCard = (prev % 2) == 0;
        float halfWidth = isCard ? CARDINAL_SNAP_DEG : (45.0f - CARDINAL_SNAP_DEG);
        float diff = fabsf(deg - prev * 45.0f);
        if (diff > 180.0f)
            diff = 360.0f - diff;
        if (diff <= halfWidth + STICK_HYSTERESIS_DEG) {
            sector = prev;
        }
    }
    if (sector < 0) {
        for (int i = 0; i < 8; i++) {
            bool isCard = (i % 2) == 0;
            float halfWidth = isCard ? CARDINAL_SNAP_DEG : (45.0f - CARDINAL_SNAP_DEG);
            float diff = fabsf(deg - i * 45.0f);
            if (diff > 180.0f)
                diff = 360.0f - diff;
            if (diff <= halfWidth) {
                sector = i;
                break;
            }
        }
    }
    if (sector < 0) {
        *prevState = DIR_NEUTRAL;
        return;
    }

    switch (sector) {
    case 0:
        SetVirtualKeyDown(keyRight);
        break;
    case 1:
        SetVirtualKeyDown(keyRight);
        SetVirtualKeyDown(keyUp);
        break;
    case 2:
        SetVirtualKeyDown(keyUp);
        break;
    case 3:
        SetVirtualKeyDown(keyLeft);
        SetVirtualKeyDown(keyUp);
        break;
    case 4:
        SetVirtualKeyDown(keyLeft);
        break;
    case 5:
        SetVirtualKeyDown(keyLeft);
        SetVirtualKeyDown(keyDown);
        break;
    case 6:
        SetVirtualKeyDown(keyDown);
        break;
    case 7:
        SetVirtualKeyDown(keyRight);
        SetVirtualKeyDown(keyDown);
        break;
    }
    *prevState = (StickDir)(sector + 1);
}

//***************************************************************************
void UpdateJoystick(void) {
    // Snapshot previous gamepad bits before clearing, so we can diff after
    // polling to detect newly-pressed scancodes.
    U32 firstByte = PAD_KEY_INDEX(K_GAMEPAD_BASE);
    U32 lastByte = PAD_KEY_INDEX(K_GAMEPAD_BASE + K_GAMEPAD_COUNT - 1);
    U8 prevBits[8];
    for (U32 i = firstByte; i <= lastByte; i++) {
        prevBits[i - firstByte] = TabKeys[i];
    }

    ClearGamepadBits();
    s_firstPressedThisFrame = 0;
    s_rstickX = 0;
    s_rstickY = 0;

    if (s_gamepad == NULL) {
        return;
    }

    struct ButtonMap {
        PlatformGamepadButton btn;
        U32 key;
    };
    static const ButtonMap kButtons[] = {
        {PLATFORM_GAMEPAD_BUTTON_SOUTH, K_GAMEPAD_A},
        {PLATFORM_GAMEPAD_BUTTON_EAST, K_GAMEPAD_B},
        {PLATFORM_GAMEPAD_BUTTON_WEST, K_GAMEPAD_X},
        {PLATFORM_GAMEPAD_BUTTON_NORTH, K_GAMEPAD_Y},
        {PLATFORM_GAMEPAD_BUTTON_BACK, K_GAMEPAD_BACK},
        {PLATFORM_GAMEPAD_BUTTON_GUIDE, K_GAMEPAD_GUIDE},
        {PLATFORM_GAMEPAD_BUTTON_START, K_GAMEPAD_START},
        {PLATFORM_GAMEPAD_BUTTON_LEFT_STICK, K_GAMEPAD_LSTICK_BTN},
        {PLATFORM_GAMEPAD_BUTTON_RIGHT_STICK, K_GAMEPAD_RSTICK_BTN},
        {PLATFORM_GAMEPAD_BUTTON_LEFT_SHOULDER, K_GAMEPAD_LSHOULDER},
        {PLATFORM_GAMEPAD_BUTTON_RIGHT_SHOULDER, K_GAMEPAD_RSHOULDER},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_UP, K_GAMEPAD_DPAD_UP},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_DOWN, K_GAMEPAD_DPAD_DOWN},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_LEFT, K_GAMEPAD_DPAD_LEFT},
        {PLATFORM_GAMEPAD_BUTTON_DPAD_RIGHT, K_GAMEPAD_DPAD_RIGHT},
    };
    for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) {
        if (g_platform.input.gamepad_button(s_gamepad, kButtons[i].btn)) {
            SetVirtualKeyDown(kButtons[i].key);
        }
    }

    // The D-pad emits only its own K_GAMEPAD_DPAD_* scancodes (above), never
    // the left-stick virtual keys. That keeps the D-pad and left stick as
    // independent, separately bindable input sources (issue #360): whatever a
    // player binds to a D-pad direction in the controls menu is all it does, no
    // hidden aliasing onto the stick's cardinals.

    S16 lx = g_platform.input.gamepad_axis(s_gamepad, PLATFORM_GAMEPAD_AXIS_LEFTX);
    S16 ly = g_platform.input.gamepad_axis(s_gamepad, PLATFORM_GAMEPAD_AXIS_LEFTY);
    S16 rx = g_platform.input.gamepad_axis(s_gamepad, PLATFORM_GAMEPAD_AXIS_RIGHTX);
    S16 ry = g_platform.input.gamepad_axis(s_gamepad, PLATFORM_GAMEPAD_AXIS_RIGHTY);
    S16 lt = g_platform.input.gamepad_axis(s_gamepad, PLATFORM_GAMEPAD_AXIS_LEFT_TRIGGER);
    S16 rt = g_platform.input.gamepad_axis(s_gamepad, PLATFORM_GAMEPAD_AXIS_RIGHT_TRIGGER);

    // Cache the raw right stick for the analog camera (EXTFUNC reads it).
    s_rstickX = rx;
    s_rstickY = ry;

    ApplyStickDirection(lx, ly, K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_LSTICK_RIGHT,
                        K_GAMEPAD_LSTICK_UP, K_GAMEPAD_LSTICK_DOWN,
                        &s_prevLeftDir);

    // Right stick stays 8-way digital for bindings, EXCEPT while the analog
    // camera owns it (Auto camera in an exterior scene): there EXTFUNC reads the
    // raw axes, so emitting RSTICK scancodes too would double-drive elevation
    // and the classic camera turn.
    if (GamepadCameraAnalog && FollowCamera && CubeMode == CUBE_EXTERIEUR) {
        s_prevRightDir = DIR_NEUTRAL;
    } else {
        ApplyStickDirection(rx, ry, K_GAMEPAD_RSTICK_LEFT, K_GAMEPAD_RSTICK_RIGHT,
                            K_GAMEPAD_RSTICK_UP, K_GAMEPAD_RSTICK_DOWN,
                            &s_prevRightDir);
    }

    S16 dz = (S16)JoystickDeadzone;
    if (lt > dz)
        SetVirtualKeyDown(K_GAMEPAD_LTRIGGER);
    if (rt > dz)
        SetVirtualKeyDown(K_GAMEPAD_RTRIGGER);

    // Edge-detect: find the first bit that went 0 -> 1 this frame.
    for (U32 i = firstByte; i <= lastByte; i++) {
        U8 pressed = (U8)(TabKeys[i] & ~prevBits[i - firstByte]);
        if (pressed) {
            for (U32 b = 0; b < 8; b++) {
                if (pressed & (1u << b)) {
                    s_firstPressedThisFrame = ((i - (256 - 32)) << 3) | b;
                    return;
                }
            }
        }
    }
}

//***************************************************************************
U32 JoystickFirstPressedScancode(void) {
    return s_firstPressedThisFrame;
}

/* Session recording (docs/RECORDING.md). These three are derived in
   UpdateJoystick during GetJoys, which runs before GetInput reaches the replay
   hook, so a replayed TabKeys arrives too late to reconstruct them. */
void Joystick_SetReplayState(U32 firstPressed, S16 rsx, S16 rsy) {
    s_firstPressedThisFrame = firstPressed;
    Joystick_SetRightStick(rsx, rsy);
}

void Joystick_SetRightStick(S16 x, S16 y) {
    s_rstickX = x;
    s_rstickY = y;
}

/* Restoring the axes is not enough on its own: the analog camera asks whether a pad is
   there before it reads them, so a pad session replayed on a machine with none put the
   values back and then skipped the code that reads them. Presence is a property of the
   session, so a recording carries it and a replay reports what the recording had. */
void Joystick_SetVirtualPad(S32 present) {
    s_virtualPad = present ? 1 : 0;
}

//***************************************************************************
// True if a gamepad *button* was newly pressed this frame -- face buttons,
// Start/Back/Guide, shoulders, D-pad, or a stick-click. Excludes the analog
// stick directions and the triggers: those rest/drift past the deadzone and
// would trip a "press any button" prompt (e.g. the credits skip) by accident.
// Reads the edge-detected first-pressed scancode, so poll the pad
// (UpdateJoystick/GetJoys) earlier in the same frame before calling this.
S32 JoystickButtonPressed(void) {
    switch (s_firstPressedThisFrame) {
    case 0: // nothing newly pressed
    case K_GAMEPAD_LSTICK_UP:
    case K_GAMEPAD_LSTICK_DOWN:
    case K_GAMEPAD_LSTICK_LEFT:
    case K_GAMEPAD_LSTICK_RIGHT:
    case K_GAMEPAD_RSTICK_UP:
    case K_GAMEPAD_RSTICK_DOWN:
    case K_GAMEPAD_RSTICK_LEFT:
    case K_GAMEPAD_RSTICK_RIGHT:
    case K_GAMEPAD_LTRIGGER:
    case K_GAMEPAD_RTRIGGER:
        return 0;
    default:
        return 1;
    }
}

//***************************************************************************
S32 JoystickIsPresent(void) {
    return (s_gamepad != NULL || s_virtualPad) ? 1 : 0;
}

//***************************************************************************
// Raw right-stick axes (SDL units, -32767..32767) from the last poll. 0,0 when
// no pad is open. The analog camera (EXTFUNC) reads these when it owns the stick.
void JoystickGetRightStick(S16 *x, S16 *y) {
    if (x != NULL)
        *x = s_rstickX;
    if (y != NULL)
        *y = s_rstickY;
}

//***************************************************************************
// Controller name for the boot log, or "not detected" when none is open.
const char *JoystickGetName(void) {
    if (s_gamepad == NULL)
        return "not detected";
    const char *name = g_platform.input.gamepad_name(s_gamepad);
    return (name != NULL) ? name : "unknown controller";
}

//***************************************************************************
// Legacy entry point: the call-site INPUT.CPP:170 passes &TabKeys[256], the
// base of the extended region. UpdateJoystick writes directly into TabKeys
// via the bit-packed virtual-scancode layout, so the bitfield parameter is
// unused — we keep the signature for source compatibility.
void GetJoys(U32 *bitfield) {
    (void)bitfield;
    UpdateJoystick();
}

#ifdef __cplusplus
}
#endif

//***************************************************************************
