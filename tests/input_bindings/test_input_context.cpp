#include "../test_harness.h"
#include <SYSTEM/INPUT.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <INPUT_PROFILE_CONFIG.H>
#include "INPUT.H"
#include "held_keys.h"

struct ContextRemap {
    U32 key;
    S32 source;
    S32 destination;
};
static const ContextRemap kRemaps[] = {LBA2_INPUT_MODAL_REMAPS{0, 0, 0}};
static const S32 kRemapCount = sizeof(kRemaps) / sizeof(kRemaps[0]) - 1;

static void reset_bindings() {
    HoldNone();
    Bindings_SetGameplay(0);
    ClearNoRepeatInput();
    for (S32 slot = 0; slot < MAX_INPUT_SLOTS; ++slot) {
        DefKeys[slot].Key1 = DefKeys[slot].Key2 = 0;
        GamepadKeys[slot].Key1 = GamepadKeys[slot].Key2 = 0;
    }
    InitInput();
}

static void poll(S32 gameplay) {
    Bindings_SetGameplay(gameplay);
    GetInput(0);
}

static void test_configured_keys_require_release() {
    U32 random = 0xDEADBEEF;
    ASSERT_TRUE(kRemapCount > 0);
    for (S32 round = 0; round < 50; ++round) {
        random = random * 1103515245u + 12345u;
        const ContextRemap &remap = kRemaps[(random >> 16) % kRemapCount];
        reset_bindings();
        T_DEF_KEY &binding = remap.key < 256 ? DefKeys[remap.source] : GamepadKeys[remap.source];
        if (round & 1)
            binding.Key2 = remap.key;
        else
            binding.Key1 = remap.key;
        GamepadKeys[20].Key1 = K_GAMEPAD_B;
        InitInput();
        HoldOnly(remap.key);
        GetInput(0);
        ASSERT_EQ_UINT(1u << remap.destination, Input);
        GetInput(1u << remap.destination);
        HoldAdd(K_GAMEPAD_B);
        for (S32 frame = 0; frame < 3; ++frame) {
            poll(1);
            ASSERT_EQ_UINT(1u << 20, Input);
        }
        HoldRemove(remap.key);
        poll(1);
        HoldAdd(remap.key);
        for (S32 frame = 0; frame < 3; ++frame) {
            poll(1);
            ASSERT_EQ_UINT((1u << remap.source) | (1u << 20), Input);
        }
        poll(0);
        ASSERT_EQ_UINT(1u << 20, Input);
        HoldRemove(remap.key);
        poll(0);
        HoldAdd(remap.key);
        poll(0);
        ASSERT_EQ_UINT((1u << remap.destination) | (1u << 20), Input);
    }
    reset_bindings();
}

static void test_unchanged_actions_and_explicit_bindings() {
    reset_bindings();
    GamepadKeys[6].Key1 = K_GAMEPAD_Y;
    GamepadKeys[9].Key1 = K_GAMEPAD_Y;
    InitInput();
    HoldOnly(K_GAMEPAD_Y);
    GetInput(0);
    ASSERT_EQ_UINT((1u << 6) | (1u << 9), Input);
    poll(1);
    ASSERT_EQ_UINT((1u << 6) | (1u << 9), Input);
    poll(0);
    ASSERT_EQ_UINT((1u << 6) | (1u << 9), Input);

    reset_bindings();
    GamepadKeys[6].Key1 = K_GAMEPAD_X;
    InitInput();
    HoldOnly(K_GAMEPAD_X);
    GetInput(0);
    ASSERT_EQ_UINT(1u << 6, Input);
    poll(1);
    ASSERT_EQ_UINT(1u << 6, Input);
    poll(0);
    ASSERT_EQ_UINT(1u << 6, Input);
    reset_bindings();
}

int main() {
    RUN_TEST(test_configured_keys_require_release);
    RUN_TEST(test_unchanged_actions_and_explicit_bindings);
    TEST_SUMMARY();
    return test_failures != 0;
}
