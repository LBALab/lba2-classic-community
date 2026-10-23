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
    ASSERT_EQ_UINT(0, GamepadKeys[9].Key1);
    ASSERT_EQ_UINT(K_GAMEPAD_B, GamepadKeys[13].Key1);
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

static void test_dodge_directions_and_modal_circle() {
    RestoreInput();
    ClearNoRepeatInput();
    Bindings_SetGameplay(1);
    const U32 directions[] = {K_GAMEPAD_LSTICK_UP, K_GAMEPAD_LSTICK_DOWN,
                              K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_LSTICK_RIGHT};
    for (U32 i = 0; i < 4; ++i) {
        HoldOnly(K_GAMEPAD_B);
        HoldAdd(directions[i]);
        GetInput(0);
        ASSERT_EQ_UINT(I_ESQUIVE | (1u << i), Input);
    }
    HoldNone();
    GetInput(0);
    Bindings_SetGameplay(0);
    HoldOnly(K_GAMEPAD_B);
    GetInput(0);
    ASSERT_EQ_UINT(I_RETURN, Input);
    GetInput(I_RETURN);
    ASSERT_EQ_UINT(0, Input);
    GetInput(0);
    ASSERT_EQ_UINT(0, Input);
    HoldNone();
    GetInput(0);
    HoldOnly(K_GAMEPAD_B);
    GetInput(0);
    ASSERT_EQ_UINT(I_RETURN, Input);
    GamepadKeys[13].Key1 = K_GAMEPAD_RSHOULDER;
    InitInput();
    Bindings_SetGameplay(1);
    HoldOnly(K_GAMEPAD_RSHOULDER);
    GetInput(0);
    ASSERT_TRUE(Input & I_ESQUIVE);
    ASSERT_TRUE(!(Input & I_RETURN));
    Bindings_SetGameplay(0);
    HoldNone();
}

static void test_circle_context_release() {
    const U32 directions[] = {K_GAMEPAD_LSTICK_UP, K_GAMEPAD_LSTICK_DOWN,
                              K_GAMEPAD_LSTICK_LEFT, K_GAMEPAD_LSTICK_RIGHT};
    RestoreInput();
    InitInput();
    ClearNoRepeatInput();
    HoldNone();
    MyGetInput();
    for (int direction = 0; direction < 4; ++direction) {
        HoldOnly(K_GAMEPAD_B);
        MyGetInput();
        ASSERT_EQ_UINT(I_RETURN, Input);
        InitWaitNoInput(I_INVENTORY | I_RETURN | I_MENUS);
        MyGetGameplayInput();
        ASSERT_EQ_UINT(0, Input);
        HoldAdd(directions[direction]);
        for (int frame = 0; frame < 3; ++frame) {
            MyGetGameplayInput();
            ASSERT_EQ_UINT(1u << direction, Input);
        }
        HoldAdd(K_X);
        MyGetGameplayInput();
        ASSERT_EQ_UINT(I_ESQUIVE | (1u << direction), Input);
        HoldRemove(K_X);
        HoldRemove(K_GAMEPAD_B);
        MyGetGameplayInput();
        ASSERT_EQ_UINT(1u << direction, Input);
        HoldAdd(K_GAMEPAD_B);
        for (int frame = 0; frame < 3; ++frame) {
            MyGetGameplayInput();
            ASSERT_EQ_UINT(I_ESQUIVE | (1u << direction), Input);
        }
        MyGetInput();
        ASSERT_EQ_UINT(1u << direction, Input);
        HoldNone();
        MyGetInput();
        HoldOnly(K_GAMEPAD_B);
        MyGetInput();
        ASSERT_EQ_UINT(I_RETURN, Input);
        HoldNone();
        MyGetInput();
    }
}

int main() {
    RUN_TEST(test_stick_defaults_and_restore);
    RUN_TEST(test_movement_and_absent_buttons);
    RUN_TEST(test_dodge_directions_and_modal_circle);
    RUN_TEST(test_circle_context_release);
    TEST_SUMMARY();
    return test_failures != 0;
}
