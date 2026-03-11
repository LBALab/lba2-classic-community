/* Test: LightList - ASM vs CPP equivalence */
#include "test_harness.h"
#include <3D/LITLISTF.H>
#include <3D/LIGHT.H>
#include <3D/LIROT3D.H>
#include <3D/CAMERA.H>
#include <string.h>
#include <stdlib.h>

/* ASM LightListF uses Watcom register convention:
   parm [ebx] [edi] [esi] [ecx], modify [eax ebx edx] */
extern "C" void asm_LightListF(void);
static void call_asm_LightListF(TYPE_MAT *m, U16 *d, TYPE_VT16 *s, S32 n) {
    __asm__ __volatile__(
        "call asm_LightListF"
        : "+b"(m), "+D"(d), "+S"(s), "+c"(n)
        :
        : "memory", "eax", "edx");
}

static void test_equivalence(void)
{
    TYPE_MAT m; memset(&m,0,sizeof(m));
    m.F.M11=1;m.F.M22=1;m.F.M33=1;
    CameraXLight=50;CameraYLight=50;CameraZLight=50;FactorLight=0.01f;
    TYPE_VT16 src[3]={{100,0,0,0},{0,100,0,0},{0,0,100,0}};
    U16 dc[3],da[3];
    memset(dc,0,sizeof(dc)); LightList(&m,dc,src,3);
    memset(da,0,sizeof(da)); call_asm_LightListF(&m,da,src,3);
    ASSERT_ASM_CPP_MEM_EQ(da,dc,sizeof(dc),"LightList");
}

static void test_random_equivalence(void)
{
    srand(42);
    TYPE_MAT m; memset(&m,0,sizeof(m));
    m.F.M11=1;m.F.M22=1;m.F.M33=1;
    FactorLight=0.01f;
    for (int i=0;i<10000;i++) {
        CameraXLight=rand()%200-100;
        CameraYLight=rand()%200-100;
        CameraZLight=rand()%200-100;
        TYPE_VT16 s;
        s.X=(S16)(rand()%200-100); s.Y=(S16)(rand()%200-100); s.Z=(S16)(rand()%200-100); s.Grp=0;
        U16 dc,da;
        dc=0;LightList(&m,&dc,&s,1);
        da=0;call_asm_LightListF(&m,&da,&s,1);
        /* Allow ±1 tolerance: the ASM pipeline (inverse rotation → dot
           product → FactorLight → fistp) keeps all intermediates in the
           x87 register stack, while the CPP version stores/reloads X0/Y0/Z0
           as integers between the two stages, causing occasional ±1
           rounding differences at the final fistp. */
        S32 diff = (S32)da - (S32)dc;
        if (diff < -1 || diff > 1) {
            char lbl[64];
            snprintf(lbl,sizeof(lbl),"LightList v=%d,%d,%d",s.X,s.Y,s.Z);
            ASSERT_ASM_CPP_EQ_INT(da,dc,lbl);
        }
    }
}

int main(void)
{
    RUN_TEST(test_equivalence);
    RUN_TEST(test_random_equivalence);
    TEST_SUMMARY();
    return test_failures != 0;
}
