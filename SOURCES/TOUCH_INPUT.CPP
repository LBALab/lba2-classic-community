#include "TOUCH_INPUT.H"

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOGPRINT.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/WINDOW.H>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#ifdef __ANDROID__
#include <jni.h>
#include <SDL3/SDL_system.h>
#endif

#include <string.h>
#include <math.h>

/* Embedded 8x8 bitmap font from SVGA module — used to draw button labels. */
extern U8 Font8x8[];
extern U32 SizeChar;

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------
 * Layout constants in normalised 0..1 coordinate space.  These are tuned
 * for a ~16:9 landscape phone held in both hands, thumbs on the screen.
 *
 *    +--(ESC)---------(HIDE)---------(MEN)(CAM)-+
 *    |                                           |
 *    |                                           |
 *    |                                           |
 *    |                                           |
 *    |      (U)               (MD) (ACT) (USE)   |
 *    |   (L) (D) (R)          (THR) (INV) (W)    |
 *    +-------------------------------------------+
 */

#define NUM_TOUCH_BUTTONS 14

/* Maximum simultaneous touch points we track. */
#define MAX_FINGERS 8

enum TouchButtonId {
    BTN_ESC,
    BTN_HIDE,
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
    float cx;     /* normalised centre X (0..1) */
    float cy;     /* normalised centre Y (0..1) */
    float rx;     /* normalised half-width */
    float ry;     /* normalised half-height */
    U32 scanCode; /* SDL scancode to inject */
} TouchButton;

static const TouchButton kButtons[NUM_TOUCH_BUTTONS] = {
    /* Top-left: ESC */
    {BTN_ESC, "ESC", 0.065f, 0.045f, 0.055f, 0.038f, K_ESC},
    /* Top-centre: HIDE toggle */
    {BTN_HIDE, "HIDE", 0.48f, 0.045f, 0.055f, 0.038f, 0},
    /* Top-right: Menu (F10) + Camera toggle */
    {BTN_MENU, "MEN", 0.85f, 0.045f, 0.055f, 0.038f, K_F10},
    {BTN_CAMERA, "CAM", 0.96f, 0.045f, 0.045f, 0.038f, K_BACKSPACE},
    /* Bottom-left: D-pad (thumb zone) */
    {BTN_DPAD_UP, "U", 0.10f, 0.65f, 0.055f, 0.048f, K_UP},
    {BTN_DPAD_DOWN, "D", 0.10f, 0.85f, 0.055f, 0.048f, K_DOWN},
    {BTN_DPAD_LEFT, "L", 0.04f, 0.75f, 0.040f, 0.048f, K_LEFT},
    {BTN_DPAD_RIGHT, "R", 0.16f, 0.75f, 0.040f, 0.048f, K_RIGHT},
    /* Bottom-right corner: action cluster */
    {BTN_CTRL, "MD", 0.68f, 0.84f, 0.055f, 0.045f, K_CTRL},
    {BTN_SPACE, "ACT", 0.81f, 0.84f, 0.065f, 0.050f, K_SPACE},
    {BTN_ENTER, "USE", 0.94f, 0.84f, 0.055f, 0.045f, K_ENTER},
    {BTN_ALT, "THR", 0.68f, 0.95f, 0.055f, 0.045f, K_ALT},
    {BTN_SHIFT, "INV", 0.81f, 0.95f, 0.055f, 0.045f, K_SHIFT},
    {BTN_ACTION, "W", 0.94f, 0.95f, 0.055f, 0.045f, K_W},
};

/* ---------------------------------------------------------------------------
 * State
 * --------------------------------------------------------------------------- */

static S32 s_active = 0; /* set to 1 on Android; also toggle-able */
static S32 s_pressed[NUM_TOUCH_BUTTONS];
static U8 s_virtual_keys[256]; /* virtual key states in 0x80-bit format */

/* Per-finger tracking: which button (or -1) each finger is on. */
static struct {
    S32 buttonIndex; /* -1 = not touching any button */
    S32 touchId;     /* SDL finger ID */
    S32 active;      /* 1 = slot in use */
} s_fingers[MAX_FINGERS];

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

static S32 FindFreeFingerSlot(void) {
    for (S32 i = 0; i < MAX_FINGERS; i++) {
        if (!s_fingers[i].active)
            return i;
    }
    return -1;
}

static S32 FindFingerByTouchId(S32 touchId) {
    for (S32 i = 0; i < MAX_FINGERS; i++) {
        if (s_fingers[i].active && s_fingers[i].touchId == touchId)
            return i;
    }
    return -1;
}

