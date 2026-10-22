#include "../test_harness.h"
#include <SYSTEM/INPUT.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include "INPUT.H"
#include "RECORD_FORMAT.H"
#include "held_keys.h"
#include <cstring>
#include <initializer_list>

static void test_context_metadata() {
    for (U32 context = 0; context <= BINDINGS_CONTEXT_MASK; ++context) {
        char header[64];
        snprintf(header, sizeof(header), "bindings.context=%u\n", (unsigned)context);
        U32 parsed = 99;
        ASSERT_EQ_INT(1, RecFmt_ReadBindingContext(header, &parsed));
        ASSERT_EQ_UINT(context, parsed);
        Bindings_SetContext(parsed);
        ASSERT_EQ_UINT(context, Bindings_GetContext());
    }
    U32 context = 99;
    ASSERT_EQ_INT(0, RecFmt_ReadBindingContext("bindings.keyboard=1:2\n", &context));
    ASSERT_EQ_UINT(99, context);
    for (const char *value : {"", "-1", "8", "4294967296", "1junk", "1:0", "\n1"}) {
        char header[64];
        snprintf(header, sizeof(header), "bindings.context=%s\n", value);
        ASSERT_EQ_INT(-1, RecFmt_ReadBindingContext(header, &context));
        ASSERT_EQ_UINT(99, context);
    }
    ASSERT_EQ_INT(1, RecFmt_ReadBindingContext("bindings.context=1\r\n", &context));
    ASSERT_EQ_UINT(1, context);
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
    U32 savedContext = Bindings_GetContext();
    for (U32 mode : {0u, BINDINGS_CONTEXT_MODAL_CIRCLE}) {
        HoldNone();
        ClearNoRepeatInput();
        memset(DefKeys, 0, sizeof(savedKeyboard));
        memset(GamepadKeys, 0, sizeof(savedGamepad));
        GamepadKeys[2].Key1 = K_GAMEPAD_LSTICK_LEFT;
        GamepadKeys[13].Key1 = K_GAMEPAD_B;
        Bindings_SetContext(mode);
        char keyboard[1024], gamepad[1024], header[2200];
        ASSERT_TRUE(RecFmt_WriteBindings(keyboard, sizeof(keyboard), DefKeys,
                                         MAX_INPUT_SLOTS, "bindings.keyboard="));
        ASSERT_TRUE(RecFmt_WriteBindings(gamepad, sizeof(gamepad), GamepadKeys,
                                         MAX_INPUT_SLOTS, "bindings.gamepad="));
        snprintf(header, sizeof(header), "%s%sbindings.context=%u\n",
                 keyboard, gamepad, (unsigned)Bindings_GetContext());

        memcpy(DefKeys, savedKeyboard, sizeof(savedKeyboard));
        memcpy(GamepadKeys, savedGamepad, sizeof(savedGamepad));
        Bindings_SetContext(mode ^ BINDINGS_CONTEXT_MODAL_CIRCLE);
        U32 playerContext = Bindings_GetContext();
        ASSERT_TRUE(RecFmt_ParseBindings(header, "bindings.keyboard=", DefKeys, MAX_INPUT_SLOTS));
        ASSERT_TRUE(RecFmt_ParseBindings(header, "bindings.gamepad=", GamepadKeys, MAX_INPUT_SLOTS));
        U32 context = 99;
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
        Bindings_SetContext(playerContext);
        ASSERT_EQ_UINT(playerContext, Bindings_GetContext());
        ASSERT_MEM_EQ(savedKeyboard, DefKeys, sizeof(savedKeyboard));
        ASSERT_MEM_EQ(savedGamepad, GamepadKeys, sizeof(savedGamepad));
    }
    HoldNone();
    Bindings_SetContext(savedContext);
}

static void test_restored_release_latch() {
    HoldNone();
    ClearNoRepeatInput();
    RestoreInput();
    GamepadKeys[9].Key1 = GamepadKeys[9].Key2 = 0;
    GamepadKeys[13].Key1 = K_GAMEPAD_B;
    GamepadKeys[13].Key2 = 0;
    U32 savedContext = Bindings_GetContext();
    for (U32 context = 0; context <= BINDINGS_CONTEXT_MASK; ++context) {
        Bindings_SetContext(context);
        HoldOnly(K_GAMEPAD_B);
        GetInput(0);
        U32 expected = I_ESQUIVE;
        if (context & BINDINGS_CONTEXT_MODAL_CIRCLE) {
            if (context & BINDINGS_CONTEXT_CIRCLE_SUPPRESSED)
                expected = 0;
            else if (!(context & BINDINGS_CONTEXT_GAMEPLAY))
                expected = I_RETURN;
        }
        ASSERT_EQ_UINT(expected, Input);
    }
    HoldNone();
    Bindings_SetContext(savedContext);
    RestoreInput();
}

int main() {
    U32 compiledContext = Bindings_GetContext();
    RUN_TEST(test_context_metadata);
    Bindings_SetContext(compiledContext);
    RUN_TEST(test_cross_layout_replay);
    RUN_TEST(test_restored_release_latch);
    Bindings_SetContext(compiledContext);
    TEST_SUMMARY();
    return test_failures != 0;
}
