#include "MENU_KEYNAV.H"

#include <SYSTEM/KEYBOARD_KEYS.H>

S32 MenuKey_NumpadToNav(S32 key) {
    switch (key) {
    case K_NUMPAD_7:
        return K_HOME;
    case K_NUMPAD_8:
        return K_UP;
    case K_NUMPAD_9:
        return K_PAGE_UP;
    case K_NUMPAD_4:
        return K_LEFT;
    case K_NUMPAD_6:
        return K_RIGHT;
    case K_NUMPAD_1:
        return K_END;
    case K_NUMPAD_2:
        return K_DOWN;
    case K_NUMPAD_3:
        return K_PAGE_DOWN;
    case K_NUMPAD_0:
        return K_INSER;
    case K_NUMPAD_POINT:
        return K_SUPPR;
    case K_NUMPAD_ENTER:
        return K_ENTER;
    default:
        return key;
    }
}
