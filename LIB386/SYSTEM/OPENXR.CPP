/* OpenXR runtime layer — minimal Quest 3 input + immersive frame present.
 *
 * Optional, behind LBA2_OPENXR. On Android + OpenXR build the real
 * session/reference-space/frame loop lives here; every other platform
 * sees no-op stubs so callers compile unconditionally with no #ifdef.
 *
 * The OpenXR C headers (openxr.h, openxr_platform.h) and the prebuilt
 * Android loader come from the official Khronos
 * org.khronos.openxr:openxr_loader_for_android AAR published to Maven
 * Central — same artifact Vice City VR for Quest uses. The headers are
 * Apache 2.0; the loader is Apache 2.0. We use the C API only because
 * the engine builds with C++98 (see AGENTS.md "Never modernize C++98
 * to C++11+") and the SDK's openxr.hpp helper requires C++14.
 *
 * Quest 3 controller input is mapped onto the same virtual-scancode
 * space JOYSTICK.CPP uses for SDL gamepads (K_GAMEPAD_*), so the
 * existing GamepadKeys[] bindings and remap screen pick it up
 * automatically. See LIB386/H/SYSTEM/KEYBOARD_KEYS.H for the
 * K_GAMEPAD_* range.
 *
 * License: Apache 2.0 throughout (Khronos OpenXR SDK). No Meta SDK is
 * fetched or linked. See docs/ANDROID.md for the rationale (the
 * Quest 3 forward loader requires xrInitializeLoaderKHR before
 * xrCreateInstance, which the Khronos 1.1.x loader performs correctly
 * against current Quest firmware).
 */
#include <SYSTEM/OPENXR.H>

#include <SYSTEM/LOG.H>

#include <cstdlib>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
#ifdef LBA2_OPENXR

#ifdef __ANDROID__
#define XR_USE_PLATFORM_ANDROID 1
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#include <jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <SDL3/SDL.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

/* Logcat bridge so we can see OpenXR init/frame state in `adb logcat`.
 * The Log_* API in SYSTEM/LOG.H goes to adeline.log + stderr, neither of
 * which is visible from `adb logcat` on a release APK; we still call
 * Log_Info/Log_Warn so the same lines land in adeline.log. */
#define OXR_TAG "lba2cc-openxr"
#define OXR_LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,  OXR_TAG, fmt, ##__VA_ARGS__)
#define OXR_LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  OXR_TAG, fmt, ##__VA_ARGS__)
#define OXR_LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, OXR_TAG, fmt, ##__VA_ARGS__)
static const char *OXR_ResultStr(XrResult r) {
    switch (r) {
        case XR_SUCCESS:                       return "XR_SUCCESS";
        case XR_ERROR_VALIDATION_FAILURE:      return "XR_ERROR_VALIDATION_FAILURE";
        case XR_ERROR_RUNTIME_FAILURE:          return "XR_ERROR_RUNTIME_FAILURE";
        case XR_ERROR_HANDLE_INVALID:          return "XR_ERROR_HANDLE_INVALID";
        case XR_ERROR_INSTANCE_LOST:           return "XR_ERROR_INSTANCE_LOST";
        case XR_ERROR_INITIALIZATION_FAILED:   return "XR_ERROR_INITIALIZATION_FAILED";
        case XR_ERROR_FORM_FACTOR_UNSUPPORTED: return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
        case XR_ERROR_SYSTEM_INVALID:          return "XR_ERROR_SYSTEM_INVALID";
        case XR_ERROR_SESSION_LOST:            return "XR_ERROR_SESSION_LOST";
        case XR_ERROR_LIMIT_REACHED:           return "XR_ERROR_LIMIT_REACHED";
        case XR_ERROR_FEATURE_UNSUPPORTED:     return "XR_ERROR_FEATURE_UNSUPPORTED";
        case XR_ERROR_EXTENSION_NOT_PRESENT:   return "XR_ERROR_EXTENSION_NOT_PRESENT";
        case XR_ERROR_PATH_UNSUPPORTED:        return "XR_ERROR_PATH_UNSUPPORTED";
        case XR_ERROR_NAME_DUPLICATED:         return "XR_ERROR_NAME_DUPLICATED";
        case XR_ERROR_NAME_INVALID:            return "XR_ERROR_NAME_INVALID";
        case XR_ERROR_ACTIONSET_NOT_ATTACHED:  return "XR_ERROR_ACTIONSET_NOT_ATTACHED";
        case XR_ERROR_ACTIONSETS_ALREADY_ATTACHED: return "XR_ERROR_ACTIONSETS_ALREADY_ATTACHED";
        case XR_ERROR_SESSION_NOT_READY:       return "XR_ERROR_SESSION_NOT_READY";
        case XR_ERROR_SESSION_RUNNING:         return "XR_ERROR_SESSION_RUNNING";
        case XR_ERROR_SWAPCHAIN_RECT_INVALID:  return "XR_ERROR_SWAPCHAIN_RECT_INVALID";
        default:                                return "XR_ERROR_?";
    }
}
#define OXR_CHECK(call) do { XrResult _r = (call); if (XR_FAILED(_r)) { OXR_LOGE("%s -> %s (0x%x)", #call, OXR_ResultStr(_r), (unsigned)_r); return 0; } } while (0)
#else
#define OXR_LOGI(...) ((void)0)
#define OXR_LOGW(...) ((void)0)
#define OXR_LOGE(...) ((void)0)
#endif

/* One-time init flag. */
static S32 s_openxrInited = 0;
static S32 s_openxrActive = 0;
static S32 s_swapchainOwnsPresent = 0;

/* Session / frame handles. */
static XrInstance              s_instance = XR_NULL_HANDLE;
static XrSession               s_session  = XR_NULL_HANDLE;
static XrSpace                 s_stageSpace = XR_NULL_HANDLE;
static XrSpace                 s_viewSpace = XR_NULL_HANDLE;
static XrActionSet             s_actionSet = XR_NULL_HANDLE;
static XrAction                s_actions[12];
static XrSessionState          s_sessionState = XR_SESSION_STATE_UNKNOWN;

/* JNI global refs kept alive for the lifetime of the OpenXR
 * instance: the loader trampoline for xrCreateInstance may call back
 * into JNI on the applicationContext, and the runtime keeps
 * applicationActivity for session-time JNI lookups. */
static jobject                 s_activityGlobal = NULL;
static jclass                  s_activityClassGlobal = NULL;