/* Map a touch coordinate from display-normalised space (0..1 of the full
 * screen) to game-area-normalised space (0..1 of the letterboxed 4:3
 * region). This is the inverse of ComputePresentationRect. */
static void MapToGameCoords(float *nx, float *ny) {
    if (ModeResX == 0 || ModeResY == 0)
        return;

    SDL_Window **windows = SDL_GetWindows(NULL);
    if (windows == NULL || windows[0] == NULL)
        return;

    int winW, winH;
    SDL_GetWindowSize(windows[0], &winW, &winH);
    if (winW <= 0 || winH <= 0)
        return;

    float pixelX = *nx * (float)winW;
    float pixelY = *ny * (float)winH;

    S32 surfX, surfY;
    WindowToSurfaceCoords((S32)pixelX, (S32)pixelY, &surfX, &surfY);

    /* Clamp to valid surface area */
    if (surfX < 0)
        surfX = 0;
    if (surfY < 0)
        surfY = 0;
    if (surfX >= (S32)ModeResX)
        surfX = (S32)ModeResX - 1;
    if (surfY >= (S32)ModeResY)
        surfY = (S32)ModeResY - 1;

    *nx = (float)surfX / (float)ModeResX;
    *ny = (float)surfY / (float)ModeResY;
}

static S32 HitTest(float nx, float ny) {
    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        float dx = (nx - kButtons[i].cx) / kButtons[i].rx;
        float dy = (ny - kButtons[i].cy) / kButtons[i].ry;
        if (dx * dx + dy * dy <= 1.0f)
            return i;
    }
    return -1;
}

static void SetButtonState(S32 idx, S32 pressed) {
    if (idx < 0 || idx >= NUM_TOUCH_BUTTONS)
        return;
    if (s_pressed[idx] == pressed)
        return;
    s_pressed[idx] = pressed;
    U32 sc = kButtons[idx].scanCode;
    if (sc < 256) {
        if (pressed) {
            TabKeys[sc] |= 0x80;
            s_virtual_keys[sc] = 0x80;
        } else {
            TabKeys[sc] &= ~0x80;
            s_virtual_keys[sc] = 0;
        }
    }
}

/* Re-apply virtual key states after UpdateKeyboardState clears TabKeys[0..255].
   Called from the weak hook in KEYBOARD.CPP:UpdateKeyboardState. */
void ApplyVirtualKeys(void) {
    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        U32 sc = kButtons[i].scanCode;
        if (sc < 256 && s_virtual_keys[sc])
            TabKeys[sc] |= 0x80;
    }
}

/* ---------------------------------------------------------------------------
 * Android TV detection (no touchscreen, skip overlay)
 * --------------------------------------------------------------------------- */

S32 IsAndroidTVDevice(void) {
#ifdef __ANDROID__
    /* Helper: check for a pending JNI exception, log and clear it. */
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
        return 1;

    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (activity == NULL)
        return 1;

    jclass activityClass = env->GetObjectClass(activity);
    if (JniCheck::Pending(env) || activityClass == NULL)
        return 1;

    jmethodID getPM = env->GetMethodID(activityClass, "getPackageManager",
                                       "()Landroid/content/pm/PackageManager;");
    if (JniCheck::Pending(env) || getPM == NULL) {
        env->DeleteLocalRef(activityClass);
        return 1;
    }

    jobject pm = env->CallObjectMethod(activity, getPM);
    if (JniCheck::Pending(env) || pm == NULL) {
        env->DeleteLocalRef(activityClass);
        return 1;
    }

    jclass pmClass = env->GetObjectClass(pm);
    if (JniCheck::Pending(env) || pmClass == NULL) {
        env->DeleteLocalRef(pm);
        env->DeleteLocalRef(activityClass);
        return 1;
    }

    jmethodID hasFeature = env->GetMethodID(pmClass, "hasSystemFeature",
                                            "(Ljava/lang/String;)Z");
    if (JniCheck::Pending(env) || hasFeature == NULL) {
        env->DeleteLocalRef(pmClass);
        env->DeleteLocalRef(pm);
        env->DeleteLocalRef(activityClass);
        return 1;
    }

    jstring feature = env->NewStringUTF("android.software.leanback");
    if (JniCheck::Pending(env) || feature == NULL) {
        env->DeleteLocalRef(pmClass);
        env->DeleteLocalRef(pm);
        env->DeleteLocalRef(activityClass);
        return 1;
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
    memset(s_virtual_keys, 0, sizeof(s_virtual_keys));
    memset(s_fingers, 0, sizeof(s_fingers));
}

void TouchOverlay_Shutdown(void) {
    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        if (s_pressed[i])
            SetButtonState(i, 0);
    }
    s_active = 0;
}

