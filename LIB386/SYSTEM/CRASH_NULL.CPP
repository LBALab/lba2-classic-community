#include <SYSTEM/CRASH_CORE.H>

#if !defined(CRASH_BACKEND_POSIX) && !defined(CRASH_BACKEND_WINDOWS)

#include <SYSTEM/CRASH.H>

#include <stdio.h>

/* The backend for a target with neither POSIX nor Windows crash handling: nothing is
   installed, and a block written anyway goes to stderr. It lives apart from CRASH.CPP
   so a test that links CRASH.CPP with its own CrashSys_Write gets one definition on
   every target. */

void Crash_Install(const char *logPath, const char *version, const char *commit) {
    (void)logPath;
    (void)version;
    (void)commit;
}

void Crash_PrepareThread(void) {
}

void CrashSys_Write(const char *data, U32 size) {
    fwrite(data, 1, size, stderr);
}

#endif
