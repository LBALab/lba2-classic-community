/*--------------------------------------------------------------------------*/
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------*/
extern S32 FlagJoystick;      // GamepadEnabled config
extern S32 JoystickDeadzone;  // GamepadDeadzone config, SDL axis units (0..32767)

/*--------------------------------------------------------------------------*/
void InitJoystick(void);
void EndJoystick(void);
void DetectJoys(void);
void UpdateJoystick(void);
void GetJoys(U32 *bitfield);
void HandleEventsJoystick(const void *event);

// Returns a scancode for menu navigation (K_ENTER / K_UP / K_DOWN / K_LEFT /
// K_RIGHT), or 0 if no menu-relevant input is active. Used by CONFIG.CPP.
S32 JoystickMenuAction(void);

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
