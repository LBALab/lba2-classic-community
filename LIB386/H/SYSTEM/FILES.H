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

S32 Read(S32 handle, void *buffer, U32 maxReadBytes);
S32 Write(S32 handle, const void *buffer, U32 maxWriteBytes);
S32 Seek(S32 handle, S32 offset, ESeekOrigin origin);

// --- Atomic (crash-safe) writes ----------------------------------------------
// Plain OpenWrite() truncates the target to zero the instant it opens; a crash,
// kill, OOM, or power loss before Close() leaves a truncated or partial file.
// For files that must never be observed half-written (the config), write to a
// temp file alongside the target, flush it, then atomically rename it onto the
// target. The target is then always either the old complete content or the new
// complete content -- never anything in between.
//
// Usage mirrors OpenWrite/Write/Close: open, Write() as many times as needed,
// then Commit (success) or Abort (give up, leave target untouched). The handle
// is an ordinary write handle, so Write()/Seek() work on it unchanged.
//
//   S32 h = OpenWriteAtomic(path);   // 0 on failure
//   Write(h, ...);                   // any number of times
//   if (!CommitWriteAtomic(h)) { ... }  // 1 = renamed onto target, 0 = failed
//                                       // (temp cleaned up; target untouched)
//
// On any path failure mid-stream, call AbortWriteAtomic(h) to drop the temp.
S32 OpenWriteAtomic(const char *finalPath);
S32 CommitWriteAtomic(S32 handle);
void AbortWriteAtomic(S32 handle);

// Convenience: atomically write a whole buffer in one call (open + write +
// commit, with automatic cleanup on any failure). Returns 1 on success.
S32 WriteFileAtomic(const char *finalPath, const void *buffer, U32 size);

U32 Delete(const char *filename);
S32 Touch(const char *filename);
U32 FileSize(const char *filename);

/// Check if file or directory \p path exists.
bool ExistsFileOrDir(const char *path);
bool IsDirectory(const char *path);

// =============================================================================
#ifdef __cplusplus
}
#endif
