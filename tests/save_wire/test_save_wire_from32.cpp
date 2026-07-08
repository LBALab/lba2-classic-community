/* Phase 1 anchor (docs/SAVE_WIRE_PLAN.md, layer 4a): pin the 32-bit retail wire
 * layout of the three pointer-bearing save structs, independently of the wire
 * structs themselves, and prove the ...FromWire32 readers decode that layout to
 * the right native fields.  This is the non-circular anchor that makes ...FromWire32
 * a trustworthy round-trip partner for the Phase 2 ...ToWire32 writers.
 *
 * No retail data: the golden vectors are the byte pattern buf[i] = i, so a
 * field at wire offset o decodes to the little-endian value of bytes o..o+3.
 * Supported platforms are all little-endian, matching the on-disk format. */

#include "SAVEGAME_WIRE.H" /* pulls native T_OBJ_3D / T_EXTRA / S_PART_FLOW */
#include "test_harness.h"

#include <cstddef>
#include <stdint.h>
#include <string.h>

/* ── layer 4a-i: static size / offset locks, justified by the derivation table
 * in docs/SAVE_WIRE_PLAN.md (140 scalar prefix + 136 wire Obj = 276 stride). ── */
static_assert(sizeof(T_OBJ_3D_WIRE32) == 136, "wire T_OBJ_3D must be 136 bytes");
static_assert(sizeof(T_EXTRA_WIRE32) == 68, "wire T_EXTRA must be 68 bytes");
static_assert(sizeof(S_PART_FLOW_WIRE32) == 60, "wire S_PART_FLOW must be 60 bytes");
static_assert(140 + sizeof(T_OBJ_3D_WIRE32) == 276, "per-object stride must be 276");

/* The 7 pointer-width fields whose 64-bit growth this wire layout freezes. */
static_assert(offsetof(T_OBJ_3D_WIRE32, BodyNum) == 24, "");
static_assert(offsetof(T_OBJ_3D_WIRE32, Texture) == 36, "");
static_assert(offsetof(T_OBJ_3D_WIRE32, LastOfsFrame) == 52, "");
static_assert(offsetof(T_OBJ_3D_WIRE32, NextOfsFrame) == 68, "");
static_assert(offsetof(T_OBJ_3D_WIRE32, LoopOfsFrame) == 84, "");
static_assert(offsetof(T_OBJ_3D_WIRE32, NbGroups) == 132, "");
static_assert(offsetof(T_EXTRA_WIRE32, PtrBody_w32) == 28, "");
static_assert(offsetof(T_EXTRA_WIRE32, Scale) == 64, "");
static_assert(offsetof(S_PART_FLOW_WIRE32, FlowTimerStart) == 52, "");
static_assert(offsetof(S_PART_FLOW_WIRE32, PtrListDot_w32) == 56, "");

static uint32_t le32(int o) {
    return (uint32_t)o | ((uint32_t)(o + 1) << 8) | ((uint32_t)(o + 2) << 16) |
           ((uint32_t)(o + 3) << 24);
}
static uint16_t le16(int o) { return (uint16_t)((uint32_t)o | ((uint32_t)(o + 1) << 8)); }

static void fill_pattern(void *p, size_t n) {
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; i++)
        b[i] = (unsigned char)i;
}

/* ── layer 4a-ii: golden vectors decode to known field values ── */

static void test_obj3d_from_wire32(void) {
    T_OBJ_3D_WIRE32 w;
    fill_pattern(&w, sizeof(w));
    T_OBJ_3D d;
    SavegameObj3dFromWire32(&w, &d);

    ASSERT_EQ_UINT(le32(0), (uint32_t)d.X);
    ASSERT_EQ_UINT(le32(20), (uint32_t)d.Gamma);
    ASSERT_EQ_UINT(le32(24), (uint32_t)d.Body.Num);
    ASSERT_EQ_UINT(le32(28), (uint32_t)d.NextBody.Num);
    ASSERT_EQ_UINT(le32(32), (uint32_t)d.Anim.Num);
    /* pointer slots are dropped, not carried across */
    ASSERT_TRUE(d.Texture == NULL);
    ASSERT_TRUE(d.NextTexture == NULL);
    ASSERT_EQ_UINT(le32(44), d.LastOfsIsPtr);
    ASSERT_EQ_UINT(le32(52), (uint32_t)(uintptr_t)d.LastOfsFrame);
    ASSERT_EQ_UINT(le32(68), (uint32_t)(uintptr_t)d.NextOfsFrame);
    ASSERT_EQ_UINT(le32(84), d.LoopOfsFrame); /* plain U32, not a pointer */
    ASSERT_EQ_UINT(le32(132), d.NbGroups);
}

static void test_extra_from_wire32(void) {
    T_EXTRA_WIRE32 w;
    fill_pattern(&w, sizeof(w));
    T_EXTRA d;
    SavegameExtraFromWire32(&w, &d);

    ASSERT_EQ_UINT(le32(0), (uint32_t)d.PosX);
    ASSERT_EQ_UINT(le32(12), (uint32_t)d.U.Org.X);
    ASSERT_EQ_UINT(le32(20), (uint32_t)d.U.Org.Z);
    ASSERT_EQ_UINT(le32(24), (uint32_t)d.Info);
    ASSERT_TRUE(d.PtrBody == NULL);
    ASSERT_EQ_UINT(le16(32), (uint16_t)d.Sprite);
    ASSERT_EQ_UINT(le32(40), d.Flags);
    ASSERT_EQ_UINT(le32(64), (uint32_t)d.Scale);
}

static void test_flow_from_wire32(void) {
    S_PART_FLOW_WIRE32 w;
    fill_pattern(&w, sizeof(w));
    S_PART_FLOW d;
    S_ONE_DOT *sentinel = (S_ONE_DOT *)(uintptr_t)0xABCD1234u;
    d.PtrListDot = sentinel; /* reader must leave this untouched */
    SavegameFlowFromWire32(&w, &d);

    ASSERT_EQ_UINT(le32(0), (uint32_t)d.Flag);
    ASSERT_EQ_UINT(le32(12), (uint32_t)d.NbDot);
    ASSERT_EQ_UINT(le32(52), d.FlowTimerStart);
    ASSERT_TRUE(d.PtrListDot == sentinel);
}

int main(void) {
    RUN_TEST(test_obj3d_from_wire32);
    RUN_TEST(test_extra_from_wire32);
    RUN_TEST(test_flow_from_wire32);
    TEST_SUMMARY();
    return test_failures != 0;
}
