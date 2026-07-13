#include "CLI_ARGS.H"

#include <stdio.h>
#include <string.h>

/* nvalues = how many arguments follow the flag; the validator skips them so a value
   that happens to start with '-' is not mistaken for a flag. */
typedef struct {
    const char *name;
    int nvalues;
    const char *args;
    const char *help;
} T_CLI_FLAG;

static const T_CLI_FLAG CliFlags[] = {
    /* General */
    {"--help", 0, "", "print this help and exit"},
    {"-h", 0, "", "same as --help"},
    {"--version", 0, "", "print the engine version and exit"},
    {"--game-dir", 1, "<dir>", "path to the retail game data (Common/)"},
    {"--data-dir", 1, "<dir>", "alias of --game-dir"},
    {"--pick-game-dir", 0, "", "always show the game-data folder picker"},
    {"--language", 1, "<name>", "override the language for this run"},
    {"--resolution", 1, "<WxH>", "override the boot render resolution"},
    {"--no-audio", 0, "", "start with sound and music disabled"},
    {"--log-level", 1, "<level>", "debug | info | warn | error (default info)"},
    {"--verbose", 0, "", "verbose control-harness logging"},

    /* Control harness: see docs/CONTROL.md */
    {"--headless", 0, "", "no window, no audio device: the only supported mode for\n"
                          "                             automation (a visible window pauses the game when it\n"
                          "                             loses focus, which makes a run nondeterministic)"},
    {"--load", 1, "<slot>", "restore a save before the loop starts"},
    {"--exec", 1, "<cmds>", "run ';'-separated console commands on the first tick"},
    {"--exec-at", 2, "<tick> <cmds>", "run console commands at tick <tick> (repeatable).\n"
                                      "                             Use this rather than chaining onto --exec when a command\n"
                                      "                             depends on a scene change having settled."},
    {"--tick", 1, "<n>", "advance n simulation ticks"},
    {"--exit", 0, "", "quit once the tick budget is spent"},
    {"--fixed-dt", 1, "<ms>", "advance the clock by a constant <ms> per tick (deterministic;\n"
                              "                             requires --headless to hold)"},
    {"--fixed-timestep", 1, "<ms>", "set the sim throttle for this run only (0 = off)"},
    {"--no-autosave", 0, "", "do not write save files during this run (protects the\n"
                             "                             player's Resume slot, which --load + --tick would otherwise\n"
                             "                             overwrite)"},
    {"--demo", 0, "", "run attract/demo mode (only does anything on a demo scene)"},
    {"--screenshot", 1, "<path>", "write a PNG of the rendered frame"},
    {"--dump-state", 1, "<path>", "write a JSON snapshot of engine state"},

    /* Test hooks */
    {"--polyrec", 1, "<path>", "record polygon draw calls (ENABLE_POLY_RECORDING builds)"},
    {"--capture-projection", 1, "<path>", "capture projection records"},
    {"--projection-hash", 1, "<path>", "write the projection hash"},
    {"--res-switch-test", 1, "<spec>", "runtime resolution-switch self-test"},
    {"--save-load-test", 1, "<path>", "save/load round-trip self-test"},
};

static const int CliFlagCount = (int)(sizeof(CliFlags) / sizeof(CliFlags[0]));

static const T_CLI_FLAG *FindFlag(const char *name) {
    int i;
    for (i = 0; i < CliFlagCount; i++) {
        if (!strcmp(CliFlags[i].name, name))
            return &CliFlags[i];
    }
    return NULL;
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
        printf("  %-26s %s\n", left, CliFlags[i].help);
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

        /* Not a flag: a positional, or a value we already stepped over. Leave it be,
           since the shell or OS can hand us paths we don't own. */
        if (strncmp(a, "-", 1) != 0) {
            i++;
            continue;
        }

        const T_CLI_FLAG *flag = FindFlag(a);

        if (!flag) {
            fprintf(stderr, "error: unknown option '%s'\n\n", a);
            fprintf(stderr, "A misspelled flag used to be ignored, which meant the run silently\n"
                            "used a default instead of what you asked for. Run with --help.\n");
            return 1;
        }

        i += 1 + flag->nvalues;
    }

    return 0;
}
