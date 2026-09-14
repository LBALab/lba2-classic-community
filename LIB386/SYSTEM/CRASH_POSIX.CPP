#include <SYSTEM/CRASH_CORE.H>

#if defined(CRASH_BACKEND_POSIX)

#include <SYSTEM/CRASH.H>
#include <SYSTEM/CRASH_POSIX.H>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* __builtin_trap is brk on arm64, which raises SIGTRAP rather than SIGILL. */
static const int k_signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};
enum { NUM_SIGNALS = sizeof(k_signals) / sizeof(k_signals[0]) };

enum {
    ALT_STACK_SIZE = 256 * 1024,
    ALT_STACK_GUARD = 16 * 1024
};

static struct sigaction s_previous[NSIG];
static U8 s_installed[NSIG];
static int s_logFd = -1;

/* Set once by the thread that writes the block, and never cleared: after the block
   the previous actions take every signal. */
static int s_claimed = 0;
static pthread_t s_handlerThread;
static volatile sig_atomic_t s_blockDone = 0;

static sigjmp_buf s_guardJump;
static volatile sig_atomic_t s_guardArmed = 0;
static const char *volatile s_guardWhat = "";

//----------------------------------------------------------------------------
void CrashSys_Write(const char *data, U32 size) {
    int fd = s_logFd >= 0 ? s_logFd : STDERR_FILENO;
    while (size > 0) {
        ssize_t written = write(fd, data, size);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return;
        data += written;
        size -= (U32)written;
    }
}

static const char *signal_name(int sig) {
    switch (sig) {
    case SIGSEGV:
        return "SIGSEGV";
    case SIGBUS:
        return "SIGBUS";
    case SIGILL:
        return "SIGILL";
    case SIGFPE:
        return "SIGFPE";
    case SIGABRT:
        return "SIGABRT";
    case SIGTRAP:
        return "SIGTRAP";
    default:
        return "?";
    }
}

void CrashPosix_RestorePrevious(void) {
    int i;
    for (i = 0; i < NUM_SIGNALS; i++) {
        int sig = k_signals[i];
        if (s_installed[sig])
            sigaction(sig, &s_previous[sig], NULL);
    }
}

int CrashPosix_Guarded(const char *what, void (*walk)(void *), void *argument) {
    sigset_t entry;
    sigset_t faults;
    s_guardWhat = what;
    pthread_sigmask(SIG_BLOCK, NULL, &entry);
    /* The mask saved here is the handler's, with the crash signal blocked;
       jumping back restores it. */
    if (sigsetjmp(s_guardJump, 1) != 0) {
        s_guardArmed = 0;
        return 1;
    }
    sigemptyset(&faults);
    sigaddset(&faults, SIGSEGV);
    sigaddset(&faults, SIGBUS);
    s_guardArmed = 1;
    pthread_sigmask(SIG_UNBLOCK, &faults, NULL);
    walk(argument);
    s_guardArmed = 0;
    pthread_sigmask(SIG_SETMASK, &entry, NULL);
    return 0;
}

//----------------------------------------------------------------------------
static void write_signal_line(int sig, const siginfo_t *info) {
    CrashLine line;
    int isMain = 0;
    U64 tid = CrashOs_ThreadId(&isMain);
    S64 sender = CrashOs_Sender(info);

    CrashLine_Begin(&line, "signal ");
    CrashLine_Dec(&line, sig);
    CrashLine_Str(&line, " ");
    CrashLine_Str(&line, signal_name(sig));
    CrashLine_Str(&line, " code=");
    CrashLine_Dec(&line, info->si_code);
    CrashLine_Str(&line, " addr=");
    CrashLine_Hex(&line, (uintptr_t)info->si_addr);
    CrashLine_Str(&line, isMain ? " thread=main tid=" : " thread=other tid=");
    CrashLine_Dec(&line, (S64)tid);
    if (sender > 0) {
        CrashLine_Str(&line, " sender=");
        CrashLine_Dec(&line, sender);
    }
    CrashLine_Write(&line);
}