/* Immersive (mono blit) state. Created in OpenXR_CreateSession when
 * the headset reports at least one view config.  The swapchain images
 * are GL_TEXTURE_2D handles owned by the runtime; we copy the engine's
 * SDL window framebuffer into them with glCopyTexSubImage2D each
 * frame and submit a single XrCompositionLayerProjection (both eyes
 * reference the same sub-image, hence "mono"). */
static XrViewConfigurationType s_viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
static U32                     s_viewCount = 0;
static XrSwapchain             s_swapchain = XR_NULL_HANDLE;
static U32                     s_swapchainWidth = 0;
static U32                     s_swapchainHeight = 0;
static XrSwapchainImageOpenGLESKHR *s_swapchainImages = NULL;
static U32                     s_swapchainImageCount = 0;
static U32                     s_swapchainImageIndex = 0;

/* Predicted display time of the frame currently inside xrWaitFrame/xrEndFrame. */
static XrTime                  s_predictedDisplayTime = 0;

/* System id captured at session-create time; needed later to enumerate
 * view configurations for the immersive (mono blit) path. */
static XrSystemId              s_systemId = XR_NULL_SYSTEM_ID;

/* Raw controller state published to JOYSTICK.CPP. */
S32 OpenXR_LeftGripDown = 0;
S32 OpenXR_RightGripDown = 0;
S32 OpenXR_LeftTrigger = 0;
S32 OpenXR_RightTrigger = 0;
S32 OpenXR_LeftThumbstickDown = 0;
S32 OpenXR_RightThumbstickDown = 0;
S16 OpenXR_LeftStickX = 0;
S16 OpenXR_LeftStickY = 0;
S16 OpenXR_RightStickX = 0;
S16 OpenXR_RightStickY = 0;
S32 OpenXR_ABtn = 0;
S32 OpenXR_BBtn = 0;
S32 OpenXR_XBtn = 0;
S32 OpenXR_YBtn = 0;

