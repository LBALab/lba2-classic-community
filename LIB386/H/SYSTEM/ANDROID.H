#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/// 1 on Android TV / leanback, 0 on phone/tablet, on any error, or off-Android.
S32 IsAndroidTVDevice(void);

/// Ensures MANAGE_EXTERNAL_STORAGE before anything downstream picks a folder.
/// When it is missing this opens app Settings and waits a bounded while for the
/// answer, because raising that screen puts the game in the background: every
/// path chosen while it is up would be chosen on the wrong information, and a
/// window created while it is up cannot be created at all.
/// Returns true when the permission is held on return. True off-Android, where
/// there is nothing to ask for.
bool Android_EnsureExternalStoragePermission(void);

/// Whether MANAGE_EXTERNAL_STORAGE is held right now. Asks nothing and opens
/// nothing, so it is safe to call when reporting a failure. True off-Android,
/// where no permission stands between the engine and the game data.
bool Android_HasExternalStoragePermission(void);

/// Saves the system's report of the app's newest native crash not saved before,
/// its tombstone, as crash-<time>.pb in dir, which is the log's folder, replacing
/// the one saved before. Call once, after the log exists. Does nothing
/// off-Android, and before Android 11, which keeps no exit records.
void Android_SavePreviousCrashReport(const char *dir);

/// What Android_SavePreviousCrashReport found, as a sentence for the boot banner,
/// or "" when it found no new crash. "" off-Android.
const char *Android_GetPreviousCrashNote(void);

/// Writes the app-specific external files dir into out (up to maxLen).
/// Returns 1 on success, 0 otherwise (including off-Android).
S32 Android_GetExternalFilesDir(char *out, U16 maxLen);

// =============================================================================
#ifdef __cplusplus
}
#endif
