/* Stubs for the engine symbols SOURCES/INPUT.CPP drags in but the binding
 * layer never touches.
 *
 * The file under test is one translation unit holding two unrelated things:
 * the binding tables and the fold that builds Input from them (pure lookups,
 * what these tests are about), and MyGetInput / WaitInput / WaitNoInput, which
 * poll the keyboard, the pad and the console. Only the second half needs any
 * of the symbols below, and none of them is reachable from the code the tests
 * drive -- they exist so the link resolves.
 *
 * That is the case for the extraction: once the tables and the fold live in
 * their own TU, its test links DefineInputKeys and nothing else.
 *
 * The two keyboard hooks are different in kind. CheckKey and ManageKeyboard
 * are what GetInput() calls to sample key state, so the tests do drive them:
 * held keys are a set the test controls, exactly as tests/input_funnel does,
 * which is what lets the fold be probed one key at a time with no SDL and no
 * game stack underneath.
 */
#include <set>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>

#include "CONSOLE/CONSOLE.H"
#include "JOYSTICK.H"

#include "engine_stubs.h"

// --- The held-key set GetInput() samples ------------------------------------
static std::set<U32> g_held;

void HoldNone(void) { g_held.clear(); }
void HoldOnly(U32 key) {
    g_held.clear();
    g_held.insert(key);
}

extern "C" {
void ManageKeyboard(void) {}
S32 CheckKey(U32 key) { return g_held.count(key) ? 1 : 0; }
}

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

int Console_IsOpen(void) { return 0; }
void Console_Update(void) {}
int Console_GetToggleScancode(void) { return 0; }
int Console_ShouldSuppressInputThisFrame(void) { return 0; }

void GetJoys(U32 *) {}
U32 JoystickFirstPressedScancode(void) { return 0; }
S32 JoystickDeadzone = 0;
}

// --- Camera preferences ReadGamepadConfig()/WriteGamepadConfig() carry -------
// Declared in C_EXTERN.H, so C++ linkage, and defined here for the same reason
// as the rest: the config half of the TU names them.
S32 GamepadCameraAnalog = 0;
S32 GamepadCameraSensX = 0;
S32 GamepadCameraSensY = 0;
S32 GamepadCameraInvertY = 0;
S32 GamepadCamMaxBeta = 0;
S32 GamepadCamMaxAlpha = 0;
