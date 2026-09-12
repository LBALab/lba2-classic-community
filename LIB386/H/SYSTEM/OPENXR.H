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
#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* One-time init: create the OpenXR instance, session, reference spaces,
 * and gameplay action set. Returns 1 on success, 0 on any failure or when
 * OpenXR is not built in. */
S32 OpenXR_Init(void);

/* Tear down the session and instance. Safe to call even if init failed. */
void OpenXR_Shutdown(void);

/* 1 while a session is running, 0 otherwise. */
S32 OpenXR_IsActive(void);
/* The headset's swapchain-recommended image size (max across eyes; 0,0
   before the session creates it). The resolution catalog uses it as the
   in-headset "display" so the menu can offer swapchain-native sizes. */
U32 OpenXR_GetSwapchainWidth(void);
U32 OpenXR_GetSwapchainHeight(void);

/* Pump OpenXR events (session state changes, loss, etc.). */
void OpenXR_PollEvents(void);

/* Run one frame of the OpenXR frame loop: xrWaitFrame / xrBeginFrame,
 * sync actions, xrEndFrame. Returns 1 on success, 0 if the session is
 * not ready or the frame should be skipped. */
S32 OpenXR_BeginFrame(void);

/* No-op in the minimal wrapper; retained for a future split where the
 * renderer owns the swapchain and wants to present after drawing.
 *
 * For the Quest 3 immersive (mono blit) path, see OpenXR_PresentMono —
 * the engine's main loop calls that instead, which copies the SDL
 * framebuffer into the OpenXR swapchain and submits the composition
 * layer to the runtime. */
void OpenXR_EndFrame(void);

/* Quest 3 immersive mono path: copy the current SDL / GLES framebuffer
 * into the OpenXR swapchain and submit a single XrCompositionLayerProjection
 * (both eyes reference the same texture) via xrEndFrame. The renderer's
 * own SDL_GL_SwapWindow is suppressed while this returns non-zero.
 * Returns 1 on success, 0 if the session is not active or the frame
 * was skipped (in which case the caller should still let SDL swap). */
S32 OpenXR_PresentMono(void);

/* 1 while an immersive OpenXR session is owning the present path (i.e.
 * Window_GL_SwapWindow should be suppressed because OpenXR_PresentMono
 * already submitted the frame). 0 otherwise. Lets the renderer skip
 * a redundant SDL swap. */
S32 OpenXR_ShouldSkipSDLSwap(void);

/* Native stereo toggle for the immersive present path. When enabled (and
 * the engine renders one frame per eye via GL_SetStereoEye),
 * OpenXR_PresentMono blits two per-eye composites into the swapchain
 * image's top/bottom halves and submits one projection layer with a
 * per-eye sub-image; otherwise both eyes share the mono composite. */
void OpenXR_SetStereoEnabled(S32 enabled);

/* Current stereo-present mode (0 = mono, 1 = per-eye). */
S32 OpenXR_GetStereoEnabled(void);

/* Read the latest Quest 3 controller state and write virtual gamepad
 * scancodes into TabKeys via the existing helpers in JOYSTICK.CPP.
 * Returns 1 if OpenXR state was written this frame, 0 otherwise
 * (inactive, no controllers, or OpenXR not built in). */
S32 OpenXR_UpdateControllerState(void);

/* ========================================================================= */
/* Quest 3 controller raw state — consumed by JOYSTICK.CPP. */

/* Button state (boolean). */
extern S32 OpenXR_LeftGripDown;
extern S32 OpenXR_RightGripDown;
extern S32 OpenXR_LeftTrigger;
extern S32 OpenXR_RightTrigger;
extern S32 OpenXR_LeftThumbstickDown;
extern S32 OpenXR_RightThumbstickDown;

/* Axes (SDL-style -32767..32767). Y is negated to match SDL's Y-down. */
extern S16 OpenXR_LeftStickX;
extern S16 OpenXR_LeftStickY;
extern S16 OpenXR_RightStickX;
extern S16 OpenXR_RightStickY;

/* Face buttons. Menu is the left controller's menu click, mapped onto
 * K_GAMEPAD_START by JOYSTICK.CPP (the default I_MENUS binding). */
extern S32 OpenXR_ABtn;
extern S32 OpenXR_BBtn;
extern S32 OpenXR_XBtn;
extern S32 OpenXR_YBtn;
extern S32 OpenXR_MenuBtn;

/* ========================================================================= */
#ifdef __cplusplus
}
#endif
