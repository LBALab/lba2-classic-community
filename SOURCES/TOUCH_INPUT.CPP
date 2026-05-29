#include "TOUCH_INPUT.H"

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOGPRINT.H>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#ifdef __ANDROID__
#include <jni.h>
#include <SDL3/SDL_android.h>
#endif

#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------
 * Layout constants in normalised 0..1 coordinate space.  These are tuned
 * for a ~16:9 landscape phone held in both hands, thumbs on the screen.
 *
 *    +--(ESC)----------------------------(MENU)--+
 *    |                                     (CAM)  |
 *    |                                            |
 *    |     (UP)                                   |
 *    |  (L) (D) (R)                              |
 *    |                                            |
 *    |               (CTRL) (SPACE) (ENTER)       |
 *    |               (ALT)  (SHIFT) (ACTION)      |
 *    +--------------------------------------------+
 */

#define NUM_TOUCH_BUTTONS 13

/* Maximum simultaneous touch points we track. */
#define MAX_FINGERS 8

enum TouchButtonId {
    BTN_ESC,
    BTN_MENU,
    BTN_CAMERA,
    BTN_DPAD_UP,
    BTN_DPAD_DOWN,
    BTN_DPAD_LEFT,
    BTN_DPAD_RIGHT,
    BTN_ACTION,
    BTN_SPACE,
    BTN_ENTER,
    BTN_ALT,
    BTN_SHIFT,
    BTN_CTRL,
};

typedef struct {
    enum TouchButtonId id;
    const char *label;
    float cx;  /* normalised centre X (0..1) */
    float cy;  /* normalised centre Y (0..1) */
    float rx;  /* normalised half-width */
    float ry;  /* normalised half-height */
    U32 scanCode;  /* SDL scancode to inject */
} TouchButton;

static const TouchButton kButtons[NUM_TOUCH_BUTTONS] = {
    /* Top-left: ESC */
    {BTN_ESC,     "ESC",   0.06f, 0.04f, 0.045f, 0.030f, K_ESC},
    /* Top-right: Menu (F10) + Camera toggle */
    {BTN_MENU,    "MEN",   0.88f, 0.04f, 0.050f, 0.030f, K_F10},
    {BTN_CAMERA,  "CAM",   0.96f, 0.04f, 0.035f, 0.030f, K_BACKSPACE},
    /* Left side: D-pad */
    {BTN_DPAD_UP,    "\xe2\x86\x91", 0.08f, 0.35f, 0.040f, 0.035f, K_UP},
    {BTN_DPAD_DOWN,  "\xe2\x86\x93", 0.08f, 0.50f, 0.040f, 0.035f, K_DOWN},
    {BTN_DPAD_LEFT,  "\xe2\x86\x90", 0.03f, 0.425f, 0.030f, 0.035f, K_LEFT},
    {BTN_DPAD_RIGHT, "\xe2\x86\x92", 0.13f, 0.425f, 0.030f, 0.035f, K_RIGHT},
    /* Bottom-right action cluster */
    {BTN_CTRL,   "MD",    0.70f, 0.87f, 0.045f, 0.035f, K_CTRL},
    {BTN_SPACE,  "ACT",   0.80f, 0.87f, 0.045f, 0.035f, K_SPACE},
    {BTN_ENTER,  "USE",   0.90f, 0.87f, 0.045f, 0.035f, K_ENTER},
    {BTN_ALT,    "THR",   0.70f, 0.95f, 0.045f, 0.035f, K_ALT},
    {BTN_SHIFT,  "INV",   0.80f, 0.95f, 0.045f, 0.035f, K_SHIFT},
    {BTN_ACTION, "W",     0.90f, 0.95f, 0.045f, 0.035f, K_W},
};

/* ---------------------------------------------------------------------------
 * State
 * --------------------------------------------------------------------------- */

static S32 s_active = 0;          /* set to 1 on Android; also toggle-able */
static S32 s_pressed[NUM_TOUCH_BUTTONS];

/* Per-finger tracking: which button (or -1) each finger is on. */
static struct {
    S32 buttonIndex;  /* -1 = not touching any button */
    S32 touchId;      /* SDL finger ID */
    S32 active;       /* 1 = slot in use */
} s_fingers[MAX_FINGERS];

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

static S32 FindFreeFingerSlot(void) {
    for (S32 i = 0; i < MAX_FINGERS; i++) {
        if (!s_fingers[i].active) return i;
    }
    return -1;
}

static S32 FindFingerByTouchId(S32 touchId) {
    for (S32 i = 0; i < MAX_FINGERS; i++) {
        if (s_fingers[i].active && s_fingers[i].touchId == touchId) return i;
    }
    return -1;
}

