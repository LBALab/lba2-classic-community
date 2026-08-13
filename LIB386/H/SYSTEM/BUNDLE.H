#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/// Writes the directory holding the file the user launched, for packagings
/// that run the executable from somewhere other than where that file lives.
/// An AppImage is the case today: it mounts itself under /tmp, so the ordinary
/// "folder the binary is in" answer is the mount point rather than the folder
/// the player put the file in.
///
/// @param execDir Directory the running executable sits in. Required: the
///        packaging is only believed when this is inside the packaging's own
///        mount, because the environment naming it is inherited by every child
///        process and an unrelated build launched from one would otherwise
///        answer for a file it has nothing to do with.
/// @param out Receives the directory, with a trailing separator.
/// @param maxLen Size of \p out, including the null terminator.
/// @param outKindName Receives a static, player-facing name for the packaging
///        ("AppImage"), for messages quoting where a path came from. Always
///        written when the return is 1; left untouched otherwise.
/// @return 1 on success, 0 otherwise, including an ordinary un-packaged run,
///         an environment belonging to a different process, and a directory
///         too long to hold.
S32 Bundle_GetLauncherDir(const char *execDir, char *out, U16 maxLen,
                          const char **outKindName);

// =============================================================================
#ifdef __cplusplus
}
#endif
