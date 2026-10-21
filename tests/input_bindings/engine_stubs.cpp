/* Engine seams for the config and UI-input regressions. The held-key fixture
 * supplies physical key state; these stubs control console isolation without
 * linking the renderer or opening a controller.
 */
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>

#include "CONSOLE/CONSOLE.H"
#include "JOYSTICK.H"
#include "TOUCH_INPUT.H"

// --- Keyboard state MyGetInput() copies out ---------------------------------
S32 Key = 0;
S32 MyKey = 0;
U8 TabKeys[TABKEYS_NUM_KEYS];
int TestConsoleOpen = 0;
int TestConsoleSuppress = 0;
bool AppActive = true;

// --- Screen geometry the console's forced present reads ---------------------
U32 ModeDesiredX = 640;
U32 ModeDesiredY = 480;

extern "C" {
void BoxStaticAdd(S32, S32, S32, S32) {}
void BoxUpdate(void) {}
int Console_IsOpen(void) { return TestConsoleOpen; }
void Console_Update(void) {}
int Console_GetToggleScancode(void) { return 0; }
int Console_ShouldSuppressInputThisFrame(void) { return TestConsoleSuppress; }

void GetJoys(U32 *) {}
void TouchInput_SetGameplay(S32) {}
U32 JoystickFirstPressedScancode(void) { return 0; }
}
