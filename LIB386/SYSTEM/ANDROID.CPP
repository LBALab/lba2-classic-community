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
    struct JniCheck {
        static S32 Pending(JNIEnv *e) {
            if (e->ExceptionCheck()) {
                LogPrintf("TouchOverlay: JNI exception in TV detection, "
                          "overlay disabled\n");
                e->ExceptionClear();
                return 1;
            }
            return 0;
        }
    };
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (env == NULL)
        return 0;

    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (activity == NULL)
        return 0;

    jclass activityClass = env->GetObjectClass(activity);
    if (JniCheck::Pending(env) || activityClass == NULL)
        return 0;

    jmethodID getPM = env->GetMethodID(activityClass, "getPackageManager",
                                       "()Landroid/content/pm/PackageManager;");
    if (JniCheck::Pending(env) || getPM == NULL) {
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    jobject pm = env->CallObjectMethod(activity, getPM);
    if (JniCheck::Pending(env) || pm == NULL) {
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    jclass pmClass = env->GetObjectClass(pm);
    if (JniCheck::Pending(env) || pmClass == NULL) {
        env->DeleteLocalRef(pm);
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    jmethodID hasFeature = env->GetMethodID(pmClass, "hasSystemFeature",
                                            "(Ljava/lang/String;)Z");
    if (JniCheck::Pending(env) || hasFeature == NULL) {
        env->DeleteLocalRef(pmClass);
        env->DeleteLocalRef(pm);
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    jstring feature = env->NewStringUTF("android.software.leanback");
    if (JniCheck::Pending(env) || feature == NULL) {
        env->DeleteLocalRef(pmClass);
        env->DeleteLocalRef(pm);
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    S32 isTV = (S32)env->CallBooleanMethod(pm, hasFeature, feature);
    JniCheck::Pending(env);

    env->DeleteLocalRef(feature);
    env->DeleteLocalRef(pmClass);
    env->DeleteLocalRef(pm);
    env->DeleteLocalRef(activityClass);
    return isTV;
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
