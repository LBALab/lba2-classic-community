#pragma once

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
void StripExt(char *fname);
char *AddExt(char *path, const char *ext);
char *GetFileName(char *pathname);

// =============================================================================
#ifdef __cplusplus
}
#endif