static S32 HitTest(float nx, float ny) {
    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        float dx = (nx - kButtons[i].cx) / kButtons[i].rx;
        float dy = (ny - kButtons[i].cy) / kButtons[i].ry;
        if (dx * dx + dy * dy <= 1.0f) return i;
    }
    return -1;
}

static void SetButtonState(S32 idx, S32 pressed) {
    if (idx < 0 || idx >= NUM_TOUCH_BUTTONS) return;
    if (s_pressed[idx] == pressed) return;
    s_pressed[idx] = pressed;
    if (pressed) {
        TabKeys[kButtons[idx].scanCode] = 1;
    } else {
        TabKeys[kButtons[idx].scanCode] = 0;
    }
}

/* ---------------------------------------------------------------------------
 * Android TV detection (no touchscreen, skip overlay)
 * --------------------------------------------------------------------------- */

static S32 IsAndroidTVDevice(void) {
#ifdef __ANDROID__
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (env == NULL) return 0;

    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (activity == NULL) return 0;

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getPM = env->GetMethodID(activityClass, "getPackageManager",
                                       "()Landroid/content/pm/PackageManager;");
    if (getPM == NULL) { env->DeleteLocalRef(activityClass); return 0; }

    jobject pm = env->CallObjectMethod(activity, getPM);
    if (pm == NULL) { env->DeleteLocalRef(activityClass); return 0; }

    jclass pmClass = env->GetObjectClass(pm);
    jmethodID hasFeature = env->GetMethodID(pmClass, "hasSystemFeature",
                                            "(Ljava/lang/String;)Z");
    jstring feature = env->NewStringUTF("android.software.leanback");
    S32 isTV = (hasFeature != NULL)
               ? (S32)env->CallBooleanMethod(pm, hasFeature, feature)
               : 0;

    env->DeleteLocalRef(feature);
    env->DeleteLocalRef(pmClass);
    env->DeleteLocalRef(pm);
    env->DeleteLocalRef(activityClass);
    return isTV;
#else
    return 0;
#endif
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void TouchOverlay_Init(void) {
#ifdef __ANDROID__
    if (IsAndroidTVDevice()) {
        LogPrintf("TouchOverlay: Android TV detected, overlay disabled "
                  "(use gamepad/remote)\n");
        s_active = 0;
    } else {
        s_active = 1;
        LogPrintf("TouchOverlay: initialised for Android (%d buttons)\n",
                  NUM_TOUCH_BUTTONS);
    }
#else
    s_active = 0;
#endif
    memset(s_pressed, 0, sizeof(s_pressed));
    memset(s_fingers, 0, sizeof(s_fingers));
}

void TouchOverlay_Shutdown(void) {
    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        if (s_pressed[i]) SetButtonState(i, 0);
    }
    s_active = 0;
}

S32 TouchOverlay_IsActive(void) {
    return s_active;
}

void TouchOverlay_SetActive(S32 active) {
    if (active == s_active) return;
    if (!active) {
        for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
            if (s_pressed[i]) SetButtonState(i, 0);
        }
        memset(s_fingers, 0, sizeof(s_fingers));
    }
    s_active = active;
}

S32 TouchOverlay_HandleEvent(const void *sdlEvent) {
    if (!s_active) return 0;

#ifdef __ANDROID__
    if (sdlEvent == NULL) return 0;

    const SDL_Event *ev = (const SDL_Event *)sdlEvent;
    S32 consumed = 0;

    switch (ev->type) {
    case SDL_EVENT_FINGER_DOWN: {
        S32 slot = FindFreeFingerSlot();
        if (slot < 0) return 1;
        float nx = ev->tfinger.x;
        float ny = ev->tfinger.y;
        S32 idx = HitTest(nx, ny);
        s_fingers[slot].active = 1;
        s_fingers[slot].touchId = (S32)ev->tfinger.fingerID;
        s_fingers[slot].buttonIndex = idx;
        if (idx >= 0) {
            SetButtonState(idx, 1);
            consumed = 1;
        }
        break;
    }
    case SDL_EVENT_FINGER_UP: {
        S32 slot = FindFingerByTouchId((S32)ev->tfinger.fingerID);
        if (slot < 0) break;
        S32 idx = s_fingers[slot].buttonIndex;
        if (idx >= 0) SetButtonState(idx, 0);
        s_fingers[slot].active = 0;
        consumed = 1;
        break;
    }
    case SDL_EVENT_FINGER_MOTION: {
        S32 slot = FindFingerByTouchId((S32)ev->tfinger.fingerID);
        if (slot < 0) break;
        float nx = ev->tfinger.x;
        float ny = ev->tfinger.y;
        S32 newIdx = HitTest(nx, ny);
        S32 oldIdx = s_fingers[slot].buttonIndex;
        if (newIdx != oldIdx) {
            if (oldIdx >= 0) SetButtonState(oldIdx, 0);
            if (newIdx >= 0) SetButtonState(newIdx, 1);
            s_fingers[slot].buttonIndex = newIdx;
        }
        consumed = 1;
        break;
    }
    default:
        break;
    }
    return consumed;
#else
    (void)sdlEvent;
    return 0;
#endif
}

