/* Host conformance test for the command-line contract (SOURCES/CLI_ARGS.CPP).
 *
 * Every case here guards one way a run can silently do the wrong thing and still exit 0,
 * which for a harness whose output nobody watches is the worst failure mode there is:
 *
 *   - an unknown flag (a typo) used to be ignored, so `--resolutionn 1280x720` ran at the
 *     default resolution and handed back a confident, wrong answer;
 *   - a flag whose value is missing used to be dropped by its own parser's `has_val` guard,
 *     so `--tick` with no count booted and did nothing;
 *   - a flag where a value belongs used to be swallowed as that value: `--exec-at 5 --tick 30`
 *     parsed cmds="--tick", ate the 30, and produced a run that ticked zero times.
 *
 * Cli_ValidateArgs is the single place that knows each flag's arity, so it is the only place
 * that can catch all three. It is pure argv-in / int-out with no engine state, which is why
 * this is a host test (CI runs `ctest -L host_quick`; the shell automation suite needs retail
 * data and does not run there).
 */

#include "CLI_ARGS.H"

#include <cstdio>
#include <cstring>

static int failures = 0;

/* Cli_ValidateArgs takes char*[] like main() does. */
static int validate(int argc, const char *const *argv) {
    char *mutableArgv[16];
    for (int i = 0; i < argc; i++) {
        mutableArgv[i] = const_cast<char *>(argv[i]);
    }
    return Cli_ValidateArgs(argc, mutableArgv);
}

static void expect(const char *what, int want, int argc, const char *const *argv) {
    int got = validate(argc, argv);
    const bool ok = (want == 0) ? (got == 0) : (got != 0);
    if (!ok) {
        std::printf("FAIL %s: Cli_ValidateArgs returned %d, wanted %s\n", what, got,
                    want == 0 ? "0 (accept)" : "non-zero (reject)");
        failures++;
    }
}

#define ACCEPT(name, ...)                                             \
    do {                                                              \
        const char *argv[] = {"lba2cc", __VA_ARGS__};                 \
        expect(name, 0, (int)(sizeof(argv) / sizeof(argv[0])), argv); \
    } while (0)

#define REJECT(name, ...)                                             \
    do {                                                              \
        const char *argv[] = {"lba2cc", __VA_ARGS__};                 \
        expect(name, 1, (int)(sizeof(argv) / sizeof(argv[0])), argv); \
    } while (0)

int main() {
    /* --- accepted: the shapes real invocations use ---------------------------------- */
    ACCEPT("no flags", "");
    ACCEPT("boolean flags", "--headless", "--no-autosave", "--exit");
    ACCEPT("flag with value", "--tick", "30");
    ACCEPT("two-value flag", "--exec-at", "10", "teleport actor 3");
    ACCEPT("full harness line", "--headless", "--no-autosave", "--fixed-dt", "16",
           "--load", "021 Palace", "--exec", "cube 154", "--tick", "60", "--exit");

    /* A negative number is a value, not a flag: only "--" prefixes are flags, so this must
       still reach the flag's own parser (which does its own range checking). */
    ACCEPT("negative value is not a flag", "--fixed-dt", "-1");

    /* RES_DISCOVERY.CPP matches --game-dir / --data-dir / --pick-game-dir with strcasecmp,
       so a mixed-case spelling works today and must not start erroring as "unknown". */
    ACCEPT("case-insensitive data-dir flag", "--GAME-DIR", "/data");
    ACCEPT("case-insensitive picker flag", "--Pick-Game-Dir");

    /* ...but only for the flags whose parser really is case-insensitive. Accepting
       --HEADLESS here would be worse than rejecting it: CONTROL.CPP compares with strcmp,
       so the run would boot with the flag silently ignored. */
    REJECT("case-insensitive only where the parser is", "--HEADLESS");

    /* --- rejected: every silent-wrong-answer shape ----------------------------------- */
    REJECT("unknown flag", "--resolutionn", "1280x720");
    REJECT("typo'd flag with no value", "--headles");
    REJECT("missing value at end of line", "--tick");
    REJECT("missing value, one-value flag", "--headless", "--load");
    REJECT("missing second value of two-value flag", "--exec-at", "5");
    REJECT("flag swallowed as a value", "--exec-at", "5", "--tick", "30");
    REJECT("flag swallowed as a one-value flag's value", "--load", "--tick", "30");

    if (failures == 0) {
        std::printf("PASS test_cli_args\n");
        return 0;
    }
    std::printf("FAIL test_cli_args: %d failure(s)\n", failures);
    return 1;
}
