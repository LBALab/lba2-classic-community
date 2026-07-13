#ifndef CLI_ARGS_H
#define CLI_ARGS_H

/* Command-line surface: one table of every flag the engine accepts, used both to
   print `--help` and to reject unknown flags. Keep the table in sync when adding a
   flag anywhere (CONTROL.CPP, RES_DISCOVERY.CPP, PERSO.CPP): an absent entry means
   the flag is rejected as unknown. */

#ifdef __cplusplus
extern "C" {
#endif

/* Prints the usage text to stdout. */
void Cli_PrintUsage(const char *exeName);

/* Returns 1 if argv contains --help or -h. */
int Cli_WantsHelp(int argc, char *argv[]);

/* Rejects unknown --flags. Returns 0 when every flag is recognised; otherwise prints
   the offending flag plus the usage text to stderr and returns non-zero, so the caller
   can exit instead of booting the game with a silently-ignored typo. */
int Cli_ValidateArgs(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* CLI_ARGS_H */
