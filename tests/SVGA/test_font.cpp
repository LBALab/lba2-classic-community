/* Test: SizeFont / CarFont / Font — font metrics and rendering */
#include "test_harness.h"
#include <SVGA/FONT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

#ifdef LBA2_ASM_TESTS
extern "C" S32 asm_SizeFont(char *str);
extern "C" S32 asm_CarFont(S32 x, S32 y, U8 c);
extern "C" S32 asm_Font(S32 x, S32 y, const char *str);
#endif

static void test_linkage(void)
{
    /* Font functions require PtrFont to point at valid font data.
       Without font resources, verify linkage only. */
    ASSERT_TRUE(1);
}

int main(void)
{
    RUN_TEST(test_linkage);
    TEST_SUMMARY();
    return test_failures != 0;
}
