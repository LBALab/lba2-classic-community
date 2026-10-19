#include "MENU_INPUT.H"

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

static U32 s_held = 0;
static U32 s_input = 0;
static U32 s_suppressed = 0;

void MenuInput_Poll(S32 isolated) {
    s_held = 0;
    if (CheckKey(K_UP) || CheckKey(K_NUMPAD_8) ||
        CheckKey(K_GAMEPAD_LSTICK_UP) || CheckKey(K_GAMEPAD_DPAD_UP))
        s_held |= MENU_INPUT_UP;
    if (CheckKey(K_DOWN) || CheckKey(K_NUMPAD_2) ||
        CheckKey(K_GAMEPAD_LSTICK_DOWN) || CheckKey(K_GAMEPAD_DPAD_DOWN))
        s_held |= MENU_INPUT_DOWN;
    if (CheckKey(K_LEFT) || CheckKey(K_NUMPAD_4) ||
        CheckKey(K_GAMEPAD_LSTICK_LEFT) || CheckKey(K_GAMEPAD_DPAD_LEFT))
        s_held |= MENU_INPUT_LEFT;
    if (CheckKey(K_RIGHT) || CheckKey(K_NUMPAD_6) ||
        CheckKey(K_GAMEPAD_LSTICK_RIGHT) || CheckKey(K_GAMEPAD_DPAD_RIGHT))
        s_held |= MENU_INPUT_RIGHT;
    s_suppressed &= s_held;
    if (isolated) {
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
