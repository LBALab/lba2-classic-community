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
 * TouchOverlay_Render must be called from the PrePresent chain.
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

/* Whether the overlay is currently visible and accepting input. Toggled at
 * runtime by TouchOverlay_Enable / TouchOverlay_Disable. */
S32 TouchOverlay_IsActive(void);
void TouchOverlay_SetActive(S32 active);
void ApplyVirtualKeys(void);

#ifdef __cplusplus
}
#endif
