/* Test: RotTransListF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/ROTRALIS.H>
#include <string.h>
#include <stdlib.h>

extern void RotTransListF(TYPE_MAT *m, TYPE_VT16 *d, TYPE_VT16 *s, S32 n);
#ifdef LBA2_ASM_TESTS
extern "C" void asm_RotTransListF(void);
static void call_asm_RotTransListF(TYPE_MAT *m, TYPE_VT16 *d, TYPE_VT16 *s, S32 n) {
    __asm__ __volatile__("call asm_RotTransListF"
        : "+c"(n), "+S"(s), "+D"(d) : "b"(m) : "memory");
}

static void test_equivalence(void)
{
    TYPE_MAT m; memset(&m,0,sizeof(m));
    m.F.M11=0.5f;m.F.M12=0.3f;m.F.M13=0.1f;
    m.F.M21=0.2f;m.F.M22=0.8f;m.F.M23=0.4f;
    m.F.M31=0.7f;m.F.M32=0.6f;m.F.M33=0.9f;
    m.F.TX=5;m.F.TY=10;m.F.TZ=15;
    TYPE_VT16 src[3]={{100,200,300,0},{-50,0,50,0},{0,0,0,0}};
    TYPE_VT16 dc[3],da[3];
    memset(dc,0,sizeof(dc)); RotTransListF(&m,dc,src,3);
    memset(da,0,sizeof(da)); call_asm_RotTransListF(&m,da,src,3);
    ASSERT_ASM_CPP_MEM_EQ(da,dc,sizeof(dc),"RotTransListF");
}

static void test_random_equivalence(void)
{
    srand(42);
    TYPE_MAT m; memset(&m,0,sizeof(m));
    m.F.M11=0.5f;m.F.M12=0.3f;m.F.M13=0.1f;
    m.F.M21=0.2f;m.F.M22=0.8f;m.F.M23=0.4f;
    m.F.M31=0.7f;m.F.M32=0.6f;m.F.M33=0.9f;
    for (int i=0;i<10000;i++) {
        m.F.TX=(float)(rand()%200-100);
        m.F.TY=(float)(rand()%200-100);
        m.F.TZ=(float)(rand()%200-100);
        TYPE_VT16 src;
        src.X=(S16)(rand()%1000-500); src.Y=(S16)(rand()%1000-500); src.Z=(S16)(rand()%1000-500); src.Grp=0;
        TYPE_VT16 dc,da;
        memset(&dc,0,sizeof(dc)); RotTransListF(&m,&dc,&src,1);
        memset(&da,0,sizeof(da)); call_asm_RotTransListF(&m,&da,&src,1);
        ASSERT_ASM_CPP_MEM_EQ(&da,&dc,sizeof(dc),"RotTransListF rand");
    }
}
#endif

int main(void)
{
#ifdef LBA2_ASM_TESTS
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
#else
    printf("SKIPPED - ASM tests not enabled\n");
#endif
    TEST_SUMMARY();
    return test_failures != 0;
}
