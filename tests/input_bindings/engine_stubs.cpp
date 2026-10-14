/* Stubs for the engine symbols SOURCES/INPUT.CPP drags in but the config round
 * trip never touches.
 *
 * What is left in that translation unit after the binding layer moved out is
 * the config IO and MyGetInput / WaitInput / WaitNoInput, which poll the
 * keyboard, the pad and the console. Only the second group needs any of the
 * symbols below, and none of it is reachable from the code this test drives --
 * they exist so the link resolves.
 *
 * tests/input_bindings needs none of this now, which is what the extraction
 * bought: that test links INPUT_BINDINGS.CPP, DefineInputKeys and the two
 * keyboard hooks, and nothing else.
 */
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>

#include "CONSOLE/CONSOLE.H"
#include "JOYSTICK.H"

// --- Keyboard state MyGetInput() copies out; unreachable from these tests ----
S32 Key = 0;
S32 MyKey = 0;
U8 TabKeys[TABKEYS_NUM_KEYS];

// --- Screen geometry the console's forced present reads ---------------------
U32 ModeDesiredX = 640;
U32 ModeDesiredY = 480;

extern "C" {
void BoxStaticAdd(S32, S32, S32, S32) {}
void BoxUpdate(void) {}
/* The console's redraw marks itself as an overlay so it does not step the fixed-dt
   clock; the clock itself is not linked here. tests/timer covers the behaviour. */
void Timer_FixedDtOverlayPresent(void) {}

int Console_IsOpen(void) { return 0; }
void Console_Update(void) {}
int Console_GetToggleScancode(void) { return 0; }
int Console_ShouldSuppressInputThisFrame(void) { return 0; }

void GetJoys(U32 *) {}
U32 JoystickFirstPressedScancode(void) { return 0; }
}
