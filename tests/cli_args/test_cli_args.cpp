/* Host conformance test for the command-line contract (SOURCES/CLI_ARGS.CPP).
 *
 * Every case here guards one way a run can silently do the wrong thing and still exit 0,
 * which for a harness whose output nobody watches is the worst failure mode there is:
 *
 *   - an unknown flag (a typo) must not be ignored: `--resolutionn 1280x720` would run at
 *     the default resolution and hand back a confident, wrong answer;
 *   - a flag whose value is missing must not be dropped by its own parser's `has_val` guard:
 *     `--tick` with no count would boot and do nothing;
 *   - a flag standing where a value belongs must not be swallowed as that value:
 *     `--exec-at 5 --tick 30` would parse cmds="--tick", eat the 30, and tick zero times.
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
    char *mutableArgv[32];
    if (argc > (int)(sizeof(mutableArgv) / sizeof(mutableArgv[0]))) {
        std::printf("FAIL test case has more arguments than the buffer holds\n");
        failures++;
        return -1;
    }
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

/* Capture one tier of the usage text into buf. full=0 is plain --help, 1 is --help-all. */
static void captureUsage(char *buf, size_t bufsz, int full) {
    /* A named file in the working directory, not tmpfile(): on Windows tmpfile() wants the
       drive root and fails without the privileges to write there, and CI runs the host tests
       on Windows too. */
    const char *capPath = "cli_args_usage.tmp";
    std::FILE *cap = std::fopen(capPath, "w+");

    buf[0] = '\0';
    if (cap == NULL) {
        std::printf("FAIL could not open %s to capture the usage text\n", capPath);
        failures++;
        return;
    }
    Cli_PrintUsageTo(cap, "lba2cc", full);
    std::rewind(cap);
    size_t n = std::fread(buf, 1, bufsz - 1, cap);
    buf[n] = '\0';
    std::fclose(cap);
    std::remove(capPath);
}

int main() {
    /* --- accepted: the shapes real invocations use ---------------------------------- */
    {
        /* A bare command line, argc == 1. (Passing "" would test an empty positional.) */
        const char *argv[] = {"lba2cc"};
        expect("no flags at all", 0, 1, argv);
    }
    ACCEPT("bare positional", "somefile");
    ACCEPT("help-all is a known flag", "--help-all");
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
    REJECT("single-dash flag standing in for a value", "--exec-at", "5", "-h");

    /* --- --help only counts in FLAG position; --help-all is a distinct tier ----------- */
    /* Scanning every token would let a flag's value hijack the run: `--exec-at 5 -h` would
       print the usage text and exit 0 instead of running. --help returns 1 (player subset),
       --help-all returns 2 (whole surface); both are truthy so the caller prints and exits. */
    {
        const char *yes1[] = {"lba2cc", "--headless", "--help"};
        const char *yes2[] = {"lba2cc", "-h"};
        const char *yes3[] = {"lba2cc", "--typo", "--help"}; /* unknown flag first */
        const char *all1[] = {"lba2cc", "--help-all"};
        const char *no1[] = {"lba2cc", "--exec-at", "5", "-h", "--tick", "10"};
        const char *no2[] = {"lba2cc", "--exec", "--help"};
        const char *no3[] = {"lba2cc", "--tick", "30"};
        const char *no4[] = {"lba2cc", "--exec", "--help-all"}; /* help-all as a value */

        struct {
            const char *what;
            int want;
            int argc;
            const char *const *argv;
        } cases[] = {
            {"--help in flag position", 1, 3, yes1},
            {"-h in flag position", 1, 2, yes2},
            {"--help after an unknown flag", 1, 3, yes3},
            {"--help-all in flag position", 2, 2, all1},
            {"-h as a value must not trigger help", 0, 6, no1},
            {"--help as a value must not trigger help", 0, 3, no2},
            {"no help asked for", 0, 3, no3},
            {"--help-all as a value must not trigger help", 0, 3, no4},
        };

        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char *mutableArgv[8];
            for (int a = 0; a < cases[i].argc; a++) {
                mutableArgv[a] = const_cast<char *>(cases[i].argv[a]);
            }
            int got = Cli_WantsHelp(cases[i].argc, mutableArgv);
            if (got != cases[i].want) {
                std::printf("FAIL Cli_WantsHelp(%s) = %d, wanted %d\n", cases[i].what, got,
                            cases[i].want);
                failures++;
            }
        }
    }

    /* --- the two help tiers reach two different readers ------------------------------ */
    {
        char plain[8192];
        char full[8192];
        captureUsage(plain, sizeof(plain), 0);
        captureUsage(full, sizeof(full), 1);

        /* A release ships README.txt beside the binary but none of the repo's docs or
           sources, so neither tier may point a reader at a path only the source tree has. */
        static const char *const forbidden[] = {"docs/", ".md", "SOURCES/", "tests/"};
        for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); i++) {
            if (std::strstr(plain, forbidden[i]) != NULL ||
                std::strstr(full, forbidden[i]) != NULL) {
                std::printf("FAIL usage text points at '%s', which no release ships\n",
                            forbidden[i]);
                failures++;
            }
        }

        /* Plain --help is the player subset: the common knobs plus a pointer to the rest,
           and none of the automation or self-test flags that would make it a wall to skim. */
        static const char *const plainHas[] = {"--resolution", "--help-all"};
        for (size_t i = 0; i < sizeof(plainHas) / sizeof(plainHas[0]); i++) {
            if (std::strstr(plain, plainHas[i]) == NULL) {
                std::printf("FAIL plain --help is missing '%s'\n", plainHas[i]);
                failures++;
            }
        }
        static const char *const plainHasnt[] = {"--headless", "--exec-at", "--polyrec"};
        for (size_t i = 0; i < sizeof(plainHasnt) / sizeof(plainHasnt[0]); i++) {
            if (std::strstr(plain, plainHasnt[i]) != NULL) {
                std::printf("FAIL plain --help leaks the non-player flag '%s'\n", plainHasnt[i]);
                failures++;
            }
        }

        /* --help-all is the whole surface, player knobs and all. */
        static const char *const fullHas[] = {"--resolution", "--headless", "--exec-at",
                                              "--polyrec", "--save-load-test"};
        for (size_t i = 0; i < sizeof(fullHas) / sizeof(fullHas[0]); i++) {
            if (std::strstr(full, fullHas[i]) == NULL) {
                std::printf("FAIL --help-all is missing '%s'\n", fullHas[i]);
                failures++;
            }
        }
    }

    if (failures == 0) {
        std::printf("PASS test_cli_args\n");
        return 0;
    }
    std::printf("FAIL test_cli_args: %d failure(s)\n", failures);
    return 1;
}
