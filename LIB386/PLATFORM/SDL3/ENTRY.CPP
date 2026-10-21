#include <SDL3/SDL_main.h>

#ifdef __ANDROID__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

int Lba2Main(int argc, char *argv[]);

#ifdef __ANDROID__
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
#ifdef __ANDROID__
    atexit(EndProcess);
#endif
    return Lba2Main(argc, argv);
}
