#ifndef TESTS_INPUT_BINDINGS_HELD_KEYS_H
#define TESTS_INPUT_BINDINGS_HELD_KEYS_H

#include <SYSTEM/ADELINE_TYPES.H>

// The held-key set CheckKey() answers from. One key at a time is what makes a
// probe of the folded table unambiguous; the suppression rules need more than
// one down at once, and need a key to go down or come up without disturbing the
// rest, since the latch turns on exactly that.
void HoldNone(void);
void HoldOnly(U32 key);
void HoldAdd(U32 key);
void HoldRemove(U32 key);

#endif // TESTS_INPUT_BINDINGS_HELD_KEYS_H
