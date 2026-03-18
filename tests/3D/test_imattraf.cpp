/* Test: InitMatrixTransF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/IMATTRA.H>
#include <string.h>
#include <stdlib.h>

extern void InitMatrixTransF(TYPE_MAT *m, S32 tx, S32 ty, S32 tz);

extern "C" void asm_InitMatrixTransF(void);
static void call_asm_InitMatrixTransF(TYPE_MAT *m, S32 tx, S32 ty, S32 tz) {
    __asm__ __volatile__("call asm_InitMatrixTransF" : : "D"(m), "a"(tx), "b"(ty), "c"(tz) : "memory");
}

static U32 float_bits(float value)
{
    U32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void initmatrixtrans_buggy_integer_overlay(TYPE_MAT *m, S32 tx, S32 ty, S32 tz)
{
    memset(m, 0, sizeof(*m));
    m->I.TX = tx;
    m->I.TY = ty;
    m->I.TZ = tz;
}

static void test_equivalence(void)
{
    struct { S32 tx,ty,tz; } cases[] = {
        {0,0,0}, {100,200,300}, {-500,-1000,-2000}, {2147483647,-2147483647,0},
    };
    for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        TYPE_MAT cm, am;
        memset(&cm,0,sizeof(cm)); memset(&am,0,sizeof(am));
        InitMatrixTransF(&cm, cases[i].tx, cases[i].ty, cases[i].tz);
        call_asm_InitMatrixTransF(&am, cases[i].tx, cases[i].ty, cases[i].tz);
        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixTransF");
    }
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        S32 tx=(S32)rand()-RAND_MAX/2, ty=(S32)rand()-RAND_MAX/2, tz=(S32)rand()-RAND_MAX/2;
        TYPE_MAT cm, am;
        memset(&cm,0,sizeof(cm)); memset(&am,0,sizeof(am));
        InitMatrixTransF(&cm, tx, ty, tz);
        call_asm_InitMatrixTransF(&am, tx, ty, tz);
        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixTransF rand");
    }
}

static void test_translation_slots_use_float_representation(void)
{
    struct { S32 tx, ty, tz; } cases[] = {
        {1, -2, 1024},
        {7, 255, -4096},
        {-1234567, 7654321, 32768},
    };

    for (int i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        TYPE_MAT cm;
        TYPE_MAT am;
        TYPE_MAT buggy;

        memset(&cm, 0xCC, sizeof(cm));
        memset(&am, 0xCC, sizeof(am));

        InitMatrixTransF(&cm, cases[i].tx, cases[i].ty, cases[i].tz);
        call_asm_InitMatrixTransF(&am, cases[i].tx, cases[i].ty, cases[i].tz);
        initmatrixtrans_buggy_integer_overlay(&buggy, cases[i].tx, cases[i].ty, cases[i].tz);

        ASSERT_ASM_CPP_MEM_EQ(&am, &cm, sizeof(TYPE_MAT), "InitMatrixTransF float translation slots");
        ASSERT_EQ_UINT(float_bits((float)cases[i].tx), (U32)cm.I.TX);
        ASSERT_EQ_UINT(float_bits((float)cases[i].ty), (U32)cm.I.TY);
        ASSERT_EQ_UINT(float_bits((float)cases[i].tz), (U32)cm.I.TZ);
        ASSERT_EQ_UINT((U32)cm.I.TX, (U32)am.I.TX);
        ASSERT_EQ_UINT((U32)cm.I.TY, (U32)am.I.TY);
        ASSERT_EQ_UINT((U32)cm.I.TZ, (U32)am.I.TZ);
        ASSERT_TRUE((U32)buggy.I.TX != (U32)cm.I.TX);
        ASSERT_TRUE((U32)buggy.I.TY != (U32)cm.I.TY);
        ASSERT_TRUE((U32)buggy.I.TZ != (U32)cm.I.TZ);
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    RUN_TEST(test_translation_slots_use_float_representation);
    TEST_SUMMARY();
    return test_failures != 0;
}
