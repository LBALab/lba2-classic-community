#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Touch screen virtual gamepad for Android / mobile devices.
 *
 * Provides an on-screen overlay with a D-pad (left side), action buttons
 * (bottom right), and utility buttons (top corners). Touch events map to
 * keyboard scancodes injected into the game's input system. The overlay
 * renders semi-transparent buttons onto the ARGB framebuffer just before
 * presentation via the PrePresent callback.
 *
 * All coordinates use normalised 0.0–1.0 range so the layout adapts to
 * any resolution and aspect ratio.
 *
 * Enabled at runtime via TouchOverlay_Init / TouchOverlay_Shutdown.
 * TouchOverlay_HandleEvent must be called from the event loop on
 * SDL_EVENT_FINGER_DOWN / SDL_EVENT_FINGER_UP / SDL_EVENT_FINGER_MOTION.
 * TouchOverlay_Render must be called from the PrePresent chain -- unconditionally,
 * once, at startup. It decides for itself whether to draw anything, so a caller
 * that registers it only when the overlay happens to be visible at boot makes
 * every later "show it again" silently draw nothing.
 *
 * WHEN IT SHOWS
 * -------------
 * The overlay follows the last input device the player actually used, and fades
 * when they stop using it:
 *
 *   a finger down          -> full opacity, held for as long as it is down
 *   ~1.5s after the lift   -> dimmed, still legible enough to aim by
 *   ~6s after the lift     -> gone
 *   a pad or keyboard used -> gone, until a finger says otherwise
 *   HIDE pressed           -> gone now, without waiting out the fade
 *
 * Every one of those states is left by the same thing: a touch. HIDE is a
 * dismiss, not a mode, which is why there is no SHOW control to undo it.
 *
 * The first touch after another device only wakes the overlay; it presses
 * nothing. Every touch after that both wakes and presses, so a player who knows
 * the layout never loses the press that mattered. See TOUCH_INPUT.CPP for why
 * this is a timer rather than a per-screen layout.
 */

void TouchOverlay_Init(void);
void TouchOverlay_Shutdown(void);

/* Returns S32 so the event filter can forward the return: 1 if the event was
 * consumed (finger on overlay), 0 if it should pass through to the game. For
 * SDL_EVENT_FINGER_DOWN/UP/MOTION only; other event types return 0. */
S32 TouchOverlay_HandleEvent(const void *sdlEvent);

/* Draw the semi-transparent overlay onto the ARGB framebuffer. Called from
 * the PrePresent chain. Parameters match the PrePresentFn signature. */
void TouchOverlay_Render(U32 *frameBuffer, U32 pitch, U32 width, U32 height,
                         const U8 *palette_rgb);

/* Whether this device gets an overlay at all: false off Android and on a
 * leanback (TV) device. Settled once by TouchOverlay_Init. Distinct from
 * whether anything is drawn this frame, which the policy above decides. */
S32 TouchOverlay_IsSupported(void);

void ApplyVirtualKeys(void);

#ifdef __cplusplus
}
#endif
