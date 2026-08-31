/* OpenXR runtime layer — minimal Quest 3 input backend.
 *
 * Optional, behind LBA2_OPENXR. On Android + OpenXR build the real
 * session/reference-space/frame loop lives here; every other platform
 * sees no-op stubs so callers compile unconditionally with no #ifdef.
 *
 * Quest 3 controller input is mapped onto the same virtual-scancode space
 * JOYSTICK.CPP uses for SDL gamepads (K_GAMEPAD_*), so the existing
 * GamepadKeys[] bindings and remap screen pick it up automatically.
 * See LIB386/H/SYSTEM/KEYBOARD_KEYS.H for the K_GAMEPAD_* range.
 */
#include <SYSTEM/OPENXR.H>

#include <SYSTEM/LOG.H>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
#ifdef LBA2_OPENXR

#ifdef __ANDROID__
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#endif

/* One-time init flag. */
static S32 s_openxrInited = 0;
static S32 s_openxrActive = 0;

/* Session / frame handles. */
static XrInstance              s_instance = XR_NULL_HANDLE;
static XrSession               s_session  = XR_NULL_HANDLE;
static XrReferenceSpace        s_stageSpace = XR_NULL_HANDLE;
static XrReferenceSpace        s_viewSpace = XR_NULL_HANDLE;
static XrActionSet             s_actionSet = XR_NULL_HANDLE;
static XrAction                s_actions[12];
static XrSessionState          s_sessionState = XR_SESSION_STATE_UNKNOWN;

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
static S32 OpenXR_CreateInstance(void) {
#ifdef __ANDROID__
    XrInstanceCreateInfo info = {XR_TYPE_INSTANCE_CREATE_INFO};
    info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    SDL_strlcpy(info.applicationInfo.applicationName, "LBA2 Classic Community", sizeof(info.applicationInfo.applicationName));
    info.applicationInfo.applicationVersion = 1;
    SDL_strlcpy(info.applicationInfo.engineName, "LBA2", sizeof(info.applicationInfo.engineName));
    info.applicationInfo.engineVersion = 1;

    const char *extensions[] = {
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
    };
    info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    info.enabledExtensionNames = extensions;

    XrResult rc = xrCreateInstance(&info);
    if (rc != XR_SUCCESS) {
        Log_Warn("OpenXR: xrCreateInstance failed: 0x%x", (unsigned)rc);
        return 0;
    }
    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
static S32 OpenXR_CreateSession(void) {
#ifdef __ANDROID__
    XrSystemGetInfo sysInfo = {XR_TYPE_SYSTEM_GET_INFO};
    sysInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId sysId = 0;
    XrResult rc = xrGetSystem(s_instance, &sysInfo, &sysId);
    if (rc != XR_SUCCESS) {
        Log_Warn("OpenXR: no headset system found (0x%x)", (unsigned)rc);
        return 0;
    }

    XrGraphicsBindingOpenGLESAndroidKHR gfxBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    gfxBinding.display = (EGLDisplay)SDL_GL_GetCurrentEGLDisplay();
    gfxBinding.context = (EGLContext)SDL_GL_GetCurrentEGLContext();

    XrSessionCreateInfo sessInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessInfo.next = &gfxBinding;
    sessInfo.createFlags = 0;
    sessInfo.systemId = sysId;

    rc = xrCreateSession(s_instance, &sessInfo, &s_session);
    if (rc != XR_SUCCESS) {
        Log_Warn("OpenXR: xrCreateSession failed (0x%x)", (unsigned)rc);
        return 0;
    }

    XrReferenceSpaceCreateInfo refInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    refInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    refInfo.poseInReferenceSpace.orientation.w = 1.0f;
    rc = xrCreateReferenceSpace(s_session, &refInfo, &s_stageSpace);
    if (rc != XR_SUCCESS) {
        Log_Warn("OpenXR: xrCreateReferenceSpace STAGE failed (0x%x)", (unsigned)rc);
        return 0;
    }

    refInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    rc = xrCreateReferenceSpace(s_session, &refInfo, &s_viewSpace);
    if (rc != XR_SUCCESS) {
        Log_Warn("OpenXR: xrCreateReferenceSpace VIEW failed (0x%x)", (unsigned)rc);
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
    if (s_session != XR_NULL_HANDLE) {
        xrDestroySession(s_session);
        s_session = XR_NULL_HANDLE;
    }
    if (s_stageSpace != XR_NULL_HANDLE) {
        xrDestroyReferenceSpace(s_stageSpace);
        s_stageSpace = XR_NULL_HANDLE;
    }
    if (s_viewSpace != XR_NULL_HANDLE) {
        xrDestroyReferenceSpace(s_viewSpace);
        s_viewSpace = XR_NULL_HANDLE;
    }
#endif
}

/* -------------------------------------------------------------------------- */
static S32 OpenXR_CreateActions(void) {
#ifdef __ANDROID__
    XrActionSetCreateInfo asInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
    SDL_strlcpy(asInfo.actionSetName, "gameplay", sizeof(asInfo.actionSetName));
    SDL_strlcpy(asInfo.localizedActionSetName, "Gameplay", sizeof(asInfo.localizedActionSetName));

    XrResult rc = xrCreateActionSet(s_instance, &asInfo, &s_actionSet);
    if (rc != XR_SUCCESS) return 0;

    static const char *boolNames[] = {
        "a_click", "b_click", "x_click", "y_click",
        "lstick_click", "rstick_click",
        "ltrigger", "rtrigger",
        "lgrip", "rgrip",
    };
    for (U32 i = 0; i < sizeof(boolNames)/sizeof(boolNames[0]); i++) {
        XrActionCreateInfo aInfo = {XR_TYPE_ACTION_CREATE_INFO};
        aInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        SDL_strlcpy(aInfo.actionName, boolNames[i], sizeof(aInfo.actionName));
        SDL_strlcpy(aInfo.localizedActionName, boolNames[i], sizeof(aInfo.localizedActionName));
        rc = xrCreateAction(s_actionSet, &aInfo, &s_actions[i]);
        if (rc != XR_SUCCESS) return 0;
    }

    static const char *vecNames[] = { "lthumbstick", "rthumbstick" };
    for (U32 i = 0; i < sizeof(vecNames)/sizeof(vecNames[0]); i++) {
        XrActionCreateInfo aInfo = {XR_TYPE_ACTION_CREATE_INFO};
        aInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
        SDL_strlcpy(aInfo.actionName, vecNames[i], sizeof(aInfo.actionName));
        SDL_strlcpy(aInfo.localizedActionName, vecNames[i], sizeof(aInfo.localizedActionName));
        rc = xrCreateAction(s_actionSet, &aInfo, &s_actions[10 + i]);
        if (rc != XR_SUCCESS) return 0;
    }

    static const char *profile = "oculus-quest-controller";
    static const XrActionSuggestedBinding bindings[] = {
        {s_actions[0],  "/user/hand/right/input/a/click"},
        {s_actions[1],  "/user/hand/right/input/b/click"},
        {s_actions[2],  "/user/hand/left/input/x/click"},
        {s_actions[3],  "/user/hand/left/input/y/click"},
        {s_actions[4],  "/user/hand/left/input/thumbstick/click"},
        {s_actions[5],  "/user/hand/right/input/thumbstick/click"},
        {s_actions[6],  "/user/hand/left/input/trigger/value"},
        {s_actions[7],  "/user/hand/right/input/trigger/value"},
        {s_actions[8],  "/user/hand/left/input/squeeze/click"},
        {s_actions[9],  "/user/hand/right/input/squeeze/click"},
        {s_actions[10], "/user/hand/left/input/thumbstick"},
        {s_actions[11], "/user/hand/right/input/thumbstick"},
    };

    XrInteractionProfileSuggestedBinding suggest = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggest.interactionProfile = profile;
    suggest.suggestedBindings = bindings;
    suggest.countSuggestedBindings = sizeof(bindings) / sizeof(bindings[0]);

    rc = xrSuggestInteractionProfileBindings(s_instance, &suggest);
    if (rc != XR_SUCCESS) {
        Log_Warn("OpenXR: xrSuggestInteractionProfileBindings failed (0x%x)", (unsigned)rc);
    }

    XrSessionActionSetsAttachInfo attach = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.actionSets = &s_actionSet;
    attach.countActionSets = 1;
    rc = xrAttachSessionActionSets(s_session, &attach);
    if (rc != XR_SUCCESS) {
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

    XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActionData = 12;
    XrActionState actionStates[12];
    for (U32 i = 0; i < 12; i++) {
        actionStates[i].type = (i < 10) ? XR_TYPE_ACTION_STATE_BOOLEAN : XR_TYPE_ACTION_STATE_VECTOR2F;
    }
    sync.actionData = actionStates;
    XrResult rc = xrSyncActions(s_session, &sync);
    if (rc != XR_SUCCESS) return;

    #define GET_BOOL(i, out) do { \
        if (actionStates[i].isActive && actionStates[i].currentBool.state) out = 1; \
        else out = 0; \
    } while(0)
    #define GET_VEC(i, outx, outy) do { \
        if (actionStates[i].isActive) { \
            outx = (S16)(actionStates[i].currentVector.x * 32767.0f); \
            outy = (S16)(-actionStates[i].currentVector.y * 32767.0f); \
        } else { \
            outx = 0; outy = 0; \
        } \
    } while(0)

    GET_BOOL(0, OpenXR_ABtn);
    GET_BOOL(1, OpenXR_BBtn);
    GET_BOOL(2, OpenXR_XBtn);
    GET_BOOL(3, OpenXR_YBtn);
    GET_BOOL(4, OpenXR_LeftThumbstickDown);
    GET_BOOL(5, OpenXR_RightThumbstickDown);
    GET_BOOL(6, OpenXR_LeftTrigger);
    GET_BOOL(7, OpenXR_RightTrigger);
    GET_BOOL(8, OpenXR_LeftGripDown);
    GET_BOOL(9, OpenXR_RightGripDown);
    GET_VEC(10, OpenXR_LeftStickX, OpenXR_LeftStickY);
    GET_VEC(11, OpenXR_RightStickX, OpenXR_RightStickY);

    #undef GET_BOOL
    #undef GET_VEC
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

    XrEventDataBuffer evt = {XR_TYPE_EVENT_DATA_BUFFER};
    XrResult rc;
    for (int i = 0; i < 30; i++) {
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
        SDL_memset(&evt, 0, sizeof(evt));
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
    XrEventDataBuffer evt = {XR_TYPE_EVENT_DATA_BUFFER};
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
        SDL_memset(&evt, 0, sizeof(evt));
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
#ifdef __ANDROID__
    XrFrameBeginInfo begin = {XR_TYPE_FRAME_BEGIN_INFO};
    XrResult rc = xrBeginFrame(s_session, &begin);
    if (rc != XR_SUCCESS) return 0;
#endif
    OpenXR_ReadActions();
    return 1;
#else
    return 0;
#endif
}

/* -------------------------------------------------------------------------- */
void OpenXR_EndFrame(void) {
#ifdef LBA2_OPENXR
    if (!s_openxrActive) return;
#ifdef __ANDROID__
    XrFrameEndInfo end = {XR_TYPE_FRAME_END_INFO};
    end.displayTime = 0;
    end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount = 0;
    end.layers = NULL;
    xrEndFrame(s_session, &end);
#endif
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
