/* Test: ProjectList3DF - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/PROJ.H>
#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <string.h>
#include <stdlib.h>

extern void ProjectList3DF(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz);
#ifdef LBA2_ASM_TESTS
extern "C" void asm_ProjectList3DF(void);
static void call_asm_ProjectList3DF(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz) {
    S32 _a, _b, _d;
    __asm__ __volatile__(
        "pushl %%edx\n\t"
        "pushl %%ebx\n\t"
        "pushl %%eax\n\t"
        "call asm_ProjectList3DF\n\t"
        "addl $12, %%esp"
        : "=a"(_a), "=b"(_b), "=d"(_d)
        : "0"(ox), "1"(oy), "2"(oz), "D"(d), "S"(s), "c"(n) : "memory");
}

static void test_equivalence(void)
{
    SetProjection(320,240,1,300,300);
    TYPE_VT16 src[3]={{0,0,-50,0},{10,10,-100,0},{-10,-10,-200,0}};
    TYPE_PT dc[3],da[3];
    ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
    ProjectList3DF(dc,src,3,0,0,0);
    ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
    call_asm_ProjectList3DF(da,src,3,0,0,0);
    ASSERT_ASM_CPP_MEM_EQ(da,dc,sizeof(dc),"ProjectList3DF");
}

static void test_random_equivalence(void)
{
    srand(42);
    SetProjection(320,240,1,300,300);
    for (int i=0;i<10000;i++) {
        TYPE_VT16 s;
        s.X=(S16)(rand()%600-300); s.Y=(S16)(rand()%600-300); s.Z=(S16)(-(rand()%500+1)); s.Grp=0;
        TYPE_PT dc,da;
        ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
        ProjectList3DF(&dc,&s,1,0,0,0);
        ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
        call_asm_ProjectList3DF(&da,&s,1,0,0,0);
        ASSERT_ASM_CPP_MEM_EQ(&da,&dc,sizeof(dc),"PL3DF rand");
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
