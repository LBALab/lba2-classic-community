#include "EXIT_SCREEN.H"

#include <stdio.h>
#include <time.h> /* time */

#if defined(_WIN32)
#include <io.h> /* _isatty, _fileno */
#else
#include <unistd.h> /* isatty, fileno */
#endif

/* Internals use an anonymous namespace for internal linkage — the house
 * new-infrastructure style (see SOURCES/RES_PICKER.CPP). */
namespace {

// Quote pool
const char *const QUOTES[] = {
    "Halt!",
    "Stop squirming, prisoner Twinsen!",
    "A prisoner has escaped! Sound the alarm!",
    "Sound the alarm! It's the escapee!",
    "What are you doing here? This sector is off limits to the public.",
    "It is forbidden to travel outside the Citadel in that kind of attire!",
    "That's him! That's the Twinsunian we're looking for! Arrest him!",
    "No way! I'm going to stop you!!",
    "Long live the venerable Dr. FunFrock, who saved us from all the superstitions.",
    "Dr. FunFrock's headquarters. Access IS FORBIDDEN!",
    "RULE #1024-GH: It is absolutely forbidden to make any allusion to the so-called Legend.",
    "All citizens must remain motionless if a surveillance device is in their vicinity.",
    "To the glory of Dr. FunFrock.",
    "This must be stopped!",
    "Red clones just hate this stupid toy.",
    "Don't ever try teleporting yourself -- only the clones can do it.",
    "Calm down! Save your strength for the clones.",
    "Only a fool or a hero would dare venture there.",
    "Hail to you, may the Sendells protect your descendants!",
    "Don't do that, you fool, it's going to blow up!",
    "Out, quick!",
    "Adieu!!!",
    "Come back when you know where you want to go.",
};
const int QUOTE_COUNT = (int)(sizeof QUOTES / sizeof QUOTES[0]);

int stdout_is_tty(void) {
#if defined(_WIN32)
    return _isatty(_fileno(stdout));
#else
    return isatty(fileno(stdout));
#endif
}

} // namespace

extern "C" void PrintExitScreen(void) {
    if (!stdout_is_tty())
        return;

    /* Non-determinism is fine and wanted: this only ever fires on an
     * interactive TTY exit, never under the (redirected) control harness. */
    unsigned long seed = (unsigned long)time(NULL);
    fprintf(stdout, "    \"%s\"\n", QUOTES[seed % QUOTE_COUNT]);
    fflush(stdout);
}
