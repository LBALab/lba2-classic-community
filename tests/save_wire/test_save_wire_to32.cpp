/* Writer-side round-trip (docs/SAVE_WIRE_PLAN.md, layer 4b).
 *
 * Contract exercised here:
 *   native -> ...ToWire32 -> ...FromWire32 -> native' , with native' == native for
 *   every round-trippable field, and the discarded wire pointer slots emitted as 0.
 * The ...ToWire32 writers are the exact inverses of the ...FromWire32 readers that
 * test_save_wire_from32 pins against hand-derived golden vectors.
 */

#include "SAVEGAME_WIRE.H" /* pulls native T_OBJ_3D / T_EXTRA / S_PART_FLOW */
#include "test_harness.h"

#include <stdint.h>
#include <string.h>

static void test_obj3d_roundtrip(void) {
    T_OBJ_3D a;
    memset(&a, 0, sizeof(a));
    a.X = 111;
    a.Z = -222;
    a.Body.Num = 7;
    a.NextBody.Num = 8;
    a.Anim.Num = 9;
    a.LastFrame = 42;
    a.LastOfsFrame = (PTR_U32)(uintptr_t)0x0BADF00Du; /* preserved: low 32 bits */
    a.NextOfsFrame = (PTR_U32)(uintptr_t)0x0000BEEFu;
    a.LoopOfsFrame = 0x1234;
    a.NbFrames = 30;
    a.Status = 0xDEADBEEF;
    a.NbGroups = 12;

    T_OBJ_3D_WIRE32 w;
    SavegameObj3dToWire32(&a, &w);

    /* Discarded-on-read pointer slots are zeroed on the wire (deterministic). */
    ASSERT_EQ_UINT(0u, w.Texture);
    ASSERT_EQ_UINT(0u, w.NextTexture);

    T_OBJ_3D b;
    SavegameObj3dFromWire32(&w, &b);
    ASSERT_EQ_INT(a.X, b.X);
    ASSERT_EQ_INT(a.Z, b.Z);
    ASSERT_EQ_INT(a.Body.Num, b.Body.Num);
    ASSERT_EQ_INT(a.Anim.Num, b.Anim.Num);
    ASSERT_EQ_INT(a.LastFrame, b.LastFrame);
    ASSERT_EQ_UINT(a.LoopOfsFrame, b.LoopOfsFrame);
    ASSERT_EQ_UINT(a.Status, b.Status);
    ASSERT_EQ_UINT(a.NbGroups, b.NbGroups);
    /* Preserved pointer-typed offsets round-trip via their low 32 bits. */
    ASSERT_EQ_UINT(0x0BADF00Du, (uint32_t)(uintptr_t)b.LastOfsFrame);
    ASSERT_EQ_UINT(0x0000BEEFu, (uint32_t)(uintptr_t)b.NextOfsFrame);
    /* Discarded pointers come back NULL. */
    ASSERT_TRUE(b.Texture == NULL);
}

static void test_extra_roundtrip(void) {
    T_EXTRA a;
    memset(&a, 0, sizeof(a));
    a.PosX = 5;
    a.PosZ = -9;
    a.U.Org.Y = 77;
    a.Info = 3;
    a.Sprite = 21;
    a.Flags = 0xCAFE;
    a.Scale = 0x4000;

    T_EXTRA_WIRE32 w;
    SavegameExtraToWire32(&a, &w);
    ASSERT_EQ_UINT(0u, w.PtrBody_w32);

    T_EXTRA b;
    SavegameExtraFromWire32(&w, &b);
    ASSERT_EQ_INT(a.PosX, b.PosX);
    ASSERT_EQ_INT(a.PosZ, b.PosZ);
    ASSERT_EQ_INT(a.U.Org.Y, b.U.Org.Y);
    ASSERT_EQ_INT(a.Sprite, b.Sprite);
    ASSERT_EQ_UINT(a.Flags, b.Flags);
    ASSERT_EQ_INT(a.Scale, b.Scale);
}

static void test_flow_roundtrip(void) {
    S_PART_FLOW a;
    memset(&a, 0, sizeof(a));
    a.Flag = 1;
    a.NbDot = 4;
    a.OrgX = 100;
    a.ZMax = -50;
    a.FlowTimerStart = 0x9000;

    S_PART_FLOW_WIRE32 w;
    SavegameFlowToWire32(&a, &w);
    ASSERT_EQ_UINT(0u, w.PtrListDot_w32);

    S_PART_FLOW b;
    memset(&b, 0, sizeof(b));
    SavegameFlowFromWire32(&w, &b);
    ASSERT_EQ_INT(a.Flag, b.Flag);
    ASSERT_EQ_INT(a.NbDot, b.NbDot);
    ASSERT_EQ_INT(a.OrgX, b.OrgX);
    ASSERT_EQ_INT(a.ZMax, b.ZMax);
    ASSERT_EQ_UINT(a.FlowTimerStart, b.FlowTimerStart);
}

int main(void) {
    RUN_TEST(test_obj3d_roundtrip);
    RUN_TEST(test_extra_roundtrip);
    RUN_TEST(test_flow_roundtrip);
    TEST_SUMMARY();
    return test_failures != 0;
}
