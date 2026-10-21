#include "PRIVATE.H"

#include <string.h>

extern "C" {

bool SDL3_InitEvents(void) { return SDL_InitSubSystem(SDL_INIT_EVENTS); }
void SDL3_EndEvents(void) { SDL_QuitSubSystem(SDL_INIT_EVENTS); }

bool SDL3_TranslateEvent(const SDL_Event *source, PlatformEvent *event) {
    if (source == NULL || event == NULL) {
        SDL_SetError("Invalid event translation arguments");
        return false;
    }
    memset(event, 0, sizeof(*event));
    switch (source->type) {
    case SDL_EVENT_QUIT:
        event->type = PLATFORM_EVENT_QUIT;
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        event->type = PLATFORM_EVENT_WINDOW_FOCUS_GAINED;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        event->type = PLATFORM_EVENT_WINDOW_FOCUS_LOST;
        break;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        event->type = PLATFORM_EVENT_WINDOW_MOUSE_ENTER;
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        event->type = PLATFORM_EVENT_WINDOW_RESIZED;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        event->type = source->type == SDL_EVENT_KEY_DOWN ? PLATFORM_EVENT_KEY_DOWN : PLATFORM_EVENT_KEY_UP;
        event->key.scancode = (int32_t)source->key.scancode;
        event->key.mod = (uint16_t)source->key.mod;
        event->key.repeat = source->key.repeat;
        break;
    case SDL_EVENT_TEXT_INPUT:
        event->type = PLATFORM_EVENT_TEXT_INPUT;
        event->text.text = source->text.text;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        event->type = PLATFORM_EVENT_MOUSE_MOTION;
        event->motion.x = source->motion.x;
        event->motion.y = source->motion.y;
        event->motion.xrel = source->motion.xrel;
        event->motion.yrel = source->motion.yrel;
        event->motion.touch = source->motion.which == SDL_TOUCH_MOUSEID;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        event->type = source->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PLATFORM_EVENT_MOUSE_BUTTON_DOWN : PLATFORM_EVENT_MOUSE_BUTTON_UP;
        event->button.button = source->button.button;
        event->button.x = source->button.x;
        event->button.y = source->button.y;
        event->button.touch = source->button.which == SDL_TOUCH_MOUSEID;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        event->type = PLATFORM_EVENT_MOUSE_WHEEL;
        event->wheel.y = source->wheel.y;
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
        event->type = source->type == SDL_EVENT_GAMEPAD_ADDED ? PLATFORM_EVENT_GAMEPAD_ADDED : PLATFORM_EVENT_GAMEPAD_REMOVED;
        event->gdevice.which = source->gdevice.which;
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        event->type = PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN;
        event->gdevice.which = source->gbutton.which;
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        event->type = PLATFORM_EVENT_GAMEPAD_AXIS_MOTION;
        event->gaxis.which = source->gaxis.which;
        event->gaxis.axis = (PlatformGamepadAxis)source->gaxis.axis;
        event->gaxis.value = source->gaxis.value;
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION: {
        event->type = source->type == SDL_EVENT_FINGER_DOWN ? PLATFORM_EVENT_FINGER_DOWN : source->type == SDL_EVENT_FINGER_UP ? PLATFORM_EVENT_FINGER_UP
                                                                                                                               : PLATFORM_EVENT_FINGER_MOTION;
        event->tfinger.fingerID = source->tfinger.fingerID;
        event->tfinger.x = source->tfinger.x;
        event->tfinger.y = source->tfinger.y;
        const SDL_TouchDeviceType device = SDL_GetTouchDeviceType(source->tfinger.touchID);
        event->tfinger.indirect = device == SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE || device == SDL_TOUCH_DEVICE_INDIRECT_RELATIVE;
        break;
    }
    default:
        event->type = PLATFORM_EVENT_OTHER;
        break;
    }
    return true;
}

bool SDL3_PollEvent(PlatformEvent *event) {
    SDL_Event source;
    if (!SDL_PollEvent(&source))
        return false;
    SDL3_HandleWindowEvent(&source);
    return SDL3_TranslateEvent(&source, event);
}
}
