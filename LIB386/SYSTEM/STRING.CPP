#include <SYSTEM/STRING.H>

#include <SDL2/SDL_stdinc.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
char *ade_strcasestr(const char *haystack, const char *needle) {
  return SDL_strcasestr(haystack, needle);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
