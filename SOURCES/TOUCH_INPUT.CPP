#include "TOUCH_INPUT.H"

#include "JOYSTICK.H" /* JoystickIsPresent: the pad half of the visibility rule */

#include <SYSTEM/ANDROID.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOGPRINT.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/WINDOW.H>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

#include <string.h>
#include <math.h>

/* Embedded 8x8 bitmap font from SVGA module — used to draw button labels. */
extern U8 Font8x8[];
extern U32 SizeChar;

#ifdef __cplusplus
extern "C" {
#endif

/* The SDL window handle, defined in LIB386/SYSTEM/WINDOW.CPP under C linkage. */
extern SDL_Window *sdlWindow;

/* Last-input device flags (C linkage, defined in LIB386/SYSTEM/KEYBOARD.CPP).
 *
 * LastInputWasKeyboard is the older of the two and answers "can we prompt for
 * typed text": a touch tap is not a physical keyboard, so clear it and the save
 * menu uses the auto-name flow instead of a prompt we cannot fill.
 *
 * LastInputWasTouch answers a question that one could not, because a pad and a
 * finger both leave it 0: which of the two the player is holding right now.
 * That is what lets the overlay step aside for a pad on a phone -- the case a
 * device-class check for Android TV never covered. */
extern S32 LastInputWasKeyboard;
extern S32 LastInputWasTouch;

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
    /* Top-centre: dismiss the overlay until the next touch */
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
 * Visibility policy
 * ---------------------------------------------------------------------------
 * One flag used to carry three unrelated facts -- whether the device has a
 * touchscreen, whether the player wants the pills drawn, and whether touch
 * input is accepted -- so there was nowhere to put a policy answer. They are
 * separate now:
 *
 *   s_supported   this build can draw an overlay at all: Android only.
 *   s_tvDevice    leanback, so assume no touchscreen until a finger proves otherwise.
 *   s_lastTouchMs when a finger was last down, which drives the fade.
 *
 * From those, one rule covers both the touch-only phone and the phone with a
 * pad clipped to it: THE OVERLAY FOLLOWS THE LAST INPUT THE PLAYER ACTUALLY
 * USED. Touch shows it, a pad or a key hides it, and going idle fades it out.
 * Nobody configures anything, and the pad case needs no device-class check --
 * Android TV is just a screen nobody has ever touched.
 *
 * Why a fade rather than a mode check: this game holds a direction to walk, so
 * a thumb is on the D-pad more or less continuously during play and the timer
 * never fires. It fires during dialogue, cutscenes and FMVs -- which is a fair
 * approximation of "the game is not asking for input" for the price of a
 * timer, instead of the per-modal mode stack the engine does not have (each
 * modal runs its own inner loop with its own exit sentinel; see docs/CONTROL.md).
 *
 * HIDE dismisses the overlay immediately rather than waiting out the fade, for
 * when the player wants to see what is under it now. It is not a mode: the next
 * touch brings the controls back, like any other touch. That is what removed the
 * SHOW pill this used to need -- a widget whose whole job was to undo a state
 * that touch already undoes, sitting on screen permanently in a design whose
 * point is to keep the screen clear. It also made the misfire cheap. HIDE is at
 * cx 0.48, cy 0.045, which is where a player taps to skip dialogue, so it does
 * get hit by accident; costing them one touch is fine, stranding them in an
 * unexplained state was not.
 */

/* Wall-clock, not the managed game clock: a UI fade should run in real time
   whether or not the simulation is paused, and staying off the game clock
   keeps this out of the recorder's determinism surface entirely. The overlay
   only ever injects keys from a real finger, so a replay never sees it. */
#define TOUCH_FULL_MS 1500u /* full opacity for this long after the last touch */
#define TOUCH_DIM_MS 6000u  /* then recessive, then gone */

typedef enum {
    OVL_OFF = 0, /* not drawn */
    OVL_DIM,     /* drawn faintly: still findable, no longer competing with the art */
    OVL_FULL
} OverlayLevel;

/* ---------------------------------------------------------------------------
 * State
 * --------------------------------------------------------------------------- */

static S32 s_supported = 0;   /* Android build: an overlay is possible here */
static S32 s_tvDevice = 0;    /* leanback: start off, but a finger still overrides */
static S32 s_dismissed = 0;   /* HIDE was pressed; cleared by the next finger down */
static U32 s_lastTouchMs = 0; /* SDL_GetTicks at the last finger-down/motion; 0 = never */
static S32 s_pressed[NUM_TOUCH_BUTTONS];
static U8 s_virtual_keys[256]; /* virtual key states in 0x80-bit format */

/* Per-finger tracking: which button (or -1) each finger is on. */
static struct {
    S32 buttonIndex; /* -1 = not touching any button */
    S32 touchId;     /* SDL finger ID */
    S32 active;      /* 1 = slot in use */
    S32 actuate;     /* 0 = this finger only woke the overlay, it presses nothing */
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

/* Any finger currently on the glass counts as activity for as long as it is
   held. A held button fires one FINGER_DOWN and then nothing, so timing the
   fade from events alone would dim the overlay in the middle of a walk. */
static S32 AnyFingerDown(void) {
    for (S32 i = 0; i < MAX_FINGERS; i++) {
        if (s_fingers[i].active)
            return 1;
    }
    return 0;
}

static void NoteTouchActivity(void) {
    s_lastTouchMs = SDL_GetTicks();
    LastInputWasTouch = 1;
    /* A touch is not a physical keyboard: keep the save menu on the auto-name
       flow, which is the one thing we cannot type into. */
    LastInputWasKeyboard = 0;
}

/* What to draw this frame. The order is the priority order: an unsupported
   device and an explicit HIDE both beat everything, then the pad, then time. */
static OverlayLevel CurrentLevel(void) {
    if (!s_supported)
        return OVL_OFF;

    /* HIDE, until the next finger clears it. */
    if (s_dismissed)
        return OVL_OFF;

    /* Nobody has touched the glass yet. A pad attached makes it a pad session
       and the pills would be 14 dead buttons over the art; a TV is a screen
       nobody is going to touch. Otherwise it is a player who has not found the
       controls yet, so leave a hint.

       This is where leanback belongs, rather than in s_supported: as a hard
       gate it was a hidden control stuck at off -- a leanback device that does
       have a touchscreen could never raise the overlay, and there is no
       setting to say otherwise. A first touch is better evidence than a
       manifest feature flag, so let it win. */
    if (s_lastTouchMs == 0)
        return (s_tvDevice || JoystickIsPresent()) ? OVL_OFF : OVL_DIM;

    /* The player picked up the pad. Step aside until a finger says otherwise --
       and the pad drives the menus too (JoystickMenuAction), so nothing here
       is reachable only by touch. */
    if (!LastInputWasTouch)
        return OVL_OFF;

    if (AnyFingerDown())
        return OVL_FULL;

    U32 idleMs = SDL_GetTicks() - s_lastTouchMs;
    if (idleMs < TOUCH_FULL_MS)
        return OVL_FULL;
    if (idleMs < TOUCH_DIM_MS)
        return OVL_DIM;
    return OVL_OFF;
}

/* Map a touch coordinate from display-normalised space (0..1 of the full
 * screen) to game-area-normalised space (0..1 of the letterboxed 4:3
 * region). This is the inverse of ComputePresentationRect. */
static void MapToGameCoords(float *nx, float *ny) {
    if (ModeResX == 0 || ModeResY == 0)
        return;

    if (sdlWindow == NULL)
        return;

    int winW, winH;
    SDL_GetWindowSize(sdlWindow, &winW, &winH);
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
 * Public API
 * --------------------------------------------------------------------------- */

void TouchOverlay_Init(void) {
#ifdef __ANDROID__
    s_supported = 1;
    s_tvDevice = IsAndroidTVDevice();
    if (s_tvDevice) {
        LogPrintf("TouchOverlay: Android TV detected, overlay starts off "
                  "(use gamepad/remote; a touch still brings it up)\n");
    } else {
        LogPrintf("TouchOverlay: initialised for Android (%d buttons)\n",
                  NUM_TOUCH_BUTTONS);
    }
#else
    s_supported = 0;
#endif
    s_dismissed = 0;
    s_lastTouchMs = 0;
    /* s_tvDevice is set above and is not cleared here: it is a property of the
       device, not of this session's input. */
    memset(s_pressed, 0, sizeof(s_pressed));
    memset(s_virtual_keys, 0, sizeof(s_virtual_keys));
    memset(s_fingers, 0, sizeof(s_fingers));
}

/* Let go of every key this overlay is holding. Called when it stops accepting
   input, so a finger that was on a direction when the overlay went away does
   not leave that direction latched in TabKeys forever. */
static void ReleaseAllButtons(void) {
    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        if (s_pressed[i])
            SetButtonState(i, 0);
    }
    memset(s_fingers, 0, sizeof(s_fingers));
}

void TouchOverlay_Shutdown(void) {
    ReleaseAllButtons();
    s_supported = 0;
}

S32 TouchOverlay_IsSupported(void) {
    return s_supported;
}

S32 TouchOverlay_HandleEvent(const void *sdlEvent) {
#ifdef __ANDROID__
    if (sdlEvent == NULL || !s_supported)
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

        /* Any finger anywhere brings the controls back: touch is the only
           "show" this needs, which is why there is no pill. */
        s_dismissed = 0;

        /* Does this touch press what it landed on, or only wake the overlay?
           Only a player already using touch can have been aiming at a button:
           coming back from a pad or a keyboard there was nothing on screen to
           aim at. That is also what protects a phone in a controller clip,
           where the glass sits under the player's palms and a brush would
           otherwise latch a direction. Read before NoteTouchActivity, which is
           what makes the answer 1 from here on. */
        const S32 actuate = LastInputWasTouch;
        NoteTouchActivity();

        s_fingers[slot].active = 1;
        s_fingers[slot].touchId = (S32)ev->tfinger.fingerID;
        s_fingers[slot].buttonIndex = idx;
        s_fingers[slot].actuate = actuate;

        if (idx < 0)
            break; /* empty glass: the wake above was the whole effect */

        if (kButtons[idx].id == BTN_HIDE) {
            /* Gated like any other button: a palm brush from a pad session
               wakes the overlay rather than dismissing it again. */
            if (actuate) {
                ReleaseAllButtons();
                s_dismissed = 1;
            }
            consumed = 1;
            break;
        }

        if (actuate) {
            SetButtonState(idx, 1);
            consumed = 1;
        }
        break;
    }
    case SDL_EVENT_FINGER_UP: {
        S32 slot = FindFingerByTouchId((S32)ev->tfinger.fingerID);
        if (slot < 0)
            break;
        S32 idx = s_fingers[slot].buttonIndex;
        if (idx >= 0)
            SetButtonState(idx, 0);
        s_fingers[slot].active = 0;
        s_fingers[slot].actuate = 0;
        /* The lift is what starts the idle clock: until it happens the finger
           is still down and the overlay is held at full. */
        s_lastTouchMs = SDL_GetTicks();
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
            if (oldIdx >= 0)
                SetButtonState(oldIdx, 0);
            /* A finger that only woke the overlay keeps pressing nothing for as
               long as it is down, however far it is dragged: the press it would
               start was never aimed. */
            if (newIdx >= 0 && s_fingers[slot].actuate)
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
   the given colour with alpha blending. Width/height are the visible
   framebuffer dimensions — writes outside are silently dropped. */
static void DrawStringARGB(U32 *fb, S32 pitchInPixels,
                           S32 x, S32 y, const char *str,
                           U8 r, U8 g, U8 b, U8 a,
                           U32 fbW, U32 fbH) {
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
                    if (px >= 0 && py >= 0 && (U32)px < fbW && (U32)py < fbH)
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
    S32 xlim = pitchInPixels - 1;
    for (S32 y = y0; y <= y1 && y >= 0; y++) {
        U32 *row = fb + y * pitchInPixels;
        for (S32 x = x0; x <= x1 && x >= 0 && x <= xlim; x++) {
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

/* Scale every alpha in a pass by one factor, so the dim level is the same
   drawing at lower contrast rather than a second set of tuned constants. */
static U8 ScaleAlpha(U8 a, U8 scale) {
    return (U8)(((U32)a * (U32)scale) / 255u);
}

/* Pixel rect for a button, clamped to the framebuffer. Returns 0 if nothing of
   it is on screen. Shared by the buttons and the pill so the thing the player
   taps and the thing they see cannot drift apart. */
static S32 ButtonRect(const TouchButton *btn, float shrink, U32 width, U32 height,
                      S32 *x0, S32 *y0, S32 *x1, S32 *y1) {
    *x0 = (S32)((btn->cx - btn->rx * shrink) * (float)width);
    *y0 = (S32)((btn->cy - btn->ry * shrink) * (float)height);
    *x1 = (S32)((btn->cx + btn->rx * shrink) * (float)width);
    *y1 = (S32)((btn->cy + btn->ry * shrink) * (float)height);
    if (*x0 < 0)
        *x0 = 0;
    if (*y0 < 0)
        *y0 = 0;
    if (*x1 >= (S32)width)
        *x1 = (S32)width - 1;
    if (*y1 >= (S32)height)
        *y1 = (S32)height - 1;
    return (*x0 <= *x1 && *y0 <= *y1);
}

static void DrawLabelCentred(U32 *fb, S32 pitchPixels, S32 x0, S32 y0, S32 x1, S32 y1,
                             const char *label, U8 alpha, U32 width, U32 height) {
    if (label == NULL || label[0] == 0)
        return;
    S32 textW = (S32)(strlen(label) * SizeChar);
    S32 textH = (S32)SizeChar;
    S32 tx = ((x0 + x1) / 2) - (textW / 2);
    S32 ty = ((y0 + y1) / 2) - (textH / 2);
    DrawStringARGB(fb, pitchPixels, tx, ty, label, 255, 255, 255, alpha, width, height);
}

void TouchOverlay_Render(U32 *frameBuffer, U32 pitch, U32 width, U32 height,
                         const U8 *palette_rgb) {
    (void)palette_rgb;
    if (frameBuffer == NULL || width == 0 || height == 0)
        return;

    S32 pitchPixels = (S32)(pitch / sizeof(U32));

    const OverlayLevel level = CurrentLevel();
    if (level == OVL_OFF)
        return;

    /* 30% keeps the layout legible against the art without competing with it,
       which is the point of dimming rather than vanishing: a player who is
       looking rather than touching is idle by definition, and that is exactly
       when a newcomer is deciding which button to reach for. */
    const U8 scale = (level == OVL_FULL) ? 255 : 76;

    for (S32 i = 0; i < NUM_TOUCH_BUTTONS; i++) {
        const TouchButton *btn = &kButtons[i];
        S32 x0, y0, x1, y1;
        if (!ButtonRect(btn, 1.0f, width, height, &x0, &y0, &x1, &y1))
            continue;

        U8 alpha = s_pressed[i] ? 180 : 100;
        U8 r = s_pressed[i] ? 200 : 80;
        U8 g = s_pressed[i] ? 200 : 80;
        U8 b8 = s_pressed[i] ? 200 : 80;
        S32 radius = 6;

        DrawFilledRoundRect(frameBuffer, pitchPixels, x0, y0, x1, y1,
                            r, g, b8, ScaleAlpha(alpha, scale), radius);

        /* Draw a simple outline */
        DrawFilledRoundRect(frameBuffer, pitchPixels, x0, y0, x1, y1,
                            255, 255, 255, ScaleAlpha(60, scale), radius);

        DrawLabelCentred(frameBuffer, pitchPixels, x0, y0, x1, y1,
                         btn->label, ScaleAlpha(220, scale), width, height);
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
