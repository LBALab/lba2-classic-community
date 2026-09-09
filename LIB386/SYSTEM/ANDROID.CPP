/* Android platform layer.
 *
 * One home for the engine's Android specifics — JNI, TV (leanback) detection,
 * storage-permission prompting, and the app-specific external-files dir. Each
 * entry point is a no-op stub off-Android, so callers compile unconditionally
 * with no #ifdef. <jni.h> is included here and nowhere else.
 * See docs/PLATFORM.md and docs/ANDROID.md.
 */
#include <SYSTEM/ANDROID.H>
#include <SYSTEM/LOGPRINT.H>

#ifdef __ANDROID__
#include <jni.h>
#include <string.h>
#include <SDL3/SDL_system.h>
#include <SDL3/SDL_timer.h>
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

/* Waiting for an answer once Settings has been opened.

   The bound is spent time, not elapsed time: Android freezes a backgrounded
   process, so this loop does not run at all while the player is on the Settings
   screen. What it measures is how long the game sits in front of somebody with
   nothing happening, which is why it is short.

   A player who grants is detected on their next poll and never sees it. A player
   who declines used to serve out the whole bound looking at a blank game, so a
   return is treated as an answer in itself: coming back is visible here as a
   poll that took seconds of wall clock instead of the quarter second it asked
   for, which is the process being thawed. Whatever the permission says at that
   moment is the answer, and the boot carries on with it. */
#define ANDROID_PERM_WAIT_MS 15000
#define ANDROID_PERM_POLL_MS 250
#define ANDROID_PERM_THAW_MS 2000

bool Android_HasExternalStoragePermission(void) {
#ifdef __ANDROID__
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (env == NULL) {
        return false;
    }
    jclass permHelper = env->FindClass("org/lbalab/lba2cc/PermissionHelper");
    if (permHelper == NULL) {
        return false;
    }
    jmethodID isMgr =
        env->GetStaticMethodID(permHelper, "isExternalStorageManager", "()Z");
    const bool held =
        (isMgr != NULL) && env->CallStaticBooleanMethod(permHelper, isMgr) != JNI_FALSE;
    env->DeleteLocalRef(permHelper);
    return held;
#else
    return true;
#endif
}

bool Android_EnsureExternalStoragePermission(void) {
#ifdef __ANDROID__
    /* On Android 11+ (API 30+), direct access to /sdcard/<dir>/ requires
     * MANAGE_EXTERNAL_STORAGE. Check at startup and open Settings for it
     * if missing, so the user can grant it and re-launch. The fallback
     * probe in RES_DISCOVERY for App-specific external files dir
     * (/sdcard/Android/data/<pkg>/files/) works without this permission. */
    bool granted = false;
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (env != NULL) {
        jclass permHelper = env->FindClass("org/lbalab/lba2cc/PermissionHelper");
        if (permHelper != NULL) {
            jmethodID isMgr = env->GetStaticMethodID(
                permHelper, "isExternalStorageManager", "()Z");
            jmethodID openMgr = env->GetStaticMethodID(
                permHelper, "openExternalStorageManagerSettings",
                "(Ljava/lang/Object;)V");
            if (isMgr != NULL) {
                granted = env->CallStaticBooleanMethod(permHelper, isMgr) != JNI_FALSE;
            }
            if (isMgr != NULL && !granted) {
                jobject activity = (jobject)SDL_GetAndroidActivity();
                LogPrintf("MANAGE_EXTERNAL_STORAGE not granted. "
                          "Opening Settings for app.\n");
                if (openMgr != NULL && activity != NULL) {
                    env->CallStaticVoidMethod(permHelper, openMgr, activity);
                    /* Clear any JNI exception (e.g. ActivityNotFoundException
                     * on Android TV which has no Settings intent handler) so
                     * subsequent JNI calls (e.g. Android_GetExternalFilesDir
                     * in ResolveGameDataDir) don't fail. */
                    if (env->ExceptionCheck())
                        env->ExceptionClear();

                    /* Wait here rather than carrying on. The folder is chosen a
                       few lines after this call and the window is created not
                       long after that, and while Settings is up both would be
                       decided against a state the player is in the middle of
                       changing. */
                    U32 waited = 0;
                    U32 lastTick = SDL_GetTicks();
                    bool returned = false;
                    while (!granted && !returned && waited < ANDROID_PERM_WAIT_MS) {
                        SDL_Delay(ANDROID_PERM_POLL_MS);
                        const U32 tick = SDL_GetTicks();
                        const U32 slept = tick - lastTick;
                        lastTick = tick;
                        waited += ANDROID_PERM_POLL_MS;
                        granted =
                            env->CallStaticBooleanMethod(permHelper, isMgr) != JNI_FALSE;
                        /* Far more clock passed than was asked for, so this
                           process was frozen and has just been thawed: the
                           player is looking at the game again. */
                        returned = (slept > ANDROID_PERM_THAW_MS);
                    }
                    if (granted) {
                        LogPrintf("MANAGE_EXTERNAL_STORAGE granted after %ums.\n", waited);
                    } else if (returned) {
                        LogPrintf("MANAGE_EXTERNAL_STORAGE declined; continuing without "
                                  "it.\n");
                    } else {
                        LogPrintf("MANAGE_EXTERNAL_STORAGE unanswered after %ums; "
                                  "continuing without it.\n",
                                  waited);
                    }
                }
            }
            env->DeleteLocalRef(permHelper);
        }
    }
    return granted;
#else
    return true;
#endif
}

S32 Android_GetExternalFilesDir(char *out, U16 maxLen) {
#ifdef __ANDROID__
    /* App-specific external files dir (/sdcard/Android/data/<pkg>/files/), via
     * Activity.getExternalFilesDir(null).getAbsolutePath(). Always accessible
     * on API 19+ without MANAGE_EXTERNAL_STORAGE. Returns the raw path (no
     * trailing separator); the caller normalizes. */
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (env == NULL)
        return 0;

    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (activity == NULL)
        return 0;

    jclass activityClass = env->GetObjectClass(activity);
    if (activityClass == NULL)
        return 0;

    jmethodID getExternalFilesDir = env->GetMethodID(activityClass, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    if (getExternalFilesDir == NULL) {
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    jobject fileObj = env->CallObjectMethod(activity, getExternalFilesDir, NULL);
    if (fileObj == NULL) {
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    jclass fileClass = env->GetObjectClass(fileObj);
    jmethodID getAbsolutePath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    if (getAbsolutePath == NULL) {
        env->DeleteLocalRef(fileClass);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    jstring pathStr = (jstring)env->CallObjectMethod(fileObj, getAbsolutePath);
    if (pathStr == NULL) {
        env->DeleteLocalRef(fileClass);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(activityClass);
        return 0;
    }

    const char *pathChars = env->GetStringUTFChars(pathStr, NULL);
    S32 result = 0;
    if (pathChars != NULL) {
        size_t len = strlen(pathChars);
        if (len + 1 < (size_t)maxLen) {
            memcpy(out, pathChars, len + 1);
            result = 1;
        }
        env->ReleaseStringUTFChars(pathStr, pathChars);
    }

    env->DeleteLocalRef(pathStr);
    env->DeleteLocalRef(fileClass);
    env->DeleteLocalRef(fileObj);
    env->DeleteLocalRef(activityClass);
    return result;
#else
    (void)out;
    (void)maxLen;
    return 0;
#endif
}
