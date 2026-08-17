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
   strcasecmp), so the validator must too, or it would reject a spelling that parser accepts.
   Everything else is matched exactly: accepting a case variant here for a flag whose parser
   is case-sensitive would be worse than rejecting it, because the run would then boot with
   the flag silently ignored.

   common = shown in plain `--help`, the handful a player setting up a run actually reaches
   for. Everything else is automation or a self-test and appears only under `--help-all`, so
   the player-facing help isn't a wall of flags they'll never type.

   writes = the flag may change the settings and saves in the user directory, and is allowed
   to. Almost nothing may: a flag names a mode for one run, and a run told to do something
   differently must not leave that behind for the next one. The exceptions are the flags
   whose whole job is to act on stored state, where persisting is the point rather than a
   side effect.

   The distinction is not decoration. --fixed-timestep and --language each overrode a setting
   the config write serialises out of a global, so one run with either changed it for good;
   --resolution wrote the size a harness pinned for a screenshot over the one the player
   picked. Each contradicted its own help text, and nothing noticed, because a leaked setting
   only shows up as the next run behaving differently for no visible reason.
   tests/automation/test_cli_flag_contract.sh holds every flag to the column: the ones marked
   0 must leave the settings byte-identical, and the ones marked 1 must actually write, or a
   flag that quietly stopped working would read as a passing contract. */
typedef struct {
    const char *name;
    int nvalues;
    int nocase;
    int common;
    int writes;
    const char *section; /* NULL to continue the previous one */
    const char *args;
    const char *help;
} T_CLI_FLAG;

/* Column where help text starts. Shared by the format string and the continuation-line
   indent, so widening one can't silently misalign the other. */
#define CLI_HELP_COL 29

