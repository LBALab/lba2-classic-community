#include "test_harness.h"

#include <SYSTEM/ADELINE_TYPES.H>

#include <string.h>
#include <stdint.h>

typedef struct {
    void **AdrPtr;
    S32 Size;
} T_MEM;

typedef struct {
    U8 Type;
    U8 Num;
} T_TABALLCUBE;

typedef struct {
    S32 X0;
    S32 Y0;
    S32 Z0;
    S32 X1;
    S32 Y1;
    S32 Z1;
    S32 Info0;
    S32 Info1;
    S32 Info2;
    S32 Info3;
    S32 Info4;
    S32 Info5;
    S32 Info6;
    S32 Info7;
    S16 Type;
    S16 Num;
} T_ZONE;

extern "C" U8 CodeJeu;
extern "C" S32 XMap;
extern "C" S32 YMap;
extern "C" S32 ZMap;
extern "C" S32 XScreen;
extern "C" S32 YScreen;
extern "C" PTR_U8 BufCube;
extern "C" PTR_U8 BufMap;
extern "C" PTR_U8 TabBlock;
extern "C" PTR_U8 BufferBrick;

extern "C" U8 asm_CodeJeu;
extern "C" S32 asm_XMap;
extern "C" S32 asm_YMap;
extern "C" S32 asm_ZMap;
extern "C" S32 asm_XScreen;
extern "C" S32 asm_YScreen;
extern "C" PTR_U8 asm_BufCube;
extern "C" PTR_U8 asm_BufMap;
extern "C" PTR_U8 asm_TabBlock;
extern "C" PTR_U8 asm_BufferBrick;

extern "C" U8 *GetAdrBlock(S32 numblock);
extern "C" void Map2Screen(S32 x, S32 y, S32 z);
extern "C" void DecompColonne(U8 *pts, U8 *ptd);
extern "C" U8 WorldCodeBrick(S32 xw, S32 yw, S32 zw);
extern "C" U8 GetBlockBrick(S32 xw, S32 yw, S32 zw);
extern "C" U8 GetWorldColBrickVisible(S32 xw, S32 yw, S32 zw);
extern "C" U8 WorldColBrickFull(S32 xw, S32 yw, S32 zw, S32 ymax);

extern "C" U8 *asm_GetAdrBlock(S32 numblock);
extern "C" void asm_Map2Screen(S32 x, S32 y, S32 z);
extern "C" void asm_DecompColonne(U8 *pts, U8 *ptd);
extern "C" U8 asm_WorldCodeBrick(S32 xw, S32 yw, S32 zw);
extern "C" U8 asm_WorldColBrickFull(S32 xw, S32 yw, S32 zw, S32 ymax);
extern "C" U8 asm_GetBlockBrick(S32 xw, S32 yw, S32 zw);
extern "C" U8 asm_GetWorldColBrickVisible(S32 xw, S32 yw, S32 zw);

static U32 rng_state;

S32 XS0 = 0, YS0 = 0, XS1 = 0, YS1 = 0, XS2 = 0, YS2 = 0, XS3 = 0, YS3 = 0;
S32 Nxw = 0, Nyw = 0, Nzw = 0;
S32 ShadowX = 0, ShadowY = 0, ShadowZ = 0;
U8 ShadowCol = 0;
S32 StartXCube = 0, StartYCube = 0, StartZCube = 0;
S32 FirstTime = 0;
S32 NbZones = 0;
T_ZONE *ListZone = 0;
T_TABALLCUBE TabAllCube[256];
T_MEM ListMem[8];

extern "C" void GetResPath(char *outPath, U16 pathMaxSize, const char *resFilename)
{
    (void)resFilename;
    if (pathMaxSize > 0) {
        outPath[0] = '\0';
    }
}

void ReajustPos(U8 col)
{
    (void)col;
}

S32 AdjustShadowObjects(S32 xw, S32 zw, S32 y0, S32 y1)
{
    (void)xw;
    (void)zw;
    (void)y1;
    return y0;
}