/* -------------------------------------------------------------------------- */
static int hasExt(const XrExtensionProperties *list, U32 count, const char *name) {
    U32 i;
    if (list == NULL || name == NULL) return 0;
    for (i = 0; i < count; i++) {
        if (SDL_strcmp(list[i].extensionName, name) == 0) return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
static S32 OpenXR_CreateInstance(void) {
#ifdef __ANDROID__
    /* The Quest 3 forward loader (libopenxr_forwardloader.so in the
     * Meta VrDriver APK) requires xrInitializeLoaderKHR to be called
     * before the first xrCreateInstance. The function is a "loader
     * function": it is resolved by the Khronos loader's
     * xrGetInstanceProcAddr with XR_NULL_HANDLE as the instance, per
     * the OpenXR loader spec.
     *
     * xrInitializeLoaderKHR's XrLoaderInitInfoAndroidKHR.applicationContext
     * MUST be a real android.content.Context (jobect), NOT a jclass.
     * The Khronos loader (1.1.43+) calls Context.getAssets() on the
     * value during initialization to obtain an AAssetManager* for
     * reading API layer manifests from the APK. java.lang.Class has no
     * getAssets() method, so passing a jclass triggers a JNI
     * NoSuchMethodError, which the loader surfaces as
     * "Unknown failure: getAssets" and returns
     * XR_ERROR_RUNTIME_FAILURE.
     *
     * The activity instance (jobject) is a Context, so we pin it as a
     * global ref and pass it directly. Both Meta's OpenXR samples and
     * vice-city-vr-androidxr use activity->clazz here, which works on
     * older Quest firmwares where the loader only stored the context
     * and called getAssets() lazily inside xrCreateInstance; on the
     * current 1.1.43 loader it's strictly an instance.
     *
     * xrCreateInstance then chains XrInstanceCreateInfoAndroidKHR with
     * applicationActivity = activityInstance (jobject). The runtime
     * uses it for JNI lookups during the session.
     */
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activityLocal = (jobject)SDL_GetAndroidActivity();
    JavaVM *vm = NULL;
    jobject activity = NULL;       /* global ref, valid until OpenXR_Shutdown */
    jclass activityClass = NULL;  /* global ref, used for xrCreateInstance chain */
    if (env != NULL) {
        if (env->GetJavaVM(&vm) != JNI_OK) vm = NULL;
        if (activityLocal != NULL) {
            /* The applicationContext passed to xrInitializeLoaderKHR
             * must be a real android.content.Context — i.e. the
             * activity INSTANCE (jobject), not its jclass. Pin it. */
            activity = (jobject)env->NewGlobalRef(activityLocal);
            jclass localClass = env->GetObjectClass(activityLocal);
            if (localClass != NULL) {
                activityClass = (jclass)env->NewGlobalRef(localClass);
                env->DeleteLocalRef(localClass);
            }
        }
    }
    if (vm == NULL || activity == NULL) {
        OXR_LOGE("xrCreateInstance: missing JNI (vm=%p activity=%p) — not on Android?",
            (void *)vm, (void *)activityLocal);
        Log_Warn("OpenXR: JNI not available, refusing to create instance");
        if (activity != NULL && env != NULL) env->DeleteGlobalRef(activity);
        if (activityClass != NULL && env != NULL) env->DeleteGlobalRef(activityClass);
        return 0;
    }
    /* Hand the global refs to the file-static slots so the runtime
     * can use them for the lifetime of the instance. They are released
     * in OpenXR_Shutdown. */
    s_activityGlobal = activity;
    s_activityClassGlobal = activityClass;

    /* Step 1: resolve and call xrInitializeLoaderKHR with the
     * activity instance global ref. applicationContext takes a
     * Context jobject (the activity instance), NOT a jclass — the
     * 1.1.43 Khronos loader calls Context.getAssets() on this value
     * to obtain an AAssetManager* for API layer manifest files, and
     * java.lang.Class has no getAssets(). */
    {
        PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = NULL;
        PFN_xrGetInstanceProcAddr pfnGipa =
            (PFN_xrGetInstanceProcAddr)
                dlsym(RTLD_DEFAULT, "xrGetInstanceProcAddr");
        OXR_LOGI("dlsym xrGetInstanceProcAddr -> %p", (void *)(uintptr_t)pfnGipa);
        XrResult rcGipa = XR_ERROR_INITIALIZATION_FAILED;
        if (pfnGipa != NULL) {
            rcGipa = pfnGipa(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                (PFN_xrVoidFunction *)&xrInitializeLoaderKHR);
        }
        OXR_LOGI("xrGetInstanceProcAddr(xrInitializeLoaderKHR) -> %p, rc=%s (0x%x)",
            (void *)(uintptr_t)xrInitializeLoaderKHR,
            OXR_ResultStr(rcGipa), (unsigned)rcGipa);
        if (XR_FAILED(rcGipa) || xrInitializeLoaderKHR == NULL) {
            OXR_LOGE("xrInitializeLoaderKHR unavailable: %s (0x%x)",
                OXR_ResultStr(rcGipa), (unsigned)rcGipa);
        } else {
            XrLoaderInitInfoAndroidKHR loaderInfo;
            memset(&loaderInfo, 0, sizeof(loaderInfo));
            loaderInfo.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
            loaderInfo.applicationVM = (void *)vm;
            loaderInfo.applicationContext = (void *)activity;
            XrResult rcInit = xrInitializeLoaderKHR(
                (const XrLoaderInitInfoBaseHeaderKHR *)&loaderInfo);
            if (XR_FAILED(rcInit)) {
                OXR_LOGE("xrInitializeLoaderKHR failed: %s (0x%x)",
                    OXR_ResultStr(rcInit), (unsigned)rcInit);
            } else {
                OXR_LOGI("xrInitializeLoaderKHR OK (vm=%p activity=%p)",
                    (void *)vm, (void *)activity);
            }
        }
    }

    /* Step 2: enumerate available extensions, then call xrCreateInstance
     * with the graphics binding we need plus
     * XR_KHR_android_create_instance. We chain
     * XrInstanceCreateInfoAndroidKHR with applicationActivity =
     * activity (jobject, the activity instance) — vicecity and the
     * Meta samples pass clazz here, but on the current 1.1.43 Khronos
     * loader the instance is what the runtime actually uses for JNI
     * lookups during the session, and the loader-init call above
     * already proved that a Context jobject is the right thing to
     * pass. */
    {
        U32 availableCount = 0;
        XrResult rcEnum = xrEnumerateInstanceExtensionProperties(
            NULL, 0, &availableCount, NULL);
        OXR_LOGI("xrEnumerateInstanceExtensionProperties count probe -> %s (0x%x), count=%u",
            OXR_ResultStr(rcEnum), (unsigned)rcEnum, (unsigned)availableCount);
        XrExtensionProperties *available = NULL;
        if (XR_SUCCEEDED(rcEnum) && availableCount > 0) {
            available = (XrExtensionProperties *)SDL_malloc(
                sizeof(XrExtensionProperties) * availableCount);
            if (available != NULL) {
                U32 i;
                for (i = 0; i < availableCount; i++) {
                    memset(&available[i], 0, sizeof(available[i]));
                    available[i].type = XR_TYPE_EXTENSION_PROPERTIES;
                }
                XrResult rcEnum2 = xrEnumerateInstanceExtensionProperties(
                    NULL, availableCount, &availableCount, available);
                if (XR_FAILED(rcEnum2)) {
                    OXR_LOGW("xrEnumerateInstanceExtensionProperties fill failed: %s (0x%x)",
                        OXR_ResultStr(rcEnum2), (unsigned)rcEnum2);
                    SDL_free(available);
                    available = NULL;
                } else {
                    OXR_LOGI("Available OpenXR instance extensions (%u):", (unsigned)availableCount);
                    for (i = 0; i < availableCount; i++) {
                        OXR_LOGI("  [%u] %s (v%u)", (unsigned)i,
                            available[i].extensionName,
                            (unsigned)available[i].extensionVersion);
                    }
                }
            }
        }

        /* Build the enabled list — only enable what the runtime
         * actually exposes, plus require the GLES graphics binding. */
        U32 cap = 8;
        const char **enabled = (const char **)SDL_malloc(sizeof(const char *) * cap);
        U32 enabledCount = 0;
        #define HAS_EXT(name) (hasExt(available, availableCount, (name)))
        #define PUSH_EXT(name) do { \
            if (enabledCount < cap) enabled[enabledCount++] = (name); \
        } while (0)

        if (!HAS_EXT(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME)) {
            OXR_LOGE("runtime does not expose %s — cannot bind GLES",
                XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
            SDL_free(available);
            SDL_free(enabled);
            return 0;
        }
        PUSH_EXT(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
        if (HAS_EXT(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME)) {
            PUSH_EXT(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
        }

        XrInstanceCreateInfoAndroidKHR androidInfo;
        memset(&androidInfo, 0, sizeof(androidInfo));
        androidInfo.type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR;
        androidInfo.applicationVM = (void *)vm;
        androidInfo.applicationActivity = (void *)activity;

        XrInstanceCreateInfo info;
        memset(&info, 0, sizeof(info));
        info.type = XR_TYPE_INSTANCE_CREATE_INFO;
        info.next = &androidInfo;
        info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        SDL_strlcpy(info.applicationInfo.applicationName, "LBA2 Classic Community", sizeof(info.applicationInfo.applicationName));
        info.applicationInfo.applicationVersion = 1;
        SDL_strlcpy(info.applicationInfo.engineName, "LBA2", sizeof(info.applicationInfo.engineName));
        info.applicationInfo.engineVersion = 1;
        info.enabledExtensionCount = enabledCount;
        info.enabledExtensionNames = enabled;

        OXR_LOGI("xrCreateInstance: env=%p activity=%p vm=%p enabledExtensions=%u",
            (void *)env, (void *)activity, (void *)vm, (unsigned)info.enabledExtensionCount);
        {
            U32 i;
            for (i = 0; i < info.enabledExtensionCount; i++) {
                OXR_LOGI("  ext[%u] = %s", (unsigned)i, info.enabledExtensionNames[i]);
            }
        }

        XrResult rc = xrCreateInstance(&info, &s_instance);
        if (XR_FAILED(rc)) {
            OXR_LOGE("xrCreateInstance failed: %s (0x%x)", OXR_ResultStr(rc), (unsigned)rc);
            Log_Warn("OpenXR: xrCreateInstance failed: %s (0x%x)", OXR_ResultStr(rc), (unsigned)rc);
            SDL_free(available);
            SDL_free(enabled);
            return 0;
        }
        OXR_LOGI("xrCreateInstance OK");
        SDL_free(available);
        SDL_free(enabled);
    }
    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
/* Immersive mono blit helpers (defined further down in the same TU; called
 * from OpenXR_CreateSession once the session handle is alive). */
static S32 OpenXR_CreateSwapchain(void);
static void OpenXR_DestroySwapchain(void);

/* -------------------------------------------------------------------------- */
static S32 OpenXR_CreateSession(void) {
#ifdef __ANDROID__
    XrSystemGetInfo sysInfo;
    memset(&sysInfo, 0, sizeof(sysInfo));
    sysInfo.type = XR_TYPE_SYSTEM_GET_INFO;
    sysInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId sysId = 0;
    XrResult rc = xrGetSystem(s_instance, &sysInfo, &sysId);
    if (XR_FAILED(rc)) {
        OXR_LOGE("xrGetSystem failed: %s (0x%x)", OXR_ResultStr(rc), (unsigned)rc);
        Log_Warn("OpenXR: no headset system found (%s, 0x%x)", OXR_ResultStr(rc), (unsigned)rc);
        return 0;
    }
    s_systemId = sysId;
    OXR_LOGI("xrGetSystem OK sysId=%llu", (unsigned long long)sysId);

    XrGraphicsBindingOpenGLESAndroidKHR gfxBinding;
    memset(&gfxBinding, 0, sizeof(gfxBinding));
    gfxBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
    gfxBinding.display = (EGLDisplay)SDL_EGL_GetCurrentDisplay();
    gfxBinding.context = (EGLContext)SDL_GL_GetCurrentContext();

    XrSessionCreateInfo sessInfo;
    memset(&sessInfo, 0, sizeof(sessInfo));
    sessInfo.type = XR_TYPE_SESSION_CREATE_INFO;
    sessInfo.next = &gfxBinding;
    sessInfo.createFlags = 0;
    sessInfo.systemId = sysId;

    rc = xrCreateSession(s_instance, &sessInfo, &s_session);
    if (XR_FAILED(rc)) {
        OXR_LOGE("xrCreateSession failed: %s (0x%x)", OXR_ResultStr(rc), (unsigned)rc);
        Log_Warn("OpenXR: xrCreateSession failed (%s, 0x%x)", OXR_ResultStr(rc), (unsigned)rc);
        return 0;
    }
    OXR_LOGI("xrCreateSession OK");

    XrReferenceSpaceCreateInfo refInfo;
    memset(&refInfo, 0, sizeof(refInfo));
    refInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    refInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    refInfo.poseInReferenceSpace.orientation.w = 1.0f;
    rc = xrCreateReferenceSpace(s_session, &refInfo, &s_stageSpace);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrCreateReferenceSpace STAGE failed (0x%x)", (unsigned)rc);
        return 0;
    }

    refInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    rc = xrCreateReferenceSpace(s_session, &refInfo, &s_viewSpace);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrCreateReferenceSpace VIEW failed (0x%x)", (unsigned)rc);
        return 0;
    }

    /* Immersive mono blit: build the swapchain now so the first frame
     * can present. Failure is non-fatal — the engine still runs in
     * "input-only" mode if the runtime cannot allocate a swapchain. */
    if (!OpenXR_CreateSwapchain()) {
        Log_Warn("OpenXR: immersive swapchain unavailable, falling back to input-only");
    }

    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
/* Immersive mono blit: enumerate view configurations, pick the largest, and
 * create a single GLES2 swapchain sized to the headset's recommended
 * resolution.  The engine's existing scene render is then blitted into
 * this swapchain each frame; both eyes reference the same sub-image so
 * the user sees the same picture in left and right (mono). */
static S32 OpenXR_CreateSwapchain(void) {
#ifdef __ANDROID__
    XrSystemId sysId = s_systemId;
    if (sysId == XR_NULL_SYSTEM_ID) {
        Log_Warn("OpenXR: no system id, cannot create swapchain");
        return 0;
    }
    /* 1. Pick a view config type. Quest 3 reports PRIMARY_STEREO; we treat
     *    both STEREO and MONO identically (mono blit). */
    U32 viewConfigCount = 0;
    XrResult rc = xrEnumerateViewConfigurations(s_instance, sysId, 0, &viewConfigCount, NULL);
    if (XR_FAILED(rc) || viewConfigCount == 0) {
        Log_Warn("OpenXR: no view configurations (0x%x)", (unsigned)rc);
        return 0;
    }
    XrViewConfigurationType *viewConfigs = (XrViewConfigurationType *)
        malloc(sizeof(XrViewConfigurationType) * viewConfigCount);
    rc = xrEnumerateViewConfigurations(s_instance, sysId, viewConfigCount, &viewConfigCount, viewConfigs);
    if (XR_FAILED(rc)) { free(viewConfigs); return 0; }
    s_viewConfigType = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    U32 i;
    for (i = 0; i < viewConfigCount; i++) {
        if (viewConfigs[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO ||
            viewConfigs[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
            s_viewConfigType = viewConfigs[i];
            break;
        }
    }
    free(viewConfigs);
    if (s_viewConfigType == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM) {
        Log_Warn("OpenXR: no PRIMARY_MONO/STEREO view config");
        return 0;
    }

    /* 2. Locate the view configuration properties (recommended image rect). */
    XrViewConfigurationProperties viewProps;
    memset(&viewProps, 0, sizeof(viewProps));
    rc = xrGetViewConfigurationProperties(s_instance, sysId, s_viewConfigType, &viewProps);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrGetViewConfigurationProperties failed (0x%x)", (unsigned)rc);
        return 0;
    }
    (void)viewProps;

    /* 3. Enumerate views (1 for MONO, 2 for STEREO) and their recommended
     *    image sizes.  We do not need the XrViewConfigurationView structs
     *    per frame (the runtime positions the eyes at xrLocateViews time
     *    for us); we just need the largest recommended image size to
     *    size the swapchain. */
    U32 viewCount = 0;
    rc = xrEnumerateViewConfigurationViews(s_instance, sysId, s_viewConfigType, 0, &viewCount, NULL);
    if (XR_FAILED(rc) || viewCount == 0 || viewCount > 2) {
        Log_Warn("OpenXR: bad view count %u (0x%x)", (unsigned)viewCount, (unsigned)rc);
        return 0;
    }
    XrViewConfigurationView *views = (XrViewConfigurationView *)
        malloc(sizeof(XrViewConfigurationView) * viewCount);
    {
        U32 j;
        for (j = 0; j < viewCount; j++) {
            views[j].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
            views[j].next = NULL;
        }
    }
    rc = xrEnumerateViewConfigurationViews(s_instance, sysId, s_viewConfigType,
        viewCount, &viewCount, views);
    if (XR_FAILED(rc)) { free(views); return 0; }
    s_swapchainWidth  = views[0].recommendedImageRectWidth;
    s_swapchainHeight = views[0].recommendedImageRectHeight;
    {
        U32 j;
        for (j = 1; j < viewCount; j++) {
            if (views[j].recommendedImageRectWidth  > s_swapchainWidth)  s_swapchainWidth  = views[j].recommendedImageRectWidth;
            if (views[j].recommendedImageRectHeight > s_swapchainHeight) s_swapchainHeight = views[j].recommendedImageRectHeight;
        }
    }
    if (s_swapchainWidth  < 320) s_swapchainWidth  = 320;
    if (s_swapchainHeight < 200) s_swapchainHeight = 200;
    s_viewCount = viewCount;
    free(views);
    Log_Info("OpenXR: immersive viewCount=%u swapchain=%ux%u",
             (unsigned)s_viewCount, (unsigned)s_swapchainWidth, (unsigned)s_swapchainHeight);

    /* 4. Create the GLES2 swapchain.  GL_RGBA is the unsized RGBA8 format
     *    in GLES 2.0; the runtime will allocate a GL_RGBA8 texture. */
    XrSwapchainCreateInfo scInfo;
    memset(&scInfo, 0, sizeof(scInfo));
    scInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    scInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    scInfo.format = GL_RGBA;
    scInfo.sampleCount = 1;
    scInfo.width = s_swapchainWidth;
    scInfo.height = s_swapchainHeight;
    scInfo.faceCount = 1;
    scInfo.arraySize = 1;
    scInfo.mipCount = 1;
    rc = xrCreateSwapchain(s_session, &scInfo, &s_swapchain);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrCreateSwapchain failed (0x%x)", (unsigned)rc);
        return 0;
    }

    /* 5. Enumerate the swapchain images (the runtime creates the GL textures). */
    rc = xrEnumerateSwapchainImages(s_swapchain, 0, &s_swapchainImageCount, NULL);
    if (XR_FAILED(rc) || s_swapchainImageCount == 0) {
        Log_Warn("OpenXR: bad swapchain image count (0x%x)", (unsigned)rc);
        return 0;
    }
    s_swapchainImages = (XrSwapchainImageOpenGLESKHR *)
        calloc(s_swapchainImageCount, sizeof(XrSwapchainImageOpenGLESKHR));
    for (i = 0; i < s_swapchainImageCount; i++) {
        s_swapchainImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
        s_swapchainImages[i].next = NULL;
    }
    rc = xrEnumerateSwapchainImages(s_swapchain, s_swapchainImageCount, &s_swapchainImageCount,
        (XrSwapchainImageBaseHeader *)s_swapchainImages);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrEnumerateSwapchainImages failed (0x%x)", (unsigned)rc);
        return 0;
    }
    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
static void OpenXR_DestroySession(void) {
#ifdef __ANDROID__
    if (s_swapchain != XR_NULL_HANDLE) {
        xrDestroySwapchain(s_swapchain);
        s_swapchain = XR_NULL_HANDLE;
    }
    if (s_swapchainImages) {
        free(s_swapchainImages);
        s_swapchainImages = NULL;
    }
    s_swapchainImageCount = 0;
    s_swapchainImageIndex = 0;
    s_swapchainWidth = 0;
    s_swapchainHeight = 0;
    s_viewCount = 0;
    s_systemId = XR_NULL_SYSTEM_ID;
    s_swapchainOwnsPresent = 0;
    if (s_session != XR_NULL_HANDLE) {
        xrDestroySession(s_session);
        s_session = XR_NULL_HANDLE;
    }
    if (s_stageSpace != XR_NULL_HANDLE) {
        xrDestroySpace(s_stageSpace);
        s_stageSpace = XR_NULL_HANDLE;
    }
    if (s_viewSpace != XR_NULL_HANDLE) {
        xrDestroySpace(s_viewSpace);
        s_viewSpace = XR_NULL_HANDLE;
    }
#endif
}

/* -------------------------------------------------------------------------- */
static S32 OpenXR_CreateActions(void) {
#ifdef __ANDROID__
    XrActionSetCreateInfo asInfo;
    memset(&asInfo, 0, sizeof(asInfo));
    asInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
    SDL_strlcpy(asInfo.actionSetName, "gameplay", sizeof(asInfo.actionSetName));
    SDL_strlcpy(asInfo.localizedActionSetName, "Gameplay", sizeof(asInfo.localizedActionSetName));

    XrResult rc = xrCreateActionSet(s_instance, &asInfo, &s_actionSet);
    if (XR_FAILED(rc)) return 0;

    static const char *boolNames[] = {
        "a_click", "b_click", "x_click", "y_click",
        "lstick_click", "rstick_click",
        "ltrigger", "rtrigger",
        "lgrip", "rgrip",
    };
    U32 i;
    for (i = 0; i < sizeof(boolNames)/sizeof(boolNames[0]); i++) {
        XrActionCreateInfo aInfo;
        memset(&aInfo, 0, sizeof(aInfo));
        aInfo.type = XR_TYPE_ACTION_CREATE_INFO;
        aInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        SDL_strlcpy(aInfo.actionName, boolNames[i], sizeof(aInfo.actionName));
        SDL_strlcpy(aInfo.localizedActionName, boolNames[i], sizeof(aInfo.localizedActionName));
        rc = xrCreateAction(s_actionSet, &aInfo, &s_actions[i]);
        if (XR_FAILED(rc)) return 0;
    }

    static const char *vecNames[] = { "lthumbstick", "rthumbstick" };
    for (i = 0; i < sizeof(vecNames)/sizeof(vecNames[0]); i++) {
        XrActionCreateInfo aInfo;
        memset(&aInfo, 0, sizeof(aInfo));
        aInfo.type = XR_TYPE_ACTION_CREATE_INFO;
        aInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
        SDL_strlcpy(aInfo.actionName, vecNames[i], sizeof(aInfo.actionName));
        SDL_strlcpy(aInfo.localizedActionName, vecNames[i], sizeof(aInfo.localizedActionName));
        rc = xrCreateAction(s_actionSet, &aInfo, &s_actions[10 + i]);
        if (XR_FAILED(rc)) return 0;
    }

    /* Meta Quest 3 controllers are exposed under the oculus/touch_controller
     * interaction profile. Both hand mappings (left=ABXY swapped, right=ABXY
     * canonical) are covered so the binding set is profile-portable. */
    static const char *profileStr = "/interaction_profiles/oculus/touch_controller";
    XrPath profilePath = XR_NULL_PATH;
    xrStringToPath(s_instance, profileStr, &profilePath);

    static const char *bindingStrs[12] = {
        "/user/hand/right/input/a/click",
        "/user/hand/right/input/b/click",
        "/user/hand/left/input/x/click",
        "/user/hand/left/input/y/click",
        "/user/hand/left/input/thumbstick/click",
        "/user/hand/right/input/thumbstick/click",
        "/user/hand/left/input/trigger/value",
        "/user/hand/right/input/trigger/value",
        "/user/hand/left/input/squeeze/value",
        "/user/hand/right/input/squeeze/value",
        "/user/hand/left/input/thumbstick",
        "/user/hand/right/input/thumbstick",
    };
    XrActionSuggestedBinding bindings[12];
    U32 j;
    for (j = 0; j < 12; j++) {
        bindings[j].action = s_actions[j];
        xrStringToPath(s_instance, bindingStrs[j], &bindings[j].binding);
    }

    XrInteractionProfileSuggestedBinding suggest;
    memset(&suggest, 0, sizeof(suggest));
    suggest.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
    suggest.interactionProfile = profilePath;
    suggest.suggestedBindings = bindings;
    suggest.countSuggestedBindings = 12;

    rc = xrSuggestInteractionProfileBindings(s_instance, &suggest);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrSuggestInteractionProfileBindings failed (0x%x)", (unsigned)rc);
    }

    XrSessionActionSetsAttachInfo attach;
    memset(&attach, 0, sizeof(attach));
    attach.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    attach.actionSets = &s_actionSet;
    attach.countActionSets = 1;
    rc = xrAttachSessionActionSets(s_session, &attach);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrAttachSessionActionSets failed (0x%x)", (unsigned)rc);
        return 0;
    }

    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
static void OpenXR_ReadActions(void) {
#ifdef __ANDROID__
    if (!s_openxrActive) return;

    XrResult rc = xrSyncActions(s_session, NULL);
    if (XR_FAILED(rc)) return;

    XrActionStateGetInfo gi;
    XrActionStateBoolean stb;
    XrActionStateVector2f stv;
    U32 i;

    for (i = 0; i < 10; i++) {
        memset(&gi, 0, sizeof(gi));
        memset(&stb, 0, sizeof(stb));
        gi.type = XR_TYPE_ACTION_STATE_GET_INFO;
        gi.action = s_actions[i];
        stb.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        S32 *out = NULL;
        switch (i) {
            case 0: out = &OpenXR_ABtn; break;
            case 1: out = &OpenXR_BBtn; break;
            case 2: out = &OpenXR_XBtn; break;
            case 3: out = &OpenXR_YBtn; break;
            case 4: out = &OpenXR_LeftThumbstickDown; break;
            case 5: out = &OpenXR_RightThumbstickDown; break;
            case 6: out = &OpenXR_LeftTrigger; break;
            case 7: out = &OpenXR_RightTrigger; break;
            case 8: out = &OpenXR_LeftGripDown; break;
            case 9: out = &OpenXR_RightGripDown; break;
        }
        if (out != NULL && XR_SUCCEEDED(xrGetActionStateBoolean(s_session, &gi, &stb))
            && stb.isActive && stb.currentState) {
            *out = 1;
        } else if (out != NULL) {
            *out = 0;
        }
    }

    for (i = 10; i < 12; i++) {
        memset(&gi, 0, sizeof(gi));
        memset(&stv, 0, sizeof(stv));
        gi.type = XR_TYPE_ACTION_STATE_GET_INFO;
        gi.action = s_actions[i];
        stv.type = XR_TYPE_ACTION_STATE_VECTOR2F;
        S16 *outx = NULL, *outy = NULL;
        switch (i) {
            case 10: outx = &OpenXR_LeftStickX;  outy = &OpenXR_LeftStickY;  break;
            case 11: outx = &OpenXR_RightStickX; outy = &OpenXR_RightStickY; break;
        }
        if (outx != NULL && outy != NULL
            && XR_SUCCEEDED(xrGetActionStateVector2f(s_session, &gi, &stv))
            && stv.isActive) {
            *outx = (S16)(stv.currentState.x * 32767.0f);
            *outy = (S16)(-stv.currentState.y * 32767.0f);
        } else if (outx != NULL && outy != NULL) {
            *outx = 0;
            *outy = 0;
        }
    }
#endif
}

/* -------------------------------------------------------------------------- */
#else
/* No OpenXR build: stubs. */
static S32 OpenXR_CreateInstance(void) { return 0; }
static S32 OpenXR_CreateSession(void) { return 0; }
static void OpenXR_DestroySession(void) {}
static S32 OpenXR_CreateActions(void) { return 0; }
static void OpenXR_ReadActions(void) {}
#endif

/* -------------------------------------------------------------------------- */
S32 OpenXR_Init(void) {
#ifdef LBA2_OPENXR
    if (s_openxrInited) return s_openxrActive;
    s_openxrInited = 1;

    if (!OpenXR_CreateInstance()) {
        Log_Warn("OpenXR: failed to create instance");
        return 0;
    }

    XrEventDataBuffer evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = XR_TYPE_EVENT_DATA_BUFFER;
    XrResult rc;
    int i;
    for (i = 0; i < 30; i++) {
        rc = xrPollEvent(s_instance, &evt);
        if (rc == XR_EVENT_UNAVAILABLE) break;
        if (evt.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged *sc = (XrEventDataSessionStateChanged *)&evt;
            s_sessionState = sc->state;
            if (sc->state == XR_SESSION_STATE_READY) {
                if (!OpenXR_CreateSession()) {
                    Log_Warn("OpenXR: session creation failed");
                    return 0;
                }
                if (!OpenXR_CreateActions()) {
                    Log_Warn("OpenXR: action creation failed");
                    return 0;
                }
                s_openxrActive = 1;
                Log_Info("OpenXR: session active");
                return 1;
            }
        }
        memset(&evt, 0, sizeof(evt));
        evt.type = XR_TYPE_EVENT_DATA_BUFFER;
    }

    xrDestroyInstance(s_instance);
    s_instance = XR_NULL_HANDLE;
    Log_Info("OpenXR: no running runtime");
    return 0;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
void OpenXR_Shutdown(void) {
#ifdef LBA2_OPENXR
    OpenXR_DestroySession();
    if (s_instance != XR_NULL_HANDLE) {
        if (s_actionSet != XR_NULL_HANDLE) {
            xrDestroyActionSet(s_actionSet);
            s_actionSet = XR_NULL_HANDLE;
        }
        xrDestroyInstance(s_instance);
        s_instance = XR_NULL_HANDLE;
    }
    if (s_activityGlobal != NULL) {
        JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
        if (env != NULL) env->DeleteGlobalRef(s_activityGlobal);
        s_activityGlobal = NULL;
    }
    if (s_activityClassGlobal != NULL) {
        JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
        if (env != NULL) env->DeleteGlobalRef(s_activityClassGlobal);
        s_activityClassGlobal = NULL;
    }
    s_openxrActive = 0;
    s_openxrInited = 0;
#endif
}

/* -------------------------------------------------------------------------- */
S32 OpenXR_IsActive(void) {
#ifdef LBA2_OPENXR
    return s_openxrActive;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
void OpenXR_PollEvents(void) {
#ifdef LBA2_OPENXR
    if (!s_openxrInited) return;
    XrEventDataBuffer evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = XR_TYPE_EVENT_DATA_BUFFER;
    XrResult rc;
    while ((rc = xrPollEvent(s_instance, &evt)) == XR_SUCCESS) {
        if (evt.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged *sc = (XrEventDataSessionStateChanged *)&evt;
            s_sessionState = sc->state;
            if (sc->state == XR_SESSION_STATE_LOSS_PENDING ||
                sc->state == XR_SESSION_STATE_EXITING) {
                OpenXR_Shutdown();
                return;
            }
        }
        memset(&evt, 0, sizeof(evt));
        evt.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
#endif
}

/* -------------------------------------------------------------------------- */
S32 OpenXR_BeginFrame(void) {
#ifdef LBA2_OPENXR
    if (!s_openxrActive) return 0;
    if (s_sessionState != XR_SESSION_STATE_FOCUSED &&
        s_sessionState != XR_SESSION_STATE_VISIBLE &&
        s_sessionState != XR_SESSION_STATE_READY) {
        return 0;
    }
    /* Quest 3 immersive (mono blit) path: wait for the predicted display
     * time BEFORE we render. xrWaitFrame is mandatory; the spec also
     * allows it to be called from xrBeginFrame's slot but doing it here
     * gives the engine a stable predicted display time to log if needed. */
    if (s_swapchain != XR_NULL_HANDLE) {
        XrFrameWaitInfo waitInfo;
        memset(&waitInfo, 0, sizeof(waitInfo));
        waitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
        XrFrameState frameState;
        memset(&frameState, 0, sizeof(frameState));
        frameState.type = XR_TYPE_FRAME_STATE;
        XrResult rc = xrWaitFrame(s_session, &waitInfo, &frameState);
        if (XR_FAILED(rc)) {
            return 0;
        }
        s_predictedDisplayTime = frameState.predictedDisplayTime;
    } else {
        /* Input-only path: no swapchain, no xrBeginFrame yet. Begin is
         * deferred to OpenXR_PresentMono (where it would no-op anyway). */
        s_predictedDisplayTime = 0;
    }
    OpenXR_ReadActions();
    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
/* Quest 3 immersive (mono blit) path.
 *
 * Called by SOURCES/PERSO.CPP after AffScene() has rendered the engine to
 * the SDL window framebuffer, and before SDL_GL_SwapWindow would normally
 * present. The window's GL context is still current.
 *
 * 1. xrBeginFrame (the spec requires it before xrEndFrame).
 * 2. xrAcquireSwapchainImage / xrWaitSwapchainImage on the mono swapchain.
 * 3. glCopyTexSubImage2D: copy the SDL window's default framebuffer (read
 *    FBO 0) into the runtime-owned swapchain image texture.  We copy the
 *    engine's actual window rect into the top-left of the swapchain; if
 *    the engine resolution is smaller than the headset's recommended
 *    image size, the remaining area of the swapchain is left as its
 *    initial state (typically zero / black), so the user sees a flat
 *    2D picture at the engine's resolution, surrounded by black.
 * 4. xrReleaseSwapchainImage.
 * 5. xrEndFrame with a single XrCompositionLayerProjection whose both eye
 *    sub-images reference the same mono sub-image, so left and right
 *    eyes get the same picture.
 *
 * Returns 1 on a presented frame, 0 if the frame was skipped (caller
 * should still let SDL swap the window so the user sees something). */
S32 OpenXR_PresentMono(void) {
#ifdef LBA2_OPENXR
#ifdef __ANDROID__
    if (!s_openxrActive) return 0;
    if (s_sessionState != XR_SESSION_STATE_FOCUSED &&
        s_sessionState != XR_SESSION_STATE_VISIBLE &&
        s_sessionState != XR_SESSION_STATE_READY) {
        OXR_LOGI("PresentMono: skip, sessionState=%d", (int)s_sessionState);
        return 0;
    }
    if (s_swapchain == XR_NULL_HANDLE) {
        /* Input-only path: no swapchain to present. */
        return 0;
    }

    /* 1. Begin frame. */
    XrFrameBeginInfo beginInfo;
    memset(&beginInfo, 0, sizeof(beginInfo));
    beginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
    XrResult rc = xrBeginFrame(s_session, &beginInfo);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrBeginFrame failed (0x%x)", (unsigned)rc);
        return 0;
    }

    /* 2. Acquire + wait for a swapchain image. */
    XrSwapchainImageAcquireInfo acquireInfo;
    memset(&acquireInfo, 0, sizeof(acquireInfo));
    acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    rc = xrAcquireSwapchainImage(s_swapchain, &acquireInfo, &s_swapchainImageIndex);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrAcquireSwapchainImage failed (0x%x)", (unsigned)rc);
        return 0;
    }
    XrSwapchainImageWaitInfo waitInfo;
    memset(&waitInfo, 0, sizeof(waitInfo));
    waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    waitInfo.timeout = XR_INFINITE_DURATION;
    rc = xrWaitSwapchainImage(s_swapchain, &waitInfo);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrWaitSwapchainImage failed (0x%x)", (unsigned)rc);
        return 0;
    }

    /* 3. Copy the SDL window framebuffer into the swapchain image.
     *
     *    Approach: bind the swapchain image's texture directly, then call
     *    glCopyTexSubImage2D which reads from the currently-bound
     *    READ_FRAMEBUFFER (we bind FBO 0 = the default / SDL window fb
     *    just to be explicit) and writes the rect into the bound
     *    TEXTURE_2D.
     *
     *    This is GLES 2.0 core (glBlitFramebuffer would be cleaner but
     *    is GLES 3.0). It copies 1:1 pixels so the engine's 640x480 (or
     *    whatever) frame appears at the bottom-left of the swapchain
     *    image with the rest of the texture as initial data. The Quest
     *    compositor then maps that to both eyes, giving a flat 2D
     *    picture floating in VR. */
    if (s_swapchainImageIndex < s_swapchainImageCount) {
        GLuint imgTex = s_swapchainImages[s_swapchainImageIndex].image;
        SDL_Window *win = SDL_GetWindowFromID(0);
        int winW = 0, winH = 0;
        if (win != NULL) {
            SDL_GetWindowSizeInPixels(win, &winW, &winH);
        }
        if (winW > 0 && winH > 0) {
            GLint prevFb = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFb);
            GLint prevTex = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, imgTex);
            GLsizei copyW = (GLsizei)(winW < (int)s_swapchainWidth  ? winW : s_swapchainWidth);
            GLsizei copyH = (GLsizei)(winH < (int)s_swapchainHeight ? winH : s_swapchainHeight);
            /* GLES 2.0 doesn't allow depth / stencil formats as color
             * destinations, and we created the swapchain with GL_RGBA8
             * so the format is valid. glCopyTexSubImage2D writes the
             * (0,0)..(winW,winH) rect of the current READ fb into the
             * (0,0)..(copyW,copyH) rect of the bound texture. */
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, copyW, copyH);

            glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFb);
        }
    }

    /* 4. Release the swapchain image. */
    XrSwapchainImageReleaseInfo releaseInfo;
    memset(&releaseInfo, 0, sizeof(releaseInfo));
    releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    rc = xrReleaseSwapchainImage(s_swapchain, &releaseInfo);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrReleaseSwapchainImage failed (0x%x)", (unsigned)rc);
    }

    /* 5. Locate the views to get the eye poses for the projection layer. */
    XrViewLocateInfo locateInfo;
    memset(&locateInfo, 0, sizeof(locateInfo));
    locateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
    locateInfo.viewConfigurationType = s_viewConfigType;
    locateInfo.displayTime = s_predictedDisplayTime;
    locateInfo.space = s_stageSpace;
    XrViewState viewState;
    memset(&viewState, 0, sizeof(viewState));
    viewState.type = XR_TYPE_VIEW_STATE;
    XrView locatedViews[2];
    U32 locatedCount = 0;
    {
        U32 j;
        for (j = 0; j < 2; j++) {
            locatedViews[j].type = XR_TYPE_VIEW;
            locatedViews[j].next = NULL;
        }
    }
    rc = xrLocateViews(s_session, &locateInfo, &viewState,
        (U32)(s_viewCount > 2 ? 2 : s_viewCount), &locatedCount, locatedViews);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrLocateViews failed (0x%x)", (unsigned)rc);
        locatedCount = 0;
    }

    /* 6. Build the projection layer. Both eye sub-images reference the
     *    same mono sub-image; the FOV is taken from the runtime's view
     *    pose. If xrLocateViews failed, fall back to a default IPD=0
     *    identity pair so xrEndFrame is still legal. */
    XrCompositionLayerProjectionView projViews[2];
    XrCompositionLayerProjection projLayer;
    XrCompositionLayerBaseHeader *layerPtr = NULL;
    if (locatedCount > 0 && s_swapchainImageIndex < s_swapchainImageCount) {
        U32 j;
        for (j = 0; j < locatedCount; j++) {
            XrCompositionLayerProjectionView *pv = &projViews[j];
            memset(pv, 0, sizeof(*pv));
            pv->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            pv->next = NULL;
            pv->subImage.swapchain = s_swapchain;
            pv->subImage.imageRect.offset.x = 0;
            pv->subImage.imageRect.offset.y = 0;
            pv->subImage.imageRect.extent.width  = (S32)s_swapchainWidth;
            pv->subImage.imageRect.extent.height = (S32)s_swapchainHeight;
            pv->subImage.imageArrayIndex = 0;
            pv->pose = locatedViews[j].pose;
            pv->fov  = locatedViews[j].fov;
        }
        memset(&projLayer, 0, sizeof(projLayer));
        projLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        projLayer.next = NULL;
        projLayer.layerFlags = 0;
        projLayer.space = s_stageSpace;
        projLayer.viewCount = locatedCount;
        projLayer.views = projViews;
        layerPtr = (XrCompositionLayerBaseHeader *)&projLayer;
    }

    /* 7. End the frame, presenting the layer (or nothing if locate failed). */
    XrFrameEndInfo endInfo;
    memset(&endInfo, 0, sizeof(endInfo));
    endInfo.type = XR_TYPE_FRAME_END_INFO;
    endInfo.displayTime = s_predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = (layerPtr != NULL) ? 1 : 0;
    endInfo.layers = (layerPtr != NULL) ? &layerPtr : NULL;
    rc = xrEndFrame(s_session, &endInfo);
    if (XR_FAILED(rc)) {
        OXR_LOGE("xrEndFrame failed: %s (0x%x)", OXR_ResultStr(rc), (unsigned)rc);
        Log_Warn("OpenXR: xrEndFrame failed (0x%x)", (unsigned)rc);
    }

    s_swapchainOwnsPresent = 1;
    return 1;
