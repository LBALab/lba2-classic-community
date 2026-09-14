/* The child the crash report test launches: installs the real handler on a log
 * file, then dies the way argv[2] names. It is its own program because a crash
 * ends the process that has it.
 *
 *   crash_child <log> <kind> [control]
 *
 * With "control" the handler is not installed, which gives the status and the
 * platform report to compare against.
 *
 * Before crashing it appends "TEST fault <address>" to the log, the address of the
 * function whose instruction faults, so the test can check the block's first frame
 * lands in it. A kind that dies inside the C library writes no such line. */
#include <SYSTEM/CRASH.H>

/* The assert case needs assert in a Release build too. */
#undef NDEBUG
#include <assert.h>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

static volatile uintptr_t g_badAddress = 16;
static volatile int g_zero = 0;
static volatile int g_sink = 0;
static const char *g_logPath;

static S32 g_island = 4;
static S32 g_hero[3] = {15360, 4096, -24576};

static void note_fault(uintptr_t function) {
    FILE *log = fopen(g_logPath, "a");
    if (!log)
        return;
    fprintf(log, "TEST fault 0x%llx\n", (unsigned long long)function);
    fclose(log);
}

__attribute__((noinline)) static void trig_segv(void) {
    *(volatile int *)g_badAddress = 1;
    g_sink++;
}

#if !defined(_WIN32)
__attribute__((noinline)) static void trig_bus(void) {
    char path[] = "/tmp/crash_child.XXXXXX";
    int fd = mkstemp(path);
    char *mapped;
    unlink(path);
    /* Writing a page past the end of an empty file is SIGBUS on Linux and macOS. */
    mapped = (char *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED)
        return;
    *(volatile char *)mapped = 1;
    g_sink++;
}

#endif

/* The fault is at address 0, in no module; only the return address the call left
   leads back to the caller. */
static void (*volatile g_nullFunction)(void) = NULL;

__attribute__((noinline)) static void trig_null_call(void) {
    g_nullFunction();
    g_sink++;
}

__attribute__((noinline)) static void trig_trap(void) {
    __builtin_trap();
}

__attribute__((noinline)) static void trig_abort(void) {
    abort();
}

static volatile int g_invariant = 0;

__attribute__((noinline)) static void trig_assert(void) {
    assert(g_invariant == 1);
    g_sink++;
}

#if defined(_WIN32)
/* Integer division by zero traps on x86, as EXCEPTION_INT_DIVIDE_BY_ZERO. */
__attribute__((noinline)) static void trig_divzero(void) {
    g_sink += 7 / g_zero;
}
#else
__attribute__((noinline)) static void trig_raise_segv(void) {
    raise(SIGSEGV);
    g_sink++;
}

__attribute__((noinline)) static void trig_raise_fpe(void) {
    raise(SIGFPE);
    g_sink++;
}
#endif

/* Deeper than any stack; a bound the compiler cannot see keeps it from calling
   the recursion infinite. */
static volatile int g_depthLimit = 1 << 30;

__attribute__((noinline)) static int recurse(int depth) {
    volatile char pad[256];
    pad[0] = (char)depth;
    int result = depth < g_depthLimit ? recurse(depth + 1) : 0;
    g_sink++;
    return result + pad[0];
}

__attribute__((noinline)) static void trig_stack(void) {
    g_sink += recurse(0);
}

/* Overwrites this frame's saved caller fp, then faults, so a walk that trusts the
   frame records reads through a wild pointer. */
__attribute__((noinline)) static void trig_smash_frame(void) {
    uintptr_t *record = (uintptr_t *)__builtin_frame_address(0);
    record[0] = g_badAddress;
    *(volatile int *)g_badAddress = 1;
    g_sink++;
}

/* The alloca makes this caller address its frame through the frame pointer, so an
   unwinder reading its unwind tables follows the smashed record too. */
static volatile int g_scratchSize = 16;

__attribute__((noinline)) static void trig_smash_frame_outer(void) {
    volatile char *scratch = (char *)__builtin_alloca((size_t)g_scratchSize);
    scratch[0] = 1;
    trig_smash_frame();
    g_sink += scratch[0];
}

/* Threads that fault at the same moment, as they do on a shared object freed under
   them; the start flag lines them up. */
enum { RACE_THREADS = 8 };
static volatile int g_raceStart = 0;

#if defined(_WIN32)
static DWORD WINAPI thread_race(LPVOID unused) {
    (void)unused;
    while (!g_raceStart)
        YieldProcessor();
    trig_segv();
    return 0;
}

__attribute__((noinline)) static void trig_thread_race(void) {
    HANDLE threads[RACE_THREADS];
    int i;
    for (i = 0; i < RACE_THREADS; i++)
        threads[i] = CreateThread(NULL, 0, thread_race, NULL, 0, NULL);
    Sleep(100);
    g_raceStart = 1;
    WaitForMultipleObjects(RACE_THREADS, threads, TRUE, INFINITE);
}

static DWORD WINAPI thread_segv(LPVOID unused) {
    (void)unused;
    trig_segv();
    return 0;
}

static DWORD WINAPI thread_overflow(LPVOID unused) {
    (void)unused;
    Crash_PrepareThread();
    g_sink += recurse(0);
    return 0;
}

