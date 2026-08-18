#ifndef TESTS_INPUT_BINDINGS_ENGINE_STUBS_H
#define TESTS_INPUT_BINDINGS_ENGINE_STUBS_H

#include <SYSTEM/ADELINE_TYPES.H>

// The held-key set CheckKey() answers from. One key at a time is what makes a
// probe of the folded table unambiguous.
void HoldNone(void);
void HoldOnly(U32 key);

#endif // TESTS_INPUT_BINDINGS_ENGINE_STUBS_H
