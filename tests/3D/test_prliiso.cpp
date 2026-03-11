/* Test: ProjectListIso - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LPROJ.H>
#include <3D/CAMERA.H>
#include <SVGA/SCREENXY.H>
#include <string.h>
#include <stdlib.h>

extern void ProjectListIso(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz);
extern "C" void asm_ProjectListIso(void);
static void call_asm_ProjectListIso(TYPE_PT *d, TYPE_VT16 *s, S32 n, S32 ox, S32 oy, S32 oz) {
    S32 _a, _b, _d;
    __asm__ __volatile__(
        "pushl %%edx\n\t"
        "pushl %%ebx\n\t"
        "pushl %%eax\n\t"
        "call asm_ProjectListIso\n\t"
        "addl $12, %%esp"
        : "=a"(_a), "=b"(_b), "=d"(_d)
        : "0"(ox), "1"(oy), "2"(oz), "D"(d), "S"(s), "c"(n) : "memory");
}

static void test_equivalence(void)
{
    XCentre=320;YCentre=240;
    TYPE_VT16 src[3]={{0,0,0,0},{100,50,100,0},{-100,-50,-100,0}};
    TYPE_PT dc[3],da[3];
    ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
    ProjectListIso(dc,src,3,0,0,0);
    ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
    call_asm_ProjectListIso(da,src,3,0,0,0);
    ASSERT_ASM_CPP_MEM_EQ(da,dc,sizeof(dc),"ProjectListIso");
}

static void test_random_equivalence(void)
{
    srand(42);
    XCentre=320;YCentre=240;
    for (int i=0;i<10000;i++) {
        TYPE_VT16 s;
        s.X=(S16)(rand()%600-300); s.Y=(S16)(rand()%600-300); s.Z=(S16)(rand()%600-300); s.Grp=0;
        TYPE_PT dc,da;
        ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
        ProjectListIso(&dc,&s,1,0,0,0);
        ScreenXMin=32767;ScreenXMax=-32767;ScreenYMin=32767;ScreenYMax=-32767;
        call_asm_ProjectListIso(&da,&s,1,0,0,0);
        ASSERT_ASM_CPP_MEM_EQ(&da,&dc,sizeof(dc),"PLIso rand");
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
