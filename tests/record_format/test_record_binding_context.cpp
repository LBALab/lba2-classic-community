#include "../test_harness.h"
#include <SYSTEM/INPUT.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include "INPUT.H"
#include "RECORD_FORMAT.H"
#include "held_keys.h"
#include <cstring>
#include <initializer_list>

static T_BINDING_CONTEXT legacy_context(U32 value) {
    char header[64];
    snprintf(header, sizeof(header), "bindings.context=%u\n", (unsigned)value);
    T_BINDING_CONTEXT parsed = {};
    ASSERT_EQ_INT(1, RecFmt_ReadBindingContext(header, &parsed));
    return parsed;
}

static void assert_context_equal(const T_BINDING_CONTEXT &expected,
                                 const T_BINDING_CONTEXT &actual) {
    ASSERT_EQ_UINT(expected.gameplay, actual.gameplay);
    ASSERT_EQ_UINT(expected.count, actual.count);
    ASSERT_MEM_EQ(expected.remaps, actual.remaps, expected.count * sizeof(expected.remaps[0]));
}

static void test_context_metadata() {
    for (U32 value = 0; value <= BINDINGS_CONTEXT_MASK; ++value) {
        T_BINDING_CONTEXT context = legacy_context(value);
        ASSERT_EQ_UINT((value & BINDINGS_CONTEXT_GAMEPLAY) != 0, context.gameplay);
        ASSERT_EQ_UINT((value & BINDINGS_CONTEXT_MODAL_CIRCLE) != 0, context.count);
        if (context.count) {
            ASSERT_EQ_UINT(K_GAMEPAD_B, context.remaps[0].key);
            ASSERT_EQ_UINT(13, context.remaps[0].gameplaySlot);
            ASSERT_EQ_UINT(9, context.remaps[0].modalSlot);
            ASSERT_EQ_UINT((value & BINDINGS_CONTEXT_CIRCLE_SUPPRESSED) != 0,
                           context.remaps[0].suppressed);
        }
        char header[1024];
        ASSERT_TRUE(RecFmt_WriteBindingContext(header, sizeof(header), context));
        T_BINDING_CONTEXT parsed = {};
        ASSERT_EQ_INT(1, RecFmt_ReadBindingContext(header, &parsed));
        assert_context_equal(context, parsed);
        Bindings_SetContext(parsed);
        assert_context_equal(context, Bindings_GetContext());
    }
    T_BINDING_CONTEXT context = Bindings_GetContext();
    T_BINDING_CONTEXT untouched = context;
    ASSERT_EQ_INT(0, RecFmt_ReadBindingContext("bindings.keyboard=1:2\n", &context));
    assert_context_equal(untouched, context);
    for (const char *value : {"", "-1", "8", "4294967296", "1junk", "1:0", "\n1",
                              "v2,0,0", "v1,2,0", "v1,0,33", "v1,0,1",
                              "v1,0,1,0:13:9:0", "v1,0,1,99999:13:9:0",
                              "v1,0,1,1025:32:9:0", "v1,0,1,1025:13:32:0",
                              "v1,0,1,1025:13:9:2", "v1,0,1,4294967296:13:9:0",
                              "v1,0,2,1025:13:9:0,1025:13:8:0", "v1,0,0,"}) {
        char header[256];
        snprintf(header, sizeof(header), "bindings.context=%s\n", value);
        ASSERT_EQ_INT(-1, RecFmt_ReadBindingContext(header, &context));
        assert_context_equal(untouched, context);
    }
    ASSERT_EQ_INT(1, RecFmt_ReadBindingContext("bindings.context=1\r\n", &context));
    ASSERT_EQ_UINT(1, context.count);
    ASSERT_EQ_INT(1, RecFmt_ReadBindingContext("bindings.context=v1,1,0\r\n", &context));
    ASSERT_EQ_UINT(1, context.gameplay);
    ASSERT_EQ_INT(0, RecFmt_WriteBindingContext(NULL, 0, context));
    char shortBuffer[8];
    ASSERT_EQ_INT(0, RecFmt_WriteBindingContext(shortBuffer, sizeof(shortBuffer), context));
}

