#include "MENU_INPUT.H"

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

static U32 s_held = 0;
static U32 s_input = 0;
static U32 s_suppressed = 0;
static S32 s_replaySample = -1;

U32 MenuInput_Capture(S32 isolated, U32 reservedKey) {
    static const U32 keys[MENU_INPUT_COUNT][4] = {
        {K_UP, K_NUMPAD_8, K_GAMEPAD_LSTICK_UP, K_GAMEPAD_DPAD_UP},
        {K_DOWN, K_NUMPAD_2, K_GAMEPAD_LSTICK_DOWN, K_GAMEPAD_DPAD_DOWN},
        {K_LEFT, K_NUMPAD_4, K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_DPAD_LEFT},
        {K_RIGHT, K_NUMPAD_6, K_GAMEPAD_LSTICK_RIGHT, K_GAMEPAD_DPAD_RIGHT}};
    U32 sample = isolated ? MENU_INPUT_ISOLATED : 0;
    for (S32 direction = 0; direction < MENU_INPUT_COUNT; ++direction)
        for (S32 key = 0; key < 4; ++key)
            if (keys[direction][key] != reservedKey && CheckKey(keys[direction][key]))
                sample |= 1u << direction;
    return sample;
}

void MenuInput_SetReplaySample(S32 sample) {
    s_replaySample = sample;
}

void MenuInput_Poll(S32 isolated) {
    U32 sample = s_replaySample >= 0 ? (U32)s_replaySample : MenuInput_Capture(isolated, 0);
    s_held = sample & MENU_INPUT_ALL;
    s_suppressed &= s_held;
    if (sample & MENU_INPUT_ISOLATED) {
        s_suppressed |= s_held;
        s_held = s_input = 0;
        return;
    }
    s_input = s_held & ~s_suppressed;
}

U32 MenuInput_Get(void) {
    return s_input;
}

U32 MenuInput_Held(void) {
    return s_held;
}

void MenuInput_Suppress(U32 directions) {
    s_suppressed |= directions;
    s_input &= ~directions;
}

U32 MenuInput_Suppressed(void) {
    return s_suppressed;
}

void MenuInput_Reset(U32 suppressed) {
    s_input = s_held = 0;
    s_suppressed = suppressed & MENU_INPUT_ALL;
}

S32 MenuInput_NavigationKey(void) {
    if (s_input & MENU_INPUT_UP)
        return K_UP;
    if (s_input & MENU_INPUT_DOWN)
        return K_DOWN;
    if (s_input & MENU_INPUT_LEFT)
        return K_LEFT;
    if (s_input & MENU_INPUT_RIGHT)
        return K_RIGHT;
    return 0;
}
