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

/* Pump OpenXR events (session state changes, loss, etc.). */
void OpenXR_PollEvents(void);

/* Run one frame of the OpenXR frame loop: xrWaitFrame / xrBeginFrame,
 * sync actions, xrEndFrame. Returns 1 on success, 0 if the session is
 * not ready or the frame should be skipped. */
S32 OpenXR_BeginFrame(void);

/* No-op in the minimal wrapper; retained for a future split where the
 * renderer owns the swapchain and wants to present after drawing. */
void OpenXR_EndFrame(void);

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

/* Face buttons. */
extern S32 OpenXR_ABtn;
extern S32 OpenXR_BBtn;
extern S32 OpenXR_XBtn;
extern S32 OpenXR_YBtn;

/* ========================================================================= */
#ifdef __cplusplus
}
#endif
