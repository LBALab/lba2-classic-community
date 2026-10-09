#include "PRIVATE.H"

#include <string.h>

extern "C" {

const uint8_t *SDL3_KeyboardState(void) {
    static uint8_t state[PLATFORM_SCANCODE_COUNT];
    int count = 0;
    const bool *native = SDL_GetKeyboardState(&count);
    memset(state, 0, sizeof(state));
    for (int i = 0; i < count && i < PLATFORM_SCANCODE_COUNT; ++i)
        state[i] = native[i] ? 1 : 0;
    return state;
}

uint32_t SDL3_Keycode(int32_t scancode, uint16_t modifiers) {
    return SDL_GetKeyFromScancode((SDL_Scancode)scancode, (SDL_Keymod)modifiers, true);
}

const char *SDL3_ScancodeName(int32_t scancode) {
    return SDL_GetScancodeName((SDL_Scancode)scancode);
}

uint16_t SDL3_Modifiers(void) { return (uint16_t)SDL_GetModState(); }

void SDL3_TextInput(bool enabled) {
    SDL_Window *window = SDL_GetKeyboardFocus();
    if (window == NULL)
        return;
    if (enabled)
        SDL_StartTextInput(window);
    else
        SDL_StopTextInput(window);
}

void SDL3_CursorVisible(bool visible) {
    if (visible)
        SDL_ShowCursor();
    else
        SDL_HideCursor();
}

bool SDL3_InitGamepads(void) { return SDL_InitSubSystem(SDL_INIT_GAMEPAD); }
void SDL3_EndGamepads(void) { SDL_QuitSubSystem(SDL_INIT_GAMEPAD); }

uint32_t *SDL3_Gamepads(int32_t *count) {
    int nativeCount = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&nativeCount);
    if (count)
        *count = nativeCount;
    return ids;
}

PlatformGamepad *SDL3_OpenGamepad(uint32_t id) {
    return (PlatformGamepad *)SDL_OpenGamepad(id);
}

void SDL3_CloseGamepad(PlatformGamepad *pad) { SDL_CloseGamepad((SDL_Gamepad *)pad); }
const char *SDL3_GamepadName(PlatformGamepad *pad) { return SDL_GetGamepadName((SDL_Gamepad *)pad); }

bool SDL3_GamepadButton(PlatformGamepad *pad, PlatformGamepadButton button) {
    static const SDL_GamepadButton buttons[PLATFORM_GAMEPAD_BUTTON_COUNT] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE,
        SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK,
        SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    if (button < 0 || button >= PLATFORM_GAMEPAD_BUTTON_COUNT) {
        Platform_Log(PLATFORM_LOG_ERROR, "Invalid gamepad button %d", (int)button);
        return false;
    }
    return SDL_GetGamepadButton((SDL_Gamepad *)pad, buttons[button]);
}

int16_t SDL3_GamepadAxis(PlatformGamepad *pad, PlatformGamepadAxis axis) {
    static const SDL_GamepadAxis axes[PLATFORM_GAMEPAD_AXIS_COUNT] = {
        SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
        SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER};
    if (axis < 0 || axis >= PLATFORM_GAMEPAD_AXIS_COUNT) {
        Platform_Log(PLATFORM_LOG_ERROR, "Invalid gamepad axis %d", (int)axis);
        return 0;
    }
    return SDL_GetGamepadAxis((SDL_Gamepad *)pad, axes[axis]);
}
}
