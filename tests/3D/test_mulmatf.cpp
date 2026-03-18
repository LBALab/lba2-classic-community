/* Test: MulMatrixF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/MULMAT.H>
#include <string.h>
#include <stdlib.h>

extern void MulMatrixF(TYPE_MAT *d, TYPE_MAT *s1, TYPE_MAT *s2);

extern "C" void asm_MulMatrixF(void);
static void call_asm_MulMatrixF(TYPE_MAT *d, TYPE_MAT *s1, TYPE_MAT *s2) {
    __asm__ __volatile__("call asm_MulMatrixF" : : "D"(d), "S"(s1), "b"(s2) : "memory", "eax");
}

static void make_rand_mat(TYPE_MAT *m) {
    float *f = &m->F.M11;
    for (int j=0;j<9;j++) f[j]=(float)((S32)rand()-RAND_MAX/2)/1000.f;
    m->F.TX=m->F.TY=m->F.TZ=0;
}

static void test_equivalence(void)
{
    TYPE_MAT a,b,cm,am;
    memset(&a,0,sizeof(a)); memset(&b,0,sizeof(b));
    a.F.M11=1.5f;a.F.M12=2.3f;a.F.M13=0.7f;
    a.F.M21=0.1f;a.F.M22=3.2f;a.F.M23=1.1f;
    a.F.M31=2.0f;a.F.M32=0.5f;a.F.M33=4.0f;
    b.F.M11=0.9f;b.F.M12=1.2f;b.F.M13=0.3f;
    b.F.M21=2.1f;b.F.M22=0.4f;b.F.M23=1.5f;
    b.F.M31=0.6f;b.F.M32=3.0f;b.F.M33=0.8f;
    MulMatrixF(&cm,&a,&b);
    call_asm_MulMatrixF(&am,&a,&b);
    for (int j=0;j<9;j++)
        ASSERT_ASM_CPP_NEAR_F((&am.F.M11)[j], (&cm.F.M11)[j], 0.01f, "MulMatrixF");
}

static void test_random_equivalence(void)
{
    srand(42);
    for (int i = 0; i < 10000; i++) {
        TYPE_MAT a,b,cm,am;
        make_rand_mat(&a); make_rand_mat(&b);
        MulMatrixF(&cm,&a,&b);
        call_asm_MulMatrixF(&am,&a,&b);
        for (int j=0;j<9;j++)
            ASSERT_ASM_CPP_NEAR_F((&am.F.M11)[j], (&cm.F.M11)[j], 0.01f, "MulMatrixF rand");
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