static void run_thread(LPTHREAD_START_ROUTINE start, SIZE_T stackSize) {
    HANDLE thread = CreateThread(NULL, stackSize, start, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (thread) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
}

__attribute__((noinline)) static void trig_thread_segv(void) {
    run_thread(thread_segv, 0);
}

/* A stack overflow on a thread the engine starts, which keeps no room for the
   filter below its stack unless it asks for it. */
__attribute__((noinline)) static void trig_thread_stack(void) {
    run_thread(thread_overflow, 512 * 1024);
}
#else
static void *thread_race(void *unused) {
    (void)unused;
    while (!g_raceStart)
        sched_yield();
    trig_segv();
    return NULL;
}

__attribute__((noinline)) static void trig_thread_race(void) {
    pthread_t threads[RACE_THREADS];
    int i;
    for (i = 0; i < RACE_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_race, NULL);
    usleep(100 * 1000);
    g_raceStart = 1;
    for (i = 0; i < RACE_THREADS; i++)
        pthread_join(threads[i], NULL);
}

static void *thread_segv(void *unused) {
    (void)unused;
    trig_segv();
    return NULL;
}

__attribute__((noinline)) static void trig_thread_segv(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, thread_segv, NULL);
    pthread_join(thread, NULL);
}

/* A stack overflow on a thread the engine starts, which has no alternate stack of
   its own unless it asks for one. */
static void *thread_overflow(void *unused) {
    (void)unused;
    Crash_PrepareThread();
    g_sink += recurse(0);
    return NULL;
}

__attribute__((noinline)) static void trig_thread_stack(void) {
    pthread_t thread;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 512 * 1024);
    pthread_create(&thread, &attributes, thread_overflow, NULL);
    pthread_join(thread, NULL);
}

/* Not a crash: a thread's alternate stack must be unmapped when the thread exits,
   or every music track would leak one. Exits 0 when it was, 3 when not. */
static void *thread_report_stack(void *out) {
    stack_t current;
    Crash_PrepareThread();
    if (sigaltstack(NULL, &current) == 0 && !(current.ss_flags & SS_DISABLE))
        *(void **)out = current.ss_sp;
    return NULL;
}

__attribute__((noinline)) static void trig_thread_release(void) {
    pthread_t thread;
    void *stack = NULL;
    pthread_create(&thread, NULL, thread_report_stack, &stack);
    pthread_join(thread, NULL);
    /* msync fails with ENOMEM on an address that is no longer mapped. */
    exit(stack && msync(stack, 4096, MS_ASYNC) != 0 && errno == ENOMEM ? 0 : 3);
}

/* Waits to be killed from outside, in a system call. */
__attribute__((noinline)) static void trig_wait(void) {
    for (;;)
        pause();
}

/* Waits to be killed from outside, running user code. */
__attribute__((noinline)) static void trig_busy(void) {
    volatile uint64_t n = 0;
    for (;;)
        n++;
}
#endif

struct Trigger {
    const char *name;
    void (*function)(void);
    uintptr_t faulting; /* 0 when the fault is in the C library */
};

#define FN(f) ((uintptr_t)(f))

static const Trigger k_triggers[] = {
    {"segv", trig_segv, FN(trig_segv)},
    {"null-call", trig_null_call, FN(trig_null_call)},
    {"trap", trig_trap, FN(trig_trap)},
    {"abort", trig_abort, 0},
    {"assert", trig_assert, 0},
    {"stack", trig_stack, FN(recurse)},
    {"smash-frame", trig_smash_frame_outer, FN(trig_smash_frame)},
    {"thread-segv", trig_thread_segv, FN(trig_segv)},
    {"thread-stack", trig_thread_stack, FN(recurse)},
    {"thread-race", trig_thread_race, FN(trig_segv)},
#if defined(_WIN32)
    {"divzero", trig_divzero, FN(trig_divzero)},
#else
    {"bus", trig_bus, FN(trig_bus)},
    {"raise-segv", trig_raise_segv, 0},
    {"raise-fpe", trig_raise_fpe, 0},
    {"thread-release", trig_thread_release, 0},
    {"wait", trig_wait, 0},
    {"busy", trig_busy, 0},
#endif
};

int main(int argc, char *argv[]) {
    size_t i;
    if (argc != 3 && !(argc == 4 && !strcmp(argv[3], "control"))) {
        fprintf(stderr, "usage: crash_child <log> <kind> [control]\n");
        return 2;
    }
    g_logPath = argv[1];
    if (argc == 3)
        Crash_Install(g_logPath, "0.0.0-test", "0123456789ab");
    Crash_AddState("island", &g_island, 4, 1);
    Crash_AddState("hero", &g_hero[0], 4, 1);
    Crash_AddState("hero", &g_hero[1], 4, 1);
    Crash_AddState("hero", &g_hero[2], 4, 1);

    for (i = 0; i < sizeof(k_triggers) / sizeof(k_triggers[0]); i++) {
        if (strcmp(argv[2], k_triggers[i].name) != 0)
            continue;
        if (k_triggers[i].faulting)
            note_fault(k_triggers[i].faulting);
        /* Tells a test waiting to send a signal that the handler is in place. */
        fprintf(stdout, "ready\n");
        fflush(stdout);
        k_triggers[i].function();
        return 0;
    }
    fprintf(stderr, "crash_child: unknown kind %s\n", argv[2]);
    return 2;
}
