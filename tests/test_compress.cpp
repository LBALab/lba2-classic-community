#include "test_harness.h"

#include "LZSS.H"

#include <string.h>

int AddString();
void DeleteString(int p);
void InitTree(int r);

extern "C" int asm_AddString();
extern "C" void asm_DeleteString(int p);

struct CompressState {
    int current_position;
    int match_position;
    unsigned char window_snapshot[WINDOW_SIZE * 5];
    deftree tree_snapshot[WINDOW_SIZE + 2];
};

static unsigned int rng_state;

static void rng_seed(unsigned int seed_value) {
    rng_state = seed_value;
}

static unsigned int rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void reset_dictionary(void) {
    int index;

    Current_position = 0;
    Match_position = 0;
    memset(window, 0, sizeof(window));
    for (index = 0; index < WINDOW_SIZE + 2; ++index) {
        tree[index].parent = UNUSED;
        tree[index].smaller_child = UNUSED;
        tree[index].larger_child = UNUSED;
    }
}

static void capture_state(CompressState *state) {
    state->current_position = Current_position;
    state->match_position = Match_position;
    memcpy(state->window_snapshot, window, sizeof(window));
    memcpy(state->tree_snapshot, tree, sizeof(tree));
}

static void restore_state(const CompressState *state) {
    Current_position = state->current_position;
    Match_position = state->match_position;
    memcpy(window, state->window_snapshot, sizeof(window));
    memcpy(tree, state->tree_snapshot, sizeof(tree));
}

static void fill_window_random(void) {
    unsigned int index;

    for (index = 0; index < sizeof(window); ++index) {
        window[index] = (unsigned char)rng_next();
    }
}

static void expect_addstring_equivalent(const CompressState *initial_state) {
    CompressState cpp_state;
    CompressState asm_state;
    int cpp_result;
    int asm_result;

    restore_state(initial_state);
    cpp_result = AddString();
    capture_state(&cpp_state);

    restore_state(initial_state);
    asm_result = asm_AddString();
    capture_state(&asm_state);

    ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, "AddString return");
    ASSERT_ASM_CPP_EQ_INT(asm_state.current_position, cpp_state.current_position, "Current_position after AddString");
    ASSERT_ASM_CPP_EQ_INT(asm_state.match_position, cpp_state.match_position, "Match_position after AddString");
    ASSERT_ASM_CPP_MEM_EQ(asm_state.window_snapshot, cpp_state.window_snapshot, sizeof(cpp_state.window_snapshot), "window after AddString");
    ASSERT_ASM_CPP_MEM_EQ(asm_state.tree_snapshot, cpp_state.tree_snapshot, sizeof(cpp_state.tree_snapshot), "tree after AddString");
}

static void run_addstring_equivalent(const CompressState *initial_state,
                                     const char *label,
                                     CompressState *next_state) {
    CompressState cpp_state;
    CompressState asm_state;
    int cpp_result;
    int asm_result;

    restore_state(initial_state);
    cpp_result = AddString();
    capture_state(&cpp_state);

    restore_state(initial_state);
    asm_result = asm_AddString();
    capture_state(&asm_state);

    ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, label);
    ASSERT_ASM_CPP_EQ_INT(asm_state.current_position, cpp_state.current_position, label);
    ASSERT_ASM_CPP_EQ_INT(asm_state.match_position, cpp_state.match_position, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_state.window_snapshot, cpp_state.window_snapshot, sizeof(cpp_state.window_snapshot), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_state.tree_snapshot, cpp_state.tree_snapshot, sizeof(cpp_state.tree_snapshot), label);

    memcpy(next_state, &cpp_state, sizeof(*next_state));
}

static void expect_deletestring_equivalent(const CompressState *initial_state, int node) {
    CompressState cpp_state;
    CompressState asm_state;

    restore_state(initial_state);
    DeleteString(node);
    capture_state(&cpp_state);

    restore_state(initial_state);
    asm_DeleteString(node);
    capture_state(&asm_state);

    ASSERT_ASM_CPP_EQ_INT(asm_state.current_position, cpp_state.current_position, "Current_position after DeleteString");
    ASSERT_ASM_CPP_EQ_INT(asm_state.match_position, cpp_state.match_position, "Match_position after DeleteString");
    ASSERT_ASM_CPP_MEM_EQ(asm_state.window_snapshot, cpp_state.window_snapshot, sizeof(cpp_state.window_snapshot), "window after DeleteString");
    ASSERT_ASM_CPP_MEM_EQ(asm_state.tree_snapshot, cpp_state.tree_snapshot, sizeof(cpp_state.tree_snapshot), "tree after DeleteString");
}

