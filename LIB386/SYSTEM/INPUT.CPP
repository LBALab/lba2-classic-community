#include <SYSTEM/INPUT.H>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/INPUT_FLOW.H>
#include <SYSTEM/KEYBOARD.H>

//***************************************************************************
U32 Input;

//***************************************************************************
static U32 NoRepeatInput;

//***************************************************************************
static S32 NbKeys;
static U32 *InputKeys;
static U32 *InputMasks;

//***************************************************************************
void DefineInputKeys(S32 nbkeys, U32 *keys, U32 *inputmasks) {
    NbKeys = nbkeys;
    InputKeys = keys;
    InputMasks = inputmasks;
}

//***************************************************************************
void GetInput(U32 norepeat) {
    S32 i;

    // update TabKeys
    ManageKeyboard();

    // update NoRepeatInput
    NoRepeatInput |= norepeat;

    // reset Input
    Input = 0;

    // Rebuild Input
    for (i = 0; i < NbKeys; i++) {
        if (CheckKey(InputKeys[i]))
            Input |= InputMasks[i];
    }

    // compute new NoRepeatInput
    norepeat = NoRepeatInput & Input;

    // mask Input
    Input &= ~NoRepeatInput;

    // update NoRepeatInput
    NoRepeatInput = norepeat;

    /* Boundary 2, with the table GetInput actually walked and the Input it
       produced. `norepeat` at this point is what the edge filter withheld,
       which is suppression working rather than a rise going missing. */
    InputFlow_NoteInputRebuilt(Input, norepeat);
}

//***************************************************************************
// Added le 06/06/97 (function becose NoRepeatInput is static)
void ClearNoRepeatInput(void) {
    NoRepeatInput = 0;
}

//***************************************************************************
