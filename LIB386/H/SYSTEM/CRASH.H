#ifndef LIB_SYSTEM_CRASH
#define LIB_SYSTEM_CRASH

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Native crash reports. When the process dies on a fatal signal or an unhandled
 * exception, a block of lines starting with "CRASH " is appended to the log: the
 * signal, the build, the registers, the game state registered below, and the
 * frames as module+offset with each module's build identity. A maintainer turns
 * the offsets into function names with the matching build's symbols.
 *
 * The report never changes how the process dies. Its handler hands the signal or
 * exception back to whatever was installed before it, so the exit status, the
 * platform's own crash report, sanitizer reports and debuggers see the crash as
 * if the handler were not there. */

/* Installs the handler for this process. logPath is the log the block is appended
 * to, opened once here; version and commit name the build. Call once, after the
 * log exists. A crash before this writes nothing. */
void Crash_Install(const char *logPath, const char *version, const char *commit);

/* Gives the calling thread an alternate signal stack, so a stack overflow on it
 * still writes a block: a handler cannot run on a stack that has no room left. Call
 * at the top of a thread the engine runs code on, or on each entry to a callback
 * a library thread runs; after the first call on a thread it costs a thread-local
 * read. The stack is freed when the thread exits, and a thread that already has
 * one, from a sanitizer or the C library, keeps it. */
void Crash_PrepareThread(void);

/* Adds one field to the block's state lines: size bytes (1, 2, 4 or 8) read at
 * address when the block is written, printed as name=value. Fields added one after
 * another under the same name print as one, name=a,b,c. The handler reads the
 * bytes and follows no pointer, so address must stay valid for the whole run, as a
 * global does, and name must be a string that lives as long. Call from the thread
 * that installed, before a crash can matter; there is no removal. */
void Crash_AddState(const char *name, const void *address, U32 size, S32 isSigned);

#ifdef __cplusplus
}
#endif

#endif /* LIB_SYSTEM_CRASH */
