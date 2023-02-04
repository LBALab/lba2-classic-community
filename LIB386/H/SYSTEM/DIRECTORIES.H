#pragma once

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
void GetRunningPath(char* outPath);   ///< Path where application was run
void GetUserPath(char* outPath);      ///< Path for user writable data
void GetResourcesPath(char *outPath); ///< Path for game resources

// =============================================================================
#ifdef __cplusplus
}
#endif
