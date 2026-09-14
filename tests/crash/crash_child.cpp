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

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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

__attribute__((noinline)) static void trig_trap(void) {
    __builtin_trap();
}

__attribute__((noinline)) static void trig_abort(void) {
    abort();
}

__attribute__((noinline)) static void trig_raise_segv(void) {
    raise(SIGSEGV);
    g_sink++;
}

__attribute__((noinline)) static void trig_raise_fpe(void) {
    raise(SIGFPE);
    g_sink++;
}

__attribute__((noinline)) static int recurse(int depth) {
    volatile char pad[4096];
    pad[0] = (char)depth;
    int result = recurse(depth + 1);
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

__attribute__((noinline)) static void trig_smash_frame_outer(void) {
    trig_smash_frame();
    g_sink++;
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

struct Trigger {
    const char *name;
    void (*function)(void);
    uintptr_t faulting; /* 0 when the fault is in the C library */
};

#define FN(f) ((uintptr_t)(f))

static const Trigger k_triggers[] = {
    {"segv", trig_segv, FN(trig_segv)},
    {"bus", trig_bus, FN(trig_bus)},
    {"trap", trig_trap, FN(trig_trap)},
    {"abort", trig_abort, 0},
    {"raise-segv", trig_raise_segv, 0},
    {"raise-fpe", trig_raise_fpe, 0},
    {"stack", trig_stack, FN(recurse)},
    {"smash-frame", trig_smash_frame_outer, FN(trig_smash_frame)},
    {"thread-segv", trig_thread_segv, FN(trig_segv)},
    {"wait", trig_wait, 0},
    {"busy", trig_busy, 0},
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
