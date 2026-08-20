//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_PLATFORM
#define LIB_SYSTEM_PLATFORM

//──────────────────────────────────────────────────────────────────────────
// Which platform this build is for, as a name a caller can print or record.
//
// It lives here so that callers do not have to ask. The engine's rule is that
// platform conditionals stay in the platform layer (CODESTYLE.md "Layers", and
// docs/PLATFORM.md for the seam), and a caller that wants to write the platform
// into a file or a banner would otherwise carry its own #ifdef chain to do it.

#ifdef __cplusplus
extern "C" {
#endif

// One of "linux", "windows", "macos", "android", or "unknown". Stable enough to
// be written into a file: the session recorder stores it as the provenance of a
// recording. Never a version, and never a reason to branch behaviour on, which
// is what the seam in docs/PLATFORM.md is for.
const char *Sys_PlatformName(void);

#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif
