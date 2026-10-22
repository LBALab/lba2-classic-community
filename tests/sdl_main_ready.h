#ifndef TESTS_SDL_MAIN_READY_H
#define TESTS_SDL_MAIN_READY_H

/* SDL3 on Android refuses SDL_Init until main has been marked ready, which in
   the app is done by the Java Activity. A test run as a plain executable on a
   device (scripts/dev/android-exec.sh) has no Activity, so it marks main ready
   itself. Declared here rather than taken from SDL_main.h: that header renames
   main on Android, and some tests reach SDL only through lba2_platform, which
   does not pass SDL's include path on. Everywhere else SDL3 starts ready and
   this does nothing; SDL2 builds keep their own SDL_SetMainReady call. */
#if defined(__ANDROID__) && !defined(LBA2_USE_SDL2)
#ifdef __cplusplus
extern "C"
#endif
    void
    SDL_SetMainReady(void);
#define TEST_SDL_MAIN_READY() SDL_SetMainReady()
#else
#define TEST_SDL_MAIN_READY() ((void)0)
#endif

#endif
