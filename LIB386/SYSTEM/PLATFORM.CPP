//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/PLATFORM.H>

//──────────────────────────────────────────────────────────────────────────
// Android defines __linux__ as well, and Apple's platforms define neither of
// the two, so the order matters: the most specific test wins.
const char *Sys_PlatformName(void) {
#if defined(_WIN32)
    return "windows";
#elif defined(__ANDROID__)
    return "android";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}
