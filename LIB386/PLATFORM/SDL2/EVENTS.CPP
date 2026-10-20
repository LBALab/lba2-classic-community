#include "PRIVATE.H"

#include <string.h>

bool SDL2_TranslateEvent(const SDL_Event *source, PlatformEvent *event) {
    if (!source || !event) {
        SDL_SetError("Invalid event translation arguments");
        return false;
    }
    memset(event, 0, sizeof(*event));
    switch (source->type) {
    case SDL_QUIT:
        event->type = PLATFORM_EVENT_QUIT;
        break;
    case SDL_WINDOWEVENT:
        switch (source->window.event) {
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            event->type = PLATFORM_EVENT_WINDOW_FOCUS_GAINED;
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            event->type = PLATFORM_EVENT_WINDOW_FOCUS_LOST;
            break;
        case SDL_WINDOWEVENT_ENTER:
            event->type = PLATFORM_EVENT_WINDOW_MOUSE_ENTER;
            break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            event->type = PLATFORM_EVENT_WINDOW_RESIZED;
            break;
        default:
            event->type = PLATFORM_EVENT_OTHER;
            break;
        }
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        event->type = source->type == SDL_KEYDOWN ? PLATFORM_EVENT_KEY_DOWN : PLATFORM_EVENT_KEY_UP;
        event->key.scancode = source->key.keysym.scancode;
        event->key.mod = source->key.keysym.mod;
        event->key.repeat = source->key.repeat != 0;
        break;
    case SDL_TEXTINPUT:
        event->type = PLATFORM_EVENT_TEXT_INPUT;
        event->text.text = source->text.text;
        break;
    case SDL_MOUSEMOTION:
        event->type = PLATFORM_EVENT_MOUSE_MOTION;
        event->motion.x = source->motion.x;
        event->motion.y = source->motion.y;
        event->motion.xrel = source->motion.xrel;
        event->motion.yrel = source->motion.yrel;
        event->motion.touch = source->motion.which == SDL_TOUCH_MOUSEID;
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        event->type = source->type == SDL_MOUSEBUTTONDOWN ? PLATFORM_EVENT_MOUSE_BUTTON_DOWN : PLATFORM_EVENT_MOUSE_BUTTON_UP;
        event->button.button = source->button.button;
        event->button.x = source->button.x;
        event->button.y = source->button.y;
        event->button.touch = source->button.which == SDL_TOUCH_MOUSEID;
        break;
    case SDL_MOUSEWHEEL:
        event->type = PLATFORM_EVENT_MOUSE_WHEEL;
        event->wheel.y = source->wheel.preciseY;
        break;
    case SDL_CONTROLLERDEVICEADDED:
        /* SDL2 adds by transient device index; the contract uses instance IDs. */
        {
            const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(source->cdevice.which);
            if (id < 0) {
                Platform_Log(PLATFORM_LOG_WARN, "SDL2 controller disappeared before delivery: %s", SDL_GetError());
                event->type = PLATFORM_EVENT_OTHER;
            } else {
                event->type = PLATFORM_EVENT_GAMEPAD_ADDED;
                event->gdevice.which = (uint32_t)id;
            }
        }
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        event->type = PLATFORM_EVENT_GAMEPAD_REMOVED;
        event->gdevice.which = (uint32_t)source->cdevice.which;
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        event->type = PLATFORM_EVENT_GAMEPAD_BUTTON_DOWN;
        event->gdevice.which = (uint32_t)source->cbutton.which;
        break;
    case SDL_CONTROLLERAXISMOTION:
        event->type = PLATFORM_EVENT_GAMEPAD_AXIS_MOTION;
        event->gaxis.which = (uint32_t)source->caxis.which;
        event->gaxis.axis = (PlatformGamepadAxis)source->caxis.axis;
        event->gaxis.value = source->caxis.value;
        break;
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION:
        event->type = source->type == SDL_FINGERDOWN ? PLATFORM_EVENT_FINGER_DOWN : source->type == SDL_FINGERUP ? PLATFORM_EVENT_FINGER_UP
                                                                                                                 : PLATFORM_EVENT_FINGER_MOTION;
        event->tfinger.fingerID = (uint64_t)source->tfinger.fingerId;
        event->tfinger.x = source->tfinger.x;
        event->tfinger.y = source->tfinger.y;
        break;
    default:
        event->type = PLATFORM_EVENT_OTHER;
        break;
    }
    return true;
}

bool SDL2_PollEvent(PlatformEvent *event) {
    /* SDL2 embeds text in the event; retain storage until the next poll. */
    static SDL_Event source;
    if (!SDL_PollEvent(&source))
        return false;
    SDL2_HandleWindowEvent(&source);
    return SDL2_TranslateEvent(&source, event);
}