void ClearScreenMinMax(void) {}
void AdjustScreenMinMax(void) {}
void Message(const char *mess, S32 flag) { (void)mess; (void)flag; }
S32 PtrProjectPoint(S32 xw, S32 yw, S32 zw) { (void)xw; (void)yw; (void)zw; return 1; }
void DrawRecover(S32 x0, S32 y0, S32 x1, S32 y1, S32 color)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)color;
}
extern "C" void SetScreenPitch(U32 *newTabOffLine) { (void)newTabOffLine; }

static U8 g_buf_cube[64 * 25 * 64 * 2];
static U8 g_buf_map[4096];
static U8 g_tab_block[4096];
static U8 g_buffer_brick[4096];

static void rng_seed(U32 seed_value)
{
    rng_state = seed_value;
}

static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static U32 cube_offset(S32 x, S32 y, S32 z)
{
    return (U32)(x * 50 + y * 2 + z * 3200);
}

static void set_cube_brick(S32 x, S32 y, S32 z, U8 block_index, U8 pos_block)
{
    U32 offset = cube_offset(x, y, z);
    g_buf_cube[offset] = (U8)(block_index + 1);
    g_buf_cube[offset + 1] = pos_block;
}

static void set_cube_empty(S32 x, S32 y, S32 z, U8 transparent_color)
{
    U32 offset = cube_offset(x, y, z);
    g_buf_cube[offset] = 0;
    g_buf_cube[offset + 1] = transparent_color;
}

static void init_block(U32 index, U32 offset, U8 color0, U8 color1, U8 code)
{
    ((U32 *)g_tab_block)[index] = offset;
    g_tab_block[offset + 0] = 1;
    g_tab_block[offset + 1] = 1;
    g_tab_block[offset + 2] = 0;
    g_tab_block[offset + 3] = color0;
    g_tab_block[offset + 4] = code;
    g_tab_block[offset + 7] = color1;
}

static void init_grille_fixture(void)
{
    memset(g_buf_cube, 0, sizeof(g_buf_cube));
    memset(g_buf_map, 0, sizeof(g_buf_map));
    memset(g_tab_block, 0, sizeof(g_tab_block));
    memset(g_buffer_brick, 0, sizeof(g_buffer_brick));

    BufCube = g_buf_cube;
    BufMap = g_buf_map;
    TabBlock = g_tab_block;
    BufferBrick = g_buffer_brick;
    asm_BufCube = g_buf_cube;
    asm_BufMap = g_buf_map;
    asm_TabBlock = g_tab_block;
    asm_BufferBrick = g_buffer_brick;

    XMap = 0;
    YMap = 0;
    ZMap = 0;
    XScreen = 0;
    YScreen = 0;
    CodeJeu = 0;
    asm_XMap = 0;
    asm_YMap = 0;
    asm_ZMap = 0;
    asm_XScreen = 0;
    asm_YScreen = 0;
    asm_CodeJeu = 0;

    init_block(0, 64, 0x11, 0x22, 0xA1);
    init_block(1, 96, 0x33, 0x44, 0xB2);
    init_block(2, 128, 0x55, 0x66, 0xC3);

    set_cube_brick(1, 2, 3, 0, 0);
    set_cube_brick(2, 4, 5, 1, 1);
    set_cube_empty(3, 6, 7, 0x7E);
    set_cube_brick(0, 0, 0, 2, 0);
    set_cube_empty(10, 10, 10, 0x5A);
    set_cube_brick(4, 8, 9, 0, 1);
    set_cube_brick(4, 9, 9, 2, 0);
}

static void assert_xyz_state(const char *label,
                             S32 asm_xmap, S32 cpp_xmap,
                             S32 asm_ymap, S32 cpp_ymap,
                             S32 asm_zmap, S32 cpp_zmap)
{
    ASSERT_ASM_CPP_EQ_INT(asm_xmap, cpp_xmap, label);
    ASSERT_ASM_CPP_EQ_INT(asm_ymap, cpp_ymap, label);
    ASSERT_ASM_CPP_EQ_INT(asm_zmap, cpp_zmap, label);
}

