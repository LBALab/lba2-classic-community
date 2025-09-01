#include <SYSTEM/CMDLINE.H>

#include <SYSTEM/ADELINE.H>
#include <SYSTEM/DEFFILE.H>

#include <string.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define MAX_ARGC 30
#define MAX_EXTRA_CMD 256

// --- Public state ------------------------------------------------------------
S32 ArgC;
char *ArgV[MAX_ARGC + 1];

// --- Private state -----------------------------------------------------------
static char ExtraCmd[MAX_EXTRA_CMD];
static char Stop = ' ';

// --- Private functions -------------------------------------------------------
static inline char *GetNextStart(char *ptr) {
  while (*ptr == ' ')
    ptr++;

  if (*ptr == '"') {
    Stop = '"';
    ptr++;
    while (*ptr == ' ')
      ptr++;
  } else {
    Stop = ' ';
  }

  return ptr;
}

static inline char *GetNextEnd(char *ptr) {
  while (*ptr && *ptr != Stop)
    ptr++;

  if (*ptr == '"') {
    *ptr = ' ';
    while (*--ptr == ' ')
      ;

    ptr++;
  }

  return ptr;
}

static char *GetNextParam(char **param) {
  char *ptr, *end;

  ptr = GetNextStart(*param);
  if (!*ptr) {
    return NULL;
  }

  end = GetNextEnd(ptr);
  if (*end) {
    *end = 0;
    end++;
  }

  *param = end;

  return ptr;
}

static void ParseString(char *str) {
  while (ArgC < MAX_ARGC) {
    char *tmp;

    tmp = GetNextParam(&str);
    if (!tmp) {
      break;
    }

    ArgV[ArgC++] = tmp;
  }
}

// --- Interface ---------------------------------------------------------------
void RemoveParam(S32 param) {
  if (param >= ArgC)
    return;

  for (; param < ArgC; param++) {
    ArgV[param] = ArgV[param + 1];
  }

  ArgC--;
}

S32 FindAndRemoveParam(const char *str) {
  S32 i, res;

  res = FALSE;

restart:
  for (i = 1; i < ArgC; i++) {
    if (!strcasecmp(ArgV[i], str)) {
      res = TRUE;
      RemoveParam(i);
      goto restart;
    }
  }

  return res;
}

void GetCmdLine(S32 argc, char *argv[]) {
  char *ptr;

  ArgV[0] = argv[0];

  ArgC = 1;

  ptr = DefFileBufferReadString("CmdLine");

  if (ptr) {
    strncpy(ExtraCmd, ptr, MAX_EXTRA_CMD - 2);
    ParseString(ExtraCmd);
  }

  argc = ade_min(argc - 1, MAX_ARGC - ArgC);

  memcpy(&ArgV[ArgC], &argv[1], argc * sizeof(char *));

  ArgC += argc;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
