#include "MENU_INPUT.H"

#include <SYSTEM/DEFFILE.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include <stdio.h>
#include <string.h>

static const T_DEF_KEY s_keyboardDefaults[MENU_INPUT_COUNT] = {
    {K_UP, K_NUMPAD_8},
    {K_DOWN, K_NUMPAD_2},
    {K_LEFT, K_NUMPAD_4},
    {K_RIGHT, K_NUMPAD_6},
};
static const T_DEF_KEY s_gamepadDefaults[MENU_INPUT_COUNT] = {
    {K_GAMEPAD_LSTICK_UP, K_GAMEPAD_DPAD_UP},
    {K_GAMEPAD_LSTICK_DOWN, K_GAMEPAD_DPAD_DOWN},
    {K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_DPAD_LEFT},
    {K_GAMEPAD_LSTICK_RIGHT, K_GAMEPAD_DPAD_RIGHT},
};
static T_DEF_KEY s_keyboard[MENU_INPUT_COUNT] = {
    s_keyboardDefaults[0],
    s_keyboardDefaults[1],
    s_keyboardDefaults[2],
    s_keyboardDefaults[3],
};
static T_DEF_KEY s_gamepad[MENU_INPUT_COUNT] = {
    s_gamepadDefaults[0],
    s_gamepadDefaults[1],
    s_gamepadDefaults[2],
    s_gamepadDefaults[3],
};
static U32 s_held = 0;
static U32 s_input = 0;
static U32 s_suppressed = 0;

T_DEF_KEY *MenuInput_KeyboardBindings(void) {
    return s_keyboard;
}

T_DEF_KEY *MenuInput_GamepadBindings(void) {
    return s_gamepad;
}

void MenuInput_RestoreDefaults(S32 gamepad) {
    memcpy(gamepad ? s_gamepad : s_keyboard,
           gamepad ? s_gamepadDefaults : s_keyboardDefaults, sizeof s_keyboard);
}

void MenuInput_ReadConfig(void) {
    char key[40];
    for (S32 device = 0; device < 2; device++) {
        T_DEF_KEY *bindings = device ? s_gamepad : s_keyboard;
        const T_DEF_KEY *defaults = device ? s_gamepadDefaults : s_keyboardDefaults;
        for (S32 n = 0; n < MENU_INPUT_COUNT; n++) {
            snprintf(key, sizeof key, "Menu%s%d_1", device ? "Gamepad" : "Input", n);
            bindings[n].Key1 = DefFileBufferReadValueDefault(key, defaults[n].Key1);
            snprintf(key, sizeof key, "Menu%s%d_2", device ? "Gamepad" : "Input", n);
            bindings[n].Key2 = DefFileBufferReadValueDefault(key, defaults[n].Key2);
        }
    }
}

void MenuInput_WriteConfig(void) {
    char key[40];
    for (S32 device = 0; device < 2; device++) {
        const T_DEF_KEY *bindings = device ? s_gamepad : s_keyboard;
        for (S32 n = 0; n < MENU_INPUT_COUNT; n++) {
            snprintf(key, sizeof key, "Menu%s%d_1", device ? "Gamepad" : "Input", n);
            DefFileBufferWriteValue(key, bindings[n].Key1);
            snprintf(key, sizeof key, "Menu%s%d_2", device ? "Gamepad" : "Input", n);
            DefFileBufferWriteValue(key, bindings[n].Key2);
        }
    }
}

static S32 BindingHeld(const T_DEF_KEY *binding) {
    return (binding->Key1 && CheckKey(binding->Key1)) || (binding->Key2 && CheckKey(binding->Key2));
}

void MenuInput_Poll(S32 isolated) {
    s_held = 0;
    for (S32 n = 0; n < MENU_INPUT_COUNT; n++) {
        if (BindingHeld(&s_keyboard[n]) || BindingHeld(&s_gamepad[n]))
            s_held |= 1u << n;
    }
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
