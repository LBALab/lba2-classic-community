/*
 * Run one call in a child process, so a crash or a hang fails an assertion
 * instead of the whole test binary. POSIX only (fork).
 *
 *   static int draw(void *ctx) { ...; return same ? 0 : 1; }
 *   int sig;
 *   int rc = run_in_child(draw, &ctx, &sig);
 *
 * Returns the child's exit code (the value fn returned), -1 when the child
 * died on a signal (its number in *sig; SIGALRM after 3 s means it hung), or
 * -2 when no child ran. The child's stdout goes to /dev/null so a trace
 * from the code under test does not break the TAP output.
 */
#ifndef RUN_IN_CHILD_H
#define RUN_IN_CHILD_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int (*child_fn)(void *ctx);

static int run_in_child(child_fn fn, void *ctx, int *sig) {
    *sig = 0;
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0)
        return -2;
    if (pid == 0) {
        alarm(3);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
            dup2(devnull, 1);
        _exit(fn(ctx));
    }
    int status = 0;
    pid_t got;
    do {
        got = waitpid(pid, &status, 0);
    } while (got < 0 && errno == EINTR);
    if (got != pid)
        return -2;
    if (WIFSIGNALED(status)) {
        *sig = WTERMSIG(status);
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

#endif /* RUN_IN_CHILD_H */
