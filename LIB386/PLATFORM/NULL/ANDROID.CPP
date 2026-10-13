#include <SYSTEM/ANDROID.H>

S32 IsAndroidTVDevice(void) { return 0; }
bool Android_EnsureExternalStoragePermission(void) { return true; }
bool Android_HasExternalStoragePermission(void) { return true; }
void Android_SavePreviousCrashReport(const char *) {}
const char *Android_GetPreviousCrashNote(void) { return ""; }
S32 Android_GetExternalFilesDir(char *, U16) { return 0; }