static void test_getadrblock_equivalence(void)
{
    init_grille_fixture();

    for (S32 index = 0; index < 3; ++index)
    {
        U8 *cpp_ptr = GetAdrBlock(index);
        U8 *asm_ptr = asm_GetAdrBlock(index);

        ASSERT_ASM_CPP_EQ_INT((S32)(cpp_ptr - TabBlock), (S32)(asm_ptr - TabBlock), "GetAdrBlock");
    }
}

static void test_map2screen_equivalence(void)
{
    struct Case { S32 x; S32 y; S32 z; } cases[] = {
        {0, 0, 0},
        {1, 2, 3},
        {20, -3, 5},
        {-7, 4, -2},
        {127, 31, 63}
    };

    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        XScreen = 0; YScreen = 0;
        Map2Screen(cases[i].x, cases[i].y, cases[i].z);
        S32 cpp_xscreen = XScreen;
        S32 cpp_yscreen = YScreen;

        XScreen = 0; YScreen = 0;
        asm_Map2Screen(cases[i].x, cases[i].y, cases[i].z);
        S32 asm_xscreen = asm_XScreen;
        S32 asm_yscreen = asm_YScreen;

        ASSERT_ASM_CPP_EQ_INT(asm_xscreen, cpp_xscreen, "Map2Screen");
        ASSERT_ASM_CPP_EQ_INT(asm_yscreen, cpp_yscreen, "Map2Screen");
    }
}

static void test_map2screen_random_stress(void)
{
    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 300; ++round)
    {
        S32 x = (S32)(rng_next() % 512) - 256;
        S32 y = (S32)(rng_next() % 128) - 64;
        S32 z = (S32)(rng_next() % 512) - 256;

        Map2Screen(x, y, z);
        S32 cpp_xscreen = XScreen;
        S32 cpp_yscreen = YScreen;

        XScreen = 0; YScreen = 0;
        asm_Map2Screen(x, y, z);

        ASSERT_ASM_CPP_EQ_INT(asm_XScreen, cpp_xscreen, "Map2Screen random");
        ASSERT_ASM_CPP_EQ_INT(asm_YScreen, cpp_yscreen, "Map2Screen random");
    }
}

static void test_decompcolonne_equivalence(void)
{
    U8 src_same[] = {1, 0x81, 0x05, 0x06};
    U8 src_diff[] = {1, 0xC1, 0x07, 0x08, 0x09, 0x0A};
    U8 src_zero[] = {1, 0x01};
    U8 src_mixed[] = {3, 0x00, 0x80, 0x0B, 0x0C, 0xC0, 0x0D, 0x0E};
    U8 *cases[] = {src_same, src_diff, src_zero, src_mixed};
    U32 sizes[] = {sizeof(src_same), sizeof(src_diff), sizeof(src_zero), sizeof(src_mixed)};

    for (U32 i = 0; i < 4; ++i)
    {
        U8 cpp_src[32];
        U8 asm_src[32];
        U8 cpp_dst[64];
        U8 asm_dst[64];

        memset(cpp_src, 0, sizeof(cpp_src));
        memset(asm_src, 0, sizeof(asm_src));
        memset(cpp_dst, 0xCC, sizeof(cpp_dst));
        memset(asm_dst, 0xCC, sizeof(asm_dst));
        memcpy(cpp_src, cases[i], sizes[i]);
        memcpy(asm_src, cases[i], sizes[i]);

        DecompColonne(cpp_src, cpp_dst);
        asm_DecompColonne(asm_src, asm_dst);

        ASSERT_ASM_CPP_MEM_EQ(asm_dst, cpp_dst, sizeof(cpp_dst), "DecompColonne dst");
        ASSERT_ASM_CPP_MEM_EQ(asm_src, cpp_src, sizes[i], "DecompColonne src");
    }
}