#else
    return 0;
#endif
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
void OpenXR_EndFrame(void) {
#ifdef LBA2_OPENXR
    /* In the Quest 3 immersive path, OpenXR_PresentMono does the entire
     * present (xrBeginFrame ... xrEndFrame). PERSO.CPP now calls that
     * directly, so this function is a no-op kept for ABI compatibility
     * with the input-only path. */
    (void)0;
#endif
}

/* -------------------------------------------------------------------------- */
S32 OpenXR_ShouldSkipSDLSwap(void) {
#ifdef LBA2_OPENXR
#ifdef __ANDROID__
    /* When we own a swapchain (Quest immersive mode), the engine's SDL
     * window has no consumer — xrEndFrame is the only present path.
     * The GL renderer's GL_Present() still does useful work (uploading
     * the SVGA ARGB pixels into the GL texture / FBO), so it must run;
     * only the final SDL_GL_SwapWindow must be suppressed. */
    return (s_swapchain != XR_NULL_HANDLE);
#else
    return 0;
#endif
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
S32 OpenXR_UpdateControllerState(void) {
#ifdef LBA2_OPENXR
    /* Reads are already done in OpenXR_BeginFrame(). Return active state so
       JOYSTICK.CPP can pull the cached raw axes/buttons. */
    return s_openxrActive ? 1 : 0;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif
