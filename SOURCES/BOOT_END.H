#ifndef BOOT_END_H
#define BOOT_END_H

/* What the player is told when the program ends, and whether that is a failure.
 *
 * The exit path decides three things from one code: whether to log an error,
 * what to say, and whether to raise a native dialog because there is no terminal
 * to read the log in. All three were spelled out inline in the exit handler,
 * where nothing could reach them: the only way to see what a player with a
 * missing HQR is told was to remove one.
 *
 * They are here because they are a decision rather than an action. Nothing in
 * this file touches SDL, the log, or engine state, so the host test in
 * tests/boot_end can ask what each code produces without a boot that fails.
 *
 * Self-contained: the test includes it on its own.
 */

#include <SYSTEM/ADELINE_TYPES.H>

#include <stddef.h>
#include <stdio.h>

/* The exit codes, from COMMON.H. Repeated rather than included because that
   header pulls the game in; the static assert in BOOT_END.CPP keeps the two
   from drifting. */
#define BOOT_END_FILE_NOT_FOUND 0
#define BOOT_END_NOT_ENOUGH_MEM 1
#define BOOT_END_PROGRAM_OK 2
#define BOOT_END_NAME_NOT_FOUND 3

/* A code the player has to be shown, rather than one the log alone can carry.
 * The three failures qualify; a clean exit and anything unrecognised do not.
 *
 * Unrecognised matters: an early exit leaves the code at -1, which is neither a
 * failure to report nor a success, and raising a dialog for it would put one in
 * front of every user who quits before the code is set. */
static inline S32 BootEnd_IsFailure(S32 code) {
    return code == BOOT_END_FILE_NOT_FOUND || code == BOOT_END_NAME_NOT_FOUND ||
           code == BOOT_END_NOT_ENOUGH_MEM;
}

/* The one line that says what went wrong, in the player's terms rather than the
 * engine's. NULL for a code with nothing to say, which is what the caller uses
 * to decide whether there is a dialog to raise at all. */
static inline const char *BootEnd_Headline(S32 code) {
    switch (code) {
    case BOOT_END_NOT_ENOUGH_MEM:
        return "The game ran out of memory.";
    case BOOT_END_NAME_NOT_FOUND:
        return "A required game object is missing.";
    case BOOT_END_FILE_NOT_FOUND:
        return "Game data is missing or unreadable.";
    default:
        return NULL;
    }
}

/* The log line for a failure, in the engine's terms, for whoever reads the file
 * afterwards. Kept beside the headline so a new code cannot get one and not the
 * other. */
static inline const char *BootEnd_LogFormat(S32 code) {
    switch (code) {
    case BOOT_END_FILE_NOT_FOUND:
        return "*** ERROR-> File not found: %s\n\n";
    case BOOT_END_NAME_NOT_FOUND:
        return "*** ERROR-> Critical error: ident name not found: %s\n\n";
    case BOOT_END_NOT_ENOUGH_MEM:
        return "*** ERROR-> Not Enough Memory: %s (SEE README.TXT)\n\n";
    default:
        return NULL;
    }
}

/* The dialog body: what went wrong, which thing, and where the log is.
 *
 * Returns what was written, or 0 for a code with nothing to report, so a caller
 * can use the return to decide rather than asking twice. A NULL detail or path
 * is written as empty rather than skipped, because a dialog that silently drops
 * a line reads as a different failure than the one that happened. */
static inline S32 BootEnd_FormatMessage(char *out, size_t outSize, S32 code, const char *detail,
                                        const char *logPath) {
    const char *headline = BootEnd_Headline(code);

    if (out == NULL || outSize == 0)
        return 0;

    out[0] = 0;
    if (headline == NULL)
        return 0;

    return (S32)snprintf(out, outSize, "%s\n\n%s\n\nThe game can't continue.\nDetails: %s", headline,
                         detail != NULL ? detail : "", logPath != NULL ? logPath : "");
}

#endif /* BOOT_END_H */