S32 TouchOverlay_IsActive(void) {
    return s_active;
}

void TouchOverlay_SetActive(S32 active) {
    if (active == s_active)
        return;
    if (!active) {
        for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
            if (s_pressed[i])
                SetButtonState(i, 0);
        }
        memset(s_fingers, 0, sizeof(s_fingers));
    }
    s_active = active;
}

S32 TouchOverlay_HandleEvent(const void *sdlEvent) {
#ifdef __ANDROID__
    if (sdlEvent == NULL)
        return 0;

    const SDL_Event *ev = (const SDL_Event *)sdlEvent;
    S32 consumed = 0;

    switch (ev->type) {
    case SDL_EVENT_FINGER_DOWN: {
        S32 slot = FindFreeFingerSlot();
        if (slot < 0)
            return 1;
        float nx = ev->tfinger.x;
        float ny = ev->tfinger.y;
        MapToGameCoords(&nx, &ny);
        S32 idx = HitTest(nx, ny);
        s_fingers[slot].active = 1;
        s_fingers[slot].touchId = (S32)ev->tfinger.fingerID;
        s_fingers[slot].buttonIndex = idx;
        if (idx >= 0) {
            /* HIDE toggles overlay visibility */
            if (kButtons[idx].id == BTN_HIDE) {
                TouchOverlay_SetActive(!s_active);
                consumed = 1;
            } else if (s_active) {
                SetButtonState(idx, 1);
                consumed = 1;
            }
        }
        break;
    }
    case SDL_EVENT_FINGER_UP: {
        S32 slot = FindFingerByTouchId((S32)ev->tfinger.fingerID);
        if (slot < 0)
            break;
        S32 idx = s_fingers[slot].buttonIndex;
        if (idx >= 0 && kButtons[idx].id != BTN_HIDE)
            SetButtonState(idx, 0);
        s_fingers[slot].active = 0;
        consumed = 1;
        break;
    }
    case SDL_EVENT_FINGER_MOTION: {
        S32 slot = FindFingerByTouchId((S32)ev->tfinger.fingerID);
        if (slot < 0)
            break;
        float nx = ev->tfinger.x;
        float ny = ev->tfinger.y;
        MapToGameCoords(&nx, &ny);
        S32 newIdx = HitTest(nx, ny);
        S32 oldIdx = s_fingers[slot].buttonIndex;
        if (newIdx != oldIdx) {
            if (oldIdx >= 0 && kButtons[oldIdx].id != BTN_HIDE)
                SetButtonState(oldIdx, 0);
            if (newIdx >= 0 && kButtons[newIdx].id != BTN_HIDE && s_active)
                SetButtonState(newIdx, 1);
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

/* Forward declarations for helpers defined below. */
static void BlendPixel(U32 *pixel, U8 r, U8 g, U8 b, U8 a);

/* Draw a string using the embedded Font8x8 onto the ARGB framebuffer.
   Each character is SizeChar x SizeChar pixels. The text is drawn in
   the given colour with alpha blending. */
static void DrawStringARGB(U32 *fb, S32 pitchInPixels,
                           S32 x, S32 y, const char *str,
                           U8 r, U8 g, U8 b, U8 a) {
    U8 character;
    while ((character = (U8)*str++) != 0) {
        U8 *glyph = &Font8x8[character * SizeChar];
        for (U32 row = 0; row < SizeChar; row++) {
            U8 line = glyph[row];
            U32 mask = 0x80;
            for (U32 col = 0; col < SizeChar; col++) {
                if (line & mask) {
                    S32 px = x + (S32)col;
                    S32 py = y + (S32)row;
                    if (px >= 0 && py >= 0)
                        BlendPixel(&fb[py * pitchInPixels + px], r, g, b, a);
                }
                mask >>= 1;
            }
        }
        x += (S32)SizeChar;
    }
}

/* Clamp integer to [0, 255]. */
static U8 Clamp255(S32 v) {
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return (U8)v;
}

/* Blends a solid colour onto the framebuffer with alpha. */
static void BlendPixel(U32 *pixel, U8 r, U8 g, U8 b, U8 a) {
    U32 src = *pixel;
    U8 sr = (U8)(src >> 16);
    U8 sg = (U8)(src >> 8);
    U8 sb = (U8)(src);
    U8 ia = (U8)(255 - a);
    *pixel = ((U32)0xFF << 24) | ((U32)((sr * ia + r * a) / 255) << 16) | ((U32)((sg * ia + g * a) / 255) << 8) | ((U32)((sb * ia + b * a) / 255));
}

static void DrawFilledRoundRect(U32 *fb, S32 pitchInPixels,
                                S32 x0, S32 y0, S32 x1, S32 y1,
                                U8 r, U8 g, U8 b, U8 a, S32 radius) {
    if (x0 > x1) {
        S32 t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        S32 t = y0;
        y0 = y1;
        y1 = t;
    }
    if (radius < 0)
        radius = 0;
    if (radius > (x1 - x0) / 2)
        radius = (x1 - x0) / 2;
    if (radius > (y1 - y0) / 2)
        radius = (y1 - y0) / 2;

    S32 rsq = radius * radius;
    for (S32 y = y0; y <= y1 && y >= 0; y++) {
        U32 *row = fb + y * pitchInPixels;
        for (S32 x = x0; x <= x1 && x >= 0; x++) {
            S32 inside = 1;
            if (x < x0 + radius && y < y0 + radius) {
                S32 dx = (x0 + radius - 1) - x;
                S32 dy = (y0 + radius - 1) - y;
                if (dx * dx + dy * dy > rsq)
                    inside = 0;
            } else if (x > x1 - radius && y < y0 + radius) {
                S32 dx = x - (x1 - radius + 1);
                S32 dy = (y0 + radius - 1) - y;
                if (dx * dx + dy * dy > rsq)
                    inside = 0;
            } else if (x < x0 + radius && y > y1 - radius) {
                S32 dx = (x0 + radius - 1) - x;
                S32 dy = y - (y1 - radius + 1);
                if (dx * dx + dy * dy > rsq)
                    inside = 0;
            } else if (x > x1 - radius && y > y1 - radius) {
                S32 dx = x - (x1 - radius + 1);
                S32 dy = y - (y1 - radius + 1);
                if (dx * dx + dy * dy > rsq)
                    inside = 0;
            }
            if (inside)
                BlendPixel(&row[x], r, g, b, a);
        }
    }
}

void TouchOverlay_Render(U32 *frameBuffer, U32 pitch, U32 width, U32 height,
                         const U8 *palette_rgb) {
    (void)palette_rgb;
    if (!s_active || frameBuffer == NULL || width == 0 || height == 0)
        return;

    S32 pitchPixels = (S32)(pitch / sizeof(U32));

    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        const TouchButton *btn = &kButtons[i];
        S32 x0 = (S32)((btn->cx - btn->rx) * (float)width);
        S32 y0 = (S32)((btn->cy - btn->ry) * (float)height);
        S32 x1 = (S32)((btn->cx + btn->rx) * (float)width);
        S32 y1 = (S32)((btn->cy + btn->ry) * (float)height);

        /* Clamp to framebuffer bounds */
        if (x0 < 0)
            x0 = 0;
        if (y0 < 0)
            y0 = 0;
        if (x1 >= (S32)width)
            x1 = (S32)width - 1;
        if (y1 >= (S32)height)
            y1 = (S32)height - 1;
        if (x0 > x1 || y0 > y1)
            continue;

        U8 alpha = s_pressed[i] ? 180 : 100;
        U8 r = s_pressed[i] ? 200 : 80;
        U8 g = s_pressed[i] ? 200 : 80;
        U8 b8 = s_pressed[i] ? 200 : 80;
        S32 radius = 6;

        DrawFilledRoundRect(frameBuffer, pitchPixels, x0, y0, x1, y1,
                            r, g, b8, alpha, radius);

        /* Draw a simple outline */
        DrawFilledRoundRect(frameBuffer, pitchPixels, x0, y0, x1, y1,
                            255, 255, 255, 60, radius);

        /* Draw the button label centred */
        if (btn->label && btn->label[0]) {
            S32 textW = (S32)(strlen(btn->label) * SizeChar);
            S32 textH = (S32)SizeChar;
            S32 tx = ((x0 + x1) / 2) - (textW / 2);
            S32 ty = ((y0 + y1) / 2) - (textH / 2);
            DrawStringARGB(frameBuffer, pitchPixels, tx, ty, btn->label,
                           255, 255, 255, 220);
        }
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
