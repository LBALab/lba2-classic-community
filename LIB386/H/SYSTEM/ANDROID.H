#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/// 1 on Android TV / leanback, 0 on phone/tablet, on any error, or off-Android.
S32 IsAndroidTVDevice(void);

/// No-op unless MANAGE_EXTERNAL_STORAGE is missing; opens app Settings if so.
/// No-op off-Android.
void Android_EnsureExternalStoragePermission(void);

/// Writes the app-specific external files dir into out (up to maxLen).
/// Returns 1 on success, 0 otherwise (including off-Android).
S32 Android_GetExternalFilesDir(char *out, U16 maxLen);

// =============================================================================
#ifdef __cplusplus
}
#endif