static U32 nextKeys;
static void inject_poll() {
    HoldNone();
    if (nextKeys & 1)
        HoldAdd(K_GAMEPAD_B);
    if (nextKeys & 2)
        HoldAdd(K_GAMEPAD_LSTICK_LEFT);
}

static void test_cross_layout_replay() {
    T_DEF_KEY savedKeyboard[MAX_INPUT_SLOTS], savedGamepad[MAX_INPUT_SLOTS];
    RestoreInput();
    memcpy(savedKeyboard, DefKeys, sizeof(savedKeyboard));
    memcpy(savedGamepad, GamepadKeys, sizeof(savedGamepad));
    T_BINDING_CONTEXT savedContext = Bindings_GetContext();
    for (U32 mode : {0u, BINDINGS_CONTEXT_MODAL_CIRCLE}) {
        HoldNone();
        ClearNoRepeatInput();
        memset(DefKeys, 0, sizeof(savedKeyboard));
        memset(GamepadKeys, 0, sizeof(savedGamepad));
        GamepadKeys[2].Key1 = K_GAMEPAD_LSTICK_LEFT;
        GamepadKeys[13].Key1 = K_GAMEPAD_B;
        Bindings_SetContext(legacy_context(mode));
        char keyboard[1024], gamepad[1024], contextLine[1024], header[3200];
        ASSERT_TRUE(RecFmt_WriteBindings(keyboard, sizeof(keyboard), DefKeys,
                                         MAX_INPUT_SLOTS, "bindings.keyboard="));
        ASSERT_TRUE(RecFmt_WriteBindings(gamepad, sizeof(gamepad), GamepadKeys,
                                         MAX_INPUT_SLOTS, "bindings.gamepad="));
        ASSERT_TRUE(RecFmt_WriteBindingContext(contextLine, sizeof(contextLine), Bindings_GetContext()));
        snprintf(header, sizeof(header), "%s%s%s", keyboard, gamepad, contextLine);

        memcpy(DefKeys, savedKeyboard, sizeof(savedKeyboard));
        memcpy(GamepadKeys, savedGamepad, sizeof(savedGamepad));
        Bindings_SetContext(savedContext);
        ASSERT_TRUE(RecFmt_ParseBindings(header, "bindings.keyboard=", DefKeys, MAX_INPUT_SLOTS));
        ASSERT_TRUE(RecFmt_ParseBindings(header, "bindings.gamepad=", GamepadKeys, MAX_INPUT_SLOTS));
        T_BINDING_CONTEXT context = {};
        ASSERT_EQ_INT(1, RecFmt_ReadBindingContext(header, &context));
        Bindings_SetContext(context);

        const U32 keys[] = {0, 1, 3, 3, 2, 3, 0, 1, 0};
        const S32 gameplay[] = {0, 0, 1, 1, 1, 1, 0, 0, 0};
        const U32 modal[] = {0, I_RETURN, I_LEFT, I_LEFT, I_LEFT,
                             I_ESQUIVE | I_LEFT, 0, I_RETURN, 0};
        const U32 plain[] = {0, I_ESQUIVE, I_ESQUIVE | I_LEFT, I_ESQUIVE | I_LEFT,
                             I_LEFT, I_ESQUIVE | I_LEFT, 0, I_ESQUIVE, 0};
        SetPollHook(inject_poll);
        for (U32 i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
            Bindings_SetGameplay(gameplay[i]);
            nextKeys = keys[i];
            GetInput(0);
            ASSERT_EQ_UINT(mode ? modal[i] : plain[i], Input);
        }
        SetPollHook(NULL);
        memcpy(DefKeys, savedKeyboard, sizeof(savedKeyboard));
        memcpy(GamepadKeys, savedGamepad, sizeof(savedGamepad));
        Bindings_SetContext(savedContext);
        assert_context_equal(savedContext, Bindings_GetContext());
        ASSERT_MEM_EQ(savedKeyboard, DefKeys, sizeof(savedKeyboard));
        ASSERT_MEM_EQ(savedGamepad, GamepadKeys, sizeof(savedGamepad));
    }
    HoldNone();
}

