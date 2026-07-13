#include "CLI_ARGS.H"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define cli_strcasecmp _stricmp
#else
#include <strings.h>
#define cli_strcasecmp strcasecmp
#endif

/* nvalues = how many arguments follow the flag. The validator uses it to check the values
   are actually there, and to step over them so a value is never mistaken for a flag.

   nocase = the flag's own parser matches case-insensitively (RES_DISCOVERY.CPP uses
   strcasecmp), so the validator must too, or a spelling that used to work would now be
   rejected as unknown. Everything else is matched exactly: a case-insensitive accept here
   for a flag whose parser is case-sensitive would be worse than a reject, because the run
   would boot with the flag silently ignored. */
typedef struct {
    const char *name;
    int nvalues;
    int nocase;
    const char *args;
    const char *help;
} T_CLI_FLAG;

/* Column where help text starts. Shared by the format string and the continuation-line
   indent, so widening one can't silently misalign the other. */
#define CLI_HELP_COL 29

static const T_CLI_FLAG CliFlags[] = {
    /* General */
    {"--help", 0, 0, "", "print this help and exit"},
    {"-h", 0, 0, "", "same as --help"},
    {"--version", 0, 0, "", "print the engine version and exit"},
    {"--game-dir", 1, 1, "<dir>", "path to the retail game data (Common/)"},
    {"--data-dir", 1, 1, "<dir>", "alias of --game-dir"},
    {"--pick-game-dir", 0, 1, "", "always show the game-data folder picker"},
    {"--language", 1, 0, "<name>", "override the language for this run"},
    {"--resolution", 1, 0, "<WxH>", "override the boot render resolution"},
    {"--no-audio", 0, 0, "", "start with sound and music disabled"},
    {"--log-level", 1, 0, "<level>", "debug | info | warn | error (default info)"},
    {"--verbose", 0, 0, "", "verbose control-harness logging"},

    /* Control harness: see docs/CONTROL.md */
    {"--headless", 0, 0, "", "no window, no audio device. Use this for automation: a\n"
                             "visible window pauses the game when it loses focus, which\n"
                             "makes a run nondeterministic. Implies --no-audio."},
    {"--load", 1, 0, "<slot>", "restore a save before the loop starts"},
    {"--exec", 1, 0, "<cmds>", "run ';'-separated console commands on the first tick"},
    {"--exec-at", 2, 0, "<tick> <cmds>", "run console commands at tick <tick> (repeatable).\n"
                                         "Use this rather than chaining onto --exec when a\n"
                                         "command depends on a scene change having settled."},
    {"--tick", 1, 0, "<n>", "advance n simulation ticks"},
    {"--exit", 0, 0, "", "quit once the tick budget is spent"},
    {"--fixed-dt", 1, 0, "<ms>", "advance the clock by a constant <ms> per tick\n"
                                 "(deterministic, but only in a --headless run)"},
    {"--fixed-timestep", 1, 0, "<ms>", "set the sim throttle for this run only (0 = off)"},
    {"--no-autosave", 0, 0, "", "don't write autosaves (autosave.lba / current.lba), which\n"
                                "an in-game scene transition would otherwise rewrite on your\n"
                                "install. Explicit saves (menu Save, savebug) still work."},
    {"--demo", 0, 0, "", "run attract/demo mode (only does anything on a demo scene)"},
    {"--screenshot", 1, 0, "<path>", "write a PNG of the rendered frame"},
    {"--dump-state", 1, 0, "<path>", "write a JSON snapshot of engine state"},

    /* Test hooks */
    {"--polyrec", 1, 0, "<path>", "record polygon draw calls (ENABLE_POLY_RECORDING builds)"},
    {"--capture-projection", 1, 0, "<path>", "capture projection records"},
    {"--projection-hash", 1, 0, "<path>", "write the projection hash"},
    {"--res-switch-test", 1, 0, "<spec>", "runtime resolution-switch self-test"},
    {"--save-load-test", 1, 0, "<path>", "save/load round-trip self-test"},
};

static const int CliFlagCount = (int)(sizeof(CliFlags) / sizeof(CliFlags[0]));

static const T_CLI_FLAG *FindFlag(const char *name) {
    int i;

    for (i = 0; i < CliFlagCount; i++) {
        if (!strcmp(CliFlags[i].name, name))
            return &CliFlags[i];
    }
    for (i = 0; i < CliFlagCount; i++) {
        if (CliFlags[i].nocase && !cli_strcasecmp(CliFlags[i].name, name))
            return &CliFlags[i];
    }
    return NULL;
}

/* A value starting with "--" is a flag the user meant to pass, not a value. Catching it is
   what stops `--exec-at 5 --tick 30` from quietly parsing cmds="--tick", eating the 30, and
   leaving a run that ticks zero times and still exits 0. */
static int LooksLikeFlag(const char *s) {
    return s && s[0] == '-' && s[1] == '-';
}

static void PrintHelpText(const char *help) {
    const char *p = help;

    while (1) {
        const char *nl = strchr(p, '\n');
        if (!nl) {
            printf("%s\n", p);
            return;
        }
        /* Continuation lines are indented from CLI_HELP_COL, not hand-padded in the table. */
        printf("%.*s\n%*s", (int)(nl - p), p, CLI_HELP_COL, "");
        p = nl + 1;
    }
}

void Cli_PrintUsage(const char *exeName) {
    int i;

    printf("Little Big Adventure 2: Classic (community source port)\n\n");
    printf("Usage: %s [options]\n\n", exeName && exeName[0] ? exeName : "lba2cc");

    for (i = 0; i < CliFlagCount; i++) {
        char left[64];

        /* -h is listed with --help; don't repeat it. */
        if (!strcmp(CliFlags[i].name, "-h"))
            continue;

        snprintf(left, sizeof(left), "%s %s", CliFlags[i].name, CliFlags[i].args);
        printf("  %-*s", CLI_HELP_COL - 2, left);
        PrintHelpText(CliFlags[i].help);
    }

    printf("\nAutomation runs should pass --headless; see docs/CONTROL.md.\n");
}

int Cli_WantsHelp(int argc, char *argv[]) {
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
            return 1;
    }
    return 0;
}

int Cli_ValidateArgs(int argc, char *argv[]) {
    int i = 1;

    while (i < argc) {
        const char *a = argv[i];
        const T_CLI_FLAG *flag;
        int v;

        /* Not a flag: a positional, or a value we already stepped over. Leave it be,
           since the shell or OS can hand us paths we don't own. */
        if (strncmp(a, "-", 1) != 0) {
            i++;
            continue;
        }

        flag = FindFlag(a);

        if (!flag) {
            fprintf(stderr, "error: unknown option '%s'\n\n", a);
            fprintf(stderr, "A misspelled flag used to be ignored, which meant the run silently\n"
                            "used a default instead of what you asked for. Run with --help.\n");
            return 1;
        }

        /* Every value the flag declares must be there, and none of them may look like a
           flag. Without this the flag's own parser either drops itself silently (its
           `has_val` guard fails) or swallows the next flag as a value, and the run proceeds
           on defaults: the same silent-wrong-answer this validator exists to stop. */
        for (v = 1; v <= flag->nvalues; v++) {
            if (i + v >= argc) {
                fprintf(stderr, "error: option '%s' needs %s\n", flag->name, flag->args);
                return 1;
            }
            if (LooksLikeFlag(argv[i + v])) {
                fprintf(stderr, "error: option '%s' needs %s, but got the flag '%s'\n",
                        flag->name, flag->args, argv[i + v]);
                return 1;
            }
        }

        i += 1 + flag->nvalues;
    }

    return 0;
}
