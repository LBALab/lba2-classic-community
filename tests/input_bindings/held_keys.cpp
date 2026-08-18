/* The two keyboard hooks GetInput() calls to sample key state.
 *
 * Held keys are a set the test controls and ManageKeyboard is a no-op, so the
 * fold can be probed one scancode at a time with no SDL and no game stack
 * underneath. Same approach as tests/input_funnel.
 */
#include <set>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD.H>

#include "held_keys.h"

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