static void test_restored_release_latch() {
    HoldNone();
    ClearNoRepeatInput();
    RestoreInput();
    GamepadKeys[9].Key1 = GamepadKeys[9].Key2 = 0;
    GamepadKeys[13].Key1 = K_GAMEPAD_B;
    GamepadKeys[13].Key2 = 0;
    for (U32 value = 0; value <= BINDINGS_CONTEXT_MASK; ++value) {
        Bindings_SetContext(legacy_context(value));
        HoldOnly(K_GAMEPAD_B);
        GetInput(0);
        U32 expected = I_ESQUIVE;
        if (value & BINDINGS_CONTEXT_MODAL_CIRCLE) {
            if (value & BINDINGS_CONTEXT_CIRCLE_SUPPRESSED)
                expected = 0;
            else if (!(value & BINDINGS_CONTEXT_GAMEPLAY))
                expected = I_RETURN;
        }
        ASSERT_EQ_UINT(expected, Input);
    }
    HoldNone();
}

static void test_generic_context_roundtrip() {
    T_BINDING_CONTEXT compiled = Bindings_GetContext();
    T_BINDING_CONTEXT context = {0, 3, {{K_GAMEPAD_A, 8, 7, 1}, {K_F12, 4, 12, 0}, {K_GAMEPAD_Y, 6, 9, 0}}};
    memset(DefKeys, 0, sizeof(DefKeys));
    memset(GamepadKeys, 0, sizeof(GamepadKeys));
    DefKeys[4].Key1 = K_F12;
    GamepadKeys[8].Key1 = K_GAMEPAD_A;
    GamepadKeys[6].Key1 = K_GAMEPAD_Y;
    GamepadKeys[13].Key1 = K_GAMEPAD_B;
    HoldNone();
    ClearNoRepeatInput();
    Bindings_SetContext(context);
    char header[2048];
    ASSERT_TRUE(RecFmt_WriteBindingContext(header, sizeof(header), Bindings_GetContext()));
    Bindings_SetContext(legacy_context(BINDINGS_CONTEXT_MODAL_CIRCLE));
    T_BINDING_CONTEXT parsed = {};
    ASSERT_EQ_INT(1, RecFmt_ReadBindingContext(header, &parsed));
    Bindings_SetContext(parsed);
    assert_context_equal(context, Bindings_GetContext());
    HoldOnly(K_GAMEPAD_A);
    HoldAdd(K_F12);
    HoldAdd(K_GAMEPAD_Y);
    HoldAdd(K_GAMEPAD_B);
    GetInput(0);
    ASSERT_EQ_UINT((1u << 12) | (1u << 9) | I_ESQUIVE, Input);
    Bindings_SetGameplay(1);
    GetInput(0);
    ASSERT_EQ_UINT(I_ESQUIVE, Input);
    HoldNone();
    Bindings_SetGameplay(1);
    HoldAdd(K_GAMEPAD_A);
    HoldAdd(K_F12);
    HoldAdd(K_GAMEPAD_Y);
    GetInput(0);
    ASSERT_EQ_UINT((1u << 8) | (1u << 4) | (1u << 6), Input);
    Bindings_SetContext(compiled);
    assert_context_equal(compiled, Bindings_GetContext());

    context.count = MAX_BINDING_REMAPS;
    for (U32 i = 0; i < context.count; ++i)
        context.remaps[i] = {K_F12, i, (i + 1) % 32, i % 2};
    ASSERT_TRUE(RecFmt_WriteBindingContext(header, sizeof(header), context));
    ASSERT_EQ_INT(1, RecFmt_ReadBindingContext(header, &parsed));
    assert_context_equal(context, parsed);
    HoldNone();
    RestoreInput();
}

int main() {
    T_BINDING_CONTEXT compiledContext = Bindings_GetContext();
    RUN_TEST(test_context_metadata);
    Bindings_SetContext(compiledContext);
    RUN_TEST(test_cross_layout_replay);
    RUN_TEST(test_restored_release_latch);
    Bindings_SetContext(compiledContext);
    RUN_TEST(test_generic_context_roundtrip);
    Bindings_SetContext(compiledContext);
    TEST_SUMMARY();
    return test_failures != 0;
}
