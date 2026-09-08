#ifdef LBA2_ANDROID_EXECUTABLE
/* The on-device automation build is a plain executable, so it owns main and
   there is no Java Activity to hand control over. Without this SDL_main.h
   renames main and installs the JNI entry point, and the binary links but never
   runs its own main. SDL_SetMainReady() below replaces what that entry did. */
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL_main.h>

#ifdef __ANDROID__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

int Lba2Main(int argc, char *argv[]);

#if defined(__ANDROID__) && !defined(LBA2_ANDROID_EXECUTABLE)
/* The game quits with exit(), which in an app process also runs the static
   destructors of the system libraries Android preloads, while their threads are
   still running: a render worker of libhwui then waits on a mutex that has been
   destroyed, and the process aborts on most quits. Registered before anything
   else, so it runs after every exit handler the game adds (log, recording, music)
   and ends the process before those destructors. The exit status is not
   available here; nothing on Android reads it. */
static void EndProcess(void) {
    fflush(NULL);
    _exit(0);
}
#endif

int main(int argc, char *argv[]) {
#ifdef LBA2_ANDROID_EXECUTABLE
    /* Stands in for the initialisation SDL's own entry point would have done
       before calling us. No EndProcess here: there is no app process and no
       preloaded system libraries, and automation reads the exit status. */
    SDL_SetMainReady();
#elif defined(__ANDROID__)
    atexit(EndProcess);
#endif
    return Lba2Main(argc, argv);
}