static const T_CLI_FLAG CliFlags[] = {
    {"--help", 0, 0, 0, 0, "Playing", "", "print this help and exit"},
    {"-h", 0, 0, 0, 0, NULL, "", "same as --help"},
    {"--resolution", 1, 0, 1, 0, NULL, "<WxH>", "render resolution, e.g. 1280x720. If the picture\n"
                                                "ever comes up wrong, 640x480 always works"},
    {"--language", 1, 0, 1, 0, NULL, "<name>", "language for this run (English, Francais, ...)"},
    {"--no-audio", 0, 0, 1, 0, NULL, "", "start with sound and music off"},
    {"--game-dir", 1, 1, 1, 0, NULL, "<dir>", "folder holding the game data, the one with the\n"
                                              "HQR files in it (often Common/)"},
    {"--data-dir", 1, 1, 0, 0, NULL, "<dir>", "same as --game-dir"},
    /* Selects which directory the run writes to; it does not add writes of its own. */
    {"--user-dir", 1, 0, 1, 0, NULL, "<dir>", "folder for this run's saves, settings and log,\n"
                                              "created if missing. LBA2_USER_DIR does the same.\n"
                                              "Use it to keep two installs apart, or to run\n"
                                              "against a fresh folder"},
    /* Creates the profile on first use and records the install it was given, which is what
       lets a later run name the profile alone. Recording is the feature, so: writes. */
    {"--profile", 1, 0, 1, 1, NULL, "<name>", "keep this run's saves and settings under a name of\n"
                                              "their own, so several installs don't share them.\n"
                                              "LBA2_PROFILE does the same. No name means the\n"
                                              "usual folder"},
    {"--disc", 1, 1, 0, 0, NULL, "<file>", "mount this disc image instead of picking one\n"
                                           "from the game folder. Takes an image or a cue\n"
                                           "(the cue names its image). Use it when a folder\n"
                                           "holds more than one, or the image lives\n"
                                           "elsewhere"},
    /* Picking a folder is the act of choosing the one to remember. */
    {"--pick-game-dir", 0, 1, 0, 1, NULL, "", "choose the game-data folder, ignoring any\n"
                                              "remembered one"},
    /* Moving a profile to a different install is its own act, because --game-dir alone is
       for the run that names it. Recording the move is the point, so: writes. */
    {"--bind-game-dir", 0, 1, 0, 1, NULL, "", "point this profile at the folder named on this\n"
                                              "command line, replacing the one it remembers"},
    {"--version", 0, 0, 1, 0, NULL, "", "print the engine version and exit"},
    {"--help-all", 0, 0, 1, 0, NULL, "", "list every option, including automation and\n"
                                         "engine self-tests"},

    {"--headless", 0, 0, 0, 0, "Automation", "", "no window, no audio device. Use this to drive the\n"
                                                 "game from a script: a visible window pauses the\n"
                                                 "game whenever it loses focus, which makes a run\n"
                                                 "unreproducible. Implies --no-audio."},
    {"--no-autosave", 0, 0, 0, 0, NULL, "", "don't write autosaves. Without this, walking the\n"
                                            "hero into a new scene rewrites your autosave.\n"
                                            "Saving from the menu still works."},
    /* Restoring a save makes its player the current one, which the config records in
       LastSave exactly as loading from the menu does. */
    {"--load", 1, 0, 0, 1, NULL, "<slot>", "restore a save before the run starts"},
    /* The console is the engine's imperative channel: `vsync off` and `fixedtimestep 40`
       are the player choosing, and they persist whether typed in-game or sent from here.
       That is the whole distinction this column draws, so these two are the flags that
       carry commands rather than name a mode. */
    {"--exec", 1, 0, 0, 1, NULL, "<cmds>", "run ';'-separated console commands on the first tick"},
    {"--exec-at", 2, 0, 0, 1, NULL, "<tick> <cmds>", "run console commands at tick <tick>; repeatable.\n"
                                                     "Use this when a command depends on a scene change\n"
                                                     "having settled, which --exec cannot wait for."},
    /* Same command bus again, but open-ended: --exec and --exec-at need the whole
       plan before boot, and a probe loop whose next step depends on the last one
       cannot be written that way. Requires a build with LBA2_CONTROL_SERVER. */
    {"--listen", 1, 0, 0, 1, NULL, "<port>", "accept console commands on 127.0.0.1:<port> while\n"
                                             "the run proceeds, for probing a live engine rather\n"
                                             "than scripting the run in advance. Debug builds only"},
    {"--tick", 1, 0, 0, 0, NULL, "<n>", "advance n simulation ticks"},
    {"--exit", 0, 0, 0, 0, NULL, "", "quit once the tick budget is spent"},
    {"--fixed-dt", 1, 0, 0, 0, NULL, "<ms>", "advance the clock by a constant <ms> per tick, so a\n"
                                             "run reproduces exactly (needs --headless)"},
    {"--fixed-timestep", 1, 0, 0, 0, NULL, "<ms>", "set the sim throttle for this run only (0 = off)"},
    {"--vsync", 1, 0, 0, 0, NULL, "<on|off>", "set the vsync setting for this run only, so a\n"
                                              "capture reads it instead of the local one"},
    {"--screenshot", 1, 0, 0, 0, NULL, "<path>", "write a PNG of the rendered frame"},
    {"--dump-state", 1, 0, 0, 0, NULL, "<path>", "write the engine state as JSON"},
    {"--demo", 0, 0, 0, 0, NULL, "", "attract/demo mode (only does anything on a demo scene)"},
    {"--log-level", 1, 0, 0, 0, NULL, "<level>", "debug | info | warn | error (default info)"},
    {"--verbose", 0, 0, 0, 0, NULL, "", "log scene and hero state as the run proceeds"},

    {"--polyrec", 1, 0, 0, 0, "Engine self-tests", "<path>", "record polygon draw calls (ENABLE_POLY_RECORDING builds)"},
    {"--capture-projection", 1, 0, 0, 0, NULL, "<path>", "capture projection records"},
    {"--projection-hash", 1, 0, 0, 0, NULL, "<path>", "write the projection hash"},
    {"--res-switch-test", 1, 0, 0, 0, NULL, "<spec>", "runtime resolution-switch self-test"},
    {"--save-load-test", 1, 0, 0, 0, NULL, "<path>", "save/load round-trip self-test"},
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

/* A flag standing where a value belongs is a flag the user meant to pass, not a value.
   Without this, `--exec-at 5 --tick 30` parses cmds="--tick" and swallows the 30, leaving a
   run that ticks zero times and still exits 0. Single-dash spellings count: `-h` is a flag. */
static int LooksLikeFlag(const char *s) {
    if (!s || s[0] != '-') {
        return 0;
    }
    return s[1] == '-' || FindFlag(s) != NULL;
}

static void PrintHelpText(FILE *out, const char *help) {
    const char *p = help;

    while (1) {
        const char *nl = strchr(p, '\n');
        if (!nl) {
            fprintf(out, "%s\n", p);
            return;
        }
        /* Continuation lines are indented from CLI_HELP_COL, not hand-padded in the table. */
        fprintf(out, "%.*s\n%*s", (int)(nl - p), p, CLI_HELP_COL, "");
        p = nl + 1;
    }
}

static void PrintFlagLine(FILE *out, const T_CLI_FLAG *flag) {
    char left[64];

    snprintf(left, sizeof(left), "%s %s", flag->name, flag->args);

    /* A flag too long for the column wraps to the next line rather than running into its
       own help text. */
    fprintf(out, "  %s", left);
    if (strlen(left) + 2 < CLI_HELP_COL) {
        fprintf(out, "%*s", (int)(CLI_HELP_COL - 2 - strlen(left)), "");
    } else {
        fprintf(out, "\n%*s", CLI_HELP_COL, "");
    }
    PrintHelpText(out, flag->help);
    /* Said out loud, because the alternative is a player discovering it by finding a setting
       changed. Everything unmarked applies to the one run and leaves the settings alone,
       which is worth being able to rely on rather than assume. Also the one machine-readable
       copy of the `writes` column, which is what test_cli_flag_contract.sh checks itself
       against, so a new flag cannot be added to the table without the test seeing it. */
    if (flag->writes)
        fprintf(out, "%*s[keeps this in your settings]\n", CLI_HELP_COL, "");
}

void Cli_PrintUsage(const char *exeName, int full) {
    Cli_PrintUsageTo(stdout, exeName, full);
}

void Cli_PrintUsageTo(FILE *out, const char *exeName, int full) {
    int i;

    fprintf(out, "Little Big Adventure 2: Classic (community source port)\n\n");
    fprintf(out, "Usage: %s [options]\n\n", exeName && exeName[0] ? exeName : "lba2cc");

    if (!full) {
        /* Plain --help stays short: only the flags a player setting up a run reaches for. The
           automation and self-test surface is one --help-all away, listed there. */
        fprintf(out, "Run lba2cc with no options to play. Common options:\n\n");
        for (i = 0; i < CliFlagCount; i++) {
            if (CliFlags[i].common)
                PrintFlagLine(out, &CliFlags[i]);
        }
        return;
    }

    for (i = 0; i < CliFlagCount; i++) {
        /* -h is listed with --help; don't repeat it. */
        if (!strcmp(CliFlags[i].name, "-h"))
            continue;

        if (CliFlags[i].section) {
            fprintf(out, "%s%s:\n", i ? "\n" : "", CliFlags[i].section);
        }
        PrintFlagLine(out, &CliFlags[i]);
    }

    /* The CLI and the in-game console are two front-ends to one engine. --exec/--exec-at are
       the seam: they feed the same console you open in-game, so the CLI never re-documents a
       console command, it points at the console's own help, which a batch run reads on stdout.
       The launch flags above set up a run; the verbs you apply to a live world live there. */
    fprintf(out, "\nDriving the engine from the console:\n");
    fprintf(out, "  --exec and --exec-at feed the same console you open in-game. The console owns\n");
    fprintf(out, "  the runtime verbs; ask it what it has, and it answers on stdout:\n");
    fprintf(out, "  lba2cc --headless --exec \"cmdlist\"   --tick 2 --exit   list the console commands\n");
    fprintf(out, "  lba2cc --headless --exec \"help cube\" --tick 2 --exit   help for one of them\n");
    fprintf(out, "  lba2cc --headless --exec \"listsaves\" --tick 2 --exit   the names --load takes\n");
}

int Cli_WantsHelp(int argc, char *argv[]) {
    int i = 1;

    /* Walk by arity, so only a token in FLAG position counts. Scanning every token would let
       a flag's value hijack the run: `--exec-at 5 -h` would print usage and exit 0 instead of
       running. An unknown flag is stepped over as if it took no values; Cli_ValidateArgs
       rejects it afterwards, and `--typo --help` still prints help. */
    while (i < argc) {
        const T_CLI_FLAG *flag;

        if (strncmp(argv[i], "-", 1) != 0) {
            i++;
            continue;
        }

        if (!strcmp(argv[i], "--help-all")) {
            return 2;
        }
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            return 1;
        }

        flag = FindFlag(argv[i]);
        i += flag ? 1 + flag->nvalues : 1;
    }
    return 0;
}

int Cli_FlagArity(const char *arg) {
    const T_CLI_FLAG *flag;

    if (arg == NULL) {
        return -1;
    }
    flag = FindFlag(arg);
    return flag ? flag->nvalues : -1;
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
            fprintf(stderr, "Flags are never ignored: a typo would leave the run on a default\n"
                            "and hand back a confident, wrong answer. Run with --help.\n");
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