/* ---------------------------------------------------------------------------
 * Overlay rendering
 * ---------------------------------------------------------------------------
 * Draws semi-transparent rounded-rect buttons onto the ARGB framebuffer
 * just before present. This runs after the 8-bit→ARGB conversion so we
 * have direct access to the final pixel buffer.
 */

/* Clamp integer to [0, 255]. */
static U8 Clamp255(S32 v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (U8)v;
}

/* Blends a solid colour onto the framebuffer with alpha. */
static void BlendPixel(U32 *pixel, U8 r, U8 g, U8 b, U8 a) {
    U32 src = *pixel;
    U8 sr = (U8)(src >> 16);
    U8 sg = (U8)(src >> 8);
    U8 sb = (U8)(src);
    U8 ia = (U8)(255 - a);
    *pixel = ((U32)0xFF << 24)
           | ((U32)((sr * ia + r * a) / 255) << 16)
           | ((U32)((sg * ia + g * a) / 255) << 8)
           | ((U32)((sb * ia + b * a) / 255));
}

static void DrawFilledRoundRect(U32 *fb, S32 pitchInPixels,
                                 S32 x0, S32 y0, S32 x1, S32 y1,
                                 U8 r, U8 g, U8 b, U8 a, S32 radius) {
    if (x0 > x1) { S32 t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { S32 t = y0; y0 = y1; y1 = t; }
    if (radius < 0) radius = 0;
    if (radius > (x1 - x0) / 2) radius = (x1 - x0) / 2;
    if (radius > (y1 - y0) / 2) radius = (y1 - y0) / 2;

    S32 rsq = radius * radius;
    for (S32 y = y0; y <= y1 && y >= 0; y++) {
        U32 *row = fb + y * pitchInPixels;
        for (S32 x = x0; x <= x1 && x >= 0; x++) {
            S32 inside = 1;
            if (x < x0 + radius && y < y0 + radius) {
                S32 dx = (x0 + radius - 1) - x;
                S32 dy = (y0 + radius - 1) - y;
                if (dx * dx + dy * dy > rsq) inside = 0;
            } else if (x > x1 - radius && y < y0 + radius) {
                S32 dx = x - (x1 - radius + 1);
                S32 dy = (y0 + radius - 1) - y;
                if (dx * dx + dy * dy > rsq) inside = 0;
            } else if (x < x0 + radius && y > y1 - radius) {
                S32 dx = (x0 + radius - 1) - x;
                S32 dy = y - (y1 - radius + 1);
                if (dx * dx + dy * dy > rsq) inside = 0;
            } else if (x > x1 - radius && y > y1 - radius) {
                S32 dx = x - (x1 - radius + 1);
                S32 dy = y - (y1 - radius + 1);
                if (dx * dx + dy * dy > rsq) inside = 0;
            }
            if (inside) BlendPixel(&row[x], r, g, b, a);
        }
    }
}

void TouchOverlay_Render(U32 *frameBuffer, U32 pitch, U32 width, U32 height,
                         const U8 *palette_rgb) {
    (void)palette_rgb;
    if (!s_active || frameBuffer == NULL || width == 0 || height == 0) return;

    S32 pitchPixels = (S32)(pitch / sizeof(U32));

    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        const TouchButton *btn = &kButtons[i];
        S32 x0 = (S32)((btn->cx - btn->rx) * (float)width);
        S32 y0 = (S32)((btn->cy - btn->ry) * (float)height);
        S32 x1 = (S32)((btn->cx + btn->rx) * (float)width);
        S32 y1 = (S32)((btn->cy + btn->ry) * (float)height);

        U8 alpha = s_pressed[i] ? 180 : 100;
        U8 r = s_pressed[i] ? 200 : 80;
        U8 g = s_pressed[i] ? 200 : 80;
        U8 b = s_pressed[i] ? 200 : 80;
        S32 radius = 6;

        DrawFilledRoundRect(frameBuffer, pitchPixels, x0, y0, x1, y1,
                            r, g, b, alpha, radius);

        /* Draw a simple outline */
        DrawFilledRoundRect(frameBuffer, pitchPixels, x0, y0, x1, y1,
                            255, 255, 255, 60, radius);
    }
}

/* ---------------------------------------------------------------------------
 * Event dispatch (strong override for the weak default in EVENTS.CPP)
 * --------------------------------------------------------------------------- */
S32 HandleEventsTouch(const void *event) {
    return TouchOverlay_HandleEvent(event);
}

#ifdef __cplusplus
}
#endif
