#include "C_EXTERN.H"

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOGPRINT.H>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>

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

//***************************************************************************
S32 JoystickDeadzone = 8000;

static SDL_Gamepad *s_gamepad = NULL;
static SDL_JoystickID s_gamepadId = 0;
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
static void OpenFirstAvailableGamepad(void) {
    if (s_gamepad != NULL) {
        return;
    }
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (ids != NULL) {
        for (int i = 0; i < count; i++) {
            SDL_Gamepad *pad = SDL_OpenGamepad(ids[i]);
            if (pad != NULL) {
                s_gamepad = pad;
                s_gamepadId = ids[i];
                LogPrintf("Gamepad connected: %s\n", SDL_GetGamepadName(pad));
                break;
            }
        }
        SDL_free(ids);
    }
}

//***************************************************************************
void InitJoystick(void) {
    if (!s_subsystemInit) {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            LogPrintf("Warning: SDL_INIT_GAMEPAD failed: %s\n", SDL_GetError());
            return;
        }
        s_subsystemInit = true;
    }
    OpenFirstAvailableGamepad();
}

//***************************************************************************
void EndJoystick(void) {
    if (s_gamepad != NULL) {
        SDL_CloseGamepad(s_gamepad);
        s_gamepad = NULL;
        s_gamepadId = 0;
    }
    if (s_subsystemInit) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
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
void HandleEventsJoystick(const void *event) {
    const SDL_Event *ev = (const SDL_Event *)event;
    switch (ev->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        if (s_gamepad == NULL) {
            SDL_Gamepad *pad = SDL_OpenGamepad(ev->gdevice.which);
            if (pad != NULL) {
                s_gamepad = pad;
                s_gamepadId = ev->gdevice.which;
                LogPrintf("Gamepad hot-plugged: %s\n", SDL_GetGamepadName(pad));
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (s_gamepad != NULL && s_gamepadId == ev->gdevice.which) {
            SDL_CloseGamepad(s_gamepad);
            s_gamepad = NULL;
            s_gamepadId = 0;
            ClearGamepadBits();
            s_prevLeftDir = DIR_NEUTRAL;
            s_prevRightDir = DIR_NEUTRAL;
            OpenFirstAvailableGamepad();
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
    float fx = (float)x;
    float fy = (float)y;
    float mag2 = fx * fx + fy * fy;
    float dz = (float)JoystickDeadzone;
    if (mag2 < dz * dz) {
        *prevState = DIR_NEUTRAL;
        return;
    }

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

    if (s_gamepad == NULL) {
        return;
    }

    struct ButtonMap {
        SDL_GamepadButton btn;
        U32 key;
    };
    static const ButtonMap kButtons[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, K_GAMEPAD_A},
        {SDL_GAMEPAD_BUTTON_EAST, K_GAMEPAD_B},
        {SDL_GAMEPAD_BUTTON_WEST, K_GAMEPAD_X},
        {SDL_GAMEPAD_BUTTON_NORTH, K_GAMEPAD_Y},
        {SDL_GAMEPAD_BUTTON_BACK, K_GAMEPAD_BACK},
        {SDL_GAMEPAD_BUTTON_GUIDE, K_GAMEPAD_GUIDE},
        {SDL_GAMEPAD_BUTTON_START, K_GAMEPAD_START},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, K_GAMEPAD_LSTICK_BTN},
        {SDL_GAMEPAD_BUTTON_RIGHT_STICK, K_GAMEPAD_RSTICK_BTN},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, K_GAMEPAD_LSHOULDER},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, K_GAMEPAD_RSHOULDER},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, K_GAMEPAD_DPAD_UP},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, K_GAMEPAD_DPAD_DOWN},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, K_GAMEPAD_DPAD_LEFT},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, K_GAMEPAD_DPAD_RIGHT},
    };
    for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) {
        if (SDL_GetGamepadButton(s_gamepad, kButtons[i].btn)) {
            SetVirtualKeyDown(kButtons[i].key);
        }
    }

    // D-pad also lights up the LSTICK directional virtual keys, so bindings
    // that target the left stick cover the D-pad for free (standard action-game
    // convention where stick and D-pad are interchangeable for movement).
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))
        SetVirtualKeyDown(K_GAMEPAD_LSTICK_UP);
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
        SetVirtualKeyDown(K_GAMEPAD_LSTICK_DOWN);
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
        SetVirtualKeyDown(K_GAMEPAD_LSTICK_LEFT);
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
        SetVirtualKeyDown(K_GAMEPAD_LSTICK_RIGHT);

    S16 lx = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    S16 ly = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    S16 rx = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    S16 ry = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
    S16 lt = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    S16 rt = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    ApplyStickDirection(lx, ly, K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_LSTICK_RIGHT,
                        K_GAMEPAD_LSTICK_UP, K_GAMEPAD_LSTICK_DOWN,
                        &s_prevLeftDir);
    ApplyStickDirection(rx, ry, K_GAMEPAD_RSTICK_LEFT, K_GAMEPAD_RSTICK_RIGHT,
                        K_GAMEPAD_RSTICK_UP, K_GAMEPAD_RSTICK_DOWN,
                        &s_prevRightDir);

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

//***************************************************************************
S32 JoystickIsPresent(void) {
    return (s_gamepad != NULL) ? 1 : 0;
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

//***************************************************************************
S32 JoystickMenuAction(void) {
    if (s_gamepad == NULL) {
        return 0;
    }
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_SOUTH) ||
        SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_START)) {
        return K_ENTER;
    }
    return JoystickMenuNavOnly();
}

//***************************************************************************
S32 JoystickMenuNavOnly(void) {
    if (s_gamepad == NULL) {
        return 0;
    }
    S16 lx = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    S16 ly = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    S16 dz = (S16)JoystickDeadzone;
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP) || ly < -dz)
        return K_UP;
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ly > dz)
        return K_DOWN;
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || lx < -dz)
        return K_LEFT;
    if (SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || lx > dz)
        return K_RIGHT;
    return 0;
}

#ifdef __cplusplus
}
#endif

//***************************************************************************