static void run_deletestring_equivalent(const CompressState *initial_state,
                                        int node,
                                        const char *label,
                                        CompressState *next_state) {
    CompressState cpp_state;
    CompressState asm_state;

    restore_state(initial_state);
    DeleteString(node);
    capture_state(&cpp_state);

    restore_state(initial_state);
    asm_DeleteString(node);
    capture_state(&asm_state);

    ASSERT_ASM_CPP_EQ_INT(asm_state.current_position, cpp_state.current_position, label);
    ASSERT_ASM_CPP_EQ_INT(asm_state.match_position, cpp_state.match_position, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_state.window_snapshot, cpp_state.window_snapshot, sizeof(cpp_state.window_snapshot), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_state.tree_snapshot, cpp_state.tree_snapshot, sizeof(cpp_state.tree_snapshot), label);

    memcpy(next_state, &cpp_state, sizeof(*next_state));
}

static void prepare_add_leaf_state(CompressState *state) {
    reset_dictionary();
    window[100] = 1;
    window[101] = 2;
    Current_position = 100;
    InitTree(Current_position);
    Current_position = 101;
    capture_state(state);
}

static void prepare_full_match_state(CompressState *state) {
    unsigned int index;

    reset_dictionary();
    for (index = 0; index < LOOK_AHEAD_SIZE + 2; ++index) {
        window[200 + index] = 0x5A;
    }
    Current_position = 200;
    InitTree(Current_position);
    Current_position = 201;
    capture_state(state);
}

static void prepare_delete_leaf_state(CompressState *state, int *node) {
    reset_dictionary();
    window[100] = 1;
    window[101] = 2;
    Current_position = 100;
    InitTree(Current_position);
    Current_position = 101;
    AddString();
    *node = 101;
    capture_state(state);
}

static void prepare_delete_two_children_state(CompressState *state, int *node) {
    reset_dictionary();
    tree[TREE_ROOT].parent = UNUSED;
    tree[TREE_ROOT].smaller_child = UNUSED;
    tree[TREE_ROOT].larger_child = 200;

    tree[200].parent = TREE_ROOT;
    tree[200].smaller_child = 150;
    tree[200].larger_child = 250;

    tree[150].parent = 200;
    tree[150].smaller_child = UNUSED;
    tree[150].larger_child = UNUSED;

    tree[250].parent = 200;
    tree[250].smaller_child = UNUSED;
    tree[250].larger_child = UNUSED;

    Current_position = 0;
    Match_position = 0;
    *node = 200;
    capture_state(state);
}

static void prepare_random_add_state(unsigned int seed_value, CompressState *state) {
    int root;
    int insert_count;
    int step;

    rng_seed(seed_value);
    reset_dictionary();
    fill_window_random();

    root = (int)rng_next() % WINDOW_SIZE;
    Current_position = root;
    InitTree(Current_position);

    Current_position = MOD_WINDOW(root + 1);
    insert_count = 1 + ((int)rng_next() % 32);
    for (step = 0; step < insert_count; ++step) {
        AddString();
        Current_position = MOD_WINDOW(Current_position + 1);
    }

    capture_state(state);
}

static void prepare_random_delete_state(unsigned int seed_value, CompressState *state, int *node) {
    int candidates[WINDOW_SIZE];
    int candidate_count;
    int root;
    int insert_count;
    int step;
    int index;

    rng_seed(seed_value);
    reset_dictionary();
    fill_window_random();

    root = (int)rng_next() % WINDOW_SIZE;
    Current_position = root;
    InitTree(Current_position);

    Current_position = MOD_WINDOW(root + 1);
    insert_count = 4 + ((int)rng_next() % 40);
    for (step = 0; step < insert_count; ++step) {
        AddString();
        Current_position = MOD_WINDOW(Current_position + 1);
    }

    candidate_count = 0;
    for (index = 0; index < WINDOW_SIZE; ++index) {
        if (tree[index].parent != UNUSED) {
            candidates[candidate_count++] = index;
        }
    }

    *node = candidates[(int)rng_next() % candidate_count];
    capture_state(state);
}

