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
#include <cmath>

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
/* glBlitFramebuffer (present scale-to-fit) is GLES 3.0; the engine's GL
 * context is created GLES 3-capable on Android. */
#include <GLES3/gl3.h>
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
        case XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED: return "XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED";
        case XR_ERROR_GRAPHICS_DEVICE_INVALID: return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
        case XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING: return "XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING";
        case XR_ERROR_TIME_INVALID:            return "XR_ERROR_TIME_INVALID";
        case XR_ERROR_POSE_INVALID:            return "XR_ERROR_POSE_INVALID";
        case XR_ERROR_RUNTIME_UNAVAILABLE:     return "XR_ERROR_RUNTIME_UNAVAILABLE";
        case XR_ERROR_PERMISSION_INSUFFICIENT: return "XR_ERROR_PERMISSION_INSUFFICIENT";
        case XR_ERROR_EXTENSION_DEPENDENCY_NOT_ENABLED: return "XR_ERROR_EXTENSION_DEPENDENCY_NOT_ENABLED";
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
static S32 s_sessionRunning = 0;
static S32 s_swapchainOwnsPresent = 0;

/* SDL ticks at the last PresentMono that passed the session gates; -1 until
 * then. OpenXR_UpdateControllerState syncs input here when this has gone
 * stale, covering input-wait loops that spin without presenting. */
static S32 s_lastPresentTick = -1;

/* Native stereo present: engine renders per eye (GL_SetStereoEye) and the
 * submit blits two per-eye composites into the swapchain halves. */
static S32 s_stereoEnabled = 0;

/* Low-volume present breakdown. xrWaitFrame happens at frame start in the
 * gameplay path, so frameBegin and presentBegin are separate buckets. */
static U64 s_oxrPerfFrameBeginTicks = 0;
static U32 s_oxrPerfFrameBeginCalls = 0;
static U64 s_oxrPerfPresentBeginTicks = 0;
static U64 s_oxrPerfAcquireTicks = 0;
static U64 s_oxrPerfWaitTicks = 0;
static U64 s_oxrPerfBlitTicks = 0;
static U64 s_oxrPerfReleaseTicks = 0;
static U64 s_oxrPerfEndTicks = 0;
static U64 s_oxrPerfTotalTicks = 0;
static U32 s_oxrPerfPresentCalls = 0;
static U32 s_oxrPerfDirectCalls = 0;

/* Session / frame handles. */
static XrInstance              s_instance = XR_NULL_HANDLE;
static XrSession               s_session  = XR_NULL_HANDLE;
static XrSpace                 s_stageSpace = XR_NULL_HANDLE;
static XrSpace                 s_localSpace = XR_NULL_HANDLE;
static XrSpace                 s_viewSpace = XR_NULL_HANDLE;
static XrActionSet             s_actionSet = XR_NULL_HANDLE;
static XrAction                s_actions[20];
/* Tracked controller spaces: [hand][0 = aim, 1 = grip]. */
static XrSpace                 s_handSpaces[2][2];
static XrPosef                 s_handPoses[2][2];
static U8                      s_handPoseValid[2][2];

