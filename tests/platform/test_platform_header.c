#include <PLATFORM/PLATFORM.H>

int Platform_HeaderProbe(void) {
    PlatformEvent event = {0};
    PlatformFrame frame = {0};
    event.type = PLATFORM_EVENT_KEY_DOWN;
    event.key.scancode = PLATFORM_SCANCODE_A;
    frame.width = 8;
    return event.key.scancode == 4 && frame.width == 8 &&
           PLATFORM_SCANCODE_TO_KEYCODE(PLATFORM_SCANCODE_UP) == UINT32_C(0x40000052);
}
