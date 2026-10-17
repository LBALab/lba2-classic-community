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
static U32 g_releaseAfterPolls = 0;

void HoldNone(void) { g_held.clear(); }
void HoldOnly(U32 key) {
    g_held.clear();
    g_held.insert(key);
}
void HoldAdd(U32 key) { g_held.insert(key); }
void HoldRemove(U32 key) { g_held.erase(key); }
void ReleaseAfterPolls(U32 polls) { g_releaseAfterPolls = polls; }

extern "C" {
void ManageKeyboard(void) {
    if (g_releaseAfterPolls && --g_releaseAfterPolls == 0)
        g_held.clear();
}
S32 CheckKey(U32 key) { return g_held.count(key) ? 1 : 0; }
}
