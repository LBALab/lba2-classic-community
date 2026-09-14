/* Process-level test for the crash report: launches crash_child once per way of
 * dying, first without the handler and then with it, and checks that
 *
 *   - the process ends the same way both times: killed by the same signal or
 *     exception, or with the same exit status when a sanitizer reports the crash
 *     and exits itself;
 *   - the run with the handler wrote exactly one complete block to its log, naming
 *     the signal or exception, the build and the state fields;
 *   - frame 0, or a near frame for a stack overflow whose fault is in a stack
 *     probe, lands in the function that faulted.
 *
 *   test_crash_report <path to crash_child>
 */
#include "test_harness.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

#include <string>
#include <vector>

static const char *g_child;
static char g_dir[512];
static std::vector<std::string> g_logs;

/* A child that is still running after this long is killed and reported as a hang,
   which is the failure a handler that blocks would cause. */
enum { CHILD_TIMEOUT_MS = 20000 };

struct Outcome {
    bool ran;
    /* waitpid status on POSIX, the exit code on Windows */
    unsigned long status;
    std::string log;
};

static std::string read_file(const char *path) {
    std::string text;
    FILE *file = fopen(path, "rb");
    char buffer[4096];
    size_t n;
    if (!file)
        return text;
    while ((n = fread(buffer, 1, sizeof buffer, file)) > 0)
        text.append(buffer, n);
    fclose(file);
    return text;
}

static std::string log_path(const char *kind, bool control) {
    std::string path = std::string(g_dir) + "/" + kind + (control ? "-control.log" : "-handler.log");
    FILE *file = fopen(path.c_str(), "wb");
    if (file)
        fclose(file);
    g_logs.push_back(path);
    return path;
}

#if defined(_WIN32)

static Outcome run_child(const char *kind, bool control, int sendSignal) {
    Outcome outcome;
    std::string logPath = log_path(kind, control);
    std::string command = std::string("\"") + g_child + "\" \"" + logPath + "\" " + kind + (control ? " control" : "");
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD code = 0;
    (void)sendSignal;

    outcome.ran = false;
    outcome.status = 0;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    std::vector<char> commandLine(command.begin(), command.end());
    commandLine.push_back('\0');
    if (!CreateProcessA(NULL, &commandLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
        return outcome;
    if (WaitForSingleObject(process.hProcess, CHILD_TIMEOUT_MS) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
        fprintf(stderr, "  %s (%s) still running after 20 s, killed\n", kind, control ? "control" : "handler");
    } else {
        outcome.ran = true;
    }
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    outcome.status = code;
    outcome.log = read_file(logPath.c_str());
    return outcome;
}

static std::string describe(unsigned long status) {
    char text[32];
    snprintf(text, sizeof text, "exit 0x%lx", status);
    return text;
}

#else

enum { STACK_LIMIT = 8 * 1024 * 1024 };

static void sleep_ms(long ms) {
    struct timespec t;
    t.tv_sec = ms / 1000;
    t.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
}

/* With sendSignal, waits for the child's "ready" line, gives it a moment to settle
   into the trigger, and sends the signal. */
static Outcome run_child(const char *kind, bool control, int sendSignal) {
    Outcome outcome;
    std::string logPath = log_path(kind, control);
    int ready[2];
    int status = 0;
    pid_t pid;
    int waited = 0;

    outcome.ran = false;
    outcome.status = 0;
    if (pipe(ready) != 0)
        return outcome;
    pid = fork();
    if (pid < 0)
        return outcome;
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        struct rlimit stack;
        /* A main thread's overflow walks one frame per recursion, and the walk stops at
           2^20 frames: under a 64 MB or unlimited stack limit the tail never leaves
           the recursion. The usual 8 MB keeps the depth in range. */
        if (getrlimit(RLIMIT_STACK, &stack) == 0 && (stack.rlim_cur == RLIM_INFINITY || stack.rlim_cur > STACK_LIMIT)) {
            stack.rlim_cur = STACK_LIMIT;
            setrlimit(RLIMIT_STACK, &stack);
        }
        dup2(ready[1], STDOUT_FILENO);
        if (devnull >= 0)
            dup2(devnull, STDERR_FILENO);
        close(ready[0]);
        if (control)
            execl(g_child, g_child, logPath.c_str(), kind, "control", (char *)NULL);
        else
            execl(g_child, g_child, logPath.c_str(), kind, (char *)NULL);
        _exit(127);
    }
    close(ready[1]);

    if (sendSignal) {
        char byte;
        while (read(ready[0], &byte, 1) == 1 && byte != '\n') {
        }
        sleep_ms(300);
        kill(pid, sendSignal);
    }

    for (;;) {
        pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid)
            break;
        if (done < 0 && errno != EINTR)
            break;
        if (waited >= CHILD_TIMEOUT_MS) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            fprintf(stderr, "  %s (%s) still running after 20 s, killed\n", kind, control ? "control" : "handler");
            close(ready[0]);
            return outcome;
        }
        sleep_ms(20);
        waited += 20;
    }
    close(ready[0]);
    outcome.ran = true;
    outcome.status = (unsigned long)status;
    outcome.log = read_file(logPath.c_str());
    return outcome;
}

static std::string describe(unsigned long status) {
    char text[64];
    int s = (int)status;
    if (WIFSIGNALED(s))
        snprintf(text, sizeof text, "killed by signal %d", WTERMSIG(s));
    else if (WIFEXITED(s))
        snprintf(text, sizeof text, "exit %d", WEXITSTATUS(s));
    else
        snprintf(text, sizeof text, "status 0x%x", s);
    return text;
}

#endif

static int count(const std::string &text, const char *needle) {
    int n = 0;
    size_t at = 0;
    while ((at = text.find(needle, at)) != std::string::npos) {
        n++;
        at++;
    }
    return n;
}

/* The value after "<key>" on the first line starting with prefix, as a number. */
static bool find_hex_after(const std::string &text, const std::string &prefix, const char *key,
                           unsigned long long *value) {
    size_t line = text.find(prefix);
    size_t at;
    if (line == std::string::npos)
        return false;
    at = text.find(key, line);
    if (at == std::string::npos || at > text.find('\n', line))
        return false;
    *value = strtoull(text.c_str() + at + strlen(key), NULL, 16);
    return true;
}

static const char *child_name(void) {
    const char *name = g_child;
    const char *p;
    for (p = g_child; *p; p++) {
        if (*p == '/' || *p == '\\')
            name = p + 1;
    }
    return name;
}

/* True when one of the first `depth` frames in the child's own module lies within
   the function that starts at fault. */
static bool frame_in_function(const std::string &log, unsigned long long fault, int depth) {
    const char *name = child_name();
    unsigned long long base;
    int i;
    if (!find_hex_after(log, std::string("CRASH module ") + name + " ", "base=0x", &base))
        return false;
    for (i = 0; i < depth; i++) {
        char prefix[128];
        unsigned long long offset;
        snprintf(prefix, sizeof prefix, "CRASH frame %02d %s+", i, name);
        if (!find_hex_after(log, prefix, "+0x", &offset))
            continue;
        if (base + offset >= fault && base + offset < fault + 512)
            return true;
    }
    return false;
}

/* The frame lines of a block, in order. */
static std::vector<std::string> frame_lines(const std::string &log) {
    std::vector<std::string> frames;
    size_t at = 0;
    while ((at = log.find("CRASH frame ", at)) != std::string::npos) {
        size_t end = log.find('\n', at);
        std::string line = log.substr(at, end - at);
        size_t address = line.find(' ', strlen("CRASH frame "));
        frames.push_back(address == std::string::npos ? line : line.substr(address + 1));
        at = end;
    }
    return frames;
}

/* A deep recursion's block must reach the frames below it, where the thread
   began: its last frame is not the recursion's return address. */
static void check_tail_leaves_recursion(const char *kind) {
    Outcome handled = run_child(kind, false, 0);
    std::vector<std::string> frames = frame_lines(handled.log);
    ASSERT_TRUE(handled.log.find("CRASH frames skipped ") != std::string::npos);
    ASSERT_TRUE(frames.size() >= 3);
    if (frames.size() >= 3)
        ASSERT_TRUE(frames[frames.size() - 1] != frames[frames.size() / 2]);
}

/* expected is the signal the control run is killed by on POSIX, or its exit code on
   Windows; blockLine starts the block's signal or exception line. */
static void check_kind(const char *kind, unsigned long expected, const char *blockLine, int sendSignal,
                       int frameDepth) {
    Outcome control = run_child(kind, true, sendSignal);
    Outcome handled = run_child(kind, false, sendSignal);
    unsigned long long fault;

    fprintf(stderr, "  %s: control %s, handler %s\n", kind, describe(control.status).c_str(),
            describe(handled.status).c_str());
    ASSERT_TRUE(control.ran && handled.ran);
    if (!control.ran || !handled.ran)
        return;

    /* A sanitizer may end the process itself; either way both runs end alike. */
    ASSERT_TRUE(control.status == handled.status);
#if defined(_WIN32)
    ASSERT_TRUE(control.status == expected);
#else
    {
        int status = (int)control.status;
        if (WIFSIGNALED(status))
            ASSERT_EQ_INT((int)expected, WTERMSIG(status));
        ASSERT_TRUE(!WIFEXITED(status) || WEXITSTATUS(status) != 0);
    }
#endif

    ASSERT_EQ_INT(0, count(control.log, "CRASH "));
    ASSERT_EQ_INT(1, count(handled.log, "CRASH ==== fatal signal ====\n"));
    ASSERT_EQ_INT(1, count(handled.log, "CRASH ==== end ====\n"));
    ASSERT_TRUE(handled.log.find(blockLine) != std::string::npos);
    ASSERT_TRUE(handled.log.find("CRASH build 0.0.0-test 0123456789ab ") != std::string::npos);
    ASSERT_TRUE(handled.log.find("CRASH state island=4 hero=15360,4096,-24576\n") != std::string::npos);
    ASSERT_TRUE(handled.log.find("CRASH frame 00 ") != std::string::npos);
    ASSERT_TRUE(handled.log.find("CRASH chain ") != std::string::npos);

    if (find_hex_after(handled.log, "TEST fault ", "0x", &fault)) {
        bool found = frame_in_function(handled.log, fault, frameDepth);
        if (!found)
            fprintf(stderr, "  %s: no frame in the faulting function 0x%llx\n%s", kind, fault, handled.log.c_str());
        ASSERT_TRUE(found);
    }
}

#if defined(_WIN32)

static void test_access_violation(void) {
    check_kind("segv", 0xC0000005UL, "CRASH exception 0xc0000005 EXCEPTION_ACCESS_VIOLATION addr=0x10 access=write ",
               0, 1);
}
/* Frame 0 is address 0; the caller must still be found. */
static void test_null_call(void) {
    check_kind("null-call", 0xC0000005UL, "CRASH exception 0xc0000005 EXCEPTION_ACCESS_VIOLATION addr=0x0 access=execute ",
               0, 2);
}
/* __builtin_trap is ud2. */
static void test_illegal_instruction(void) {
    check_kind("trap", 0xC000001DUL, "CRASH exception 0xc000001d EXCEPTION_ILLEGAL_INSTRUCTION ", 0, 1);
}
static void test_divide_by_zero(void) {
    check_kind("divzero", 0xC0000094UL, "CRASH exception 0xc0000094 EXCEPTION_INT_DIVIDE_BY_ZERO ", 0, 1);
}
/* abort() ends in __fastfail after the SIGABRT handler returns. */
static void test_abort(void) {
    check_kind("abort", 0xC0000409UL, "CRASH signal 22 SIGABRT ", 0, 0);
}
static void test_assert(void) {
    check_kind("assert", 0xC0000409UL, "CRASH signal 22 SIGABRT ", 0, 0);
}
static void test_stack_overflow(void) {
    check_kind("stack", 0xC00000FDUL, "CRASH exception 0xc00000fd EXCEPTION_STACK_OVERFLOW ", 0, 3);
    check_tail_leaves_recursion("stack");
}
/* A saved frame pointer overwritten with a wild value must end the walk, not the
   block, and not change how the process dies. */
static void test_smashed_frame_record(void) {
    check_kind("smash-frame", 0xC0000005UL, "CRASH exception 0xc0000005 EXCEPTION_ACCESS_VIOLATION ", 0, 1);
}
static void test_thread_access_violation(void) {
    check_kind("thread-segv", 0xC0000005UL, "CRASH exception 0xc0000005 EXCEPTION_ACCESS_VIOLATION ", 0, 1);
}
/* Threads crashing together write one whole block. Losing the race ends a block
   early most runs, so a few runs catch it. */
static void test_thread_race(void) {
    int i;
    for (i = 0; i < 3; i++)
        check_kind("thread-race", 0xC0000005UL, "CRASH exception 0xc0000005 EXCEPTION_ACCESS_VIOLATION ", 0, 1);
}
static void test_thread_stack_overflow(void) {
    check_kind("thread-stack", 0xC00000FDUL, "CRASH exception 0xc00000fd EXCEPTION_STACK_OVERFLOW ", 0, 3);
    check_tail_leaves_recursion("thread-stack");
}

static int make_directory(void) {
    char base[MAX_PATH];
    DWORD length = GetTempPathA(sizeof base, base);
    if (length == 0 || length >= sizeof base)
        return 0;
    snprintf(g_dir, sizeof g_dir, "%scrash_report_%lu", base, (unsigned long)GetCurrentProcessId());
    return CreateDirectoryA(g_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void remove_directory(void) {
    size_t i;
    for (i = 0; i < g_logs.size(); i++)
        DeleteFileA(g_logs[i].c_str());
    RemoveDirectoryA(g_dir);
}

#else

static void test_segv(void) {
    check_kind("segv", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, 1);
}
static void test_bus(void) {
    char line[64];
    snprintf(line, sizeof line, "CRASH signal %d SIGBUS ", SIGBUS);
    check_kind("bus", SIGBUS, line, 0, 1);
}
/* Frame 0 is address 0; the caller must still be found. */
static void test_null_call(void) {
    check_kind("null-call", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, 2);
}
/* __builtin_trap is ud2 on x86 and brk on arm64. */
static void test_trap(void) {
#if defined(__x86_64__) || defined(__i386__)
    check_kind("trap", SIGILL, "CRASH signal 4 SIGILL ", 0, 1);
#else
    check_kind("trap", SIGTRAP, "CRASH signal 5 SIGTRAP ", 0, 1);
#endif
}
static void test_abort(void) {
    check_kind("abort", SIGABRT, "CRASH signal 6 SIGABRT ", 0, 0);
}
static void test_assert(void) {
    check_kind("assert", SIGABRT, "CRASH signal 6 SIGABRT ", 0, 0);
}
static void test_raise_segv(void) {
    check_kind("raise-segv", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, 0);
}
static void test_raise_fpe(void) {
    check_kind("raise-fpe", SIGFPE, "CRASH signal 8 SIGFPE ", 0, 0);
}
/* The fault can be in a stack probe the overflowing function calls, or in the
   sanitizer runtime it calls, which under ASan was three frames deep. */
enum { OVERFLOW_FRAME_DEPTH = 8 };

static void test_stack_overflow(void) {
    check_kind("stack", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, OVERFLOW_FRAME_DEPTH);
    check_tail_leaves_recursion("stack");
}
/* A frame record overwritten with a wild pointer must end the walk, not the block. */
static void test_smashed_frame_record(void) {
    check_kind("smash-frame", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, 1);
}
static void test_thread_segv(void) {
    check_kind("thread-segv", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, 1);
}
/* Threads crashing together write one whole block. Losing the race ends a block
   early most runs, so a few runs catch it. */
static void test_thread_race(void) {
    int i;
    for (i = 0; i < 3; i++)
        check_kind("thread-race", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, 1);
}
/* A thread that asked for its own alternate stack reports its overflow. A
   secondary thread's guard page raises SIGBUS on macOS. */
static void test_thread_stack_overflow(void) {
#if defined(__APPLE__)
    check_kind("thread-stack", SIGBUS, "CRASH signal 10 SIGBUS ", 0, OVERFLOW_FRAME_DEPTH);
#else
    check_kind("thread-stack", SIGSEGV, "CRASH signal 11 SIGSEGV ", 0, OVERFLOW_FRAME_DEPTH);
#endif
    check_tail_leaves_recursion("thread-stack");
}
/* The alternate stack a thread asked for is unmapped when it exits. */
static void test_thread_stack_released(void) {
    Outcome outcome = run_child("thread-release", false, 0);
    int status = (int)outcome.status;
    ASSERT_TRUE(outcome.ran);
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ_INT(0, WEXITSTATUS(status));
}
/* A signal from another process, to a thread waiting in the kernel and to one
   running user code. */
static void test_sent_segv_waiting(void) {
    check_kind("wait", SIGSEGV, "CRASH signal 11 SIGSEGV ", SIGSEGV, 0);
}
static void test_sent_segv_busy(void) {
    check_kind("busy", SIGSEGV, "CRASH signal 11 SIGSEGV ", SIGSEGV, 0);
}
static void test_sent_abrt_busy(void) {
    check_kind("busy", SIGABRT, "CRASH signal 6 SIGABRT ", SIGABRT, 0);
}

static int make_directory(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(g_dir, sizeof g_dir, "%s/crash_report_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    return mkdtemp(g_dir) != NULL;
}

static void remove_directory(void) {
    size_t i;
    for (i = 0; i < g_logs.size(); i++)
        unlink(g_logs[i].c_str());
    rmdir(g_dir);
}

#endif

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_crash_report <crash_child>\n");
        return 2;
    }
    g_child = argv[1];
    if (!make_directory()) {
        fprintf(stderr, "test_crash_report: cannot create a temporary directory\n");
        return 2;
    }

#if defined(_WIN32)
    RUN_TEST(test_access_violation);
    RUN_TEST(test_null_call);
    RUN_TEST(test_illegal_instruction);
    RUN_TEST(test_divide_by_zero);
    RUN_TEST(test_abort);
    RUN_TEST(test_assert);
    RUN_TEST(test_stack_overflow);
    RUN_TEST(test_smashed_frame_record);
    RUN_TEST(test_thread_access_violation);
    RUN_TEST(test_thread_race);
    RUN_TEST(test_thread_stack_overflow);
#else
    RUN_TEST(test_segv);
    RUN_TEST(test_null_call);
    RUN_TEST(test_bus);
    RUN_TEST(test_trap);
    RUN_TEST(test_abort);
    RUN_TEST(test_assert);
    RUN_TEST(test_raise_segv);
    RUN_TEST(test_raise_fpe);
    RUN_TEST(test_stack_overflow);
    RUN_TEST(test_smashed_frame_record);
    RUN_TEST(test_thread_segv);
    RUN_TEST(test_thread_race);
    RUN_TEST(test_thread_stack_overflow);
    RUN_TEST(test_thread_stack_released);
    RUN_TEST(test_sent_segv_waiting);
    RUN_TEST(test_sent_segv_busy);
    RUN_TEST(test_sent_abrt_busy);
#endif
    TEST_SUMMARY();

    if (test_failures == 0)
        remove_directory();
    else
        fprintf(stderr, "logs kept in %s\n", g_dir);
    return test_failures != 0;
}
