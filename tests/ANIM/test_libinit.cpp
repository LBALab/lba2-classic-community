/* Test: InitObjects / ClearObjects — animation library state init */
#include "test_harness.h"

#include <ANIM/LIBINIT.H>
#include <string.h>
#include <stdlib.h>

#ifdef LBA2_ASM_TESTS
extern "C" void asm_InitObjects(void *buffer, U32 size, Func_TransNumPtr *fctbody, Func_TransNumPtr *fctanim);
#endif

static void *dummy_trans(S32 num) { (void)num; return NULL; }

static void test_init_with_buffer(void)
{
    U8 buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    InitObjects(buffer, sizeof(buffer), (Func_TransNumPtr *)dummy_trans, (Func_TransNumPtr *)dummy_trans);
    ASSERT_TRUE(Lib3DBufferAnim == buffer);
    ASSERT_TRUE(PtrLib3DBufferAnim == buffer);
}

static void test_init_with_null(void)
{
    /* NULL buffer → malloc internally. ClearObjects is internal (no header decl),
       so cleanup happens via atexit. */
    InitObjects(NULL, 8192, (Func_TransNumPtr *)dummy_trans, (Func_TransNumPtr *)dummy_trans);
    ASSERT_TRUE(Lib3DBufferAnim != NULL);
}

#ifdef LBA2_ASM_TESTS
static void test_asm_equiv(void)
{
    U8 buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    asm_InitObjects(buffer, sizeof(buffer), (Func_TransNumPtr *)dummy_trans, (Func_TransNumPtr *)dummy_trans);
    U8 *asm_buf = Lib3DBufferAnim;
    U8 *asm_ptr = PtrLib3DBufferAnim;

    InitObjects(buffer, sizeof(buffer), (Func_TransNumPtr *)dummy_trans, (Func_TransNumPtr *)dummy_trans);
    ASSERT_ASM_CPP_EQ_INT((long long)(intptr_t)asm_buf, (long long)(intptr_t)Lib3DBufferAnim, "InitObjects Lib3DBufferAnim");
    ASSERT_ASM_CPP_EQ_INT((long long)(intptr_t)asm_ptr, (long long)(intptr_t)PtrLib3DBufferAnim, "InitObjects PtrLib3DBufferAnim");
}
#endif

int main(void)
{
    RUN_TEST(test_init_with_buffer);
    RUN_TEST(test_init_with_null);
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_asm_equiv);
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
