#ifndef LIB_SYSTEM_CRASH_POSIX
#define LIB_SYSTEM_CRASH_POSIX

#include <SYSTEM/ADELINE_TYPES.H>

#include <signal.h>

/* The crash report's POSIX half, shared by Linux, Android and macOS. CRASH_POSIX.CPP
 * installs the handler and writes the block in order; each platform answers the
 * questions that differ, in CRASH_LINUX.CPP or CRASH_MACOS.CPP. Every hook runs in
 * the handler unless it says otherwise. */

/* Extra sigaction flags, beyond SA_SIGINFO and SA_ONSTACK. */
int CrashOs_ActionFlags(void);

/* At install, before the handler exists: fill the module table, and anything else
   that must not first happen inside the handler. */
void CrashOs_Prepare(void);

/* Non-zero when this delivery only confirms an earlier block's crash and must be
   handed on without writing another block. */
int CrashOs_Reentered(int sig, void *context);

/* The thread's id, and whether it is the process's main thread. */
U64 CrashOs_ThreadId(int *isMain);

/* The pid that sent the signal, or 0 when the kernel raised it. */
S64 CrashOs_Sender(const siginfo_t *info);

/* The reg line. */
void CrashOs_WriteRegisters(void *context);

/* CrashBlock_Frame for each frame, innermost first. A walk that reads through stack
   contents runs inside CrashPosix_Guarded. */
void CrashOs_WriteFrames(void *context);

/* How CrashOs_HandOn will hand this signal on, the word after "chain". */
const char *CrashOs_ChainName(int sig, void *context);

/* Hands the signal back to the previous action so the process dies of it as if the
   handler were not there. Runs after the block's end line: on a kernel that
   delivered the signal at once, the block would otherwise be cut. */
void CrashOs_HandOn(int sig, siginfo_t *info, void *context);

/* Puts back the actions that were installed before ours, for every signal: once a
   block is handed on the process is dying, and a later signal on the way, such as a
   sanitizer's abort after its report, is not a crash of its own. */
void CrashPosix_RestorePrevious(void);

/* Runs walk(argument) so that a SIGSEGV or SIGBUS inside it returns here instead of
   ending the process, with a nested line naming what faulted. Non-zero when it did.
   A fault left to repeat would replace the crash in the platform's own report with
   one in the walk. */
int CrashPosix_Guarded(const char *what, void (*walk)(void *), void *argument);

#endif /* LIB_SYSTEM_CRASH_POSIX */