/* Optical hand tracking (XR_EXT_hand_tracking). */
#define OXR_HAND_JOINT_COUNT 26
#define OXR_HAND_JOINT_PALM 0
#define OXR_HAND_JOINT_WRIST 1
static S32                     s_extHandTracking = 0;
static XrHandTrackerEXT        s_handTrackers[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
static XrHandJointLocationEXT  s_handJoints[2][OXR_HAND_JOINT_COUNT];
static U8                      s_handJointsValid[2] = {0, 0};
static PFN_xrCreateHandTrackerEXT p_xrCreateHandTrackerEXT = NULL;
static PFN_xrDestroyHandTrackerEXT p_xrDestroyHandTrackerEXT = NULL;
static PFN_xrLocateHandJointsEXT p_xrLocateHandJointsEXT = NULL;

/* Subaction paths for both hands. The Quest runtime reports controller
 * actions per hand: created without subaction paths, actions bind
 * nominally (profile matches) but every state query reads back inactive.
 * Every action is therefore created filtered to both hands, and each
 * query is made once per hand. */
static XrPath                  s_handPaths[2];
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
static U32                     s_swapchainWidth = 0;  /* per-eye recommended */
static U32                     s_swapchainHeight = 0; /* per-eye recommended */
static U32                     s_swapchainAllocW = 0; /* actual texture width (2x for stereo) */
static float                   s_headYawDeg = 0.0f;
static float                   s_headPitchDeg = 0.0f;
static float                   s_headRollDeg = 0.0f;
/* Per-eye views from the last xrLocateViews, kept for the next frame's
 * render: the engine renders each eye from exactly this pose, so the
 * compositor's time-warp from this pose to the fresh one is a clean
 * reprojection instead of a warp from an unrelated (rig) camera. */
static XrView                  s_renderViews[2];
static U32                     s_renderViewCount = 0;
static XrSwapchainImageOpenGLESKHR *s_swapchainImages = NULL;
static U32                     s_swapchainImageCount = 0;
static U32                     s_swapchainImageIndex = 0;
#ifdef __ANDROID__
/* Opt-in direct-to-swapchain stereo holds the acquired image across the
 * scene render, so present only has to release it. */
static S32                     s_directStereoArmed = 0;
#endif
/* Swapchain FBO completeness is validated on first use of each image; the
 * image format does not change inside a swapchain. Bit 0 means the index is
 * out of the cached range and the status is checked every time. */
static U64                     s_swapFboChecked = 0;
static U64                     s_swapFboInvalid = 0;

/* Predicted display time of the frame currently inside xrWaitFrame/xrEndFrame. */
/* 1 between a successful xrBeginFrame and its xrEndFrame (see
 * OpenXR_EnsureFrameBegun): gameplay begins the frame at frame start, so
 * present must not wait/begin a second time. */
static S32 s_frameBegun = 0;
static XrTime                  s_predictedDisplayTime = 0;

/* System id captured at session-create time; needed later to enumerate
 * view configurations for the immersive (mono blit) path. */
static XrSystemId              s_systemId = XR_NULL_SYSTEM_ID;

/* Optional runtime extensions (enabled only when enumerated, functions
 * resolved with xrGetInstanceProcAddr): performance clocks. */
static S32 s_extPerfSettings = 0;
static PFN_xrPerfSettingsSetPerformanceLevelEXT p_xrPerfSettingsSetPerformanceLevelEXT = NULL;

/* Passthrough (XR_FB_passthrough).  Enabled only when the runtime also
 * exposes the extensions it builds on (triangle mesh and the alpha-blend
 * composition layer), and when the system reports the passthrough
 * capability.  The feature and its layer are created lazily on the first
 * interior frame with the option on, then paused while it is unneeded. */
static S32 s_extPassthrough = 0;
static S32 s_passthroughCapable = 0;
static S32 s_passthroughFailed = 0;
static XrPassthroughFB s_passthrough = XR_NULL_HANDLE;
static XrPassthroughLayerFB s_passthroughLayer = XR_NULL_HANDLE;
static S32 s_passthroughFeatureRunning = 0;
static S32 s_passthroughLayerRunning = 0;
static PFN_xrCreatePassthroughFB p_xrCreatePassthroughFB = NULL;
static PFN_xrDestroyPassthroughFB p_xrDestroyPassthroughFB = NULL;
static PFN_xrPassthroughStartFB p_xrPassthroughStartFB = NULL;
static PFN_xrPassthroughPauseFB p_xrPassthroughPauseFB = NULL;
static PFN_xrCreatePassthroughLayerFB p_xrCreatePassthroughLayerFB = NULL;
static PFN_xrDestroyPassthroughLayerFB p_xrDestroyPassthroughLayerFB = NULL;
static PFN_xrPassthroughLayerPauseFB p_xrPassthroughLayerPauseFB = NULL;
static PFN_xrPassthroughLayerResumeFB p_xrPassthroughLayerResumeFB = NULL;

/* Game globals consumed by the interior present (CUBE_INTERIEUR is 0). */
extern S32 CubeMode;
extern S32 VrFovScale;
extern S32 VrPassthrough;
extern U8 CinemaMode;

/* GL renderer stereo eye capture (RENDER_GL.CPP). */
extern "C" U32 GL_GetStereoEyeFBO(S32 eye);
extern "C" U32 GL_GetStereoPairFBO(void);
extern "C" U32 GL_GetStereoPairWidth(void);
extern "C" U32 GL_GetStereoPairHeight(void);
extern "C" S32 GL_StereoPairNeedsPresentShader(void);
extern "C" S32 GL_BlitStereoPairToCurrent(U32 srcWidth, U32 srcHeight,
                                           U32 dstWidth, U32 dstHeight);
extern "C" S32 GL_DrawStereoOverlayToSwapchain(U32 dstWidth, U32 dstHeight);
extern "C" S32 GL_BlitStereoRectToCurrent(
    U32 srcX, U32 srcY, U32 srcWidth, U32 srcHeight, U32 texWidth,
    U32 texHeight, S32 dstX, S32 dstY, S32 dstWidth, S32 dstHeight,
    S32 viewportWidth, S32 viewportHeight);
extern "C" S32 GL_StereoEyeSceneDirect(void);
extern "C" S32 GL_TakeStereoPair(void);
extern "C" S32 GL_PresentSceneDirect(void);
extern "C" U32 GL_Get3DColorTex(void);
extern "C" U32 GL_Get3DFBO(void);
extern "C" S32 GL_DirectStereoSupported(void);
extern "C" S32 GL_DirectStereoActive(void);
extern "C" void GL_EndDirectStereo(void);

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
        s_extPerfSettings = HAS_EXT(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        if (s_extPerfSettings) {
            PUSH_EXT(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        }
        /* Passthrough depends on the triangle-mesh and alpha-blend layer
         * extensions; enable the whole set or none of it. */
        s_extPassthrough =
            HAS_EXT(XR_FB_PASSTHROUGH_EXTENSION_NAME) &&
            HAS_EXT(XR_FB_TRIANGLE_MESH_EXTENSION_NAME) &&
            HAS_EXT(XR_FB_COMPOSITION_LAYER_ALPHA_BLEND_EXTENSION_NAME);
        if (s_extPassthrough) {
            PUSH_EXT(XR_FB_PASSTHROUGH_EXTENSION_NAME);
            PUSH_EXT(XR_FB_TRIANGLE_MESH_EXTENSION_NAME);
            PUSH_EXT(XR_FB_COMPOSITION_LAYER_ALPHA_BLEND_EXTENSION_NAME);
        }
        s_extHandTracking = HAS_EXT(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        if (s_extHandTracking) {
            PUSH_EXT(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
            /* Optional companion that reports whether the joints came from
               optical tracking or a controller; harmless when absent. */
            if (HAS_EXT(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME)) {
                PUSH_EXT(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME);
            }
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
        /* Resolve the optional-extension entry points now that the instance
         * exists; each is only used when its extension was enabled. */
        if (s_extPerfSettings) {
            xrGetInstanceProcAddr(
                s_instance, "xrPerfSettingsSetPerformanceLevelEXT",
                (PFN_xrVoidFunction *)&p_xrPerfSettingsSetPerformanceLevelEXT);
        }
        if (s_extHandTracking) {
            xrGetInstanceProcAddr(s_instance, "xrCreateHandTrackerEXT",
                (PFN_xrVoidFunction *)&p_xrCreateHandTrackerEXT);
            xrGetInstanceProcAddr(s_instance, "xrDestroyHandTrackerEXT",
                (PFN_xrVoidFunction *)&p_xrDestroyHandTrackerEXT);
            xrGetInstanceProcAddr(s_instance, "xrLocateHandJointsEXT",
                (PFN_xrVoidFunction *)&p_xrLocateHandJointsEXT);
        }
        if (s_extPassthrough) {
            xrGetInstanceProcAddr(s_instance, "xrCreatePassthroughFB",
                (PFN_xrVoidFunction *)&p_xrCreatePassthroughFB);
            xrGetInstanceProcAddr(s_instance, "xrDestroyPassthroughFB",
                (PFN_xrVoidFunction *)&p_xrDestroyPassthroughFB);
            xrGetInstanceProcAddr(s_instance, "xrPassthroughStartFB",
                (PFN_xrVoidFunction *)&p_xrPassthroughStartFB);
            xrGetInstanceProcAddr(s_instance, "xrPassthroughPauseFB",
                (PFN_xrVoidFunction *)&p_xrPassthroughPauseFB);
            xrGetInstanceProcAddr(s_instance, "xrCreatePassthroughLayerFB",
                (PFN_xrVoidFunction *)&p_xrCreatePassthroughLayerFB);
            xrGetInstanceProcAddr(s_instance, "xrDestroyPassthroughLayerFB",
                (PFN_xrVoidFunction *)&p_xrDestroyPassthroughLayerFB);
            xrGetInstanceProcAddr(s_instance, "xrPassthroughLayerPauseFB",
                (PFN_xrVoidFunction *)&p_xrPassthroughLayerPauseFB);
            xrGetInstanceProcAddr(s_instance, "xrPassthroughLayerResumeFB",
                (PFN_xrVoidFunction *)&p_xrPassthroughLayerResumeFB);
        }
        OXR_LOGI("optional ext: perfSettings=%d passthrough=%d",
                 s_extPerfSettings, s_extPassthrough);
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

    /* Passthrough is only usable when the headset reports the capability;
     * enables the Display → VR row and gates layer creation. */
    s_passthroughCapable = 0;
    if (s_extPassthrough) {
        XrSystemPassthroughProperties2FB ptProps;
        XrSystemProperties sysProps;
        memset(&ptProps, 0, sizeof(ptProps));
        ptProps.type = XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES2_FB;
        memset(&sysProps, 0, sizeof(sysProps));
        sysProps.type = XR_TYPE_SYSTEM_PROPERTIES;
        sysProps.next = &ptProps;
        XrResult ptRc = xrGetSystemProperties(s_instance, sysId, &sysProps);
        if (XR_SUCCEEDED(ptRc)) {
            s_passthroughCapable =
                (ptProps.capabilities & XR_PASSTHROUGH_CAPABILITY_BIT_FB) != 0;
        } else {
            OXR_LOGW("xrGetSystemProperties(passthrough) failed: %s (0x%x)",
                     OXR_ResultStr(ptRc), (unsigned)ptRc);
        }
        OXR_LOGI("passthrough capabilities=0x%llx supported=%d",
                 (unsigned long long)ptProps.capabilities,
                 s_passthroughCapable);
    }

    /* Per the OpenXR spec, xrGetGraphicsRequirementsKHR MUST be called for
     * the chosen graphics API before xrCreateSession, otherwise the
     * runtime returns XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING (-50).
     * The returned minOpenGLESVersion is the lowest GLES major.minor the
     * runtime supports on this system; we don't gate on it (the engine is
     * GLES 2/3 via SDL) but we do log it for diagnostics. */
    PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetReqs = NULL;
    XrResult grRc = xrGetInstanceProcAddr(s_instance,
        "xrGetOpenGLESGraphicsRequirementsKHR",
        (PFN_xrVoidFunction *)&pfnGetReqs);
    if (XR_FAILED(grRc) || pfnGetReqs == NULL) {
        OXR_LOGE("xrGetInstanceProcAddr(xrGetOpenGLESGraphicsRequirementsKHR) failed: %s (0x%x)",
                 OXR_ResultStr(grRc), (unsigned)grRc);
        Log_Warn("OpenXR: xrGetOpenGLESGraphicsRequirementsKHR unavailable");
        return 0;
    }
    XrGraphicsRequirementsOpenGLESKHR gfxReqs;
    memset(&gfxReqs, 0, sizeof(gfxReqs));
    gfxReqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
    XrResult grcRc = pfnGetReqs(s_instance, sysId, &gfxReqs);
    if (XR_FAILED(grcRc)) {
        OXR_LOGE("xrGetOpenGLESGraphicsRequirementsKHR failed: %s (0x%x)",
                 OXR_ResultStr(grcRc), (unsigned)grcRc);
        return 0;
    }
    OXR_LOGI("xrGetOpenGLESGraphicsRequirementsKHR OK minApi=%llu maxApi=%llu",
             (unsigned long long)gfxReqs.minApiVersionSupported,
             (unsigned long long)gfxReqs.maxApiVersionSupported);

    XrGraphicsBindingOpenGLESAndroidKHR gfxBinding;
    memset(&gfxBinding, 0, sizeof(gfxBinding));
    gfxBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
    gfxBinding.display = (EGLDisplay)SDL_EGL_GetCurrentDisplay();
    gfxBinding.context = (EGLContext)SDL_GL_GetCurrentContext();
    OXR_LOGI("xrCreateSession gfxBinding display=%p context=%p",
             (void *)gfxBinding.display, (void *)gfxBinding.context);

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
    OXR_LOGI("xrCreateReferenceSpace STAGE...");
    rc = xrCreateReferenceSpace(s_session, &refInfo, &s_stageSpace);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrCreateReferenceSpace STAGE failed (0x%x)", (unsigned)rc);
        return 0;
    }
    OXR_LOGI("xrCreateReferenceSpace STAGE OK");

    /* XR_REFERENCE_SPACE_TYPE_LOCAL is always valid on Quest 3 even when
     * no Guardian play area is configured (where STAGE tracking returns
     * IsValid=false). It is the headset's origin at startup, and the
     * mono-blit path needs it for both xrLocateViews and the projection
     * layer space, otherwise xrEndFrame is submitted with layerCount=0
     * and the runtime shows the "still running" placeholder panel. */
    refInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    OXR_LOGI("xrCreateReferenceSpace LOCAL...");
    rc = xrCreateReferenceSpace(s_session, &refInfo, &s_localSpace);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrCreateReferenceSpace LOCAL failed (0x%x)", (unsigned)rc);
        return 0;
    }
    OXR_LOGI("xrCreateReferenceSpace LOCAL OK");

    refInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    OXR_LOGI("xrCreateReferenceSpace VIEW...");
    rc = xrCreateReferenceSpace(s_session, &refInfo, &s_viewSpace);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrCreateReferenceSpace VIEW failed (0x%x)", (unsigned)rc);
        return 0;
    }
    OXR_LOGI("xrCreateReferenceSpace VIEW OK");

    /* Immersive mono blit: build the swapchain now so the first frame
     * can present. Failure is non-fatal — the engine still runs in
     * "input-only" mode if the runtime cannot allocate a swapchain. */
    OXR_LOGI("OpenXR_CreateSwapchain...");
    if (!OpenXR_CreateSwapchain()) {
        Log_Warn("OpenXR: immersive swapchain unavailable, falling back to input-only");
    }
    OXR_LOGI("OpenXR_CreateSwapchain returned");

    /* Optical hand trackers.  Absent extension or a rejected tracker just
       leaves the controller-pose fallback in place. */
    if (s_extHandTracking && p_xrCreateHandTrackerEXT != NULL) {
        static const XrHandEXT handKinds[2] = {XR_HAND_LEFT_EXT,
                                               XR_HAND_RIGHT_EXT};
        for (S32 h = 0; h < 2; h++) {
            XrHandTrackerCreateInfoEXT ci;
            memset(&ci, 0, sizeof(ci));
            ci.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT;
            ci.hand = handKinds[h];
            ci.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            XrResult hrc = p_xrCreateHandTrackerEXT(s_session, &ci,
                                                    &s_handTrackers[h]);
            if (XR_FAILED(hrc)) {
                s_handTrackers[h] = XR_NULL_HANDLE;
                OXR_LOGW("xrCreateHandTrackerEXT h%d failed: %s (0x%x)", h,
                         OXR_ResultStr(hrc), (unsigned)hrc);
            }
        }
        OXR_LOGI("hand tracking: ext=%d left=%d right=%d", s_extHandTracking,
                 s_handTrackers[0] != XR_NULL_HANDLE,
                 s_handTrackers[1] != XR_NULL_HANDLE);
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
    s_swapFboChecked = 0;
    s_swapFboInvalid = 0;
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

    /* GLES3+ sized formats used below; the engine's GL headers may not
     * have them on every toolchain, so define defensively. */
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

    /* 4. Enumerate the swapchain formats the runtime supports and pick a
     *    color format.  The Meta Quest OpenXR runtime advertises GL_RGBA8
     *    (0x8058) and GL_SRGB8_ALPHA8 (0x8C43); the unsized GL_RGBA
     *    (0x1908) is NOT in the supported list and using it makes the
     *    runtime allocate a swapchain that the compositor later rejects
     *    with "swapchain format unsupported; 6408".  Prefer sRGB first if
     *    available, otherwise fall back to RGBA8. */
    U32 formatCount = 0;
    rc = xrEnumerateSwapchainFormats(s_session, 0, &formatCount, NULL);
    if (XR_FAILED(rc) || formatCount == 0) {
        Log_Warn("OpenXR: no swapchain formats (0x%x)", (unsigned)rc);
        return 0;
    }
    int64_t *formats = (int64_t *)malloc(sizeof(int64_t) * formatCount);
    rc = xrEnumerateSwapchainFormats(s_session, formatCount, &formatCount, formats);
    if (XR_FAILED(rc)) { free(formats); return 0; }
    int64_t pickedFormat = 0;
    {
        U32 k;
        /* Prefer GL_RGBA8 over GL_SRGB8_ALPHA8: glCopyTexSubImage2D in
         * GLES 2.0 does not support SRGB formats (returns GL_INVALID_OPERATION
         * 0x502). The Quest runtime supports GL_RGBA8, so use that for the
         * swapchain to allow the CPU-side blit to succeed. */
        for (k = 0; k < formatCount; k++) {
            if (formats[k] == GL_RGBA8) { pickedFormat = formats[k]; break; }
        }
        if (pickedFormat == 0) {
            for (k = 0; k < formatCount; k++) {
                if (formats[k] == GL_RGBA) { pickedFormat = formats[k]; break; }
            }
        }
        if (pickedFormat == 0) {
            for (k = 0; k < formatCount; k++) {
                if (formats[k] == GL_SRGB8_ALPHA8) { pickedFormat = formats[k]; break; }
            }
        }
        if (pickedFormat == 0) {
            pickedFormat = formats[0];
        }
    }
    OXR_LOGI("swapchain format: picked=0x%llx (from %u supported)", (unsigned long long)pickedFormat, (unsigned)formatCount);
    free(formats);

    XrSwapchainCreateInfo scInfo;
    memset(&scInfo, 0, sizeof(scInfo));
    scInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    scInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    scInfo.format = pickedFormat;
    scInfo.sampleCount = 1;
    /* Stereo view configs: allocate 2x the per-eye width so each eye gets
     * the FULL recommended resolution side-by-side (left/right), not a
     * vertically halved top-bottom split. */
    scInfo.width = (s_viewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
                       ? s_swapchainWidth * 2
                       : s_swapchainWidth;
    scInfo.height = s_swapchainHeight;
    s_swapchainAllocW = scInfo.width;
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
    /* Per-image composite FBOs are created fresh at present time (the GL
     * context only exists there) — swapchain textures must not keep a
     * persistent FBO attachment across acquire/release cycles. */
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
/* Passthrough lifecycle.  The feature and its reconstruction layer are
 * created on demand, then paused whenever the player is not in an interior
 * or has the option off; leaving it running would keep environment
 * reconstruction busy for nothing. */
static S32 OpenXR_PassthroughRequested(void) {
    return VrPassthrough && s_extPassthrough && s_passthroughCapable &&
           !s_passthroughFailed;
}

static S32 OpenXR_EnsurePassthroughLayer(void) {
    if (s_passthroughLayer != XR_NULL_HANDLE)
        return 1;
    if (!OpenXR_PassthroughRequested() || s_session == XR_NULL_HANDLE)
        return 0;
    if (p_xrCreatePassthroughFB == NULL ||
        p_xrCreatePassthroughLayerFB == NULL ||
        p_xrPassthroughStartFB == NULL ||
        p_xrPassthroughLayerResumeFB == NULL) {
        Log_Warn("OpenXR: passthrough entry points unavailable");
        s_passthroughFailed = 1;
        return 0;
    }
    if (s_passthrough == XR_NULL_HANDLE) {
        XrPassthroughCreateInfoFB createInfo;
        memset(&createInfo, 0, sizeof(createInfo));
        createInfo.type = XR_TYPE_PASSTHROUGH_CREATE_INFO_FB;
        /* Created paused; OpenXR_UpdatePassthrough starts it for interiors. */
        createInfo.flags = 0;
        XrResult rc = p_xrCreatePassthroughFB(s_session, &createInfo,
                                              &s_passthrough);
        if (XR_FAILED(rc)) {
            OXR_LOGW("xrCreatePassthroughFB failed: %s (0x%x)",
                     OXR_ResultStr(rc), (unsigned)rc);
            Log_Warn("OpenXR: passthrough feature unavailable (0x%x)",
                     (unsigned)rc);
            s_passthrough = XR_NULL_HANDLE;
            s_passthroughFailed = 1;
            return 0;
        }
        OXR_LOGI("xrCreatePassthroughFB OK");
    }
    {
        XrPassthroughLayerCreateInfoFB layerInfo;
        memset(&layerInfo, 0, sizeof(layerInfo));
        layerInfo.type = XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB;
        layerInfo.passthrough = s_passthrough;
        layerInfo.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
        layerInfo.flags = 0;
        XrResult rc = p_xrCreatePassthroughLayerFB(
            s_session, &layerInfo, &s_passthroughLayer);
        if (XR_FAILED(rc)) {
            OXR_LOGW("xrCreatePassthroughLayerFB failed: %s (0x%x)",
                     OXR_ResultStr(rc), (unsigned)rc);
            Log_Warn("OpenXR: passthrough layer unavailable (0x%x)",
                     (unsigned)rc);
            s_passthroughLayer = XR_NULL_HANDLE;
            s_passthroughFailed = 1;
            return 0;
        }
        OXR_LOGI("xrCreatePassthroughLayerFB OK");
    }
    return 1;
}

static void OpenXR_UpdatePassthrough(void) {
    /* CUBE_INTERIEUR is 0 (see SOURCES/COMMON.H); LIB386 does not include
       the game header. */
    const S32 want = OpenXR_PassthroughRequested() && CubeMode == 0 &&
                     !CinemaMode;
    if (!want) {
        if (s_passthroughLayer != XR_NULL_HANDLE && s_passthroughLayerRunning &&
            p_xrPassthroughLayerPauseFB != NULL) {
            p_xrPassthroughLayerPauseFB(s_passthroughLayer);
            s_passthroughLayerRunning = 0;
            OXR_LOGI("passthrough layer paused");
        }
        if (s_passthrough != XR_NULL_HANDLE && s_passthroughFeatureRunning &&
            p_xrPassthroughPauseFB != NULL) {
            p_xrPassthroughPauseFB(s_passthrough);
            s_passthroughFeatureRunning = 0;
        }
        return;
    }
    if (!OpenXR_EnsurePassthroughLayer())
        return;
    if (!s_passthroughFeatureRunning) {
        XrResult rc = p_xrPassthroughStartFB(s_passthrough);
        if (XR_FAILED(rc)) {
            OXR_LOGW("xrPassthroughStartFB failed: %s (0x%x)",
                     OXR_ResultStr(rc), (unsigned)rc);
            s_passthroughFailed = 1;
            return;
        }
        s_passthroughFeatureRunning = 1;
    }
    if (!s_passthroughLayerRunning) {
        XrResult rc = p_xrPassthroughLayerResumeFB(s_passthroughLayer);
        if (XR_FAILED(rc)) {
            OXR_LOGW("xrPassthroughLayerResumeFB failed: %s (0x%x)",
                     OXR_ResultStr(rc), (unsigned)rc);
            s_passthroughFailed = 1;
            return;
        }
        s_passthroughLayerRunning = 1;
        OXR_LOGI("passthrough layer running");
    }
}

/* -------------------------------------------------------------------------- */
static void OpenXR_DestroySession(void) {
#ifdef __ANDROID__
    if (s_passthroughLayer != XR_NULL_HANDLE &&
        p_xrDestroyPassthroughLayerFB != NULL) {
        p_xrDestroyPassthroughLayerFB(s_passthroughLayer);
    }
    s_passthroughLayer = XR_NULL_HANDLE;
    s_passthroughLayerRunning = 0;
    if (s_passthrough != XR_NULL_HANDLE &&
        p_xrDestroyPassthroughFB != NULL) {
        p_xrDestroyPassthroughFB(s_passthrough);
    }
    s_passthrough = XR_NULL_HANDLE;
    s_passthroughFeatureRunning = 0;
    s_passthroughFailed = 0;

    for (S32 h = 0; h < 2; h++) {
        if (s_handTrackers[h] != XR_NULL_HANDLE) {
            if (p_xrDestroyHandTrackerEXT != NULL) {
                p_xrDestroyHandTrackerEXT(s_handTrackers[h]);
            }
            s_handTrackers[h] = XR_NULL_HANDLE;
        }
        s_handJointsValid[h] = 0;
        for (S32 t = 0; t < 2; t++) {
            if (s_handSpaces[h][t] != XR_NULL_HANDLE) {
                xrDestroySpace(s_handSpaces[h][t]);
                s_handSpaces[h][t] = XR_NULL_HANDLE;
            }
            s_handPoseValid[h][t] = 0;
        }
    }

    if (s_swapchain != XR_NULL_HANDLE) {        xrDestroySwapchain(s_swapchain);
        s_swapchain = XR_NULL_HANDLE;
    }
    if (s_swapchainImages) {
        free(s_swapchainImages);
        s_swapchainImages = NULL;
    }
    s_swapchainImageCount = 0;
    s_swapchainImageIndex = 0;
    s_swapFboChecked = 0;
    s_swapFboInvalid = 0;
    s_frameBegun = 0;
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
    if (s_localSpace != XR_NULL_HANDLE) {
        xrDestroySpace(s_localSpace);
        s_localSpace = XR_NULL_HANDLE;
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
    OXR_LOGI("OpenXR_CreateActions: entering");
    XrActionSetCreateInfo asInfo;
    memset(&asInfo, 0, sizeof(asInfo));
    asInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
    SDL_strlcpy(asInfo.actionSetName, "gameplay", sizeof(asInfo.actionSetName));
    SDL_strlcpy(asInfo.localizedActionSetName, "Gameplay", sizeof(asInfo.localizedActionSetName));

    XrResult rc = xrCreateActionSet(s_instance, &asInfo, &s_actionSet);
    if (XR_FAILED(rc)) return 0;

    xrStringToPath(s_instance, "/user/hand/left", &s_handPaths[0]);
    xrStringToPath(s_instance, "/user/hand/right", &s_handPaths[1]);

    static const char *boolNames[] = {
        "a_click", "b_click", "x_click", "y_click",
        "lstick_click", "rstick_click",
        "ltrigger", "rtrigger",
        "lgrip", "rgrip",
        "menu_click",
        /* Capacitive thumb-presence sensors on the left hand.  The separate
           thumbrest sensor covers the controller-body rest area; the stick
           touch is a fallback on runtimes that expose it. */
        "lthumbstick_touch", "lthumbrest_touch",
    };
    U32 i;
    for (i = 0; i < sizeof(boolNames)/sizeof(boolNames[0]); i++) {
        XrActionCreateInfo aInfo;
        memset(&aInfo, 0, sizeof(aInfo));
        aInfo.type = XR_TYPE_ACTION_CREATE_INFO;
        aInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        aInfo.countSubactionPaths = 2;
        aInfo.subactionPaths = s_handPaths;
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
        aInfo.countSubactionPaths = 2;
        aInfo.subactionPaths = s_handPaths;
        SDL_strlcpy(aInfo.actionName, vecNames[i], sizeof(aInfo.actionName));
        SDL_strlcpy(aInfo.localizedActionName, vecNames[i], sizeof(aInfo.localizedActionName));
        rc = xrCreateAction(s_actionSet, &aInfo,
                            &s_actions[13 + i]);
        if (XR_FAILED(rc)) return 0;
    }

    /* Meta Quest 3 controllers are exposed under the oculus/touch_controller
     * interaction profile. Both hand mappings (left=ABXY swapped, right=ABXY
     * canonical) are covered so the binding set is profile-portable. */
    static const char *poseNames[] = {
        "laim_pose", "raim_pose", "lgrip_pose", "rgrip_pose",
    };
    for (i = 0; i < sizeof(poseNames) / sizeof(poseNames[0]); i++) {
        XrActionCreateInfo aInfo;
        memset(&aInfo, 0, sizeof(aInfo));
        aInfo.type = XR_TYPE_ACTION_CREATE_INFO;
        aInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        aInfo.countSubactionPaths = 2;
        aInfo.subactionPaths = s_handPaths;
        SDL_strlcpy(aInfo.actionName, poseNames[i], sizeof(aInfo.actionName));
        SDL_strlcpy(aInfo.localizedActionName, poseNames[i],
                    sizeof(aInfo.localizedActionName));
        rc = xrCreateAction(s_actionSet, &aInfo, &s_actions[15 + i]);
        if (XR_FAILED(rc)) return 0;
    }

    /* Right-hand thumb rest, used as the cheat modifier. The left hand uses
       the same capacitive pad to switch the interior camera to zoom, so the
       gesture is already familiar. */
    {
        XrActionCreateInfo aInfo;
        memset(&aInfo, 0, sizeof(aInfo));
        aInfo.type = XR_TYPE_ACTION_CREATE_INFO;
        aInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        aInfo.countSubactionPaths = 2;
        aInfo.subactionPaths = s_handPaths;
        SDL_strlcpy(aInfo.actionName, "rthumbrest_touch",
                    sizeof(aInfo.actionName));
        SDL_strlcpy(aInfo.localizedActionName, "rthumbrest_touch",
                    sizeof(aInfo.localizedActionName));
        rc = xrCreateAction(s_actionSet, &aInfo, &s_actions[19]);
        if (XR_FAILED(rc)) return 0;
    }

    /* Meta Quest 3 controllers are exposed under the oculus/touch_controller
     * interaction profile. Both hand mappings (left=ABXY swapped, right=ABXY
     * canonical) are covered so the binding set is profile-portable. */
    static const char *profileStr = "/interaction_profiles/oculus/touch_controller";
    XrPath profilePath = XR_NULL_PATH;
    xrStringToPath(s_instance, profileStr, &profilePath);

    static const char *bindingStrs[20] = {
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
        "/user/hand/left/input/menu/click",
        "/user/hand/left/input/thumbstick/touch",
        "/user/hand/left/input/thumbrest/touch",
        "/user/hand/left/input/thumbstick",
        "/user/hand/right/input/thumbstick",
        "/user/hand/left/input/aim/pose",
        "/user/hand/right/input/aim/pose",
        "/user/hand/left/input/grip/pose",
        "/user/hand/right/input/grip/pose",
        "/user/hand/right/input/thumbrest/touch",
    };
    XrActionSuggestedBinding bindings[20];
    U32 j;
    for (j = 0; j < 20; j++) {
        bindings[j].action = s_actions[j];
        xrStringToPath(s_instance, bindingStrs[j], &bindings[j].binding);
    }

    XrInteractionProfileSuggestedBinding suggest;
    memset(&suggest, 0, sizeof(suggest));
    suggest.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
    suggest.interactionProfile = profilePath;
    suggest.suggestedBindings = bindings;
    suggest.countSuggestedBindings = 20;

    rc = xrSuggestInteractionProfileBindings(s_instance, &suggest);
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrSuggestInteractionProfileBindings failed (0x%x); "
                 "retrying without the optional right-thumb sensor",
                 (unsigned)rc);
        suggest.countSuggestedBindings = 19;
        rc = xrSuggestInteractionProfileBindings(s_instance, &suggest);
        if (XR_FAILED(rc))
            Log_Warn("OpenXR: xrSuggestInteractionProfileBindings core failed (0x%x)",
                     (unsigned)rc);
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

    for (S32 h = 0; h < 2; h++) {
        for (S32 t = 0; t < 2; t++) {
            XrActionSpaceCreateInfo spaceInfo;
            memset(&spaceInfo, 0, sizeof(spaceInfo));
            spaceInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
            spaceInfo.action = s_actions[15 + t * 2 + h];
            spaceInfo.subactionPath = s_handPaths[h];
            spaceInfo.poseInActionSpace.orientation.w = 1.0f;
            s_handSpaces[h][t] = XR_NULL_HANDLE;
            rc = xrCreateActionSpace(s_session, &spaceInfo, &s_handSpaces[h][t]);
            if (XR_FAILED(rc)) {
                s_handSpaces[h][t] = XR_NULL_HANDLE;
                Log_Warn("OpenXR: hand action space h%d t%d failed (0x%x)", h,
                         t, (unsigned)rc);
            }
        }
    }
    OXR_LOGI("OpenXR_CreateActions: returning 1 (success)");

    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
static void OpenXR_ReadActions(void) {
#ifdef __ANDROID__
    if (!s_openxrActive) return;
    /* Per the OpenXR spec, xrSyncActions takes an XrActionsSyncInfo* which
     * describes which action sets to sync. Passing NULL is only valid when
     * there are no active action sets, and the Meta Quest runtime crashes
     * with a NULL-pointer dereference inside its vrapiimpl if called with
     * NULL after xrAttachSessionActionSets. Match the reference layout in
     * dubrovskiy-yevhen-stakelogic/vice-city-vr-quest xr_vulkan_session.cpp
     * syncInput(): one active action set, no subaction path filter, and
     * skip the call entirely when the session is not focused (the runtime
     * is not in a state to deliver input). */
    if (s_sessionState != XR_SESSION_STATE_FOCUSED) return;

    XrActiveActionSet active;
    memset(&active, 0, sizeof(active));
    active.actionSet = s_actionSet;
    active.subactionPath = XR_NULL_PATH;
    XrActionsSyncInfo syncInfo;
    memset(&syncInfo, 0, sizeof(syncInfo));
    syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &active;

    XrResult rc = xrSyncActions(s_session, &syncInfo);
    if (XR_FAILED(rc)) return;

    XrActionStateGetInfo gi;
    XrActionStateBoolean stb;
    XrActionStateVector2f stv;
    U32 i;

    for (i = 0; i < 13; i++) {
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
            case 10: out = &OpenXR_MenuBtn; break;
            case 11: out = &OpenXR_LeftThumbstickTouched; break;
            case 12: out = &OpenXR_LeftThumbrestTouched; break;
        }
        if (out != NULL) {
            /* Actions are filtered to both hand subaction paths, so query
             * once per hand and OR the results. */
            S32 pressed = 0;
            U32 h;
            for (h = 0; h < 2; h++) {
                gi.subactionPath = s_handPaths[h];
                if (XR_SUCCEEDED(xrGetActionStateBoolean(s_session, &gi, &stb))
                    && stb.isActive && stb.currentState) {
                    pressed = 1;
                }
            }
            *out = pressed;
        }
    }

    {
        /* Optional right-thumb rest, used by the cheat chord. It is read
           separately because the core bool loop only covers actions 0-12;
           an unbound runtime simply leaves it at 0. */
        S32 pressed = 0;
        U32 h;
        memset(&gi, 0, sizeof(gi));
        memset(&stb, 0, sizeof(stb));
        gi.type = XR_TYPE_ACTION_STATE_GET_INFO;
        gi.action = s_actions[19];
        stb.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        for (h = 0; h < 2; h++) {
            gi.subactionPath = s_handPaths[h];
            if (XR_SUCCEEDED(xrGetActionStateBoolean(s_session, &gi, &stb))
                && stb.isActive && stb.currentState) {
                pressed = 1;
            }
        }
        OpenXR_RightThumbrestTouched = pressed;
    }

    for (i = 13; i < 15; i++) {
        memset(&gi, 0, sizeof(gi));
        memset(&stv, 0, sizeof(stv));
        gi.type = XR_TYPE_ACTION_STATE_GET_INFO;
        gi.action = s_actions[i];
        stv.type = XR_TYPE_ACTION_STATE_VECTOR2F;
        S16 *outx = NULL, *outy = NULL;
        switch (i) {
            case 13: outx = &OpenXR_LeftStickX;  outy = &OpenXR_LeftStickY;  break;
            case 14: outx = &OpenXR_RightStickX; outy = &OpenXR_RightStickY; break;
        }
        if (outx != NULL && outy != NULL) {
            /* Take whichever hand reports an active stick state. */
            *outx = 0;
            *outy = 0;
            U32 h;
            for (h = 0; h < 2; h++) {
                gi.subactionPath = s_handPaths[h];
                if (XR_SUCCEEDED(xrGetActionStateVector2f(s_session, &gi, &stv))
                    && stv.isActive) {
                    *outx = (S16)(stv.currentState.x * 32767.0f);
                    *outy = (S16)(-stv.currentState.y * 32767.0f);
                }
            }
        }
    }

    /* Input diagnostics: which interaction profile the runtime matched (a
     * mismatch leaves every action isActive=0), then the synced state at a
     * low rate so a button press is observable in logcat without per-frame
     * spam. */
    {
        static S32 s_profileLogged = 0;
        if (!s_profileLogged) {
            s_profileLogged = 1;
            static const char *hands[2] = { "/user/hand/left", "/user/hand/right" };
            U32 h;
            for (h = 0; h < 2; h++) {
                XrPath handPath = XR_NULL_PATH;
                if (XR_SUCCEEDED(xrStringToPath(s_instance, hands[h], &handPath))) {
                    XrInteractionProfileState profState;
                    memset(&profState, 0, sizeof(profState));
                    profState.type = XR_TYPE_INTERACTION_PROFILE_STATE;
                    XrResult prc = xrGetCurrentInteractionProfile(s_session, handPath, &profState);
                    char buf[XR_MAX_PATH_LENGTH];
                    memset(buf, 0, sizeof(buf));
                    if (XR_SUCCEEDED(prc) && profState.interactionProfile != XR_NULL_PATH) {
                        uint32_t plen = 0;
                        xrPathToString(s_instance, profState.interactionProfile,
                                       (uint32_t)sizeof(buf), &plen, buf);
                    }
                    OXR_LOGI("interaction profile %s -> rc=0x%x '%s'",
                             hands[h], (unsigned)prc, buf);
                }
            }
        }
        {
            static U32 s_diagFrame = 0;
            if (s_diagFrame < 8 || (s_diagFrame % 180) == 0) {
                OXR_LOGI("ReadActions[%u]: state=%d A=%d B=%d X=%d Y=%d Menu=%d "
                         "Ltrig=%d Rtrig=%d Lgrip=%d Rgrip=%d "
                         "L=%d,%d R=%d,%d",
                         (unsigned)s_diagFrame, (int)s_sessionState,
                         OpenXR_ABtn, OpenXR_BBtn, OpenXR_XBtn, OpenXR_YBtn,
                         OpenXR_MenuBtn,
                         OpenXR_LeftTrigger, OpenXR_RightTrigger,
                         OpenXR_LeftGripDown, OpenXR_RightGripDown,
                         OpenXR_LeftStickX, OpenXR_LeftStickY,
                         OpenXR_RightStickX, OpenXR_RightStickY);
            }
            s_diagFrame++;
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

/* Raw controller state published to JOYSTICK.CPP. Defined unconditionally
 * (zero) so callers compile and link with no #ifdef; the OpenXR build
 * mutates them in OpenXR_ReadActions / OpenXR_UpdateControllerState. */
S32 OpenXR_LeftGripDown = 0;
S32 OpenXR_RightGripDown = 0;
S32 OpenXR_LeftTrigger = 0;
S32 OpenXR_RightTrigger = 0;
S32 OpenXR_LeftThumbstickDown = 0;
S32 OpenXR_RightThumbstickDown = 0;
S32 OpenXR_LeftThumbstickTouched = 0;
S32 OpenXR_LeftThumbrestTouched = 0;
S32 OpenXR_RightThumbrestTouched = 0;
S16 OpenXR_LeftStickX = 0;
S16 OpenXR_LeftStickY = 0;
S16 OpenXR_RightStickX = 0;
S16 OpenXR_RightStickY = 0;
S32 OpenXR_ABtn = 0;
S32 OpenXR_BBtn = 0;
S32 OpenXR_XBtn = 0;
S32 OpenXR_YBtn = 0;
S32 OpenXR_MenuBtn = 0;

/* -------------------------------------------------------------------------- */
S32 OpenXR_Init(void) {
#ifdef LBA2_OPENXR
    if (s_openxrInited) return s_openxrActive;
    s_openxrInited = 1;

    if (!OpenXR_CreateInstance()) {
        Log_Warn("OpenXR: failed to create instance");
        return 0;
    }

    /* The session (and its swapchain / reference spaces) must exist before
     * the runtime can transition into XR_SESSION_STATE_READY. The previous
     * version of this loop tried to create the session inside the READY
     * event handler, which is unreachable: without a session, no session-
     * state events are ever emitted and xrPollEvent returns
     * XR_EVENT_UNAVAILABLE on the first call, so the loop breaks and the
     * engine tears the instance back down. Create the session first; then
     * wait for the IDLE state-change event (spec section 8.2) and call
     * xrBeginSession from inside that handler; then wait for READY. */
    if (!OpenXR_CreateSession()) {
        Log_Warn("OpenXR: session creation failed");
        xrDestroyInstance(s_instance);
        s_instance = XR_NULL_HANDLE;
        return 0;
    }

    /* OpenXR session lifecycle (spec section 8.2 / vice-city-vr-quest
     * xr_vulkan_session.cpp):
     *   xrCreateInstance  -> instance alive
     *   xrCreateSession   -> session in IDLE; runtime begins emitting state
     *                        events; we do NOT call xrBeginSession here
     *   createActions + createSwapchain
     *   OpenXR_Init returns; the game thread starts calling OpenXR_PollEvents
     *   OpenXR_PollEvents receives IDLE then READY; on the FIRST READY event
     *   it calls xrBeginSession and sets s_openxrActive. xrBeginSession
     *   succeeds in READY (and on Quest 3 also succeeds from the spec's
     *   IDLE state, but the canonical trigger is the READY event).
     *   After xrBeginSession the runtime advances the state to SYNCHRONIZED
     *   then VISIBLE then FOCUSED. The frame loop calls xrWaitFrame /
     *   xrBeginFrame / xrEndFrame while shouldRender is true. */
    if (!OpenXR_CreateActions()) {
        Log_Warn("OpenXR: action creation failed");
        OpenXR_DestroySession();
        xrDestroyInstance(s_instance);
        s_instance = XR_NULL_HANDLE;
        return 0;
    }

    s_sessionState = XR_SESSION_STATE_IDLE;
    s_openxrActive = 1;
    Log_Info("OpenXR: instance + session + actions ready; awaiting READY for xrBeginSession");
    return 1;
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
    s_sessionRunning = 0;
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

U32 OpenXR_GetSwapchainWidth(void) {
#ifdef LBA2_OPENXR
    return s_swapchainWidth;
#else
    return 0;
#endif
}

float OpenXR_HeadYawDeg(void) {
#ifdef LBA2_OPENXR
    return s_headYawDeg;
#else
    return 0.0f;
#endif
}

float OpenXR_HeadRollDeg(void) {
#ifdef LBA2_OPENXR
    return s_headRollDeg;
#else
    return 0.0f;
#endif
}

float OpenXR_HeadPitchDeg(void) {
#ifdef LBA2_OPENXR
    return s_headPitchDeg;
#else
    return 0.0f;
#endif
}

/* Per-eye view from the last xrLocateViews (one frame stale by design: the
 * compositor time-warps from exactly this pose to the fresh one, so
 * rendering the eye from this pose is what makes head rotation warp-free).
 * quat = x,y,z,w; pos = x,y,z in LOCAL tracking space (the engine renders
 * from its own rig position plus the tracked lean, with the orientation
 * applied verbatim). Returns
 * 0 when the eye has no fresh view (first frames, mono session). */
S32 OpenXR_GetEyeView(S32 eye, float *quat, float *pos) {
#ifdef LBA2_OPENXR
    if (eye < 0 || eye > 1 || s_renderViewCount <= (U32)eye) {
        return 0;
    }
    quat[0] = s_renderViews[eye].pose.orientation.x;
    quat[1] = s_renderViews[eye].pose.orientation.y;
    quat[2] = s_renderViews[eye].pose.orientation.z;
    quat[3] = s_renderViews[eye].pose.orientation.w;
    pos[0] = s_renderViews[eye].pose.position.x;
    pos[1] = s_renderViews[eye].pose.position.y;
    pos[2] = s_renderViews[eye].pose.position.z;
    return 1;
#else
    (void)eye; (void)quat; (void)pos;
    return 0;
#endif
}

S32 OpenXR_GetHandPose(S32 hand, S32 aimOrGrip, float *quat, float *pos) {
#ifdef LBA2_OPENXR
    if (hand < 0 || hand > 1 || aimOrGrip < 0 || aimOrGrip > 1) {
        return 0;
    }
    if (!s_handPoseValid[hand][aimOrGrip]) {
        return 0;
    }
    const XrPosef *p = &s_handPoses[hand][aimOrGrip];
    quat[0] = p->orientation.x;
    quat[1] = p->orientation.y;
    quat[2] = p->orientation.z;
    quat[3] = p->orientation.w;
    pos[0] = p->position.x;
    pos[1] = p->position.y;
    pos[2] = p->position.z;
    return 1;
#else
    (void)hand; (void)aimOrGrip; (void)quat; (void)pos;
    return 0;
#endif
}

S32 OpenXR_GetHandJointPose(S32 hand, S32 joint, float *quat, float *pos) {
#ifdef LBA2_OPENXR
    if (hand < 0 || hand > 1 || joint < 0 || joint >= OXR_HAND_JOINT_COUNT) {
        return 0;
    }
    if (!s_handJointsValid[hand]) {
        return 0;
    }
    const XrHandJointLocationEXT *j = &s_handJoints[hand][joint];
    const XrSpaceLocationFlags need =
        XR_SPACE_LOCATION_POSITION_VALID_BIT |
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((j->locationFlags & need) != need) {
        return 0;
    }
    quat[0] = j->pose.orientation.x;
    quat[1] = j->pose.orientation.y;
    quat[2] = j->pose.orientation.z;
    quat[3] = j->pose.orientation.w;
    pos[0] = j->pose.position.x;
    pos[1] = j->pose.position.y;
    pos[2] = j->pose.position.z;
    return 1;
#else
    (void)hand; (void)joint; (void)quat; (void)pos;
    return 0;
#endif
}

S32 OpenXR_HandTrackingActive(S32 hand) {
#ifdef LBA2_OPENXR
    return (hand >= 0 && hand < 2 && s_handJointsValid[hand]) ? 1 : 0;
#else
    (void)hand;
    return 0;
#endif
}

U32 OpenXR_GetSwapchainHeight(void) {
#ifdef LBA2_OPENXR
    return s_swapchainHeight;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
void OpenXR_PollEvents(void) {
#ifdef LBA2_OPENXR
    static int s_pollLogCount = 0;
    if (!s_openxrInited) return;
    XrEventDataBuffer evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = XR_TYPE_EVENT_DATA_BUFFER;
    XrResult rc;
    if (s_pollLogCount < 3) {
        OXR_LOGI("PollEvents: entered (s_openxrInited=%d s_openxrActive=%d s_sessionState=%d)",
                 (int)s_openxrInited, (int)s_openxrActive, (int)s_sessionState);
        s_pollLogCount++;
    }
    while ((rc = xrPollEvent(s_instance, &evt)) == XR_SUCCESS) {
        if (evt.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged *sc = (XrEventDataSessionStateChanged *)&evt;
            s_sessionState = sc->state;
            OXR_LOGI("PollEvents: session state -> %d", (int)sc->state);
            /* On the first XR_SESSION_STATE_READY event, the runtime is
             * asking us to begin rendering.  Spec section 9.2: xrBeginSession
             * is valid in the READY state.  Before calling it we set up
             * any per-session hints (here: nothing) and then transition
             * the state machine into SYNCHRONIZED. */
            if (sc->state == XR_SESSION_STATE_READY && !s_sessionRunning) {
                XrSessionBeginInfo beginInfo;
                memset(&beginInfo, 0, sizeof(beginInfo));
                beginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
                beginInfo.primaryViewConfigurationType = s_viewConfigType;
                OXR_LOGI("PollEvents: calling xrBeginSession primaryViewConfigType=%d...",
                         (int)s_viewConfigType);
                XrResult beginRc = xrBeginSession(s_session, &beginInfo);
                OXR_LOGI("PollEvents: xrBeginSession returned rc=0x%x", (unsigned)beginRc);
                if (XR_SUCCEEDED(beginRc)) {
                    s_sessionRunning = 1;
                    /* Clock governor: the compositor targets 72 Hz and the
                     * engine needs the GPU at boost to hold it; CPU goes to
                     * sustained-high (boost there mostly burns battery). */
                    if (s_extPerfSettings && p_xrPerfSettingsSetPerformanceLevelEXT) {
                        XrResult prc = p_xrPerfSettingsSetPerformanceLevelEXT(
                            s_session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT,
                            XR_PERF_SETTINGS_LEVEL_BOOST_EXT);
                        OXR_LOGI("PerfSettings GPU=boost -> 0x%x", (unsigned)prc);
                        prc = p_xrPerfSettingsSetPerformanceLevelEXT(
                            s_session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT,
                            XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
                        OXR_LOGI("PerfSettings CPU=sustained-high -> 0x%x",
                                 (unsigned)prc);
                    }
                } else {
                    Log_Warn("OpenXR: xrBeginSession failed (0x%x)", (unsigned)beginRc);
                }
            }
            if (sc->state == XR_SESSION_STATE_STOPPING && s_sessionRunning) {
                s_sessionRunning = 0;
                xrEndSession(s_session);
            }
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
static S32 OpenXR_EnsureFrameBegun(void);
S32 OpenXR_BeginFrame(void) {
#ifdef LBA2_OPENXR
    if (!s_openxrActive) return 0;
    if (!s_sessionRunning) {
        OXR_LOGI("BeginFrame: skip, session not running (state=%d)", (int)s_sessionState);
        return 0;
    }
    if (s_sessionState != XR_SESSION_STATE_FOCUSED &&
        s_sessionState != XR_SESSION_STATE_VISIBLE &&
        s_sessionState != XR_SESSION_STATE_READY &&
        s_sessionState != XR_SESSION_STATE_SYNCHRONIZED) {
        OXR_LOGI("BeginFrame: skip, sessionState=%d", (int)s_sessionState);
        return 0;
    }
    /* Menu frames reach this through PresentMono instead; doing it here too
       lets the frame that enters an interior clear its scene with alpha 0
       immediately. */
    OpenXR_UpdatePassthrough();
    /* Quest 3 immersive (mono blit) path: xrWaitFrame + xrBeginFrame run
     * HERE, at frame start, so the CPU-side render work overlaps the
     * compositor's wait window instead of pushing the submit past its
     * slot.  PresentMono funnels through the same one-shot, so title
     * screen and menu presents (which skip this function) still begin
     * their frame correctly, and gameplay never waits twice. */
    if (s_swapchain != XR_NULL_HANDLE) {
        U64 perfT0 = SDL_GetPerformanceCounter();
        S32 begun = OpenXR_EnsureFrameBegun();
        s_oxrPerfFrameBeginTicks += SDL_GetPerformanceCounter() - perfT0;
        s_oxrPerfFrameBeginCalls++;
        return begun;
    }
    /* Input-only path (no swapchain): no frame to present, but still sync
     * actions every frame so JOYSTICK.CPP sees live controller state. */
    s_predictedDisplayTime = 0;
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
/* xrWaitFrame + xrBeginFrame, exactly once per frame.  OpenXR_BeginFrame
 * calls this at frame START so the CPU-side render work overlaps the
 * compositor's wait window and the frame submits on time; present paths
 * that never went through BeginFrame (title screen, menus) reach the same
 * one-shot from OpenXR_PresentMono instead. */
static S32 OpenXR_EnsureFrameBegun(void) {
#ifdef LBA2_OPENXR
    if (s_frameBegun)
        return 1;
    if (!s_openxrActive || !s_sessionRunning)
        return 0;
    if (s_sessionState != XR_SESSION_STATE_FOCUSED &&
        s_sessionState != XR_SESSION_STATE_VISIBLE &&
        s_sessionState != XR_SESSION_STATE_READY &&
        s_sessionState != XR_SESSION_STATE_SYNCHRONIZED) {
        return 0;
    }
    {
        XrFrameWaitInfo waitInfo;
        memset(&waitInfo, 0, sizeof(waitInfo));
        waitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
        XrFrameState frameState;
        memset(&frameState, 0, sizeof(frameState));
        frameState.type = XR_TYPE_FRAME_STATE;
        XrResult waitRc = xrWaitFrame(s_session, &waitInfo, &frameState);
        if (XR_FAILED(waitRc)) {
            Log_Warn("OpenXR: xrWaitFrame failed (0x%x)", (unsigned)waitRc);
            return 0;
        }
        s_predictedDisplayTime = frameState.predictedDisplayTime;
    }
    OpenXR_ReadActions();
    {
        XrFrameBeginInfo beginInfo;
        memset(&beginInfo, 0, sizeof(beginInfo));
        beginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
        XrResult rc = xrBeginFrame(s_session, &beginInfo);
        if (XR_FAILED(rc)) {
            Log_Warn("OpenXR: xrBeginFrame failed (0x%x)", (unsigned)rc);
            return 0;
        }
    }
    s_frameBegun = 1;
    return 1;
#else
    return 0;
#endif
}

#ifdef __ANDROID__
/* GL error checks are a runtime diagnostic, not part of every present. */
static S32 OpenXR_GLDebugEnabled(void) {
    static S32 enabled = -1;
    if (enabled < 0) {
        const char *value = getenv("LBA2_OXR_GLDEBUG");
        enabled = (value != NULL && value[0] != '\0' && value[0] != '0') ? 1 : 0;
    }
    return enabled;
}

static void OpenXR_GLDebugCheck(const char *where) {
    if (!OpenXR_GLDebugEnabled()) {
        return;
    }
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        OXR_LOGW("PresentMono: GL error 0x%x at %s", (unsigned)err, where);
    }
}
#endif

/* Direct-to-swapchain stereo is a device experiment, not the default: the
 * runtime-owned texture is attached to the MSAA FBO before either eye draws,
 * which removes the resolve/blit pair but has no fallback if the driver
 * resolves runtime textures differently.  LBA2_OXR_DIRECT=0 overrides a
 * build-time default of 1. */
#ifndef LBA2_OXR_DIRECT_DEFAULT
#define LBA2_OXR_DIRECT_DEFAULT 0
#endif
#ifdef __ANDROID__
static S32 OpenXR_DirectStereoRequested(void) {
    static S32 enabled = -1;
    if (enabled < 0) {
        const char *value = getenv("LBA2_OXR_DIRECT");
        if (value != NULL && value[0] != '\0')
            enabled = (value[0] != '0') ? 1 : 0;
        else
            enabled = LBA2_OXR_DIRECT_DEFAULT;
    }
    return enabled;
}
#endif

void OpenXR_AbortDirectStereo(void) {
#ifdef LBA2_OPENXR
#ifdef __ANDROID__
    if (!s_directStereoArmed)
        return;
    GL_EndDirectStereo();
    if (s_swapchain != XR_NULL_HANDLE) {
        XrSwapchainImageReleaseInfo releaseInfo;
        memset(&releaseInfo, 0, sizeof(releaseInfo));
        releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
        XrResult rc = xrReleaseSwapchainImage(s_swapchain, &releaseInfo);
        if (XR_FAILED(rc)) {
            OXR_LOGW("Direct stereo: release failed (0x%x)", (unsigned)rc);
        }
    }
    s_directStereoArmed = 0;
#endif
#endif
}

S32 OpenXR_AcquireStereoImage(U32 *texture, U32 *width, U32 *height) {
#ifdef LBA2_OPENXR
#ifdef __ANDROID__
    if (texture != NULL)
        *texture = 0;
    if (width != NULL)
        *width = 0;
    if (height != NULL)
        *height = 0;
    if (!OpenXR_DirectStereoRequested() || s_directStereoArmed ||
        !s_stereoEnabled || s_swapchain == XR_NULL_HANDLE ||
        s_swapchainImages == NULL || s_swapchainImageCount == 0 ||
        s_swapchainAllocW == 0 || s_swapchainHeight == 0)
        return 0;
    if (!GL_DirectStereoSupported())
        return 0;
    if (!OpenXR_EnsureFrameBegun())
        return 0;

    XrSwapchainImageAcquireInfo acquireInfo;
    memset(&acquireInfo, 0, sizeof(acquireInfo));
    acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    XrResult rc;
    {
        U64 perfT0 = SDL_GetPerformanceCounter();
        rc = xrAcquireSwapchainImage(s_swapchain, &acquireInfo,
                                     &s_swapchainImageIndex);
        s_oxrPerfAcquireTicks += SDL_GetPerformanceCounter() - perfT0;
    }
    if (XR_FAILED(rc)) {
        OXR_LOGW("Direct stereo: acquire failed (0x%x)", (unsigned)rc);
        return 0;
    }

    XrSwapchainImageWaitInfo waitInfo;
    memset(&waitInfo, 0, sizeof(waitInfo));
    waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    waitInfo.timeout = XR_INFINITE_DURATION;
    {
        U64 perfT0 = SDL_GetPerformanceCounter();
        rc = xrWaitSwapchainImage(s_swapchain, &waitInfo);
        s_oxrPerfWaitTicks += SDL_GetPerformanceCounter() - perfT0;
    }
    if (XR_FAILED(rc) || s_swapchainImageIndex >= s_swapchainImageCount) {
        OXR_LOGW("Direct stereo: wait failed (0x%x) image=%u/%u",
                 (unsigned)rc, (unsigned)s_swapchainImageIndex,
                 (unsigned)s_swapchainImageCount);
        XrSwapchainImageReleaseInfo releaseInfo;
        memset(&releaseInfo, 0, sizeof(releaseInfo));
        releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
        xrReleaseSwapchainImage(s_swapchain, &releaseInfo);
        return 0;
    }

    s_directStereoArmed = 1;
    if (texture != NULL)
        *texture = (U32)s_swapchainImages[s_swapchainImageIndex].image;
    if (width != NULL)
        *width = s_swapchainAllocW;
    if (height != NULL)
        *height = s_swapchainHeight;
    {
        static int s_directAcquireLogCount = 0;
        if (s_directAcquireLogCount < 3) {
            OXR_LOGI("Direct stereo: acquired image=%u tex=%u %ux%u",
                     (unsigned)s_swapchainImageIndex,
                     (unsigned)(texture != NULL ? *texture : 0),
                     (unsigned)s_swapchainAllocW,
                     (unsigned)s_swapchainHeight);
            s_directAcquireLogCount++;
        }
    }
    return 1;
#else
    (void)texture;
    (void)width;
    (void)height;
    return 0;
#endif
#else
    (void)texture;
    (void)width;
    (void)height;
    return 0;
#endif
}

/* Composite FBO for a swapchain image.  A fresh FBO is created and attached
 * for every present: keeping a persistent attachment on a runtime-owned
 * swapchain texture across xrAcquire/xrRelease cycles rendered the
 * composited frame into orphaned memory on the Quest (black output — the
 * per-present glGenFramebuffers cost is negligible next to the blit).
 * Completeness is checked once per image; an incomplete image is marked and
 * takes the copy fallback without creating another FBO. */
static U32 OpenXR_SwapchainFBO(U32 imageIndex) {
#ifdef LBA2_OPENXR
    if (imageIndex >= s_swapchainImageCount) {
        return 0;
    }
    U64 bit = (imageIndex < 64) ? ((U64)1 << imageIndex) : 0;
    if (bit != 0 && (s_swapFboInvalid & bit) != 0) {
        return 0;
    }
    GLuint fbo;
    GLuint imgTex = s_swapchainImages[imageIndex].image;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, imgTex, 0);
    if (bit == 0 || (s_swapFboChecked & bit) == 0) {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (bit != 0) {
            s_swapFboChecked |= bit;
        }
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            if (bit != 0) {
                s_swapFboInvalid |= bit;
            }
            OXR_LOGW("PresentMono: swapchain FBO incomplete (0x%x) image=%u",
                     (unsigned)status, (unsigned)imageIndex);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo);
            return 0;
        }
    }
    return fbo;
#else
    (void)imageIndex;
    return 0;
#endif
}

S32 OpenXR_PresentMono(void) {
#ifdef LBA2_OPENXR
#ifdef __ANDROID__
    S32 stereoThisFrame = 0; /* set by the copy step; drives the layer rects */
    {
        static int s_enterLogCount = 0;
        if (s_enterLogCount < 3) {
            OXR_LOGI("PresentMono: ENTERED");
            s_enterLogCount++;
        }
    }
    static int s_callCount = 0;
    s_callCount++;
    if (s_callCount <= 5) OXR_LOGI("PresentMono: called #%d (active=%d running=%d state=%d)", s_callCount, s_openxrActive, s_sessionRunning, (int)s_sessionState);
    /* Frame limiter: when the session is not visible/focused (e.g. during
     * Guardian transitions, or when the user has taken the headset off),
     * xrWaitFrame is skipped and we return early. Without a yield the
     * engine's main loop spins at 100% CPU. Sleep ~1ms per skipped frame
     * so the loop runs at a bounded rate instead. */
    static S32 s_skip = 0;
    if (!s_openxrActive) {
        OpenXR_AbortDirectStereo();
        if (s_skip < 3) { OXR_LOGI("PresentMono: skip, not active"); s_skip++; }
        SDL_Delay(1);
        return 0;
    }
    OpenXR_PollEvents();
    if (!s_openxrActive) { /* PollEvents may have torn the session down */
        OpenXR_AbortDirectStereo();
        return 0;
    }
    if (!s_sessionRunning) {
        OpenXR_AbortDirectStereo();
        if (s_skip < 3) { OXR_LOGI("PresentMono: skip, session not running (state=%d)", (int)s_sessionState); s_skip++; }
        SDL_Delay(1);
        return 0;
    }
    if (s_sessionState != XR_SESSION_STATE_FOCUSED &&
        s_sessionState != XR_SESSION_STATE_VISIBLE &&
        s_sessionState != XR_SESSION_STATE_READY &&
        s_sessionState != XR_SESSION_STATE_SYNCHRONIZED) {
        OpenXR_AbortDirectStereo();
        if (s_skip < 3) { OXR_LOGI("PresentMono: skip, sessionState=%d", (int)s_sessionState); s_skip++; }
        SDL_Delay(1);
        return 0;
    }
    if (s_swapchain == XR_NULL_HANDLE) {
        /* Input-only path: no swapchain to present. */
        OpenXR_AbortDirectStereo();
        OXR_LOGI("PresentMono: skip, no swapchain");
        SDL_Delay(1);
        return 0;
    }
    s_skip = 0; /* reset counter when we successfully present */
    s_lastPresentTick = (S32)SDL_GetTicks();
    /* Start/resume the passthrough layer for interiors and pause it
       everywhere else, including while the option is off. */
    OpenXR_UpdatePassthrough();
    U64 perfPresentT0 = SDL_GetPerformanceCounter();
    {
        static int s_presentLogCount = 0;
        if (s_presentLogCount < 3) {
            OXR_LOGI("PresentMono: presenting, sessionState=%u swapchainW=%u swapchainH=%u imgIdx=%u/%u",
                     (unsigned)s_sessionState, (unsigned)s_swapchainWidth, (unsigned)s_swapchainHeight,
                     (unsigned)s_swapchainImageIndex, (unsigned)s_swapchainImageCount);
            s_presentLogCount++;
        }
    }

    /* 0.+1. Wait for the predicted display time and begin the frame —
     * exactly once, even when OpenXR_BeginFrame already did it at frame
     * start (the normal gameplay path, which lets the CPU render work
     * overlap the compositor's wait window). */
    XrResult rc;
    {
        U64 perfT0 = SDL_GetPerformanceCounter();
        if (!OpenXR_EnsureFrameBegun()) {
            OpenXR_AbortDirectStereo();
            return 0;
        }
        s_oxrPerfPresentBeginTicks += SDL_GetPerformanceCounter() - perfT0;
    }

    /* 2. Acquire + wait for a swapchain image.  Direct stereo already holds
     * the image acquired before the scene render; it only has to be released
     * after xrEndFrame. */
    const S32 directStereo = s_directStereoArmed && GL_DirectStereoActive();
    if (!s_directStereoArmed) {
        XrSwapchainImageAcquireInfo acquireInfo;
        memset(&acquireInfo, 0, sizeof(acquireInfo));
        acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
        {
            U64 perfT0 = SDL_GetPerformanceCounter();
            rc = xrAcquireSwapchainImage(s_swapchain, &acquireInfo, &s_swapchainImageIndex);
            s_oxrPerfAcquireTicks += SDL_GetPerformanceCounter() - perfT0;
        }
        if (XR_FAILED(rc)) {
            Log_Warn("OpenXR: xrAcquireSwapchainImage failed (0x%x)", (unsigned)rc);
            return 0;
        }
        {
            static int s_acquireLogCount = 0;
            if (s_acquireLogCount < 3) {
                OXR_LOGI("xrAcquireSwapchainImage OK, imgIdx=%u", (unsigned)s_swapchainImageIndex);
                s_acquireLogCount++;
            }
        }
        XrSwapchainImageWaitInfo waitInfo;
        memset(&waitInfo, 0, sizeof(waitInfo));
        waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
        waitInfo.timeout = XR_INFINITE_DURATION;
        {
            U64 perfT0 = SDL_GetPerformanceCounter();
            rc = xrWaitSwapchainImage(s_swapchain, &waitInfo);
            s_oxrPerfWaitTicks += SDL_GetPerformanceCounter() - perfT0;
        }
        if (XR_FAILED(rc)) {
            Log_Warn("OpenXR: xrWaitSwapchainImage failed (0x%x)", (unsigned)rc);
            return 0;
        }
    } else if (s_swapchainImageIndex >= s_swapchainImageCount) {
        OpenXR_AbortDirectStereo();
        return 0;
    }

    /* 3. Copy the composited frame into the swapchain image.
       On Quest with OpenXR there is no SDL window, so the default framebuffer
       (FBO 0) has no color attachment. GL_Present draws into the GL renderer's
       present FBO instead; we copy from that into the swapchain texture.
       Direct stereo already rendered both eyes and the HUD in place. */
    if (directStereo) {
        /* Drop the per-frame attachment before the runtime takes the image
         * back; the command stream keeps the queued eye/HUD draws ordered. */
        stereoThisFrame = 1;
        s_oxrPerfDirectCalls++;
        GL_TakeStereoPair();
        GL_EndDirectStereo();
        OpenXR_GLDebugCheck("direct stereo present");
    } else if (s_swapchainImageIndex < s_swapchainImageCount) {
        GLuint imgTex = s_swapchainImages[s_swapchainImageIndex].image;
        extern SDL_Window *Window_GetSDLWindow(void);
        extern void Window_GetRenderSize(int *w, int *h);
        extern U32 ModeDesiredX;
        extern U32 ModeDesiredY;
        /* Access the GL renderer's present FBO (created in GL_Renderer_Init). */
        extern GLuint GL_GetPresentFBO(void);
        extern GLuint GL_GetPresentColorTex(void);
        SDL_Window *win = Window_GetSDLWindow();
        int winW = 0, winH = 0;
        if (win != NULL) {
            SDL_GetWindowSizeInPixels(win, &winW, &winH);
        } else {
            /* No SDL window (Quest immersive mode). Use the engine's render
             * resolution to sample the present FBO. */
            Window_GetRenderSize(&winW, &winH);
        }
        /* Also get the logical (points) size to compare with GL's letterbox */
        int winWPoints = 0, winHPoints = 0;
        if (win != NULL) {
            SDL_GetWindowSize(win, &winWPoints, &winHPoints);
        }
        {
            static int s_winLogCount = 0;
            if (s_winLogCount < 1) {
                OXR_LOGI("PresentMono: win=%p pixels=%dx%d points=%dx%d imgTex=%u content=%ux%u presentFBO=%u",
                         (void*)win, winW, winH, winWPoints, winHPoints, (unsigned)imgTex, (unsigned)ModeDesiredX, (unsigned)ModeDesiredY, (unsigned)GL_GetPresentFBO());
                s_winLogCount++;
            }
        }
        if (winW > 0 && winH > 0) {
            GLuint presentFBO = GL_GetPresentFBO();

            glBindTexture(GL_TEXTURE_2D, imgTex);
            /* Clear the swapchain texture to black first (runtime may leave it
             * as a debug color). Upload a black image since GLES 2.0 has no
             * glClearTexImage. */
            {
                static int s_clearLogCount = 0;
                if (s_clearLogCount < 1) {
                    int texSize = (int)s_swapchainWidth * (int)s_swapchainHeight;
                    GLubyte *blackImg = (GLubyte *)calloc(texSize, 4);
                    if (blackImg) {
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)s_swapchainWidth, (GLsizei)s_swapchainHeight,
                                        GL_RGBA, GL_UNSIGNED_BYTE, blackImg);
                        OXR_LOGI("PresentMono: cleared swapchain to black");
                        OpenXR_GLDebugCheck("swapchain black clear");
                        free(blackImg);
                    }
                    s_clearLogCount++;
                }
            }

            /* The engine renders ModeDesiredX x ModeDesiredY content into the
             * present FBO. glCopyTexSubImage2D reads from that FBO, so source
             * coordinates must be relative to the FBO, not the SDL window.
             * The FBO is exactly ModeDesiredX x ModeDesiredY, so the source
             * region is the full FBO with no letterbox bars to strip. */
            const double contentAspect = (double)ModeDesiredX / (double)ModeDesiredY;
            int vpX = 0, vpY = 0;
            int vpW = (int)ModeDesiredX;
            int vpH = (int)ModeDesiredY;
            {
                static int s_vpLogCount = 0;
                if (s_vpLogCount < 1) {
                    OXR_LOGI("PresentMono: OXR source vpX=%d vpY=%d vpW=%d vpH=%d contentAspect=%.3f",
                             vpX, vpY, vpW, vpH, contentAspect);
                    s_vpLogCount++;
                }
            }

            /* Scale-to-fit: the engine frame (vpW x vpH, full present FBO)
             * is blitted into the largest aspect-correct rectangle that fits
             * the swapchain, centred. Equal sizes give an exact 1:1 blit;
             * supersampled resolutions (above the swapchain size) downscale
             * instead of cropping. */
            /* Scale-to-fit: the engine frame (vpW x vpH, full present FBO)
             * is blitted into the largest aspect-correct rectangle that fits
             * the swapchain, centred. Equal sizes give an exact 1:1 blit;
             * supersampled resolutions (above the swapchain size) downscale
             * instead of cropping. */
            int srcW = vpW, srcH = vpH;
            int srcX = 0, srcY = 0;
            /* Scale-to-fill: ladder steps below the swapchain size upscale to
             * the full panel — same field of view (the exterior focal floors
             * at the 720p envelope regardless of resolution), same aspect
             * (the head-mount ladder is swapchain-aspect), fewer pixels.
             * Rendering "more screen than the eye sees" is not a thing at
             * the frame level: the whole frame fills the panel, and choosing
             * a lower step is how the user trades detail for framerate. */
            double fit = (double)s_swapchainWidth / (double)srcW;
            double fitH = (double)s_swapchainHeight / (double)srcH;
            if (fitH < fit)
                fit = fitH;
            int dstW = (int)((double)srcW * fit + 0.5);
            int dstH = (int)((double)srcH * fit + 0.5);
            int dstX = ((int)s_swapchainWidth - dstW) / 2;
            int dstY = ((int)s_swapchainHeight - dstH) / 2;
            if (dstX < 0)
                dstX = 0;
            if (dstY < 0)
                dstY = 0;

            /* No clear of the swapchain sub-image here: each frame overwrites
             * the same dstX..dstX+srcW / dstY..dstY+srcH rectangle, and the
             * runtime zero-inits swapchain images on creation. The first few
             * frames may show a faint ghost in the un-used rows until the
             * image stabilises; that beats the alternative of a missing
             * glClearTexImage (GLES 3.1+, not exposed by the NDK GLES
             * headers we include here). */

            /* Stereo layer only when this frame actually captured a fresh
             * per-eye pair; menu/movie frames (no per-eye render) fall back
             * to the mono full-image copy so no stale half is shown. */
            U32 pairFbo = GL_GetStereoPairFBO();
            U32 pairW = GL_GetStereoPairWidth();
            U32 pairH = GL_GetStereoPairHeight();
            stereoThisFrame = s_stereoEnabled && GL_TakeStereoPair() &&
                                  ((pairFbo != 0 && pairW > 0 && pairH > 0) ||
                                   (GL_GetStereoEyeFBO(0) != 0 &&
                                    GL_GetStereoEyeFBO(1) != 0));
            U64 perfBlitT0 = SDL_GetPerformanceCounter();
            if (stereoThisFrame) {
                /* Fill the FULL per-eye half — the compositor maps the whole
                 * imageRect to the eye FOV, maximising the visible area; no
                 * black margins.  The engine's projection maps the 3D
                 * geometry; the stretch from the engine's aspect to the eye
                 * FOV aspect is the correct mapping for VR. */
                S32 halfW = (S32)s_swapchainWidth;
                S32 eyeDstW = halfW;
                S32 eyeDstH = (S32)s_swapchainHeight;
                S32 eyeDstX0 = 0;
                S32 eyeDstX1 = halfW;
                S32 marginY = 0;

                GLuint swapFbo = OpenXR_SwapchainFBO(s_swapchainImageIndex);
                if (swapFbo != 0) {
                    S32 interiorZoomed = 0;
                    if (pairFbo != 0 && CubeMode == 0 && VrFovScale != 100) {
                        const S32 eyeW = (S32)(pairW / 2);
                        const S32 eyeH = (S32)pairH;
                        const S32 dstW = (S32)s_swapchainWidth;
                        const S32 dstH = (S32)s_swapchainHeight;
                        float zoom = (float)VrFovScale / 100.0f;
                        if (zoom < 0.5f)
                            zoom = 0.5f;
                        if (zoom > 1.2f)
                            zoom = 1.2f;
                        glDisable(GL_SCISSOR_TEST);
                        glViewport(0, 0, dstW * 2, dstH);
                        glClearColor(
                            0.0f, 0.0f, 0.0f,
                            OpenXR_InteriorPassthroughActive() ? 0.0f : 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        const S32 applyAdjust =
                            GL_StereoPairNeedsPresentShader();
                        for (S32 eye = 0; eye < 2; eye++) {
                            S32 sx0, sy0, sx1, sy1;
                            S32 dx0, dy0, dx1, dy1;
                            if (zoom < 1.0f) {
                                /* Zoom in: sample a smaller centre of the eye. */
                                const S32 sw = (S32)((float)eyeW * zoom);
                                const S32 sh = (S32)((float)eyeH * zoom);
                                sx0 = eye * eyeW + (eyeW - sw) / 2;
                                sy0 = (eyeH - sh) / 2;
                                sx1 = sx0 + sw;
                                sy1 = sy0 + sh;
                                dx0 = eye * dstW;
                                dy0 = 0;
                                dx1 = dx0 + dstW;
                                dy1 = dstH;
                            } else {
                                /* Zoom out: the whole eye shrinks into a
                                   centred rectangle; the rest is black. */
                                const S32 dw = (S32)((float)dstW / zoom);
                                const S32 dh = (S32)((float)dstH / zoom);
                                sx0 = eye * eyeW;
                                sy0 = 0;
                                sx1 = sx0 + eyeW;
                                sy1 = eyeH;
                                dx0 = eye * dstW + (dstW - dw) / 2;
                                dy0 = (dstH - dh) / 2;
                                dx1 = dx0 + dw;
                                dy1 = dy0 + dh;
                            }
                            S32 drawn = 0;
                            if (applyAdjust && swapFbo != 0) {
                                /* The zoom is a present pass too: route it
                                   through the display transform instead of
                                   bypassing it with a raw blit. */
                                glBindFramebuffer(GL_FRAMEBUFFER, swapFbo);
                                drawn = GL_BlitStereoRectToCurrent(
                                    (U32)sx0, (U32)sy0, (U32)(sx1 - sx0),
                                    (U32)(sy1 - sy0), pairW, pairH, dx0, dy0,
                                    dx1 - dx0, dy1 - dy0, dstW * 2, dstH);
                            }
                            if (!drawn) {
                                glBindFramebuffer(GL_READ_FRAMEBUFFER, pairFbo);
                                glBlitFramebuffer(
                                    sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1,
                                    GL_COLOR_BUFFER_BIT, GL_LINEAR);
                            }
                        }
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                        interiorZoomed = 1;
                    }
                    if (!interiorZoomed) {
                    if (pairFbo != 0) {
                        /* Both eyes are already side by side in one
                         * single-sample source.  With neutral display
                         * sliders this is a plain blit; otherwise the pair
                         * shader applies brightness/contrast/saturation to
                         * the whole composite in this same pass. */
                        S32 pairDrawn = 0;
                        if (GL_StereoPairNeedsPresentShader()) {
                            pairDrawn = GL_BlitStereoPairToCurrent(
                                pairW, pairH,
                                (U32)(eyeDstX1 + eyeDstW), (U32)eyeDstH);
                            if (!pairDrawn) {
                                static int s_adjFallbackLogCount = 0;
                                if (s_adjFallbackLogCount < 3) {
                                    OXR_LOGW("PresentMono: display-transform "
                                             "shader unavailable; plain pair "
                                             "blit");
                                    s_adjFallbackLogCount++;
                                }
                            }
                        }
                        if (!pairDrawn) {
                            GLenum pairFilter =
                                (pairW == (U32)(eyeDstX1 + eyeDstW) &&
                                 pairH == (U32)eyeDstH)
                                    ? GL_NEAREST
                                    : GL_LINEAR;
                            glBindFramebuffer(GL_READ_FRAMEBUFFER, pairFbo);
                            glBlitFramebuffer(
                                0, 0, (GLint)pairW, (GLint)pairH, eyeDstX0,
                                marginY, eyeDstX1 + eyeDstW,
                                marginY + eyeDstH, GL_COLOR_BUFFER_BIT,
                                pairFilter);
                            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                        }
                    } else {
                        /* Fallback: per-eye capture FBOs. */
                        GLuint eyeFBO0 = GL_GetStereoEyeFBO(0);
                        GLuint eyeFBO1 = GL_GetStereoEyeFBO(1);
                        if (GL_StereoEyeSceneDirect()) {
                            GLuint sceneFbo = GL_Get3DFBO();
                            if (sceneFbo != 0) {
                                eyeFBO1 = sceneFbo;
                            }
                        }
                        GLenum eyeFilter =
                            (srcW == eyeDstW && srcH == eyeDstH)
                                ? GL_NEAREST
                                : GL_LINEAR;
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, eyeFBO0);
                        /* Eye 0: blit into the LEFT half of the swapchain. */
                        glBlitFramebuffer(0, 0, srcW, srcH,
                                          eyeDstX0, marginY,
                                          eyeDstX0 + eyeDstW, marginY + eyeDstH,
                                          GL_COLOR_BUFFER_BIT, eyeFilter);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, eyeFBO1);
                        /* Eye 1: blit into the RIGHT half. */
                        glBlitFramebuffer(0, 0, srcW, srcH,
                                          eyeDstX1, marginY,
                                          eyeDstX1 + eyeDstW, marginY + eyeDstH,
                                          GL_COLOR_BUFFER_BIT, eyeFilter);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                    }
                    }
                } else if (pairFbo != 0 &&
                           pairW == (U32)(eyeDstX1 + eyeDstW) &&
                           pairH == (U32)eyeDstH) {
                    /* FBO attach rejected but dimensions are exact: a 1:1
                     * copy of the pair still works. */
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, pairFbo);
                    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                                        (GLsizei)pairW, (GLsizei)pairH);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                } else if (pairFbo == 0) {
                    /* FBO attach rejected: 1:1 copies as before. */
                    GLuint eyeFBO0 = GL_GetStereoEyeFBO(0);
                    GLuint eyeFBO1 = GL_GetStereoEyeFBO(1);
                    if (GL_StereoEyeSceneDirect()) {
                        GLuint sceneFbo = GL_Get3DFBO();
                        if (sceneFbo != 0) {
                            eyeFBO1 = sceneFbo;
                        }
                    }
                    glBindFramebuffer(GL_FRAMEBUFFER, eyeFBO0);
                    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, eyeDstX0, marginY,
                                        0, 0, (GLsizei)eyeDstW, (GLsizei)eyeDstH);
                    glBindFramebuffer(GL_FRAMEBUFFER, eyeFBO1);
                    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, eyeDstX1, marginY,
                                        0, 0, (GLsizei)eyeDstW, (GLsizei)eyeDstH);
                }
                if (swapFbo != 0)
                    GL_DrawStereoOverlayToSwapchain(
                        (U32)s_swapchainWidth, (U32)s_swapchainHeight);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (swapFbo != 0) {
                    glDeleteFramebuffers(1, &swapFbo);
                }
                OpenXR_GLDebugCheck("stereo present");
                {
                    static int s_stBlitLogCount = 0;
                    if (s_stBlitLogCount < 3) {
                        OXR_LOGI("PresentMono stereo: %s src=%ux%u "
                                 "dst=(%d,%d)/(%d,%d)",
                                 pairFbo != 0 ? "double-wide" : "per-eye",
                                 (unsigned)(pairFbo != 0 ? pairW : (U32)srcW),
                                 (unsigned)(pairFbo != 0 ? pairH : (U32)srcH),
                                 eyeDstX0, marginY, eyeDstX1, marginY);
                        s_stBlitLogCount++;
                    }
                }
            }

            if (!stereoThisFrame) {
                /* Attach the swapchain image to an FBO and blit with scaling:
                   resolutions above the swapchain size (supersampled options)
                   downscale to fit instead of cropping, which the 1:1
                   glCopyTexSubImage2D path could not do. */
                GLuint swapFbo = OpenXR_SwapchainFBO(s_swapchainImageIndex);
                if (swapFbo != 0) {
                    /* Fast path: an empty 2D overlay means the composite is
                     * just the 3D FBO — blit it directly and skip the
                     * intermediate fullscreen pass entirely. */
                    GLuint readFbo = presentFBO;
                    if (GL_PresentSceneDirect() && GL_Get3DFBO() != 0) {
                        readFbo = GL_Get3DFBO();
                        srcX = 0;
                        srcY = 0;
                        srcW = (int)ModeDesiredX;
                        srcH = (int)ModeDesiredY;
                    }
                    {
                        /* Throwaway diag: which read source does the mono
                         * present use, and does it think the overlay is
                         * empty? */
                        static U32 s_pmDiag = 0;
                        if (++s_pmDiag % 300 == 1) {
                            OXR_LOGI("PMono: direct=%d read=%s",
                                     GL_PresentSceneDirect(),
                                     readFbo == GL_Get3DFBO() ? "3dfbo"
                                                              : "composite");
                        }
                    }
                    /* Read from the engine's FBO, draw into the swapchain
                       image — an explicit READ rebind is required because
                       binding swapFbo above set READ = swapFbo, and a
                       same-buffer blit is INVALID_OPERATION. */
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
                    GLenum filter = (srcW == dstW && srcH == dstH)
                                        ? GL_NEAREST
                                        : GL_LINEAR;
                    glBlitFramebuffer(srcX, srcY, srcX + srcW, srcY + srcH,
                                      dstX, dstY, dstX + dstW, dstY + dstH,
                                      GL_COLOR_BUFFER_BIT, filter);
                    OpenXR_GLDebugCheck("mono present");
                } else {
                    /* FBO attach rejected: fall back to the 1:1 copy. */
                    OXR_LOGW("swapchain FBO unavailable, 1:1 copy");
                    GLuint copyFbo = presentFBO;
                    if (GL_PresentSceneDirect() && GL_Get3DFBO() != 0) {
                        copyFbo = GL_Get3DFBO();
                    }
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, copyFbo);
                    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, dstX, dstY, srcX,
                                        srcY, (GLsizei)srcW, (GLsizei)srcH);
                    OpenXR_GLDebugCheck("mono copy");
                }
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (swapFbo != 0) {
                    glDeleteFramebuffers(1, &swapFbo);
                }

                {
                    static int s_blitLogCount = 0;
                    if (s_blitLogCount < 3) {
                        OXR_LOGI("PresentMono: blit src=(%d,%d %dx%d) win=(%dx%d) -> dst=(%d,%d) swapchain=(%ux%u) imgIdx=%u",
                                 srcX, srcY, srcW, srcH, winW, winH, dstX, dstY,
                                 (unsigned)s_swapchainWidth, (unsigned)s_swapchainHeight,
                                 (unsigned)s_swapchainImageIndex);
                        s_blitLogCount++;
                    }
                }
            }
            s_oxrPerfBlitTicks += SDL_GetPerformanceCounter() - perfBlitT0;
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    /* 4. Release the swapchain image. */
    s_directStereoArmed = 0;
    XrSwapchainImageReleaseInfo releaseInfo;
    memset(&releaseInfo, 0, sizeof(releaseInfo));
    releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    {
        U64 perfT0 = SDL_GetPerformanceCounter();
        rc = xrReleaseSwapchainImage(s_swapchain, &releaseInfo);
        s_oxrPerfReleaseTicks += SDL_GetPerformanceCounter() - perfT0;
    }
    if (XR_FAILED(rc)) {
        Log_Warn("OpenXR: xrReleaseSwapchainImage failed (0x%x)", (unsigned)rc);
    }

    /* 5. Locate the views to get the eye poses for the projection layer.
     *    Use LOCAL space because the Quest's STAGE space is invalid until
     *    the user sets up Guardian boundaries. */
    XrViewLocateInfo locateInfo;
    memset(&locateInfo, 0, sizeof(locateInfo));
    locateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
    locateInfo.viewConfigurationType = s_viewConfigType;
    locateInfo.displayTime = s_predictedDisplayTime;
    locateInfo.space = s_localSpace;
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
    /* Cache for the next frame's per-eye render (see s_renderViews). */
    s_renderViewCount = (locatedCount > 2) ? 2 : locatedCount;
    {
        U32 j;
        for (j = 0; j < s_renderViewCount; j++) {
            s_renderViews[j] = locatedViews[j];
        }
    }

    /* Controller grip/aim poses and optical hand joints, located with the
       same display time and LOCAL space as the eye views so head and hands
       stay in one frame. */
    for (S32 h = 0; h < 2; h++) {
        for (S32 t = 0; t < 2; t++) {
            s_handPoseValid[h][t] = 0;
            if (s_handSpaces[h][t] == XR_NULL_HANDLE) {
                continue;
            }
            XrActionStateGetInfo gi;
            XrActionStatePose ps;
            memset(&gi, 0, sizeof(gi));
            memset(&ps, 0, sizeof(ps));
            gi.type = XR_TYPE_ACTION_STATE_GET_INFO;
            gi.action = s_actions[15 + t * 2 + h];
            gi.subactionPath = s_handPaths[h];
            ps.type = XR_TYPE_ACTION_STATE_POSE;
            if (XR_FAILED(xrGetActionStatePose(s_session, &gi, &ps)) ||
                !ps.isActive) {
                continue;
            }
            XrSpaceLocation sl;
            memset(&sl, 0, sizeof(sl));
            sl.type = XR_TYPE_SPACE_LOCATION;
            if (XR_FAILED(xrLocateSpace(s_handSpaces[h][t], s_localSpace,
                                        s_predictedDisplayTime, &sl))) {
                continue;
            }
            const XrSpaceLocationFlags need =
                XR_SPACE_LOCATION_POSITION_VALID_BIT |
                XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if ((sl.locationFlags & need) != need) {
                continue;
            }
            s_handPoses[h][t] = sl.pose;
            s_handPoseValid[h][t] = 1;
        }

        s_handJointsValid[h] = 0;
        if (s_handTrackers[h] != XR_NULL_HANDLE &&
            p_xrLocateHandJointsEXT != NULL) {
            XrHandJointsLocateInfoEXT li;
            memset(&li, 0, sizeof(li));
            li.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT;
            li.baseSpace = s_localSpace;
            li.time = s_predictedDisplayTime;
            XrHandJointLocationsEXT locs;
            memset(&locs, 0, sizeof(locs));
            locs.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT;
            locs.jointCount = OXR_HAND_JOINT_COUNT;
            locs.jointLocations = s_handJoints[h];
            if (XR_SUCCEEDED(p_xrLocateHandJointsEXT(s_handTrackers[h], &li,
                                                    &locs)) &&
                locs.isActive) {
                const XrSpaceLocationFlags need =
                    XR_SPACE_LOCATION_POSITION_VALID_BIT |
                    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                if ((s_handJoints[h][OXR_HAND_JOINT_WRIST].locationFlags &
                     need) == need) {
                    s_handJointsValid[h] = 1;
                }
            }
        }
    }
    { /* TEMP diag: locate health regardless of outcome */
        static U32 dbg = 0;
        if ((dbg++ % 300) == 0) {
            OXR_LOGI("LOCATE rc=0x%x count=%u posValid=%d oriValid=%d "
                     "displayTime=%lld state=%d",
                     (unsigned)rc, (unsigned)locatedCount,
                     (int)(viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0,
                     (int)(viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0,
                     (long long)s_predictedDisplayTime, (int)s_sessionState);
        }
    }

    /* Head orientation for the follow-camera: rotate the forward vector
     * (0,0,-1) by view 0's pose quaternion (v' = v + 2q x (q x v) for unit
     * q), then read heading/elevation off the result. Convention-proof
     * against Euler-order pitfalls. */
    if (locatedCount > 0) {
        const float qx = locatedViews[0].pose.orientation.x;
        const float qy = locatedViews[0].pose.orientation.y;
        const float qz = locatedViews[0].pose.orientation.z;
        const float qw = locatedViews[0].pose.orientation.w;
        /* t = 2 * cross(q.xyz, v), v = (0,0,-1): cross((qx,qy,qz),(0,0,-1))
         * = (qy*(-1) - qz*0, qz*0 - qx*(-1), qx*0 - qy*0) = (-qy, qx, 0) */
        const float tx = -2.0f * qy, ty = 2.0f * qx, tz = 0.0f;
        /* v' = v + qw*t + cross(q.xyz, t) */
        const float cx = qy * tz - qz * ty;
        const float cy = qz * tx - qx * tz;
        const float cz = qx * ty - qy * tx;
        const float fx = qw * tx + cx;          /* forward.x */
        const float fy = qw * ty + cy;          /* forward.y */
        const float fz = -1.0f + qw * tz + cz;  /* forward.z */
        s_headYawDeg = (float)(atan2(fx, -fz) * 180.0 / 3.14159265358979);
        s_headPitchDeg = (float)(asin(fy) * 180.0 / 3.14159265358979);
        /* Head roll: where the head's up vector leans relative to the world
         * up, in the plane perpendicular to the forward direction.  Without
         * it, tilting the head produces a spurious yaw/pitch mix. */
        {   /* Head up vector: q rotated (0,1,0) */
            const float ux = 2.0f * qx * qy - 2.0f * qw * qz;
            const float uy = 1.0f - 2.0f * qx * qx - 2.0f * qz * qz;
            const float uz = 2.0f * qw * qx + 2.0f * qy * qz;
            /* Roll: lean of the head up vector toward cross(forward, up) */
            const float rx = -fz, rz = fx;
            s_headRollDeg = (float)(atan2(ux * rx + uz * rz, uy) * 180.0 /
                                    3.14159265358979);
        }
        { /* TEMP diag */
            static U32 dbg = 0;
            if ((dbg++ % 180) == 0) {
                OXR_LOGI("HEADPOSE located=%u yaw=%.1f pitch=%.1f",
                         (unsigned)locatedCount, s_headYawDeg, s_headPitchDeg);
            }
        }
    }

    /* 6. Build the projection layer. Both eye sub-images reference the
     *    same mono sub-image; the FOV is taken from the runtime's view
     *    pose. If xrLocateViews returned 0 views for any reason, fall
     *    back to a synthetic pair with default 90 deg FOV and an
     *    identity pose anchored in LOCAL space so xrEndFrame is still
     *    legal and the user sees the engine framebuffer. */
    XrCompositionLayerProjectionView projViews[2];
    XrCompositionLayerProjection projLayer;
    XrCompositionLayerAlphaBlendFB projBlend;
    XrCompositionLayerPassthroughFB ptCompLayer;
    XrCompositionLayerBaseHeader *layerPtr = NULL;
    const S32 passthrough = OpenXR_InteriorPassthroughActive();
    U32 eyeCount = locatedCount;
    if (eyeCount == 0 && s_swapchainImageIndex < s_swapchainImageCount) {
        /* Synthesize a stereo pair with default FOV. The Quest
         * compositor will use the pose/fov to warp our sub-image into
         * each eye. */
        const XrFovf defaultFov = { -1.39626f, 1.39626f, 1.39626f, -1.39626f };
        const float ipd = 0.064f; /* ~64mm, average IPD */
        U32 j;
        for (j = 0; j < 2; j++) {
            XrView *v = &locatedViews[j];
            memset(v, 0, sizeof(*v));
            v->type = XR_TYPE_VIEW;
            v->next = NULL;
            v->fov = defaultFov;
            v->pose.orientation.w = 1.0f;
            v->pose.position.x = (j == 0) ? -ipd * 0.5f : ipd * 0.5f;
            v->pose.position.y = 0.0f;
            v->pose.position.z = 0.0f;
        }
        locatedCount = 2;
        eyeCount = 2;
    }
    if (eyeCount > 0 && s_swapchainImageIndex < s_swapchainImageCount) {
        U32 j;
        /* Classic/auto cameras keep the world's own horizon: the rendered
         * frames carry no head roll, so the declared poses must not either —
         * the compositor places the image at these poses and any roll in
         * them rolls the whole view with the head.  Rebuild the orientation
         * from the view forward's yaw/pitch alone (roll stripped).  First
         * person renders the tracked roll itself and keeps the full pose. */
        extern S32 CameraFirstPerson;
        if (!CameraFirstPerson && locatedCount > 0) {
            for (j = 0; j < locatedCount && j < 2; j++) {
                const XrQuaternionf q = locatedViews[j].pose.orientation;
                const float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
                /* forward = q * (0,0,-1) */
                const float fx = -2.0f * (qx * qz + qw * qy);
                const float fy = -2.0f * (qy * qz - qw * qx);
                const float fz = 2.0f * (qx * qx + qy * qy) - 1.0f;
                const float pitch = asinf(fy < -1.0f ? -1.0f
                                          : (fy > 1.0f ? 1.0f : fy));
                const float yaw = atan2f(-fx, -fz);
                const float cy = cosf(yaw * 0.5f);
                const float sy = sinf(yaw * 0.5f);
                const float cx = cosf(pitch * 0.5f);
                const float sx = sinf(pitch * 0.5f);
                locatedViews[j].pose.orientation.x = cy * sx;
                locatedViews[j].pose.orientation.y = sy * cx;
                locatedViews[j].pose.orientation.z = -sy * sx;
                locatedViews[j].pose.orientation.w = cy * cx;
            }
        }
        /* Declare the engine's own projection.  The compositor maps each eye
         * rect to these angles linearly in tan-space — the exact transform
         * the engine's pinhole projection applied (focal LFactorX/LFactorY
         * pixels at the render resolution, centred).  Declaring the
         * headset's per-eye FOV instead told the compositor the rendered
         * pixels covered angles they never had: everything stretched to the
         * panel FOV, and the time-warp reprojected head rotation at the
         * wrong angular rate, skewing the frame whenever the head moved.
         * Both eyes render symmetric with the same focal. */
        {
            extern S32 StereoFocalX;
            extern S32 StereoFocalY;
            extern U32 ModeDesiredX;
            extern U32 ModeDesiredY;
            extern S32 VrFovScale;
            /* Symmetrize to the widest span each axis covers across both
             * eyes.  The engine focal is overridden to match (the camera
             * code applies StereoFocalX/Y), so the rendered pixels span
             * exactly these angles: the panel fills edge to edge with
             * undistorted geometry, and the time-warp reprojects head
             * rotation at the true rate. */
            double hx = 0.0, hy = 0.0;
            for (j = 0; j < eyeCount && j < 2; j++) {
                double tl = fabs(tan(locatedViews[j].fov.angleLeft));
                double tr = fabs(tan(locatedViews[j].fov.angleRight));
                double tu = fabs(tan(locatedViews[j].fov.angleUp));
                double td = fabs(tan(locatedViews[j].fov.angleDown));
                if (tl > hx) hx = tl;
                if (tr > hx) hx = tr;
                if (tu > hy) hy = tu;
                if (td > hy) hy = td;
            }
            if (hx > 0.0 && hy > 0.0) {
                /* VrFovScale > 100 renders a wider world into the same panel
                 * span (zoomed out).  Past about 120 the frame outgrows the
                 * authored terrain grid and sky backdrop. */
                double zoom = (double)VrFovScale / 100.0;
                if (zoom < 0.5)
                    zoom = 0.5;
                else if (zoom > 1.2)
                    zoom = 1.2;
                StereoFocalX = (S32)(((double)ModeDesiredX * 0.5) / (hx * zoom));
                StereoFocalY = (S32)(((double)ModeDesiredY * 0.5) / (hy * zoom));
                for (j = 0; j < eyeCount && j < 2; j++) {
                    locatedViews[j].fov.angleLeft = (float)-atan(hx);
                    locatedViews[j].fov.angleRight = (float)atan(hx);
                    locatedViews[j].fov.angleUp = (float)atan(hy);
                    locatedViews[j].fov.angleDown = (float)-atan(hy);
                }
            } else {
                StereoFocalX = 0;
                StereoFocalY = 0;
            }
        }
        for (j = 0; j < eyeCount; j++) {
            XrCompositionLayerProjectionView *pv = &projViews[j];
            memset(pv, 0, sizeof(*pv));
            pv->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            pv->next = NULL;
            pv->subImage.swapchain = s_swapchain;
            if (stereoThisFrame && eyeCount >= 2 && j < 2) {
                /* Stereo: each eye samples its own LEFT/RIGHT half of the
                 * swapchain image — full per-eye height, no vertical loss. */
                pv->subImage.imageRect.offset.x = (j == 0)
                    ? 0
                    : (S32)s_swapchainWidth;
                pv->subImage.imageRect.offset.y = 0;
                pv->subImage.imageRect.extent.width = (S32)s_swapchainWidth;
                pv->subImage.imageRect.extent.height = (S32)s_swapchainHeight;
            } else {
                pv->subImage.imageRect.offset.x = 0;
                pv->subImage.imageRect.offset.y = 0;
                pv->subImage.imageRect.extent.width = (S32)s_swapchainWidth;
                pv->subImage.imageRect.extent.height = (S32)s_swapchainHeight;
            }
            pv->subImage.imageArrayIndex = 0;
            pv->pose = locatedViews[j].pose;
            pv->fov = locatedViews[j].fov;
        }
        memset(&projLayer, 0, sizeof(projLayer));
        projLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        projLayer.next = NULL;
        projLayer.layerFlags = 0;
        projLayer.space = s_localSpace;
        projLayer.viewCount = eyeCount;
        projLayer.views = projViews;
        if (passthrough) {
            /* The scene texture is transparent where no geometry covers the
               interior's black margin.  Straight alpha keeps those pixels
               out of the composite even when the display transform has
               lifted their RGB away from black. */
            memset(&projBlend, 0, sizeof(projBlend));
            projBlend.type = XR_TYPE_COMPOSITION_LAYER_ALPHA_BLEND_FB;
            projBlend.next = NULL;
            projBlend.srcFactorColor = XR_BLEND_FACTOR_SRC_ALPHA_FB;
            projBlend.dstFactorColor = XR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA_FB;
            projBlend.srcFactorAlpha = XR_BLEND_FACTOR_ONE_FB;
            projBlend.dstFactorAlpha = XR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA_FB;
            projLayer.next = &projBlend;
            projLayer.layerFlags |=
                XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
        }
        layerPtr = (XrCompositionLayerBaseHeader *)&projLayer;
    }

    /* 7. End the frame, presenting the layer (or nothing if locate failed).
     *    Passthrough goes below the projection layer, so the transparent
     *    parts of the scene show the camera feed instead of the clear
     *    colour. */
    XrCompositionLayerBaseHeader *layers[2];
    U32 layerCount = 0;
    if (passthrough && layerPtr != NULL && s_passthroughLayer != XR_NULL_HANDLE) {
        memset(&ptCompLayer, 0, sizeof(ptCompLayer));
        ptCompLayer.type = XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB;
        ptCompLayer.next = NULL;
        ptCompLayer.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        ptCompLayer.space = XR_NULL_HANDLE;
        ptCompLayer.layerHandle = s_passthroughLayer;
        layers[layerCount++] = (XrCompositionLayerBaseHeader *)&ptCompLayer;
    }
    if (layerPtr != NULL) {
        layers[layerCount++] = layerPtr;
    }
    XrFrameEndInfo endInfo;
    memset(&endInfo, 0, sizeof(endInfo));
    endInfo.type = XR_TYPE_FRAME_END_INFO;
    endInfo.displayTime = s_predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = layerCount;
    endInfo.layers = (layerCount > 0) ? layers : NULL;
    {
        U64 perfT0 = SDL_GetPerformanceCounter();
        rc = xrEndFrame(s_session, &endInfo);
        s_oxrPerfEndTicks += SDL_GetPerformanceCounter() - perfT0;
    }
    s_frameBegun = 0; /* the next frame must wait+begin again */
    if (XR_FAILED(rc)) {
        OXR_LOGE("xrEndFrame failed: %s (0x%x)", OXR_ResultStr(rc), (unsigned)rc);
        Log_Warn("OpenXR: xrEndFrame failed (0x%x)", (unsigned)rc);
    } else {
        static int s_endFrameLogCount = 0;
        if (s_endFrameLogCount < 3) {
            OXR_LOGI("xrEndFrame OK, displayTime=%lld layers=%u", (long long)s_predictedDisplayTime, endInfo.layerCount);
            s_endFrameLogCount++;
        }
    }

    s_oxrPerfTotalTicks += SDL_GetPerformanceCounter() - perfPresentT0;
    s_oxrPerfPresentCalls++;
    if (s_oxrPerfPresentCalls >= 300) {
        const double msPerTick =
            1000.0 / (double)SDL_GetPerformanceFrequency();
        const double calls = (double)s_oxrPerfPresentCalls;
        const double frameBegin =
            s_oxrPerfFrameBeginCalls
                ? (double)s_oxrPerfFrameBeginTicks * msPerTick /
                      (double)s_oxrPerfFrameBeginCalls
                : 0.0;
        Log_Info("OpenXRPerf: n=%u direct=%u frameBegin=%.2f presentBegin=%.2f "
                 "acquire=%.2f wait=%.2f blit=%.2f release=%.2f end=%.2f "
                 "total=%.2f (ms)",
                 (unsigned)s_oxrPerfPresentCalls,
                 (unsigned)s_oxrPerfDirectCalls, frameBegin,
                 (double)s_oxrPerfPresentBeginTicks * msPerTick / calls,
                 (double)s_oxrPerfAcquireTicks * msPerTick / calls,
                 (double)s_oxrPerfWaitTicks * msPerTick / calls,
                 (double)s_oxrPerfBlitTicks * msPerTick / calls,
                 (double)s_oxrPerfReleaseTicks * msPerTick / calls,
                 (double)s_oxrPerfEndTicks * msPerTick / calls,
                 (double)s_oxrPerfTotalTicks * msPerTick / calls);
        s_oxrPerfFrameBeginTicks = 0;
        s_oxrPerfFrameBeginCalls = 0;
        s_oxrPerfPresentBeginTicks = 0;
        s_oxrPerfAcquireTicks = 0;
        s_oxrPerfWaitTicks = 0;
        s_oxrPerfBlitTicks = 0;
        s_oxrPerfReleaseTicks = 0;
        s_oxrPerfEndTicks = 0;
        s_oxrPerfTotalTicks = 0;
        s_oxrPerfPresentCalls = 0;
        s_oxrPerfDirectCalls = 0;
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
    {
        static int s_skipLogCount = 0;
        if (s_skipLogCount < 3) {
            OXR_LOGI("ShouldSkipSDLSwap: swapchain=%lx result=%d", (unsigned long)s_swapchain, (s_swapchain != XR_NULL_HANDLE) ? 1 : 0);
            s_skipLogCount++;
        }
    }
    return (s_swapchain != XR_NULL_HANDLE);
#else
    return 0;
#endif
#else
    return 0;
#endif
}

void OpenXR_SetStereoEnabled(S32 enabled) {
#ifdef LBA2_OPENXR
#ifdef __ANDROID__
    s_stereoEnabled = (enabled != 0);
    OXR_LOGI("OpenXR_SetStereoEnabled: %d", s_stereoEnabled);
#else
    (void)enabled;
#endif
#else
    (void)enabled;
#endif
}

S32 OpenXR_GetStereoEnabled(void) {
#ifdef LBA2_OPENXR
#ifdef __ANDROID__
    return s_stereoEnabled;
#else
    return 0;
#endif
#else
    return 0;
#endif
}

S32 OpenXR_PassthroughSupported(void) {
#ifdef LBA2_OPENXR
    return (s_extPassthrough && s_passthroughCapable &&
            !s_passthroughFailed) ? 1 : 0;
#else
    return 0;
#endif
}

S32 OpenXR_InteriorPassthroughActive(void) {
#ifdef LBA2_OPENXR
    return (s_sessionRunning && s_passthroughLayerRunning &&
            s_extPassthrough && s_passthroughCapable && !s_passthroughFailed &&
            VrPassthrough && CubeMode == 0 /* CUBE_INTERIEUR */ &&
            !CinemaMode)
               ? 1
               : 0;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
S32 OpenXR_UpdateControllerState(void) {
#ifdef LBA2_OPENXR
    if (!s_openxrActive)
        return 0;
    /* Input waits that spin without presenting (WaitNoInput and friends)
     * never reach OpenXR_PresentMono, and the action sync lives there — the
     * last synced controller state would be frozen into the wait condition
     * forever, deadlocking the menu on its first input. When the present
     * loop has gone quiet, sync here so input state keeps refreshing;
     * during normal rendering PresentMono owns the sync (in its
     * xrWaitFrame..xrBeginFrame window). */
    {
        S32 now = (S32)SDL_GetTicks();
        if (s_lastPresentTick < 0 || now - s_lastPresentTick > 250) {
            OpenXR_ReadActions();
        }
    }
    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif
