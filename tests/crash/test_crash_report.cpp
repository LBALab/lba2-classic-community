/* Process-level test for the crash report: launches crash_child once per way of
 * dying, first without the handler and then with it, and checks that
 *
 *   - the process ends the same way both times: killed by the same signal, or with
 *     the same exit status when a sanitizer reports the crash and exits itself;
 *   - the run with the handler wrote exactly one complete block to its log, naming
 *     the signal, the build and the state fields;
 *   - frame 0, or a near frame for a stack overflow whose fault is in a stack
 *     probe, lands in the function that faulted.
 *
 *   test_crash_report <path to crash_child>
 */
#include "test_harness.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

static const char *g_child;
static char g_dir[256];
static std::vector<std::string> g_logs;

struct Outcome {
    bool ran;
    int status; /* waitpid status */
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

static void sleep_ms(long ms) {
    struct timespec t;
    t.tv_sec = ms / 1000;
    t.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
}

/* Runs the child. With sendSignal, waits for its "ready" line, gives it a moment
   to settle into the trigger, and sends the signal. A child still running after
   20 s is killed and reported as a hang, which is the failure a handler that
   blocks would cause. */
static Outcome run_child(const char *kind, bool control, int sendSignal) {
    Outcome outcome;
    char logPath[512];
    int ready[2];
    pid_t pid;
    int waited = 0;

    outcome.ran = false;
    outcome.status = 0;
    snprintf(logPath, sizeof logPath, "%s/%s-%s.log", g_dir, kind, control ? "control" : "handler");
    close(open(logPath, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    g_logs.push_back(logPath);

    if (pipe(ready) != 0)
        return outcome;
    pid = fork();
    if (pid < 0)
        return outcome;
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        dup2(ready[1], STDOUT_FILENO);
        if (devnull >= 0)
            dup2(devnull, STDERR_FILENO);
        close(ready[0]);
        if (control)
            execl(g_child, g_child, logPath, kind, "control", (char *)NULL);
        else
            execl(g_child, g_child, logPath, kind, (char *)NULL);
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
        pid_t done = waitpid(pid, &outcome.status, WNOHANG);
        if (done == pid)
            break;
        if (done < 0 && errno != EINTR)
            break;
        if (waited >= 20000) {
            kill(pid, SIGKILL);
            waitpid(pid, &outcome.status, 0);
            fprintf(stderr, "  %s (%s) still running after 20 s, killed\n", kind, control ? "control" : "handler");
            close(ready[0]);
            return outcome;
        }
        sleep_ms(20);
        waited += 20;
    }
    close(ready[0]);
    outcome.ran = true;
    outcome.log = read_file(logPath);
    return outcome;
}

static std::string describe(int status) {
    char text[64];
    if (WIFSIGNALED(status))
        snprintf(text, sizeof text, "killed by signal %d", WTERMSIG(status));
    else if (WIFEXITED(status))
        snprintf(text, sizeof text, "exit %d", WEXITSTATUS(status));
    else
        snprintf(text, sizeof text, "status 0x%x", status);
    return text;
}

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

/* True when one of the first `depth` frames in the child's own module lies within
   the function that starts at fault. */
static bool frame_in_function(const std::string &log, unsigned long long fault, int depth) {
    const char *name = strrchr(g_child, '/') ? strrchr(g_child, '/') + 1 : g_child;
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

static void check_kind(const char *kind, int expectSignal, const char *signalName, int sendSignal,
                       int frameDepth) {
    Outcome control = run_child(kind, true, sendSignal);
    Outcome handled = run_child(kind, false, sendSignal);
    char signalLine[64];
    unsigned long long fault;

    fprintf(stderr, "  %s: control %s, handler %s\n", kind, describe(control.status).c_str(),
            describe(handled.status).c_str());
    ASSERT_TRUE(control.ran && handled.ran);
    if (!control.ran || !handled.ran)
        return;

    /* A sanitizer may end the process itself; either way both runs end alike. */
    ASSERT_EQ_INT(control.status, handled.status);
    if (WIFSIGNALED(control.status))
        ASSERT_EQ_INT(expectSignal, WTERMSIG(control.status));
    ASSERT_TRUE(!WIFEXITED(control.status) || WEXITSTATUS(control.status) != 0);

    ASSERT_EQ_INT(0, count(control.log, "CRASH "));
    ASSERT_EQ_INT(1, count(handled.log, "CRASH ==== fatal signal ====\n"));
    ASSERT_EQ_INT(1, count(handled.log, "CRASH ==== end ====\n"));
    snprintf(signalLine, sizeof signalLine, "CRASH signal %d %s ", expectSignal, signalName);
    ASSERT_TRUE(handled.log.find(signalLine) != std::string::npos);
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

static void test_segv(void) {
    check_kind("segv", SIGSEGV, "SIGSEGV", 0, 1);
}
static void test_bus(void) {
    check_kind("bus", SIGBUS, "SIGBUS", 0, 1);
}
/* __builtin_trap is ud2 on x86 and brk on arm64. */
static void test_trap(void) {
#if defined(__x86_64__) || defined(__i386__)
    check_kind("trap", SIGILL, "SIGILL", 0, 1);
#else
    check_kind("trap", SIGTRAP, "SIGTRAP", 0, 1);
#endif
}
static void test_abort(void) {
    check_kind("abort", SIGABRT, "SIGABRT", 0, 0);
}
static void test_raise_segv(void) {
    check_kind("raise-segv", SIGSEGV, "SIGSEGV", 0, 0);
}
static void test_raise_fpe(void) {
    check_kind("raise-fpe", SIGFPE, "SIGFPE", 0, 0);
}
/* The fault can be in a stack probe the overflowing function calls. */
static void test_stack_overflow(void) {
    check_kind("stack", SIGSEGV, "SIGSEGV", 0, 3);
}
/* A frame record overwritten with a wild pointer must end the walk, not the block. */
static void test_smashed_frame_record(void) {
    check_kind("smash-frame", SIGSEGV, "SIGSEGV", 0, 1);
}
static void test_thread_segv(void) {
    check_kind("thread-segv", SIGSEGV, "SIGSEGV", 0, 1);
}
/* A thread that asked for its own alternate stack reports its overflow. A
   secondary thread's guard page raises SIGBUS on macOS. */
static void test_thread_stack_overflow(void) {
#if defined(__APPLE__)
    check_kind("thread-stack", SIGBUS, "SIGBUS", 0, 3);
#else
    check_kind("thread-stack", SIGSEGV, "SIGSEGV", 0, 3);
#endif
}
/* The alternate stack a thread asked for is unmapped when it exits. */
static void test_thread_stack_released(void) {
    Outcome outcome = run_child("thread-release", false, 0);
    ASSERT_TRUE(outcome.ran);
    ASSERT_TRUE(WIFEXITED(outcome.status));
    ASSERT_EQ_INT(0, WEXITSTATUS(outcome.status));
}
/* A signal from another process, to a thread waiting in the kernel and to one
   running user code. */
static void test_sent_segv_waiting(void) {
    check_kind("wait", SIGSEGV, "SIGSEGV", SIGSEGV, 0);
}
static void test_sent_segv_busy(void) {
    check_kind("busy", SIGSEGV, "SIGSEGV", SIGSEGV, 0);
}
static void test_sent_abrt_busy(void) {
    check_kind("busy", SIGABRT, "SIGABRT", SIGABRT, 0);
}

int main(int argc, char *argv[]) {
    const char *tmp = getenv("TMPDIR");
    if (argc != 2) {
        fprintf(stderr, "usage: test_crash_report <crash_child>\n");
        return 2;
    }
    g_child = argv[1];
    snprintf(g_dir, sizeof g_dir, "%s/crash_report_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(g_dir)) {
        perror("mkdtemp");
        return 2;
    }

    RUN_TEST(test_segv);
    RUN_TEST(test_bus);
    RUN_TEST(test_trap);
    RUN_TEST(test_abort);
    RUN_TEST(test_raise_segv);
    RUN_TEST(test_raise_fpe);
    RUN_TEST(test_stack_overflow);
    RUN_TEST(test_smashed_frame_record);
    RUN_TEST(test_thread_segv);
    RUN_TEST(test_thread_stack_overflow);
    RUN_TEST(test_thread_stack_released);
    RUN_TEST(test_sent_segv_waiting);
    RUN_TEST(test_sent_segv_busy);
    RUN_TEST(test_sent_abrt_busy);
    TEST_SUMMARY();

    if (test_failures == 0) {
        size_t i;
        for (i = 0; i < g_logs.size(); i++)
            unlink(g_logs[i].c_str());
        rmdir(g_dir);
    } else {
        fprintf(stderr, "logs kept in %s\n", g_dir);
    }
    return test_failures != 0;
}
