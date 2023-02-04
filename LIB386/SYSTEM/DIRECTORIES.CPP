#include <SYSTEM/DIRECTORIES.H>

#include <SYSTEM/LIMITS.H>

#include <SDL2/SDL_filesystem.h>
#include <string.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
void GetRunningPath(char* outPath) {
  static bool initialized = false;
  static char basePath[ADELINE_MAX_PATH] = "";

  if (!initialized) {
    initialized = true;

    char *sdlBasePath = SDL_GetBasePath();
    strncpy(basePath, sdlBasePath, ADELINE_MAX_PATH);
    basePath[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

    SDL_free(sdlBasePath);
  }

  strncpy(outPath, basePath, ADELINE_MAX_PATH);
}

void GetUserPath(char* outPath) {
  static bool initialized = false;
  static char userPath[ADELINE_MAX_PATH] = "";

  if (!initialized) {
    initialized = true;

    char *sdlUserPath = SDL_GetPrefPath("Twinsen", "LBA2");
    strncpy(userPath, sdlUserPath, ADELINE_MAX_PATH);
    userPath[ADELINE_MAX_PATH - 1] = '\0'; // Guarantee

    SDL_free(sdlUserPath);
  }

  strncpy(outPath, userPath, ADELINE_MAX_PATH);
}

void GetResourcesPath(char *outPath) {
  // TODO: Implement configurable game resources path
  GetRunningPath(outPath);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
