#include "../test_harness.h"
#include <SYSTEM/INPUT.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include "INPUT.H"
#include "held_keys.h"

static void test_stick_defaults_and_restore() {
    RestoreInput();
    ASSERT_EQ_UINT(K_GAMEPAD_LSTICK_UP, GamepadKeys[0].Key1);
    ASSERT_EQ_UINT(K_GAMEPAD_LSTICK_DOWN, GamepadKeys[1].Key1);
    ASSERT_EQ_UINT(K_GAMEPAD_LSTICK_LEFT, GamepadKeys[2].Key1);
    ASSERT_EQ_UINT(K_GAMEPAD_LSTICK_RIGHT, GamepadKeys[3].Key1);
    ASSERT_EQ_UINT(K_GAMEPAD_RSHOULDER, GamepadKeys[4].Key1);
    ASSERT_EQ_UINT(K_GAMEPAD_LSHOULDER, GamepadKeys[5].Key1);
    ASSERT_EQ_UINT(0, GamepadKeys[13].Key1);
    ASSERT_EQ_UINT(0, GamepadKeys[13].Key2);
    GamepadKeys[0].Key1 = K_GAMEPAD_RTRIGGER;
    RestoreInput();
    ASSERT_EQ_UINT(K_GAMEPAD_LSTICK_UP, GamepadKeys[0].Key1);
    ASSERT_MEM_EQ(DefKeysDefault95, DefKeys, sizeof(T_DEF_KEY) * MAX_INPUT_SLOTS);
}

static void test_movement_and_absent_buttons() {
    RestoreInput();
    InitInput();
    const U32 directions[] = {K_GAMEPAD_LSTICK_UP, K_GAMEPAD_LSTICK_DOWN,
                              K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_LSTICK_RIGHT};
    for (U32 i = 0; i < 4; ++i) {
        ClearNoRepeatInput();
        HoldOnly(directions[i]);
        GetInput(0);
        ASSERT_EQ_UINT(1u << i, Input);
    }
    const U32 absent[] = {K_GAMEPAD_LTRIGGER, K_GAMEPAD_RTRIGGER,
                          K_GAMEPAD_LSTICK_BTN, K_GAMEPAD_RSTICK_BTN};
    for (U32 i = 0; i < 4; ++i) {
        ClearNoRepeatInput();
        HoldOnly(absent[i]);
        GetInput(0);
        ASSERT_EQ_UINT(0, Input);
    }
    HoldNone();
}

int main() {
    RUN_TEST(test_stick_defaults_and_restore);
    RUN_TEST(test_movement_and_absent_buttons);
    TEST_SUMMARY();
    return test_failures != 0;
}
