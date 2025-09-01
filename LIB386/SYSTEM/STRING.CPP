#include <SYSTEM/STRING.H>

#include <SDL2/SDL_stdinc.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
char *ade_strcasestr(const char *haystack, const char *needle) {
  // Copied from SDL since Ubuntu does not support the latest releases of SDL
  size_t length = SDL_strlen(needle);
  while (*haystack) {
    if (SDL_strncasecmp(haystack, needle, length) == 0) {
      return (char *) haystack;
    }
    ++haystack;
  }
  return NULL;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