static void test_worldcodebrick_equivalence(void)
{
    struct Case { S32 xw; S32 yw; S32 zw; } cases[] = {
        {1 * 512, 2 * 256, 3 * 512},
        {2 * 512, 4 * 256, 5 * 512},
        {3 * 512, 6 * 256, 7 * 512},
        {511, 2 * 256, 3 * 512},
        {1 * 512, -1, 3 * 512}
    };

    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        init_grille_fixture();
        U8 cpp_result = WorldCodeBrick(cases[i].xw, cases[i].yw, cases[i].zw);
        S32 cpp_xmap = XMap, cpp_ymap = YMap, cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_result = asm_WorldCodeBrick(cases[i].xw, cases[i].yw, cases[i].zw);
        S32 asm_xmap = asm_XMap, asm_ymap = asm_YMap, asm_zmap = asm_ZMap;

        ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, "WorldCodeBrick");
        assert_xyz_state("WorldCodeBrick state", asm_xmap, cpp_xmap, asm_ymap, cpp_ymap, asm_zmap, cpp_zmap);
    }
}

static void test_getblockbrick_equivalence(void)
{
    struct Case { S32 xw; S32 yw; S32 zw; } cases[] = {
        {1 * 512, 2 * 256, 3 * 512},
        {10 * 512, 10 * 256, 10 * 512},
        {-1, 2 * 256, 3 * 512},
        {1 * 512, -1, 3 * 512},
        {64 * 512, 0, 0}
    };

    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        init_grille_fixture();
        U8 cpp_result = GetBlockBrick(cases[i].xw, cases[i].yw, cases[i].zw);
        S32 cpp_xmap = XMap, cpp_ymap = YMap, cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_result = asm_GetBlockBrick(cases[i].xw, cases[i].yw, cases[i].zw);
        S32 asm_xmap = asm_XMap, asm_ymap = asm_YMap, asm_zmap = asm_ZMap;

        ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, "GetBlockBrick");
        assert_xyz_state("GetBlockBrick state", asm_xmap, cpp_xmap, asm_ymap, cpp_ymap, asm_zmap, cpp_zmap);
    }
}

static void test_getworldcolbrickvisible_equivalence(void)
{
    struct Case { S32 xw; S32 yw; S32 zw; } cases[] = {
        {1 * 512, 2 * 256, 3 * 512},
        {3 * 512, 6 * 256, 7 * 512},
        {-1, 2 * 256, 3 * 512},
        {1 * 512, -1, 3 * 512},
        {64 * 512, 0, 0}
    };

    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        init_grille_fixture();
        U8 cpp_result = GetWorldColBrickVisible(cases[i].xw, cases[i].yw, cases[i].zw);
        S32 cpp_xmap = XMap, cpp_ymap = YMap, cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_result = asm_GetWorldColBrickVisible(cases[i].xw, cases[i].yw, cases[i].zw);
        S32 asm_xmap = asm_XMap, asm_ymap = asm_YMap, asm_zmap = asm_ZMap;

        ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, "GetWorldColBrickVisible");
        assert_xyz_state("GetWorldColBrickVisible state", asm_xmap, cpp_xmap, asm_ymap, cpp_ymap, asm_zmap, cpp_zmap);
    }
}

