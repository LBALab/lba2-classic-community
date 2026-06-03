/* Android platform layer.
 *
 * One home for the engine's Android specifics (JNI, TV detection, storage
 * permission, external-files dir). Every entry point is a no-op stub
 * off-Android, so callers compile unconditionally with no #ifdef. <jni.h> is
 * included here and nowhere else. See docs/PLATFORM.md and docs/ANDROID.md.
 *
 * Real implementations are migrated in one at a time (Phase 2 / A'); the
 * bodies below are placeholders that compile and link on every platform.
 */
#include <SYSTEM/ANDROID.H>
#include <SYSTEM/LOGPRINT.H>

#ifdef __ANDROID__
#include <jni.h>
#include <SDL3/SDL_system.h>
#endif

S32 IsAndroidTVDevice(void) {
#ifdef __ANDROID__
    /* Real implementation moves here in Phase 2a. */
    return 0;
#else
    return 0;
#endif
}

void Android_EnsureExternalStoragePermission(void) {
#ifdef __ANDROID__
    /* Real implementation moves here in Phase 2c. */
    return;
#else
    return;
#endif
}

S32 Android_GetExternalFilesDir(char *out, U16 maxLen) {
    (void)out;
    (void)maxLen;
#ifdef __ANDROID__
    /* Real implementation moves here in PR A'. */
    return 0;
#else
    return 0;
#endif
}