static void prepare_pipeline_state(unsigned int seed_value, CompressState *state) {
    int root;
    int insert_count;
    int step;

    rng_seed(seed_value);
    reset_dictionary();
    fill_window_random();

    root = (int)rng_next() % WINDOW_SIZE;
    Current_position = root;
    InitTree(Current_position);

    Current_position = MOD_WINDOW(root + 1);
    insert_count = LOOK_AHEAD_SIZE + 32 + ((int)rng_next() % 16);
    for (step = 0; step < insert_count; ++step) {
        AddString();
        Current_position = MOD_WINDOW(Current_position + 1);
    }

    capture_state(state);
}

static void test_addstring_inserts_leaf(void) {
    CompressState initial_state;

    prepare_add_leaf_state(&initial_state);
    expect_addstring_equivalent(&initial_state);
}

static void test_addstring_replaces_full_match(void) {
    CompressState initial_state;

    prepare_full_match_state(&initial_state);
    expect_addstring_equivalent(&initial_state);
}

static void test_deletestring_removes_leaf(void) {
    CompressState initial_state;
    int node;

    prepare_delete_leaf_state(&initial_state, &node);
    expect_deletestring_equivalent(&initial_state, node);
}

static void test_deletestring_removes_two_child_node(void) {
    CompressState initial_state;
    int node;

    prepare_delete_two_children_state(&initial_state, &node);
    expect_deletestring_equivalent(&initial_state, node);
}

static void test_addstring_random_stress(void) {
    CompressState initial_state;
    int round;

    for (round = 0; round < 300; ++round) {
        prepare_random_add_state(0xDEADBEEFu + (unsigned int)round, &initial_state);
        expect_addstring_equivalent(&initial_state);
    }
}

static void test_deletestring_random_stress(void) {
    CompressState initial_state;
    int node;
    int round;

    for (round = 0; round < 150; ++round) {
        prepare_random_delete_state(0x0BADCAFEu + (unsigned int)round, &initial_state, &node);
        expect_deletestring_equivalent(&initial_state, node);
    }
}

static void test_addstring_deletestring_mixed_stress(void) {
    CompressState state;
    CompressState deleted_state;
    CompressState next_state;

    for (int round = 0; round < 80; ++round) {
        unsigned int sequence_seed = 0x13572468u + (unsigned int)round;
        int operations = 8 + (int)(sequence_seed % 5u);

        prepare_pipeline_state(0x2468ACE0u + (unsigned int)round, &state);
        rng_seed(sequence_seed);

        for (int step = 0; step < operations; ++step) {
            int delete_node = MOD_WINDOW(state.current_position + LOOK_AHEAD_SIZE);
            char delete_label[128];
            char add_label[128];

            snprintf(delete_label, sizeof(delete_label),
                     "DeleteString pipeline round=%d step=%d node=%d",
                     round, step, delete_node);
            run_deletestring_equivalent(&state, delete_node, delete_label, &deleted_state);

            deleted_state.window_snapshot[delete_node] = (unsigned char)rng_next();

            snprintf(add_label, sizeof(add_label),
                     "AddString pipeline round=%d step=%d pos=%d",
                     round, step, deleted_state.current_position);
            run_addstring_equivalent(&deleted_state, add_label, &next_state);
            next_state.current_position = MOD_WINDOW(next_state.current_position + 1);

            memcpy(&state, &next_state, sizeof(state));
        }
    }
}

int main(void) {
    RUN_TEST(test_addstring_inserts_leaf);
    RUN_TEST(test_addstring_replaces_full_match);
    RUN_TEST(test_deletestring_removes_leaf);
    RUN_TEST(test_deletestring_removes_two_child_node);
    RUN_TEST(test_addstring_random_stress);
    RUN_TEST(test_deletestring_random_stress);
    RUN_TEST(test_addstring_deletestring_mixed_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}