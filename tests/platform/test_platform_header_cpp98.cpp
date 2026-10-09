#include <stdint.h>

/* C++98 implementations may omit the C99 integer-constant macros. */
#undef UINT32_C

#include <PLATFORM/PLATFORM.H>

typedef char Platform_KeycodeMaskCheck[PLATFORM_KEYCODE_SCANCODE_MASK == 0x40000000u ? 1 : -1];
typedef char Platform_KeycodeValueCheck[PLATFORM_KEYCODE_UP == 0x40000052u ? 1 : -1];
typedef char Platform_KeycodeConversionCheck[PLATFORM_SCANCODE_TO_KEYCODE(PLATFORM_SCANCODE_UP) == 0x40000052u ? 1 : -1];