static void crash_handler(int sig, siginfo_t *info, void *context) {
    int savedErrno = errno;

    if (CrashOs_Reentered(sig, context)) {
        errno = savedErrno;
        return;
    }

    if (!__sync_bool_compare_and_swap(&s_claimed, 0, 1)) {
        if (!pthread_equal(s_handlerThread, pthread_self())) {
            /* Another thread writes the block. This one waits for it, up to five
               seconds, and returns: a fault repeats and reaches the previous action
               the writer restored, and abort() ending the process now would cut the
               block short. */
            struct timespec pause = {0, 1000 * 1000};
            int waited;
            for (waited = 0; !s_blockDone && waited < 5000; waited++)
                nanosleep(&pause, NULL);
            errno = savedErrno;
            return;
        }
        if (s_guardArmed) {
            CrashLine line;
            s_guardArmed = 0;
            CrashLine_Begin(&line, "nested ");
            CrashLine_Str(&line, signal_name(sig));
            CrashLine_Str(&line, " recovered in ");
            CrashLine_Str(&line, s_guardWhat);
            CrashLine_Write(&line);
            siglongjmp(s_guardJump, 1);
        }
        /* A fault in the handler outside a guard: the previous actions take it. */
        CrashPosix_RestorePrevious();
        errno = savedErrno;
        return;
    }
    s_handlerThread = pthread_self();

    CrashBlock_Begin();
    write_signal_line(sig, info);
    CrashBlock_Build();
    CrashOs_WriteRegisters(context);
    CrashBlock_State();
    CrashBlock_EngineModule();
    CrashOs_WriteFrames(context);
    CrashBlock_EndFrames();
    CrashBlock_Modules();
    {
        CrashLine line;
        CrashLine_Begin(&line, "chain ");
        CrashLine_Str(&line, CrashOs_ChainName(sig, context));
        CrashLine_Write(&line);
    }
    CrashBlock_End();
    s_blockDone = 1;
    CrashOs_HandOn(sig, info, context);

    errno = savedErrno;
}

//----------------------------------------------------------------------------
/* A thread's own alternate stack, freed when the thread exits. The key's value is
   the mapping, or KEPT_STACK where the thread already had one. */
static pthread_key_t s_stackKey;
static int s_stackKeyReady = 0;
static char s_keptStack;
#define KEPT_STACK ((void *)&s_keptStack)

static void release_alt_stack(void *memory) {
    stack_t current;
    stack_t off;
    if (memory == KEPT_STACK)
        return;
    if (sigaltstack(NULL, &current) == 0 && current.ss_sp == (char *)memory + ALT_STACK_GUARD) {
        memset(&off, 0, sizeof(off));
        off.ss_flags = SS_DISABLE;
        sigaltstack(&off, NULL);
    }
    munmap(memory, ALT_STACK_GUARD + ALT_STACK_SIZE);
}

void Crash_PrepareThread(void) {
    stack_t current;
    stack_t stack;
    void *memory;

    if (!s_stackKeyReady || pthread_getspecific(s_stackKey))
        return;
    if (sigaltstack(NULL, &current) != 0 || !(current.ss_flags & SS_DISABLE)) {
        pthread_setspecific(s_stackKey, KEPT_STACK);
        return;
    }
    memory = mmap(NULL, ALT_STACK_GUARD + ALT_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (memory == MAP_FAILED)
        return;
    mprotect(memory, ALT_STACK_GUARD, PROT_NONE);
    stack.ss_sp = (char *)memory + ALT_STACK_GUARD;
    stack.ss_size = ALT_STACK_SIZE;
    stack.ss_flags = 0;
    if (sigaltstack(&stack, NULL) != 0) {
        munmap(memory, ALT_STACK_GUARD + ALT_STACK_SIZE);
        return;
    }
    pthread_setspecific(s_stackKey, memory);
}

void Crash_Install(const char *logPath, const char *version, const char *commit) {
    static int installed = 0;
    struct sigaction action;
    int i;

    if (installed)
        return;
    installed = 1;

    CrashCore_SetBuild(version, commit);
    if (logPath && logPath[0])
        s_logFd = open(logPath, O_WRONLY | O_APPEND | O_CLOEXEC);

    CrashOs_Prepare();
    s_stackKeyReady = pthread_key_create(&s_stackKey, release_alt_stack) == 0;
    Crash_PrepareThread();

    memset(&action, 0, sizeof(action));
    action.sa_sigaction = crash_handler;
    /* No SA_NODEFER: the crash signal stays blocked while the block is written,
       so a signal handed on from the handler waits until it returns. */
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | CrashOs_ActionFlags();
    sigemptyset(&action.sa_mask);
    for (i = 0; i < NUM_SIGNALS; i++) {
        int sig = k_signals[i];
        if (sigaction(sig, &action, &s_previous[sig]) == 0)
            s_installed[sig] = 1;
    }
}

#endif
