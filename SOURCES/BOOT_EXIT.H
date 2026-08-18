#ifndef BOOT_EXIT_H
#define BOOT_EXIT_H

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/HQRRESS.H>

/* How the program ends, and what it tells the player on the way out.
 *
 * Defined in BOOT_EXIT.CPP, which owns the exit report: the code and the detail
 * string it prints, logs and may raise a dialog for. Those two are private to
 * that file, so an exit is decided by calling one of these rather than by
 * assigning them. What the report then says is BOOT_END.H's, which is the half
 * with a host test.
 */

/* Record an outcome without leaving. InitTheEnd is the pair a caller uses when
   the exit happens elsewhere; TheEnd is the same thing and then goes. */
extern void InitTheEnd(S32 num, char *error);
LBA_NORETURN extern void TheEnd(S32 num, const char *error);

/* A file the program cannot go on without: reports whether it is missing or
   merely unusable, and exits non-zero either way. */
LBA_NORETURN extern void TheEndCheckFile(const char *filename);

/* A subsystem that failed to come up, before there is enough of an engine for
   the ordinary exit path. Logs, surfaces a dialog on a terminal-less launch, and
   exits without teardown: nothing finished starting, so there is nothing safe to
   tear down. */
LBA_NORETURN extern void BootFatal(const char *fmt, ...);

/* What the exit report should name if a failure arrives before anything more
   specific has been recorded.
 *
 * Boot sets this to the config path on its way through, so an early failure
 * points at the file being read rather than at nothing. A named call rather than
 * the variable, because that is the module's own state and this is the one place
 * outside it that has a reason to move it. */
extern void BootExit_NoteContext(const char *what);

/* Run at exit: prints the report, raises the dialog if the code calls for one,
   writes the config back and tears the 3D extension down. */
extern void TheEndInfo(void);

/* HQR_Get's error hook: reports a buffer too small for the resource asked for. */
extern void ErrorHQRGet(char *name, S32 index);

#if defined(DEBUG_TOOLS) || defined(TEST_TOOLS)
extern void DebugHQR(const char *titre, T_HQR_HEADER *hqr);
#endif

#endif /* BOOT_EXIT_H */
