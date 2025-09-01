#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
enum ESeekOrigin {
  SEEK_FROM_START,
  SEEK_FROM_CURRENT,
  SEEK_FROM_END
};

// -----------------------------------------------------------------------------
S32 OpenRead(const char *filename);
S32 OpenWrite(const char *filename);
S32 OpenReadWrite(const char *filename);
S32 Close(S32 handle);

S32 Read(S32 handle, void* buffer, U32 maxReadBytes);
S32 Write(S32 handle, const void* buffer, U32 maxWriteBytes);
S32 Seek(S32 handle, S32 offset, ESeekOrigin origin);

U32 Delete(const char* filename);
S32 Touch(const char* filename);
U32 FileSize(const char* filename);

/// Check if file or directory \p path exists.
bool ExistsFileOrDir(const char* path);
bool IsDirectory(const char* path);

// =============================================================================
#ifdef __cplusplus
}
#endif
