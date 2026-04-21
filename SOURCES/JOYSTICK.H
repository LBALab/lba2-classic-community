/*--------------------------------------------------------------------------*/
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------*/
extern S32 JoystickDeadzone; // GamepadDeadzone config, SDL axis units (0..32767)

/*--------------------------------------------------------------------------*/
void InitJoystick(void);
void EndJoystick(void);
void DetectJoys(void);
void UpdateJoystick(void);
void GetJoys(U32 *bitfield);
void HandleEventsJoystick(const void *event);

// True iff a gamepad is currently opened. Use this to gate gamepad-dependent
// code paths; there is no separate enable flag — if a pad is connected, it's on.
S32 JoystickIsPresent(void);

// Returns a scancode for menu navigation (K_ENTER / K_UP / K_DOWN / K_LEFT /
// K_RIGHT), or 0 if no menu-relevant input is active. Used by CONFIG.CPP.
S32 JoystickMenuAction(void);

// D-pad + left stick arrows only (no face buttons). Used on the gamepad remap
// screen so list navigation stays usable when movement is on triggers and
// confirm/clear/exit come from player bindings instead.
S32 JoystickMenuNavOnly(void);

// Virtual scancode of the first gamepad button/stick/trigger pressed this
// frame (edge-detected), or 0 if none. Used to feed MyKey so "press any key"
// waits light up on any pad input without per-callsite changes.
U32 JoystickFirstPressedScancode(void);

#ifdef __cplusplus
}
#endif

/*--------------------------------------------------------------------------*/
#endif //JOYSTICK_H

/*--------------------------------------------------------------------------*/