static void test_worldcolbrickfull_equivalence(void)
{
    struct Case { S32 xw; S32 yw; S32 zw; S32 ymax; } cases[] = {
        {4 * 512, 8 * 256, 9 * 512, 0},
        {4 * 512, 8 * 256, 9 * 512, 512},
        {3 * 512, 6 * 256, 7 * 512, 256},
        {-1, 0, 0, 256},
        {1 * 512, 24 * 256, 3 * 512, 256}
    };

    for (U32 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        init_grille_fixture();
        U8 cpp_result = WorldColBrickFull(cases[i].xw, cases[i].yw, cases[i].zw, cases[i].ymax);
        S32 cpp_xmap = XMap, cpp_ymap = YMap, cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_result = asm_WorldColBrickFull(cases[i].xw, cases[i].yw, cases[i].zw, cases[i].ymax);
        S32 asm_xmap = asm_XMap, asm_ymap = asm_YMap, asm_zmap = asm_ZMap;

        ASSERT_ASM_CPP_EQ_INT(asm_result, cpp_result, "WorldColBrickFull");
        assert_xyz_state("WorldColBrickFull state", asm_xmap, cpp_xmap, asm_ymap, cpp_ymap, asm_zmap, cpp_zmap);
    }
}

static void test_grille_random_stress(void)
{
    rng_seed(0xA55A5AA5u);
    for (int round = 0; round < 300; ++round)
    {
        S32 xw = (S32)(rng_next() % (64 * 512));
        S32 yw = (S32)(rng_next() % (25 * 256));
        S32 zw = (S32)(rng_next() % (64 * 512));
        S32 ymax = (S32)(rng_next() % 1024);

        init_grille_fixture();
        U8 cpp_code = WorldCodeBrick(xw, yw, zw);
        S32 cpp_xmap = XMap, cpp_ymap = YMap, cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_code = asm_WorldCodeBrick(xw, yw, zw);
        ASSERT_ASM_CPP_EQ_INT(asm_code, cpp_code, "WorldCodeBrick random");
        assert_xyz_state("WorldCodeBrick random state", asm_XMap, cpp_xmap, asm_YMap, cpp_ymap, asm_ZMap, cpp_zmap);

        init_grille_fixture();
        U8 cpp_block = GetBlockBrick(xw, yw, zw);
        cpp_xmap = XMap; cpp_ymap = YMap; cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_block = asm_GetBlockBrick(xw, yw, zw);
        ASSERT_ASM_CPP_EQ_INT(asm_block, cpp_block, "GetBlockBrick random");
        assert_xyz_state("GetBlockBrick random state", asm_XMap, cpp_xmap, asm_YMap, cpp_ymap, asm_ZMap, cpp_zmap);

        init_grille_fixture();
        U8 cpp_visible = GetWorldColBrickVisible(xw, yw, zw);
        cpp_xmap = XMap; cpp_ymap = YMap; cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_visible = asm_GetWorldColBrickVisible(xw, yw, zw);
        ASSERT_ASM_CPP_EQ_INT(asm_visible, cpp_visible, "GetWorldColBrickVisible random");
        assert_xyz_state("GetWorldColBrickVisible random state", asm_XMap, cpp_xmap, asm_YMap, cpp_ymap, asm_ZMap, cpp_zmap);

        init_grille_fixture();
        U8 cpp_full = WorldColBrickFull(xw, yw, zw, ymax);
        cpp_xmap = XMap; cpp_ymap = YMap; cpp_zmap = ZMap;

        init_grille_fixture();
        U8 asm_full = asm_WorldColBrickFull(xw, yw, zw, ymax);
        ASSERT_ASM_CPP_EQ_INT(asm_full, cpp_full, "WorldColBrickFull random");
        assert_xyz_state("WorldColBrickFull random state", asm_XMap, cpp_xmap, asm_YMap, cpp_ymap, asm_ZMap, cpp_zmap);
    }
}

int main(void)
{
    RUN_TEST(test_getadrblock_equivalence);
    RUN_TEST(test_map2screen_equivalence);
    RUN_TEST(test_map2screen_random_stress);
    RUN_TEST(test_decompcolonne_equivalence);
    RUN_TEST(test_worldcodebrick_equivalence);
    RUN_TEST(test_getblockbrick_equivalence);
    RUN_TEST(test_getworldcolbrickvisible_equivalence);
    RUN_TEST(test_worldcolbrickfull_equivalence);
    RUN_TEST(test_grille_random_stress);
    TEST_SUMMARY();
    return test_failures != 0;
}