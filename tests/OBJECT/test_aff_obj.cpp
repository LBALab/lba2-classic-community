#include "test_harness.h"

#include <OBJECT/AFF_OBJ.H>
#include <3D/CAMERA.H>
#include <3D/DATAMAT.H>
#include <3D/IMATSTD.H>
#include <3D/IMATTRA.H>
#include <3D/LITLISTF.H>
#include <3D/LPROJ.H>
#include <3D/LROT3D.H>
#include <3D/PROJ.H>
#include <3D/ROT3D.H>
#include <3D/ROTMAT.H>
#include <3D/ROTRALIS.H>
#include <POLYGON/POLY.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>

#include "../pol_work/poly_test_fixture.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <fenv.h>
#include <string.h>

static const U32 kObjProjectedPointCount = 550;
static const U32 kObjRotatedPointCount = 550;
static const U32 kAffObjLightCount = 1100;
static const U32 kAffObjSortCount = 1100;
static const U32 kAffObjGroupCount = 30;
static const U32 kFixturePointCount = 3;
static const U32 kFixtureFaceLightIndex = 3;
static const U32 kComplexGroupCount = 2;
static const U32 kComplexPointCount = 7;
static const U32 kComplexFaceLightBase = kComplexPointCount;
static const U32 kQuadFixturePointCount = 4;
static const U32 kQuadFixtureFaceLightIndex = 4;
static const U32 kComplexFaceLightCount = 2;

enum
{
    kPolySolid = 0,
    kPolyFlat = 1,
    kPolyGouraud = 4,
    kPolyTextureSolid = 8,
    kPolyTextureFlat = 9,
    kPolyTextureGouraud = 10,
    kPolyTextureZSolid = 16,
    kPolyTextureZFlat = 17,
    kPolyTextureZGouraud = 18,
    kPolyEnvTextureSolid = 0x4008,
    kPolyEnvTextureFlat = 0x4009,
    kPolyEnvTextureGouraud = 0x400A,
    kPolyQuadMask = 0x8000,
};

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 Padding;
    U16 Couleur;
    U16 Normale;
} STRUC_POLY3_LIGHT;

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 HandleText;
    U16 Couleur;
    U16 Normale;
    U16 U1;
    U16 V1;
    U16 U2;
    U16 V2;
    U16 U3;
    U16 V3;
} STRUC_POLY3_TEXTURE;

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 P4;
    U16 Couleur;
    U16 Normale;
    U16 U1;
    U16 V1;
    U16 U2;
    U16 V2;
    U16 U3;
    U16 V3;
    U16 U4;
    U16 V4;
    U16 HandleText;
    U16 Padding;
} STRUC_POLY4_TEXTURE;

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 HandleEnv;
    U16 Couleur;
    U16 Normale;
    U16 Scale;
    U16 Padding;
} STRUC_POLY3_ENV;

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 P4;
    U16 Couleur;
    U16 Normale;
    U16 Scale;
    U16 HandleEnv;
} STRUC_POLY4_ENV;

typedef struct
{
    U16 P1;
    U16 P2;
    U16 P3;
    U16 P4;
    U16 Couleur;
    U16 Normale;
} STRUC_POLY4_LIGHT;

#pragma pack(push, 1)
typedef struct
{
    U16 OrgGroupe;
    U16 OrgPoint;
    U16 NbPts;
    U16 NbNorm;
} TEST_OBJ_GROUP;

typedef struct
{
    U16 TypePoly;
    U16 NbPoly;
    U32 OffNextType;
} TEST_POLY_HEADER;

typedef struct
{
    T_BODY_HEADER Header;
    TEST_OBJ_GROUP Group;
    T_OBJ_POINT Points[kFixturePointCount];
    TYPE_VT16 Normals[kFixtureFaceLightIndex];
    TYPE_VT16 FaceNormals[1];
    TEST_POLY_HEADER PolyHeader;
    STRUC_POLY3_LIGHT Triangle;
} TEST_BODY_FIXTURE;

typedef struct
{
    T_BODY_HEADER Header;
    TEST_OBJ_GROUP Groups[kComplexGroupCount];
    T_OBJ_POINT Points[kComplexPointCount];
    TYPE_VT16 Normals[kComplexPointCount];
    TYPE_VT16 FaceNormals[kComplexFaceLightCount];
    TEST_POLY_HEADER FlatTriangleHeader;
    STRUC_POLY3_LIGHT FlatTriangle;
    TEST_POLY_HEADER SolidTriangleHeader;
    STRUC_POLY3_LIGHT SolidTriangle;
    TEST_POLY_HEADER GouraudTriangleHeader;
    STRUC_POLY3_LIGHT GouraudTriangle;
    TEST_POLY_HEADER FlatQuadHeader;
    STRUC_POLY4_LIGHT FlatQuad;
} TEST_COMPLEX_BODY_FIXTURE;

typedef struct
{
    T_BODY_HEADER Header;
    TEST_OBJ_GROUP Group;
    T_OBJ_POINT Points[kFixturePointCount];
    TYPE_VT16 Normals[kFixtureFaceLightIndex];
    TYPE_VT16 FaceNormals[1];
    TEST_POLY_HEADER PolyHeader;
    STRUC_POLY3_TEXTURE Triangle;
    U32 Textures[1];
} TEST_TEXTURED_BODY_FIXTURE;

typedef struct
{
    T_BODY_HEADER Header;
    TEST_OBJ_GROUP Group;
    T_OBJ_POINT Points[kFixturePointCount];
    TYPE_VT16 Normals[kFixtureFaceLightIndex];
    TYPE_VT16 FaceNormals[1];
    TEST_POLY_HEADER PolyHeader;
    STRUC_POLY3_ENV Triangle;
    U32 Textures[1];
} TEST_ENV_BODY_FIXTURE;

typedef struct
{
    T_BODY_HEADER Header;
    TEST_OBJ_GROUP Group;
    T_OBJ_POINT Points[kQuadFixturePointCount];
    TYPE_VT16 Normals[kQuadFixtureFaceLightIndex];
    TYPE_VT16 FaceNormals[1];
    TEST_POLY_HEADER PolyHeader;
    STRUC_POLY4_TEXTURE Quad;
    U32 Textures[1];
} TEST_TEXTURED_QUAD_BODY_FIXTURE;

typedef struct
{
    T_BODY_HEADER Header;
    TEST_OBJ_GROUP Group;
    T_OBJ_POINT Points[kQuadFixturePointCount];
    TYPE_VT16 Normals[kQuadFixtureFaceLightIndex];
    TYPE_VT16 FaceNormals[1];
    TEST_POLY_HEADER PolyHeader;
    STRUC_POLY4_ENV Quad;
    U32 Textures[1];
} TEST_ENV_QUAD_BODY_FIXTURE;
#pragma pack(pop)

typedef void (*AffObjEnvironmentSetupFn)(void);

typedef struct
{
    S32 Result;
    S32 ScreenXMinValue;
    S32 ScreenYMinValue;
    S32 ScreenXMaxValue;
    S32 ScreenYMaxValue;
    S32 PosXWrValue;
    S32 PosYWrValue;
    S32 PosZWrValue;
    S32 NbSortValue;
    S32 OffSortWords;
    uintptr_t BodyPointerValue;
    uintptr_t PtrTexturesValue;
    S32 X0Value;
    S32 Y0Value;
    S32 Z0Value;
    S32 XpValue;
    S32 YpValue;
    int NonZeroPixels;
    T_OBJ_POINT RotatedPoints[kObjRotatedPointCount];
    TYPE_PT ProjectedPoints[kObjProjectedPointCount];
    U16 ListLightsValues[kAffObjLightCount];
    S32 ListSortValues[kAffObjSortCount];
    TYPE_MAT TabMatValues[kAffObjGroupCount];
    Struc_Point FillPolyValues[4];
    U8 Framebuffer[TEST_POLY_SIZE];
} RENDER_SNAPSHOT;

void QuickSort(S32 *beginning, S32 *end);
void QuickSortInv(S32 *beginning, S32 *end);
bool TestVisible(STRUC_POLY3_ENV *poly);

extern TYPE_PT Obj_ListProjectedPoints[];
extern T_OBJ_POINT Obj_ListRotatedPoints[];
extern T_BODY_HEADER *lBody;
extern S32 *Off_Sort;
extern S32 Nb_Sort;
extern S32 PosXWr;
extern S32 PosYWr;
extern S32 PosZWr;
extern TYPE_MAT TabMat[];
extern U16 ListLights[];
extern S32 ListSort[];
extern Struc_Point ListFillPoly[];
extern U32 *PtrTextures;

extern "C" TYPE_PT asm_Obj_ListProjectedPoints[];
extern "C" T_OBJ_POINT asm_Obj_ListRotatedPoints[];
extern "C" T_BODY_HEADER *asm_lBody;
extern "C" S32 *asm_Off_Sort;
extern "C" S32 asm_Nb_Sort;
extern "C" S32 asm_PosXWr;
extern "C" S32 asm_PosYWr;
extern "C" S32 asm_PosZWr;
extern "C" TYPE_MAT asm_TabMat[];
extern "C" U16 asm_ListLights[];
extern "C" S32 asm_ListSort[];
extern "C" Struc_Point asm_ListFillPoly[];
extern "C" U32 *asm_PtrTextures;
extern "C" U8 *asm_ObjPtrMap;
extern "C" Func_TransNumPtr *asm_TransFctBody;

extern "C" S32 asm_BodyDisplay(S32 x, S32 y, S32 z, S32 alpha, S32 beta,
                                S32 gamma, void *obj);
extern "C" S32 asm_BodyDisplay_AlphaBeta(S32 x, S32 y, S32 z, S32 alpha,
                                          S32 beta, S32 gamma, void *obj);
extern "C" S32 asm_ObjectDisplay(T_OBJ_3D *obj);
extern "C" int CallTestVisibleI(STRUC_POLY3_LIGHT *poly);
extern "C" int CallTestVisibleF(STRUC_POLY3_ENV *poly);
extern "C" void CallQuickSort(S32 *beginning, S32 *end);
extern "C" void CallQuickSortInv(S32 *beginning, S32 *end);

void LongRotatePointF(TYPE_MAT *Mat, S32 x, S32 y, S32 z);
void RotateMatrixU(TYPE_MAT *MatDst, TYPE_MAT *MatSrc, S32 x, S32 y, S32 z);
void RotTransListF(TYPE_MAT *Mat, TYPE_VT16 *Dst, TYPE_VT16 *Src, S32 NbPoints);
void ProjectList3DF(TYPE_PT *Dst, TYPE_VT16 *Src, S32 NbPt, S32 OrgX,
                    S32 OrgY, S32 OrgZ);
void InitMatrixTransF(TYPE_MAT *MatDst, S32 tx, S32 ty, S32 tz);

extern "C" S32 CallCppFillPolyFast(S32 type_poly, S32 color_poly,
                                    S32 nb_points, Struc_Point *ptr_points)
{
    return Fill_Poly(type_poly, color_poly, nb_points, ptr_points);
}

extern "C" void CallCppLongRotatePoint(TYPE_MAT *mat, S32 x, S32 y, S32 z)
{
    LongRotatePointF(mat, x, y, z);
}

extern "C" void CallCppRotatePoint(TYPE_MAT *mat, S32 x, S32 y, S32 z)
{
    LongRotatePointF(mat, x, y, z);
}

extern "C" void CallCppRotateMatrix(TYPE_MAT *dst, TYPE_MAT *src, S32 x, S32 y, S32 z)
{
    RotateMatrixU(dst, src, x, y, z);
}

extern "C" void CallCppRotTransList(TYPE_MAT *mat, TYPE_VT16 *dst, TYPE_VT16 *src, S32 count)
{
    RotTransListF(mat, dst, src, count);
}

extern "C" void CallCppProjectList(TYPE_PT *dst, TYPE_VT16 *src, S32 count,
                                     S32 org_x, S32 org_y, S32 org_z)
{
    ProjectList3DF(dst, src, count, org_x, org_y, org_z);
}

extern "C" void CallCppInitMatrixTrans(TYPE_MAT *dst, S32 tx, S32 ty, S32 tz)
{
    InitMatrixTransF(dst, tx, ty, tz);
}

extern "C" void CallCppLightList(TYPE_MAT *mat, U16 *dst, TYPE_VT16 *src, S32 count)
{
    LightList(mat, dst, src, count);
}

asm(
    ".globl Fill_PolyFast\n"
    "Fill_PolyFast:\n"
    "push %esi\n"
    "push %ecx\n"
    "push %ebx\n"
    "push %eax\n"
    "call CallCppFillPolyFast\n"
    "add $16, %esp\n"
    "ret\n");

extern "C" void AsmAbi_LongRotatePoint_Thunk(void);
extern "C" void AsmAbi_RotatePoint_Thunk(void);
extern "C" void AsmAbi_RotateMatrix_Thunk(void);
extern "C" void AsmAbi_RotTransList_Thunk(void);
extern "C" void AsmAbi_ProjectList_Thunk(void);
extern "C" void AsmAbi_InitMatrixTrans_Thunk(void);
extern "C" void AsmAbi_LightList_Thunk(void);

asm(
    ".globl AsmAbi_LongRotatePoint_Thunk\n"
    "AsmAbi_LongRotatePoint_Thunk:\n"
    "push %ecx\n"
    "push %ebx\n"
    "push %eax\n"
    "push %esi\n"
    "call CallCppLongRotatePoint\n"
    "add $16, %esp\n"
    "ret\n"
    ".globl AsmAbi_RotatePoint_Thunk\n"
    "AsmAbi_RotatePoint_Thunk:\n"
    "push %ecx\n"
    "push %ebx\n"
    "push %eax\n"
    "push %esi\n"
    "call CallCppRotatePoint\n"
    "add $16, %esp\n"
    "ret\n"
    ".globl AsmAbi_RotateMatrix_Thunk\n"
    "AsmAbi_RotateMatrix_Thunk:\n"
    "push %ecx\n"
    "push %ebx\n"
    "push %eax\n"
    "push %esi\n"
    "push %edi\n"
    "call CallCppRotateMatrix\n"
    "add $20, %esp\n"
    "ret\n"
    ".globl AsmAbi_RotTransList_Thunk\n"
    "AsmAbi_RotTransList_Thunk:\n"
    "push %ecx\n"
    "push %esi\n"
    "push %edi\n"
    "push %ebx\n"
    "call CallCppRotTransList\n"
    "add $16, %esp\n"
    "ret\n"
    ".globl AsmAbi_ProjectList_Thunk\n"
    "AsmAbi_ProjectList_Thunk:\n"
    "mov 4(%esp), %eax\n"
    "mov 8(%esp), %edx\n"
    "mov 12(%esp), %ebx\n"
    "push %ebx\n"
    "push %edx\n"
    "push %eax\n"
    "push %ecx\n"
    "push %esi\n"
    "push %edi\n"
    "call CallCppProjectList\n"
    "add $24, %esp\n"
    "ret\n"
    ".globl AsmAbi_InitMatrixTrans_Thunk\n"
    "AsmAbi_InitMatrixTrans_Thunk:\n"
    "push %ecx\n"
    "push %ebx\n"
    "push %eax\n"
    "push %edi\n"
    "call CallCppInitMatrixTrans\n"
    "add $16, %esp\n"
    "ret\n"
    ".globl AsmAbi_LightList_Thunk\n"
    "AsmAbi_LightList_Thunk:\n"
    "push %ecx\n"
    "push %esi\n"
    "push %edi\n"
    "push %ebx\n"
    "call CallCppLightList\n"
    "add $16, %esp\n"
    "ret\n");

static void restore_cpp_3d_callbacks(void)
{
    LongRotatePoint = LongRotatePointF;
    RotatePoint = LongRotatePointF;
    RotatePointNoMMX = LongRotatePointF;
    RotateMatrix = RotateMatrixU;
    RotTransList = RotTransListF;
    ProjectList = ProjectList3DF;
    InitMatrixTrans = InitMatrixTransF;
    LightListPtr = LightList;
}

static void install_asm_compatible_3d_callbacks(void)
{
    LongRotatePoint = (Func_LongRotatePoint *)AsmAbi_LongRotatePoint_Thunk;
    RotatePoint = (Func_RotatePoint *)AsmAbi_RotatePoint_Thunk;
    RotatePointNoMMX = (Func_RotatePoint *)AsmAbi_RotatePoint_Thunk;
    RotateMatrix = (Func_RotateMatrix *)AsmAbi_RotateMatrix_Thunk;
    RotTransList = (Func_RotTransList *)AsmAbi_RotTransList_Thunk;
    ProjectList = (Func_ProjectList *)AsmAbi_ProjectList_Thunk;
    InitMatrixTrans = (Func_InitMatrixTrans *)AsmAbi_InitMatrixTrans_Thunk;
    LightListPtr = (Func_LightList *)AsmAbi_LightList_Thunk;
}

static U32 rng_state;
static fenv_t g_initial_fenv;

static void rng_seed(U32 seed_value)
{
    rng_state = seed_value;
}

static U32 rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static void set_point(TYPE_PT *point, S16 x, S16 y)
{
    point->X = x;
    point->Y = y;
}

static void set_hidden_point(TYPE_PT *point)
{
    point->X = (S16)-0x8000;
    point->Y = (S16)-0x8000;
}

static void build_projected_points_snapshot(TYPE_PT *snapshot, const TYPE_PT *points, U32 count)
{
    ASSERT_TRUE(count <= kObjProjectedPointCount);

    for (U32 index = 0; index < kObjProjectedPointCount; ++index)
    {
        set_hidden_point(&snapshot[index]);
    }

    memcpy(snapshot, points, count * sizeof(TYPE_PT));
}

static void fill_strictly_increasing(S32 *values, U32 count)
{
    S32 current = (S32)(rng_next() & 0xFF);
    for (U32 index = 0; index < count; ++index)
    {
        current += (S32)((rng_next() % 17) + 1);
        values[index] = current;
    }
}

static void fill_strictly_decreasing(S32 *values, U32 count)
{
    S32 current = (S32)(rng_next() & 0xFF) + 1024;
    for (U32 index = 0; index < count; ++index)
    {
        current -= (S32)((rng_next() % 17) + 1);
        values[index] = current;
    }
}

static void fill_random_values(S32 *values, U32 count)
{
    for (U32 index = 0; index < count; ++index)
    {
        values[index] = (S32)(((S32)rng_next() << 1) - (S32)rng_next());
    }
}

static bool is_strictly_increasing(const S32 *values, U32 count)
{
    for (U32 index = 1; index < count; ++index)
    {
        if (values[index - 1] >= values[index])
        {
            return false;
        }
    }

    return true;
}

static bool is_strictly_decreasing(const S32 *values, U32 count)
{
    for (U32 index = 1; index < count; ++index)
    {
        if (values[index - 1] <= values[index])
        {
            return false;
        }
    }

    return true;
}

static bool is_quicksort_asm_valid(const S32 *values, U32 count)
{
    S32 scratch[32];
    S32 pivot;
    U32 left;
    U32 next;

    if (count <= 1)
    {
        return true;
    }

    memcpy(scratch, values, count * sizeof(S32));

    pivot = scratch[0];
    left = 0;
    next = 1;
    while (next < count)
    {
        S32 nextValue = scratch[next];
        next += 1;

        if (pivot >= nextValue)
        {
            scratch[left] = nextValue;
            scratch[next - 1] = scratch[left + 1];
            scratch[left + 1] = pivot;
            left += 1;
        }
    }

    if (left == count - 1)
    {
        return false;
    }

    return is_quicksort_asm_valid(scratch, left + 1)
        && is_quicksort_asm_valid(scratch + left + 1, count - left - 1);
}

static bool is_quicksortinv_asm_valid(const S32 *values, U32 count)
{
    S32 scratch[32];
    S32 pivot;
    U32 left;
    U32 next;

    if (count <= 1)
    {
        return true;
    }

    memcpy(scratch, values, count * sizeof(S32));

    pivot = scratch[0];
    left = 0;
    next = 1;
    while (next < count)
    {
        S32 nextValue = scratch[next];
        next += 1;

        if (pivot <= nextValue)
        {
            scratch[left] = nextValue;
            scratch[next - 1] = scratch[left + 1];
            scratch[left + 1] = pivot;
            left += 1;
        }
    }

    if (left == count - 1)
    {
        return false;
    }

    return is_quicksortinv_asm_valid(scratch, left + 1)
        && is_quicksortinv_asm_valid(scratch + left + 1, count - left - 1);
}

static void fill_random_unordered_quicksort_values(S32 *values, U32 count)
{
    for (int attempt = 0; attempt < 4096; ++attempt)
    {
        fill_random_values(values, count);
        if (!is_strictly_increasing(values, count)
            && !is_strictly_decreasing(values, count)
            && is_quicksort_asm_valid(values, count))
        {
            return;
        }
    }

    ASSERT_TRUE(false);
}

static void fill_random_unordered_quicksortinv_values(S32 *values, U32 count)
{
    for (int attempt = 0; attempt < 4096; ++attempt)
    {
        fill_random_values(values, count);
        if (!is_strictly_increasing(values, count)
            && !is_strictly_decreasing(values, count)
            && is_quicksortinv_asm_valid(values, count))
        {
            return;
        }
    }

    ASSERT_TRUE(false);
}

static void run_sort_case(const char *label, const S32 *values, U32 count)
{
    S32 cpp_storage[34];
    S32 asm_storage[34];
    S32 *cpp_array = cpp_storage + 1;
    S32 *asm_array = asm_storage + 1;

    ASSERT_TRUE(count >= 1 && count <= 32);

    cpp_storage[0] = 0x11223344;
    asm_storage[0] = 0x11223344;
    cpp_storage[count + 1] = 0x55667788;
    asm_storage[count + 1] = 0x55667788;

    memcpy(cpp_array, values, count * sizeof(S32));
    memcpy(asm_array, values, count * sizeof(S32));

    QuickSort(cpp_array, cpp_array + count - 1);
    CallQuickSort(asm_array, asm_array + count - 1);

    ASSERT_ASM_CPP_MEM_EQ(asm_storage, cpp_storage, (count + 2) * sizeof(S32), label);
}

static void run_sortinv_case(const char *label, const S32 *values, U32 count)
{
    S32 cpp_storage[34];
    S32 asm_storage[34];
    S32 *cpp_array = cpp_storage + 1;
    S32 *asm_array = asm_storage + 1;

    ASSERT_TRUE(count >= 1 && count <= 32);

    cpp_storage[0] = 0x12345678;
    asm_storage[0] = 0x12345678;
    cpp_storage[count + 1] = 0x87654321;
    asm_storage[count + 1] = 0x87654321;

    memcpy(cpp_array, values, count * sizeof(S32));
    memcpy(asm_array, values, count * sizeof(S32));

    QuickSortInv(cpp_array, cpp_array + count - 1);
    CallQuickSortInv(asm_array, asm_array + count - 1);

    ASSERT_ASM_CPP_MEM_EQ(asm_storage, cpp_storage, (count + 2) * sizeof(S32), label);
}

static void run_testvisiblei_case(const char *label, const TYPE_PT *points, U32 count, const STRUC_POLY3_LIGHT *poly)
{
    TYPE_PT baseline[kObjProjectedPointCount];

    build_projected_points_snapshot(baseline, points, count);

    memcpy(Obj_ListProjectedPoints, baseline, sizeof(baseline));
    const int cpp_visible = TestVisible((STRUC_POLY3_ENV *)poly) ? 1 : 0;
    ASSERT_MEM_EQ(baseline, Obj_ListProjectedPoints, sizeof(baseline));

    memcpy(asm_Obj_ListProjectedPoints, baseline, sizeof(baseline));
    const int asm_visible = CallTestVisibleI((STRUC_POLY3_LIGHT *)poly);
    ASSERT_MEM_EQ(baseline, asm_Obj_ListProjectedPoints, sizeof(baseline));

    ASSERT_ASM_CPP_EQ_INT(asm_visible, cpp_visible, label);
}

static S32 packed_projected_point_value(const TYPE_PT *points, U16 index)
{
    S32 packedValue;
    memcpy(&packedValue, &points[index], sizeof(packedValue));
    return packedValue;
}

static int testvisiblef_cpp_mirror(const STRUC_POLY3_ENV *poly)
{
    const S32 packedP1 = packed_projected_point_value(Obj_ListProjectedPoints, poly->P1);
    const S32 packedP2 = packed_projected_point_value(Obj_ListProjectedPoints, poly->P2);
    const S32 packedP3 = packed_projected_point_value(Obj_ListProjectedPoints, poly->P3);

    if (packedP1 == (S32)0x80008000u
        || packedP2 == (S32)0x80008000u
        || packedP3 == (S32)0x80008000u)
    {
        return 0;
    }

    const S32 packedP1Next = packed_projected_point_value(Obj_ListProjectedPoints, (U16)(poly->P1 + 1));
    const S32 packedP2Next = packed_projected_point_value(Obj_ListProjectedPoints, (U16)(poly->P2 + 1));
    const S32 packedP3Next = packed_projected_point_value(Obj_ListProjectedPoints, (U16)(poly->P3 + 1));
    const long double result = ((long double)packedP2 - (long double)packedP1)
        * ((long double)packedP1Next - (long double)packedP3Next)
        - ((long double)packedP2Next - (long double)packedP1Next)
        * ((long double)packedP1 - (long double)packedP3);
    const float storedResult = (float)result;
    U32 storedBits;

    memcpy(&storedBits, &storedResult, sizeof(storedBits));

    return (storedBits & 0x80000000u) == 0 ? 1 : 0;
}

static void run_testvisiblef_case(const char *label, const TYPE_PT *points, U32 count, const STRUC_POLY3_ENV *poly)
{
    TYPE_PT baseline[kObjProjectedPointCount];

    build_projected_points_snapshot(baseline, points, count);

    memcpy(Obj_ListProjectedPoints, baseline, sizeof(baseline));
    const int cpp_visible = testvisiblef_cpp_mirror(poly);
    ASSERT_MEM_EQ(baseline, Obj_ListProjectedPoints, sizeof(baseline));

    memcpy(asm_Obj_ListProjectedPoints, baseline, sizeof(baseline));
    const int asm_visible = CallTestVisibleF((STRUC_POLY3_ENV *)poly);
    ASSERT_MEM_EQ(baseline, asm_Obj_ListProjectedPoints, sizeof(baseline));

    ASSERT_ASM_CPP_EQ_INT(asm_visible, cpp_visible, label);
}

static void build_test_body_fixture(TEST_BODY_FIXTURE *fixture)
{
    memset(fixture, 0, sizeof(*fixture));

    fixture->Header.Info = 0;
    fixture->Header.SizeHeader = (S16)sizeof(T_BODY_HEADER);
    fixture->Header.Dummy = 0;
    fixture->Header.XMin = 0;
    fixture->Header.XMax = 64;
    fixture->Header.YMin = 0;
    fixture->Header.YMax = 64;
    fixture->Header.ZMin = 256;
    fixture->Header.ZMax = 256;
    fixture->Header.NbGroupes = 1;
    fixture->Header.OffGroupes = (S32)offsetof(TEST_BODY_FIXTURE, Group);
    fixture->Header.NbPoints = kFixturePointCount;
    fixture->Header.OffPoints = (S32)offsetof(TEST_BODY_FIXTURE, Points);
    fixture->Header.NbNormales = kFixtureFaceLightIndex;
    fixture->Header.OffNormales = (S32)offsetof(TEST_BODY_FIXTURE, Normals);
    fixture->Header.NbNormFaces = 1;
    fixture->Header.OffNormFaces = (S32)offsetof(TEST_BODY_FIXTURE, FaceNormals);
    fixture->Header.NbPolys = 1;
    fixture->Header.OffPolys = (S32)offsetof(TEST_BODY_FIXTURE, PolyHeader);
    fixture->Header.NbLines = 0;
    fixture->Header.OffLines = (S32)sizeof(*fixture);
    fixture->Header.NbSpheres = 0;
    fixture->Header.OffSpheres = (S32)sizeof(*fixture);
    fixture->Header.NbTextures = 0;
    fixture->Header.OffTextures = (S32)sizeof(*fixture);

    fixture->Group.OrgGroupe = 0;
    fixture->Group.OrgPoint = 0;
    fixture->Group.NbPts = kFixturePointCount;
    fixture->Group.NbNorm = 1;

    fixture->Points[0].X = 0;
    fixture->Points[0].Y = 0;
    fixture->Points[0].Z = 256;
    fixture->Points[0].Group = 0;

    fixture->Points[1].X = 64;
    fixture->Points[1].Y = 0;
    fixture->Points[1].Z = 256;
    fixture->Points[1].Group = 0;

    fixture->Points[2].X = 0;
    fixture->Points[2].Y = 64;
    fixture->Points[2].Z = 256;
    fixture->Points[2].Group = 0;

    for (U32 index = 0; index < kFixtureFaceLightIndex; ++index)
    {
        fixture->Normals[index].X = 15360;
        fixture->Normals[index].Y = 0;
        fixture->Normals[index].Z = 0;
        fixture->Normals[index].Grp = 0;
    }

    fixture->FaceNormals[0].X = 15360;
    fixture->FaceNormals[0].Y = 0;
    fixture->FaceNormals[0].Z = 0;
    fixture->FaceNormals[0].Grp = 0;

    fixture->PolyHeader.TypePoly = kPolyFlat;
    fixture->PolyHeader.NbPoly = 1;
    fixture->PolyHeader.OffNextType = (U32)sizeof(*fixture);

    fixture->Triangle.P1 = 0;
    fixture->Triangle.P2 = 1;
    fixture->Triangle.P3 = 2;
    fixture->Triangle.Padding = 0;
    fixture->Triangle.Couleur = 0x20;
    fixture->Triangle.Normale = kFixtureFaceLightIndex;
}

static void set_body_point(T_OBJ_POINT *point, S16 x, S16 y, S16 z, S16 group)
{
    point->X = x;
    point->Y = y;
    point->Z = z;
    point->Group = group;
}

static void set_body_normal(TYPE_VT16 *normal, S16 x, S16 y, S16 z, S16 group)
{
    normal->X = x;
    normal->Y = y;
    normal->Z = z;
    normal->Grp = group;
}

static void build_complex_test_body_fixture(TEST_COMPLEX_BODY_FIXTURE *fixture)
{
    memset(fixture, 0, sizeof(*fixture));

    fixture->Header.Info = 0;
    fixture->Header.SizeHeader = (S16)sizeof(T_BODY_HEADER);
    fixture->Header.Dummy = 0;
    fixture->Header.XMin = -64;
    fixture->Header.XMax = 128;
    fixture->Header.YMin = -64;
    fixture->Header.YMax = 128;
    fixture->Header.ZMin = 96;
    fixture->Header.ZMax = 320;
    fixture->Header.NbGroupes = kComplexGroupCount;
    fixture->Header.OffGroupes = (S32)offsetof(TEST_COMPLEX_BODY_FIXTURE, Groups);
    fixture->Header.NbPoints = kComplexPointCount;
    fixture->Header.OffPoints = (S32)offsetof(TEST_COMPLEX_BODY_FIXTURE, Points);
    fixture->Header.NbNormales = kComplexPointCount;
    fixture->Header.OffNormales = (S32)offsetof(TEST_COMPLEX_BODY_FIXTURE, Normals);
    fixture->Header.NbNormFaces = kComplexFaceLightCount;
    fixture->Header.OffNormFaces = (S32)offsetof(TEST_COMPLEX_BODY_FIXTURE, FaceNormals);
    fixture->Header.NbPolys = 4;
    fixture->Header.OffPolys = (S32)offsetof(TEST_COMPLEX_BODY_FIXTURE, FlatTriangleHeader);
    fixture->Header.NbLines = 0;
    fixture->Header.OffLines = (S32)sizeof(*fixture);
    fixture->Header.NbSpheres = 0;
    fixture->Header.OffSpheres = (S32)sizeof(*fixture);
    fixture->Header.NbTextures = 0;
    fixture->Header.OffTextures = (S32)sizeof(*fixture);

    fixture->Groups[0].OrgGroupe = 0;
    fixture->Groups[0].OrgPoint = 0;
    fixture->Groups[0].NbPts = 3;
    fixture->Groups[0].NbNorm = 1;

    fixture->Groups[1].OrgGroupe = 0;
    fixture->Groups[1].OrgPoint = 0;
    fixture->Groups[1].NbPts = 4;
    fixture->Groups[1].NbNorm = 1;

    set_body_point(&fixture->Points[0], 0, 0, 256, 0);
    set_body_point(&fixture->Points[1], 96, 0, 256, 0);
    set_body_point(&fixture->Points[2], 0, 96, 256, 0);
    set_body_point(&fixture->Points[3], -48, -48, 128, 1);
    set_body_point(&fixture->Points[4], 48, -48, 128, 1);
    set_body_point(&fixture->Points[5], -48, 48, 128, 1);
    set_body_point(&fixture->Points[6], 48, 48, 128, 1);

    set_body_normal(&fixture->Normals[0], 15360, 0, 0, 0);
    set_body_normal(&fixture->Normals[1], 15360, 0, 0, 0);
    set_body_normal(&fixture->Normals[2], 15360, 0, 0, 0);
    set_body_normal(&fixture->Normals[3], 15360, 0, 0, 1);
    set_body_normal(&fixture->Normals[4], 12288, 4096, 0, 1);
    set_body_normal(&fixture->Normals[5], 12288, 0, 4096, 1);
    set_body_normal(&fixture->Normals[6], 8192, 8192, 0, 1);

    set_body_normal(&fixture->FaceNormals[0], 15360, 0, 0, 0);
    set_body_normal(&fixture->FaceNormals[1], 12288, 4096, 0, 1);

    fixture->FlatTriangleHeader.TypePoly = kPolyFlat;
    fixture->FlatTriangleHeader.NbPoly = 1;
    fixture->FlatTriangleHeader.OffNextType = (U32)offsetof(TEST_COMPLEX_BODY_FIXTURE, SolidTriangleHeader);
    fixture->FlatTriangle.P1 = 0;
    fixture->FlatTriangle.P2 = 1;
    fixture->FlatTriangle.P3 = 2;
    fixture->FlatTriangle.Padding = 0;
    fixture->FlatTriangle.Couleur = 0x20;
    fixture->FlatTriangle.Normale = kComplexFaceLightBase;

    fixture->SolidTriangleHeader.TypePoly = kPolySolid;
    fixture->SolidTriangleHeader.NbPoly = 1;
    fixture->SolidTriangleHeader.OffNextType = (U32)offsetof(TEST_COMPLEX_BODY_FIXTURE, GouraudTriangleHeader);
    fixture->SolidTriangle.P1 = 0;
    fixture->SolidTriangle.P2 = 2;
    fixture->SolidTriangle.P3 = 1;
    fixture->SolidTriangle.Padding = 0;
    fixture->SolidTriangle.Couleur = 0x34;
    fixture->SolidTriangle.Normale = 0;

    fixture->GouraudTriangleHeader.TypePoly = kPolyGouraud;
    fixture->GouraudTriangleHeader.NbPoly = 1;
    fixture->GouraudTriangleHeader.OffNextType = (U32)offsetof(TEST_COMPLEX_BODY_FIXTURE, FlatQuadHeader);
    fixture->GouraudTriangle.P1 = 3;
    fixture->GouraudTriangle.P2 = 4;
    fixture->GouraudTriangle.P3 = 5;
    fixture->GouraudTriangle.Padding = 0;
    fixture->GouraudTriangle.Couleur = 0x40;
    fixture->GouraudTriangle.Normale = 0;

    fixture->FlatQuadHeader.TypePoly = (U16)(kPolyQuadMask | kPolyFlat);
    fixture->FlatQuadHeader.NbPoly = 1;
    fixture->FlatQuadHeader.OffNextType = (U32)sizeof(*fixture);
    fixture->FlatQuad.P1 = 3;
    fixture->FlatQuad.P2 = 5;
    fixture->FlatQuad.P3 = 6;
    fixture->FlatQuad.P4 = 4;
    fixture->FlatQuad.Couleur = 0x28;
    fixture->FlatQuad.Normale = kComplexFaceLightBase + 1;
}

static void build_textured_test_body_fixture(TEST_TEXTURED_BODY_FIXTURE *fixture,
                                             U16 poly_type)
{
    memset(fixture, 0, sizeof(*fixture));

    fixture->Header.Info = 0;
    fixture->Header.SizeHeader = (S16)sizeof(T_BODY_HEADER);
    fixture->Header.Dummy = 0;
    fixture->Header.XMin = 0;
    fixture->Header.XMax = 96;
    fixture->Header.YMin = 0;
    fixture->Header.YMax = 96;
    fixture->Header.ZMin = 256;
    fixture->Header.ZMax = 256;
    fixture->Header.NbGroupes = 1;
    fixture->Header.OffGroupes = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, Group);
    fixture->Header.NbPoints = kFixturePointCount;
    fixture->Header.OffPoints = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, Points);
    fixture->Header.NbNormales = kFixturePointCount;
    fixture->Header.OffNormales = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, Normals);
    fixture->Header.NbNormFaces = 1;
    fixture->Header.OffNormFaces = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, FaceNormals);
    fixture->Header.NbPolys = 1;
    fixture->Header.OffPolys = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, PolyHeader);
    fixture->Header.NbLines = 0;
    fixture->Header.OffLines = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, Textures);
    fixture->Header.NbSpheres = 0;
    fixture->Header.OffSpheres = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, Textures);
    fixture->Header.NbTextures = 1;
    fixture->Header.OffTextures = (S32)offsetof(TEST_TEXTURED_BODY_FIXTURE, Textures);

    fixture->Group.OrgGroupe = 0;
    fixture->Group.OrgPoint = 0;
    fixture->Group.NbPts = kFixturePointCount;
    fixture->Group.NbNorm = 1;

    set_body_point(&fixture->Points[0], 0, 0, 256, 0);
    set_body_point(&fixture->Points[1], 96, 0, 256, 0);
    set_body_point(&fixture->Points[2], 0, 96, 256, 0);

    set_body_normal(&fixture->Normals[0], 15360, 0, 0, 0);
    set_body_normal(&fixture->Normals[1], 12288, 4096, 0, 0);
    set_body_normal(&fixture->Normals[2], 12288, 0, 4096, 0);
    set_body_normal(&fixture->FaceNormals[0], 15360, 0, 0, 0);

    fixture->PolyHeader.TypePoly = poly_type;
    fixture->PolyHeader.NbPoly = 1;
    fixture->PolyHeader.OffNextType = (U32)offsetof(TEST_TEXTURED_BODY_FIXTURE, Textures);

    fixture->Triangle.P1 = 0;
    fixture->Triangle.P2 = 1;
    fixture->Triangle.P3 = 2;
    fixture->Triangle.HandleText = 0;
    fixture->Triangle.Couleur = 0;
    fixture->Triangle.Normale = kFixtureFaceLightIndex;
    fixture->Triangle.U1 = 0;
    fixture->Triangle.V1 = 0;
    fixture->Triangle.U2 = (U16)(128 << 8);
    fixture->Triangle.V2 = 0;
    fixture->Triangle.U3 = 0;
    fixture->Triangle.V3 = (U16)(128 << 8);

    fixture->Textures[0] = 0xFFFF0000u;
}

static void build_textured_quad_test_body_fixture(TEST_TEXTURED_QUAD_BODY_FIXTURE *fixture,
                                                  U16 poly_type)
{
    memset(fixture, 0, sizeof(*fixture));

    fixture->Header.Info = 0;
    fixture->Header.SizeHeader = (S16)sizeof(T_BODY_HEADER);
    fixture->Header.Dummy = 0;
    fixture->Header.XMin = 0;
    fixture->Header.XMax = 96;
    fixture->Header.YMin = 0;
    fixture->Header.YMax = 96;
    fixture->Header.ZMin = 256;
    fixture->Header.ZMax = 256;
    fixture->Header.NbGroupes = 1;
    fixture->Header.OffGroupes = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, Group);
    fixture->Header.NbPoints = kQuadFixturePointCount;
    fixture->Header.OffPoints = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, Points);
    fixture->Header.NbNormales = kQuadFixturePointCount;
    fixture->Header.OffNormales = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, Normals);
    fixture->Header.NbNormFaces = 1;
    fixture->Header.OffNormFaces = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, FaceNormals);
    fixture->Header.NbPolys = 1;
    fixture->Header.OffPolys = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, PolyHeader);
    fixture->Header.NbLines = 0;
    fixture->Header.OffLines = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, Textures);
    fixture->Header.NbSpheres = 0;
    fixture->Header.OffSpheres = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, Textures);
    fixture->Header.NbTextures = 1;
    fixture->Header.OffTextures = (S32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, Textures);

    fixture->Group.OrgGroupe = 0;
    fixture->Group.OrgPoint = 0;
    fixture->Group.NbPts = kQuadFixturePointCount;
    fixture->Group.NbNorm = 1;

    set_body_point(&fixture->Points[0], 0, 0, 256, 0);
    set_body_point(&fixture->Points[1], 96, 0, 256, 0);
    set_body_point(&fixture->Points[2], 96, 96, 256, 0);
    set_body_point(&fixture->Points[3], 0, 96, 256, 0);

    set_body_normal(&fixture->Normals[0], 15360, 0, 0, 0);
    set_body_normal(&fixture->Normals[1], 12288, 4096, 0, 0);
    set_body_normal(&fixture->Normals[2], 8192, 8192, 0, 0);
    set_body_normal(&fixture->Normals[3], 12288, 0, 4096, 0);
    set_body_normal(&fixture->FaceNormals[0], 15360, 0, 0, 0);

    fixture->PolyHeader.TypePoly = poly_type;
    fixture->PolyHeader.NbPoly = 1;
    fixture->PolyHeader.OffNextType = (U32)offsetof(TEST_TEXTURED_QUAD_BODY_FIXTURE, Textures);

    fixture->Quad.P1 = 0;
    fixture->Quad.P2 = 1;
    fixture->Quad.P3 = 2;
    fixture->Quad.P4 = 3;
    fixture->Quad.Couleur = 0;
    fixture->Quad.Normale = kQuadFixtureFaceLightIndex;
    fixture->Quad.U1 = 0;
    fixture->Quad.V1 = 0;
    fixture->Quad.U2 = (U16)(128 << 8);
    fixture->Quad.V2 = 0;
    fixture->Quad.U3 = (U16)(128 << 8);
    fixture->Quad.V3 = (U16)(128 << 8);
    fixture->Quad.U4 = 0;
    fixture->Quad.V4 = (U16)(128 << 8);
    fixture->Quad.HandleText = 0;
    fixture->Quad.Padding = 0;

    fixture->Textures[0] = 0xFFFF0000u;
}

static void build_env_test_body_fixture(TEST_ENV_BODY_FIXTURE *fixture,
                                        U16 poly_type,
                                        U16 scale)
{
    memset(fixture, 0, sizeof(*fixture));

    fixture->Header.Info = 0;
    fixture->Header.SizeHeader = (S16)sizeof(T_BODY_HEADER);
    fixture->Header.Dummy = 0;
    fixture->Header.XMin = 0;
    fixture->Header.XMax = 96;
    fixture->Header.YMin = 0;
    fixture->Header.YMax = 96;
    fixture->Header.ZMin = 256;
    fixture->Header.ZMax = 256;
    fixture->Header.NbGroupes = 1;
    fixture->Header.OffGroupes = (S32)offsetof(TEST_ENV_BODY_FIXTURE, Group);
    fixture->Header.NbPoints = kFixturePointCount;
    fixture->Header.OffPoints = (S32)offsetof(TEST_ENV_BODY_FIXTURE, Points);
    fixture->Header.NbNormales = kFixturePointCount;
    fixture->Header.OffNormales = (S32)offsetof(TEST_ENV_BODY_FIXTURE, Normals);
    fixture->Header.NbNormFaces = 1;
    fixture->Header.OffNormFaces = (S32)offsetof(TEST_ENV_BODY_FIXTURE, FaceNormals);
    fixture->Header.NbPolys = 1;
    fixture->Header.OffPolys = (S32)offsetof(TEST_ENV_BODY_FIXTURE, PolyHeader);
    fixture->Header.NbLines = 0;
    fixture->Header.OffLines = (S32)offsetof(TEST_ENV_BODY_FIXTURE, Textures);
    fixture->Header.NbSpheres = 0;
    fixture->Header.OffSpheres = (S32)offsetof(TEST_ENV_BODY_FIXTURE, Textures);
    fixture->Header.NbTextures = 1;
    fixture->Header.OffTextures = (S32)offsetof(TEST_ENV_BODY_FIXTURE, Textures);

    fixture->Group.OrgGroupe = 0;
    fixture->Group.OrgPoint = 0;
    fixture->Group.NbPts = kFixturePointCount;
    fixture->Group.NbNorm = 1;

    set_body_point(&fixture->Points[0], 0, 0, 256, 0);
    set_body_point(&fixture->Points[1], 96, 0, 256, 0);
    set_body_point(&fixture->Points[2], 0, 96, 256, 0);

    set_body_normal(&fixture->Normals[0], 15360, 0, 0, 0);
    set_body_normal(&fixture->Normals[1], 12288, 4096, 0, 0);
    set_body_normal(&fixture->Normals[2], 12288, 0, 4096, 0);
    set_body_normal(&fixture->FaceNormals[0], 15360, 0, 0, 0);

    fixture->PolyHeader.TypePoly = poly_type;
    fixture->PolyHeader.NbPoly = 1;
    fixture->PolyHeader.OffNextType = (U32)offsetof(TEST_ENV_BODY_FIXTURE, Textures);

    fixture->Triangle.P1 = 0;
    fixture->Triangle.P2 = 1;
    fixture->Triangle.P3 = 2;
    fixture->Triangle.HandleEnv = 0;
    fixture->Triangle.Couleur = 0;
    fixture->Triangle.Normale = kFixtureFaceLightIndex;
    fixture->Triangle.Scale = scale;
    fixture->Triangle.Padding = 0;

    fixture->Textures[0] = 0xFFFF0000u;
}

static void build_env_quad_test_body_fixture(TEST_ENV_QUAD_BODY_FIXTURE *fixture,
                                             U16 poly_type,
                                             U16 scale)
{
    memset(fixture, 0, sizeof(*fixture));

    fixture->Header.Info = 0;
    fixture->Header.SizeHeader = (S16)sizeof(T_BODY_HEADER);
    fixture->Header.Dummy = 0;
    fixture->Header.XMin = 0;
    fixture->Header.XMax = 96;
    fixture->Header.YMin = 0;
    fixture->Header.YMax = 96;
    fixture->Header.ZMin = 256;
    fixture->Header.ZMax = 256;
    fixture->Header.NbGroupes = 1;
    fixture->Header.OffGroupes = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, Group);
    fixture->Header.NbPoints = kQuadFixturePointCount;
    fixture->Header.OffPoints = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, Points);
    fixture->Header.NbNormales = kQuadFixturePointCount;
    fixture->Header.OffNormales = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, Normals);
    fixture->Header.NbNormFaces = 1;
    fixture->Header.OffNormFaces = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, FaceNormals);
    fixture->Header.NbPolys = 1;
    fixture->Header.OffPolys = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, PolyHeader);
    fixture->Header.NbLines = 0;
    fixture->Header.OffLines = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, Textures);
    fixture->Header.NbSpheres = 0;
    fixture->Header.OffSpheres = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, Textures);
    fixture->Header.NbTextures = 1;
    fixture->Header.OffTextures = (S32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, Textures);

    fixture->Group.OrgGroupe = 0;
    fixture->Group.OrgPoint = 0;
    fixture->Group.NbPts = kQuadFixturePointCount;
    fixture->Group.NbNorm = 1;

    set_body_point(&fixture->Points[0], 0, 0, 256, 0);
    set_body_point(&fixture->Points[1], 96, 0, 256, 0);
    set_body_point(&fixture->Points[2], 96, 96, 256, 0);
    set_body_point(&fixture->Points[3], 0, 96, 256, 0);

    set_body_normal(&fixture->Normals[0], 15360, 0, 0, 0);
    set_body_normal(&fixture->Normals[1], 12288, 4096, 0, 0);
    set_body_normal(&fixture->Normals[2], 8192, 8192, 0, 0);
    set_body_normal(&fixture->Normals[3], 12288, 0, 4096, 0);
    set_body_normal(&fixture->FaceNormals[0], 15360, 0, 0, 0);

    fixture->PolyHeader.TypePoly = poly_type;
    fixture->PolyHeader.NbPoly = 1;
    fixture->PolyHeader.OffNextType = (U32)offsetof(TEST_ENV_QUAD_BODY_FIXTURE, Textures);

    fixture->Quad.P1 = 0;
    fixture->Quad.P2 = 1;
    fixture->Quad.P3 = 2;
    fixture->Quad.P4 = 3;
    fixture->Quad.Couleur = 0;
    fixture->Quad.Normale = kQuadFixtureFaceLightIndex;
    fixture->Quad.Scale = scale;
    fixture->Quad.HandleEnv = 0;

    fixture->Textures[0] = 0xFFFF0000u;
}

static void build_test_object_fixture(T_OBJ_3D *obj, void *body, S32 x, S32 y,
                                      S32 z, S32 alpha, S32 beta, S32 gamma,
                                      U32 nb_groups, const T_GROUP_INFO *group_info)
{
    memset(obj, 0, sizeof(*obj));
    obj->X = x;
    obj->Y = y;
    obj->Z = z;
    obj->Alpha = alpha;
    obj->Beta = beta;
    obj->Gamma = gamma;
    obj->Body.Ptr = body;
    obj->Texture = NULL;
    obj->NbGroups = nb_groups;

    for (U32 index = 0; index < nb_groups && index < kAffObjGroupCount; ++index)
    {
        if (group_info != NULL)
        {
            obj->CurrentFrame[index] = group_info[index];
        }
        else
        {
            obj->CurrentFrame[index].Type = 0;
            obj->CurrentFrame[index].Alpha = 0;
            obj->CurrentFrame[index].Beta = 0;
            obj->CurrentFrame[index].Gamma = 0;
        }
    }
}

static void setup_common_aff_obj_environment(void)
{
    fesetenv(&g_initial_fenv);
    setup_polygon_screen();
    SetProjection(TEST_POLY_W / 2, TEST_POLY_H / 2, 1, 256, 256);
    CameraXr = 0;
    CameraYr = 0;
    CameraZr = 1000;
    CameraZrClip = CameraZr - NearClip;
    InitMatrixStd(&MatriceWorld, 0, 0, 0);
    X0 = 0;
    Y0 = 0;
    Z0 = 0;
    Xp = 0;
    Yp = 0;
}

static void setup_textured_aff_obj_environment(void)
{
    setup_common_aff_obj_environment();
    init_test_texture();
    init_test_clut();
    PtrCLUTGouraud = g_test_clut;
    Switch_Fillers(FILL_POLY_TEXTURES);
}

static void seed_cpp_aff_obj_state(void)
{
    memset(Obj_ListRotatedPoints, 0xA5, sizeof(T_OBJ_POINT) * kObjRotatedPointCount);
    memset(Obj_ListProjectedPoints, 0x5A, sizeof(TYPE_PT) * kObjProjectedPointCount);
    memset(ListLights, 0x33, sizeof(U16) * kAffObjLightCount);
    memset(ListSort, 0x44, sizeof(S32) * kAffObjSortCount);
    memset(TabMat, 0x66, sizeof(TYPE_MAT) * kAffObjGroupCount);
    memset(ListFillPoly, 0x77, sizeof(Struc_Point) * 4);
    lBody = (T_BODY_HEADER *)(uintptr_t)0x11111111u;
    Off_Sort = ListSort + 17;
    Nb_Sort = 0x22222222;
    PosXWr = 0x13572468;
    PosYWr = 0x24681357;
    PosZWr = 0x10293847;
    PtrTextures = (U32 *)(uintptr_t)0x33333333u;
    ObjPtrMap = NULL;
    TransFctBody = NULL;
}

static void seed_asm_aff_obj_state(void)
{
    memset(asm_Obj_ListRotatedPoints, 0xA5, sizeof(T_OBJ_POINT) * kObjRotatedPointCount);
    memset(asm_Obj_ListProjectedPoints, 0x5A, sizeof(TYPE_PT) * kObjProjectedPointCount);
    memset(asm_ListLights, 0x33, sizeof(U16) * kAffObjLightCount);
    memset(asm_ListSort, 0x44, sizeof(S32) * kAffObjSortCount);
    memset(asm_TabMat, 0x66, sizeof(TYPE_MAT) * kAffObjGroupCount);
    memset(asm_ListFillPoly, 0x77, sizeof(Struc_Point) * 4);
    asm_lBody = (T_BODY_HEADER *)(uintptr_t)0x11111111u;
    asm_Off_Sort = asm_ListSort + 17;
    asm_Nb_Sort = 0x22222222;
    asm_PosXWr = 0x13572468;
    asm_PosYWr = 0x24681357;
    asm_PosZWr = 0x10293847;
    asm_PtrTextures = (U32 *)(uintptr_t)0x33333333u;
    asm_ObjPtrMap = NULL;
    asm_TransFctBody = NULL;
}

static void capture_cpp_snapshot(RENDER_SNAPSHOT *snapshot)
{
    snapshot->ScreenXMinValue = ScreenXMin;
    snapshot->ScreenYMinValue = ScreenYMin;
    snapshot->ScreenXMaxValue = ScreenXMax;
    snapshot->ScreenYMaxValue = ScreenYMax;
    snapshot->PosXWrValue = PosXWr;
    snapshot->PosYWrValue = PosYWr;
    snapshot->PosZWrValue = PosZWr;
    snapshot->NbSortValue = Nb_Sort;
    snapshot->OffSortWords = (S32)(Off_Sort - ListSort);
    snapshot->BodyPointerValue = (uintptr_t)lBody;
    snapshot->PtrTexturesValue = (uintptr_t)PtrTextures;
    snapshot->X0Value = X0;
    snapshot->Y0Value = Y0;
    snapshot->Z0Value = Z0;
    snapshot->XpValue = Xp;
    snapshot->YpValue = Yp;
    snapshot->NonZeroPixels = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    memcpy(snapshot->RotatedPoints, Obj_ListRotatedPoints,
           sizeof(T_OBJ_POINT) * kObjRotatedPointCount);
    memcpy(snapshot->ProjectedPoints, Obj_ListProjectedPoints,
           sizeof(TYPE_PT) * kObjProjectedPointCount);
    memcpy(snapshot->ListLightsValues, ListLights,
           sizeof(U16) * kAffObjLightCount);
    memcpy(snapshot->ListSortValues, ListSort,
           sizeof(S32) * kAffObjSortCount);
    memcpy(snapshot->TabMatValues, TabMat,
           sizeof(TYPE_MAT) * kAffObjGroupCount);
    memcpy(snapshot->FillPolyValues, ListFillPoly, sizeof(Struc_Point) * 4);
    memcpy(snapshot->Framebuffer, g_poly_framebuf, sizeof(g_poly_framebuf));
}

static void capture_asm_snapshot(RENDER_SNAPSHOT *snapshot)
{
    snapshot->ScreenXMinValue = ScreenXMin;
    snapshot->ScreenYMinValue = ScreenYMin;
    snapshot->ScreenXMaxValue = ScreenXMax;
    snapshot->ScreenYMaxValue = ScreenYMax;
    snapshot->PosXWrValue = asm_PosXWr;
    snapshot->PosYWrValue = asm_PosYWr;
    snapshot->PosZWrValue = asm_PosZWr;
    snapshot->NbSortValue = asm_Nb_Sort;
    snapshot->OffSortWords = (S32)(asm_Off_Sort - asm_ListSort);
    snapshot->BodyPointerValue = (uintptr_t)asm_lBody;
    snapshot->PtrTexturesValue = (uintptr_t)asm_PtrTextures;
    snapshot->X0Value = X0;
    snapshot->Y0Value = Y0;
    snapshot->Z0Value = Z0;
    snapshot->XpValue = Xp;
    snapshot->YpValue = Yp;
    snapshot->NonZeroPixels = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    memcpy(snapshot->RotatedPoints, asm_Obj_ListRotatedPoints,
           sizeof(T_OBJ_POINT) * kObjRotatedPointCount);
    memcpy(snapshot->ProjectedPoints, asm_Obj_ListProjectedPoints,
           sizeof(TYPE_PT) * kObjProjectedPointCount);
    memcpy(snapshot->ListLightsValues, asm_ListLights,
           sizeof(U16) * kAffObjLightCount);
    memcpy(snapshot->ListSortValues, asm_ListSort,
           sizeof(S32) * kAffObjSortCount);
    memcpy(snapshot->TabMatValues, asm_TabMat,
           sizeof(TYPE_MAT) * kAffObjGroupCount);
    memcpy(snapshot->FillPolyValues, asm_ListFillPoly, sizeof(Struc_Point) * 4);
    memcpy(snapshot->Framebuffer, g_poly_framebuf, sizeof(g_poly_framebuf));
}

static void compare_render_snapshots(const char *label, const RENDER_SNAPSHOT *asm_snapshot,
                                     const RENDER_SNAPSHOT *cpp_snapshot)
{
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->Result, cpp_snapshot->Result, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->ScreenXMinValue, cpp_snapshot->ScreenXMinValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->ScreenYMinValue, cpp_snapshot->ScreenYMinValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->ScreenXMaxValue, cpp_snapshot->ScreenXMaxValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->ScreenYMaxValue, cpp_snapshot->ScreenYMaxValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->PosXWrValue, cpp_snapshot->PosXWrValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->PosYWrValue, cpp_snapshot->PosYWrValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->PosZWrValue, cpp_snapshot->PosZWrValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->NbSortValue, cpp_snapshot->NbSortValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->OffSortWords, cpp_snapshot->OffSortWords, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->BodyPointerValue, cpp_snapshot->BodyPointerValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->PtrTexturesValue, cpp_snapshot->PtrTexturesValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->X0Value, cpp_snapshot->X0Value, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->Y0Value, cpp_snapshot->Y0Value, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->Z0Value, cpp_snapshot->Z0Value, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->XpValue, cpp_snapshot->XpValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->YpValue, cpp_snapshot->YpValue, label);
    ASSERT_ASM_CPP_EQ_INT(asm_snapshot->NonZeroPixels, cpp_snapshot->NonZeroPixels, label);
    ASSERT_ASM_CPP_MEM_EQ(asm_snapshot->RotatedPoints, cpp_snapshot->RotatedPoints,
                          sizeof(asm_snapshot->RotatedPoints), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_snapshot->ProjectedPoints, cpp_snapshot->ProjectedPoints,
                          sizeof(asm_snapshot->ProjectedPoints), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_snapshot->ListLightsValues, cpp_snapshot->ListLightsValues,
                          sizeof(asm_snapshot->ListLightsValues), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_snapshot->ListSortValues, cpp_snapshot->ListSortValues,
                          sizeof(asm_snapshot->ListSortValues), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_snapshot->TabMatValues, cpp_snapshot->TabMatValues,
                          sizeof(asm_snapshot->TabMatValues), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_snapshot->FillPolyValues, cpp_snapshot->FillPolyValues,
                          sizeof(asm_snapshot->FillPolyValues), label);
    ASSERT_ASM_CPP_MEM_EQ(asm_snapshot->Framebuffer, cpp_snapshot->Framebuffer,
                          sizeof(asm_snapshot->Framebuffer), label);
}

static void run_bodydisplay_render_case(const char *label, S32 x, S32 y, S32 z,
                                        S32 alpha, S32 beta, S32 gamma,
                                        S32 expected_result, int expect_pixels)
{
    TEST_BODY_FIXTURE fixture;
    RENDER_SNAPSHOT cpp_snapshot;
    RENDER_SNAPSHOT asm_snapshot;

    build_test_body_fixture(&fixture);

    restore_cpp_3d_callbacks();
    setup_common_aff_obj_environment();
    seed_cpp_aff_obj_state();
    cpp_snapshot.Result = BodyDisplay(x, y, z, alpha, beta, gamma, &fixture);
    capture_cpp_snapshot(&cpp_snapshot);

    setup_common_aff_obj_environment();
    install_asm_compatible_3d_callbacks();
    seed_asm_aff_obj_state();
    asm_snapshot.Result = asm_BodyDisplay(x, y, z, alpha, beta, gamma, &fixture);
    capture_asm_snapshot(&asm_snapshot);

    compare_render_snapshots(label, &asm_snapshot, &cpp_snapshot);
    ASSERT_EQ_INT(expected_result, cpp_snapshot.Result);
    ASSERT_EQ_INT(expected_result, asm_snapshot.Result);
    if (expect_pixels)
    {
        ASSERT_TRUE(cpp_snapshot.NonZeroPixels > 0);
        ASSERT_TRUE(asm_snapshot.NonZeroPixels > 0);
        ASSERT_EQ_INT(1, cpp_snapshot.NbSortValue);
        ASSERT_EQ_INT(1, asm_snapshot.NbSortValue);
    }
    else
    {
        ASSERT_EQ_INT(0, cpp_snapshot.NonZeroPixels);
        ASSERT_EQ_INT(0, asm_snapshot.NonZeroPixels);
    }
}

static void run_bodydisplay_alphabeta_render_case(const char *label, S32 x, S32 y,
                                                  S32 z, S32 alpha, S32 beta,
                                                  S32 gamma, S32 expected_result,
                                                  int expect_pixels)
{
    TEST_BODY_FIXTURE fixture;
    RENDER_SNAPSHOT cpp_snapshot;
    RENDER_SNAPSHOT asm_snapshot;

    build_test_body_fixture(&fixture);

    restore_cpp_3d_callbacks();
    setup_common_aff_obj_environment();
    seed_cpp_aff_obj_state();
    cpp_snapshot.Result = BodyDisplay_AlphaBeta(x, y, z, alpha, beta, gamma, &fixture);
    capture_cpp_snapshot(&cpp_snapshot);

    setup_common_aff_obj_environment();
    install_asm_compatible_3d_callbacks();
    seed_asm_aff_obj_state();
    asm_snapshot.Result = asm_BodyDisplay_AlphaBeta(x, y, z, alpha, beta, gamma, &fixture);
    capture_asm_snapshot(&asm_snapshot);

    compare_render_snapshots(label, &asm_snapshot, &cpp_snapshot);
    ASSERT_EQ_INT(expected_result, cpp_snapshot.Result);
    ASSERT_EQ_INT(expected_result, asm_snapshot.Result);
    if (expect_pixels)
    {
        ASSERT_TRUE(cpp_snapshot.NonZeroPixels > 0);
        ASSERT_TRUE(asm_snapshot.NonZeroPixels > 0);
        ASSERT_EQ_INT(1, cpp_snapshot.NbSortValue);
        ASSERT_EQ_INT(1, asm_snapshot.NbSortValue);
    }
    else
    {
        ASSERT_EQ_INT(0, cpp_snapshot.NonZeroPixels);
        ASSERT_EQ_INT(0, asm_snapshot.NonZeroPixels);
    }
}

static void run_objectdisplay_render_case_ex(const char *label, void *fixture,
                                             U32 nb_groups,
                                             const T_GROUP_INFO *group_info,
                                             void *texture,
                                             AffObjEnvironmentSetupFn setup_environment,
                                             S32 x, S32 y, S32 z,
                                             S32 alpha, S32 beta, S32 gamma,
                                             S32 expected_result,
                                             S32 expected_sort_count,
                                             int expect_pixels)
{
    T_OBJ_3D cpp_object;
    T_OBJ_3D asm_object;
    RENDER_SNAPSHOT cpp_snapshot;
    RENDER_SNAPSHOT asm_snapshot;

    build_test_object_fixture(&cpp_object, fixture, x, y, z, alpha, beta, gamma,
                              nb_groups, group_info);
    build_test_object_fixture(&asm_object, fixture, x, y, z, alpha, beta, gamma,
                              nb_groups, group_info);
    cpp_object.Texture = texture;
    asm_object.Texture = texture;

    restore_cpp_3d_callbacks();
    setup_environment();
    seed_cpp_aff_obj_state();
    cpp_snapshot.Result = ObjectDisplay(&cpp_object);
    capture_cpp_snapshot(&cpp_snapshot);

    setup_environment();
    install_asm_compatible_3d_callbacks();
    seed_asm_aff_obj_state();
    asm_snapshot.Result = asm_ObjectDisplay(&asm_object);
    capture_asm_snapshot(&asm_snapshot);

    compare_render_snapshots(label, &asm_snapshot, &cpp_snapshot);
    ASSERT_EQ_INT(expected_result, cpp_snapshot.Result);
    ASSERT_EQ_INT(expected_result, asm_snapshot.Result);
    if (expected_sort_count >= 0)
    {
        ASSERT_EQ_INT(expected_sort_count, cpp_snapshot.NbSortValue);
        ASSERT_EQ_INT(expected_sort_count, asm_snapshot.NbSortValue);
    }
    if (expect_pixels)
    {
        ASSERT_TRUE(cpp_snapshot.NonZeroPixels > 0);
        ASSERT_TRUE(asm_snapshot.NonZeroPixels > 0);
    }
    else
    {
        ASSERT_EQ_INT(0, cpp_snapshot.NonZeroPixels);
        ASSERT_EQ_INT(0, asm_snapshot.NonZeroPixels);
    }
}

static void run_objectdisplay_render_case(const char *label, S32 x, S32 y, S32 z,
                                          S32 alpha, S32 beta, S32 gamma,
                                          S32 expected_result, int expect_pixels)
{
    TEST_BODY_FIXTURE fixture;

    build_test_body_fixture(&fixture);
    run_objectdisplay_render_case_ex(label, &fixture, 1, NULL, NULL,
                                     setup_common_aff_obj_environment, x, y, z,
                                     alpha, beta, gamma, expected_result,
                                     expected_result != 0 ? 1 : -1,
                                     expect_pixels);
}

static void test_bodydisplay_visible_render(void)
{
    ASSERT_EQ_INT(96, (int)sizeof(T_BODY_HEADER));
    ASSERT_EQ_INT(180, (int)sizeof(TEST_BODY_FIXTURE));
    run_bodydisplay_render_case("BodyDisplay visible render", 0, 0, 0,
                                64, 96, 32, 1, 0);
}

static void test_bodydisplay_hidden_render(void)
{
    run_bodydisplay_render_case("BodyDisplay clipped render", 3000, 0, 0,
                                64, 96, 32, 0, 0);
}

static void test_bodydisplay_alphabeta_visible_render(void)
{
    run_bodydisplay_alphabeta_render_case("BodyDisplay_AlphaBeta visible render",
                                          0, 0, 0, 96, 128, 64, 1, 0);
}

static void test_objectdisplay_visible_render(void)
{
    run_objectdisplay_render_case("ObjectDisplay visible render", 0, 0, 0,
                                  64, 96, 32, 1, 1);
}

static void test_objectdisplay_hidden_render(void)
{
    run_objectdisplay_render_case("ObjectDisplay clipped render", 3000, 0, 0,
                                  64, 96, 32, 0, 0);
}

static void test_objectdisplay_multigroup_visible_render(void)
{
    TEST_COMPLEX_BODY_FIXTURE fixture;
    T_GROUP_INFO group_info[kComplexGroupCount];

    ASSERT_TRUE((int)sizeof(TEST_COMPLEX_BODY_FIXTURE) > (int)sizeof(TEST_BODY_FIXTURE));

    build_complex_test_body_fixture(&fixture);
    memset(group_info, 0, sizeof(group_info));
    group_info[1].Type = TYPE_ROTATE;
    group_info[1].Alpha = 96;
    group_info[1].Beta = 64;
    group_info[1].Gamma = 0;

    run_objectdisplay_render_case_ex("ObjectDisplay multigroup visible render",
                                     &fixture, kComplexGroupCount, group_info, NULL,
                                     setup_common_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 2, 1);
}

static void test_objectdisplay_multigroup_translate_render(void)
{
    TEST_COMPLEX_BODY_FIXTURE fixture;
    T_GROUP_INFO group_info[kComplexGroupCount];

    build_complex_test_body_fixture(&fixture);
    memset(group_info, 0, sizeof(group_info));
    group_info[1].Type = TYPE_TRANSLATE;
    group_info[1].Alpha = 80;
    group_info[1].Beta = -32;
    group_info[1].Gamma = 48;

    run_objectdisplay_render_case_ex("ObjectDisplay multigroup translate render",
                                     &fixture, kComplexGroupCount, group_info, NULL,
                                     setup_common_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 2, 1);
}

static void test_objectdisplay_textured_flat_render(void)
{
    TEST_TEXTURED_BODY_FIXTURE fixture;

    ASSERT_TRUE((int)sizeof(TEST_TEXTURED_BODY_FIXTURE) > (int)sizeof(TEST_BODY_FIXTURE));

    build_textured_test_body_fixture(&fixture, kPolyTextureFlat);
    run_objectdisplay_render_case_ex("ObjectDisplay textured flat render",
                                     &fixture, 1, NULL, g_test_texture,
                                     setup_textured_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 1, 1);
}

static void test_objectdisplay_textured_z_flat_render(void)
{
    TEST_TEXTURED_BODY_FIXTURE fixture;

    ASSERT_TRUE((int)sizeof(TEST_TEXTURED_BODY_FIXTURE) > (int)sizeof(TEST_BODY_FIXTURE));

    build_textured_test_body_fixture(&fixture, kPolyTextureZFlat);
    run_objectdisplay_render_case_ex("ObjectDisplay textured Z flat render",
                                     &fixture, 1, NULL, g_test_texture,
                                     setup_textured_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 1, 1);
}

static void test_objectdisplay_textured_solid_render(void)
{
    TEST_TEXTURED_BODY_FIXTURE fixture;

    build_textured_test_body_fixture(&fixture, kPolyTextureSolid);
    run_objectdisplay_render_case_ex("ObjectDisplay textured solid render",
                                     &fixture, 1, NULL, g_test_texture,
                                     setup_textured_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 1, 1);
}

static void test_objectdisplay_textured_gouraud_render(void)
{
    TEST_TEXTURED_BODY_FIXTURE fixture;

    build_textured_test_body_fixture(&fixture, kPolyTextureGouraud);
    run_objectdisplay_render_case_ex("ObjectDisplay textured gouraud render",
                                     &fixture, 1, NULL, g_test_texture,
                                     setup_textured_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 1, 1);
}

static void test_objectdisplay_textured_quad_flat_render(void)
{
    TEST_TEXTURED_QUAD_BODY_FIXTURE fixture;

    build_textured_quad_test_body_fixture(&fixture, (U16)(kPolyQuadMask | kPolyTextureFlat));
    run_objectdisplay_render_case_ex("ObjectDisplay textured quad flat render",
                                     &fixture, 1, NULL, g_test_texture,
                                     setup_textured_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 1, 1);
}

static void test_objectdisplay_env_flat_render(void)
{
    TEST_ENV_BODY_FIXTURE fixture;

    ASSERT_TRUE((int)sizeof(TEST_ENV_BODY_FIXTURE) > (int)sizeof(TEST_BODY_FIXTURE));

    build_env_test_body_fixture(&fixture, kPolyEnvTextureFlat, 0);
    run_objectdisplay_render_case_ex("ObjectDisplay env flat render",
                                     &fixture, 1, NULL, g_test_texture,
                                     setup_textured_aff_obj_environment,
                                     0, 0, 0, 64, 96, 32, 1, 1, 1);
}

static void test_quicksort_fixed_cases(void)
{
    static const S32 ascending_small[] = {1, 3, 5, 7, 9, 11};
    static const S32 ascending_mixed[] = {-12, -4, 0, 6, 18, 27};
    static const S32 ascending_long[] = {2, 4, 8, 16, 32, 64, 128, 256};

    run_sort_case("QuickSort ascending small", ascending_small, sizeof(ascending_small) / sizeof(ascending_small[0]));
    run_sort_case("QuickSort ascending mixed", ascending_mixed, sizeof(ascending_mixed) / sizeof(ascending_mixed[0]));
    run_sort_case("QuickSort ascending long", ascending_long, sizeof(ascending_long) / sizeof(ascending_long[0]));
}

static void test_testvisible_fixed_cases(void)
{
    TYPE_PT points[4];
    STRUC_POLY3_LIGHT light_visible = {0, 1, 2, 0, 0x12, 0x34};
    STRUC_POLY3_LIGHT light_hidden = {0, 2, 1, 0, 0x56, 0x78};
    STRUC_POLY3_ENV env_visible = {0, 1, 2, 3, 0x9A, 0xBC, 0x11, 0};
    STRUC_POLY3_ENV env_hidden = {0, 2, 1, 4, 0xCD, 0xEF, 0x22, 0};

    set_point(&points[0], 0, 0);
    set_point(&points[1], 0, 10);
    set_point(&points[2], 10, 0);
    set_point(&points[3], 5, 5);

    run_testvisiblei_case("TestVisibleI visible winding", points, 4, &light_visible);
    run_testvisiblei_case("TestVisibleI hidden winding", points, 4, &light_hidden);
    run_testvisiblef_case("TestVisibleF visible winding", points, 4, &env_visible);
    run_testvisiblef_case("TestVisibleF hidden winding", points, 4, &env_hidden);
}

static void test_testvisible_edge_cases(void)
{
    TYPE_PT points[4];
    STRUC_POLY3_LIGHT light_invalid = {0, 1, 2, 0, 1, 2};
    STRUC_POLY3_LIGHT light_collinear = {0, 1, 2, 0, 3, 4};
    STRUC_POLY3_ENV env_invalid = {0, 1, 2, 5, 6, 7, 8, 0};
    STRUC_POLY3_ENV env_duplicate = {0, 1, 1, 9, 10, 11, 12, 0};

    set_hidden_point(&points[0]);
    set_point(&points[1], 6, -2);
    set_point(&points[2], -4, 9);
    set_point(&points[3], 3, 3);
    run_testvisiblei_case("TestVisibleI invalid hidden point", points, 4, &light_invalid);

    set_point(&points[0], -8, -8);
    set_point(&points[1], 0, 0);
    set_point(&points[2], 8, 8);
    set_point(&points[3], 12, -4);
    run_testvisiblei_case("TestVisibleI collinear", points, 4, &light_collinear);

    set_point(&points[0], -7, 11);
    set_hidden_point(&points[1]);
    set_point(&points[2], 4, -5);
    set_point(&points[3], 1, 1);
    run_testvisiblef_case("TestVisibleF invalid hidden point", points, 4, &env_invalid);

    set_point(&points[0], 2, 2);
    set_point(&points[1], -6, 5);
    set_point(&points[2], 9, -3);
    set_point(&points[3], -1, -1);
    run_testvisiblef_case("TestVisibleF duplicate point", points, 4, &env_duplicate);
}

static void test_quicksortinv_fixed_cases(void)
{
    static const S32 descending_small[] = {11, 9, 7, 5, 3, 1};
    static const S32 descending_mixed[] = {27, 18, 6, 0, -4, -12};
    static const S32 descending_long[] = {256, 128, 64, 32, 16, 8, 4, 2};

    run_sortinv_case("QuickSortInv descending small", descending_small, sizeof(descending_small) / sizeof(descending_small[0]));
    run_sortinv_case("QuickSortInv descending mixed", descending_mixed, sizeof(descending_mixed) / sizeof(descending_mixed[0]));
    run_sortinv_case("QuickSortInv descending long", descending_long, sizeof(descending_long) / sizeof(descending_long[0]));
}

static void test_quicksort_edge_cases(void)
{
    static const S32 pair_ascending[] = {1, 2};
    static const S32 pair_descending[] = {2, 1};
    static const S32 single[] = {42};

    run_sort_case("QuickSort pair ascending", pair_ascending, sizeof(pair_ascending) / sizeof(pair_ascending[0]));
    run_sort_case("QuickSort single", single, sizeof(single) / sizeof(single[0]));
    run_sortinv_case("QuickSortInv pair descending", pair_descending, sizeof(pair_descending) / sizeof(pair_descending[0]));
    run_sortinv_case("QuickSortInv single", single, sizeof(single) / sizeof(single[0]));
}

static void test_quicksort_random_stress(void)
{
    S32 values[32];

    rng_seed(0xDEADBEEFu);
    for (int round = 0; round < 300; ++round)
    {
        U32 count = (rng_next() % 31) + 2;

        fill_strictly_increasing(values, count);
        run_sort_case("QuickSort random", values, count);

        fill_strictly_decreasing(values, count);
        run_sortinv_case("QuickSortInv random", values, count);
    }
}

static void test_testvisible_random_stress(void)
{
    TYPE_PT points[8];

    rng_seed(0x13572468u);
    for (int round = 0; round < 300; ++round)
    {
        STRUC_POLY3_LIGHT light_poly;
        STRUC_POLY3_ENV env_poly;

        for (int index = 0; index < 8; ++index)
        {
            if ((rng_next() % 7u) == 0)
            {
                set_hidden_point(&points[index]);
            }
            else
            {
                set_point(&points[index],
                          (S16)((S32)(rng_next() % 401u) - 200),
                          (S16)((S32)(rng_next() % 401u) - 200));
            }
        }

        light_poly.P1 = (U16)(rng_next() % 8u);
        light_poly.P2 = (U16)(rng_next() % 8u);
        light_poly.P3 = (U16)(rng_next() % 8u);
        light_poly.Padding = 0;
        light_poly.Couleur = (U16)rng_next();
        light_poly.Normale = (U16)rng_next();

        env_poly.P1 = (U16)(rng_next() % 8u);
        env_poly.P2 = (U16)(rng_next() % 8u);
        env_poly.P3 = (U16)(rng_next() % 8u);
        env_poly.HandleEnv = (U16)rng_next();
        env_poly.Couleur = (U16)rng_next();
        env_poly.Normale = (U16)rng_next();
        env_poly.Scale = (U16)rng_next();
        env_poly.Padding = 0;

        run_testvisiblei_case("TestVisibleI random", points, 8, &light_poly);
        run_testvisiblef_case("TestVisibleF random", points, 8, &env_poly);
    }
}

static void test_quicksort_random_unordered(void)
{
    S32 values[32];

    rng_seed(0xA5A55A5Au);
    for (int round = 0; round < 300; ++round)
    {
        U32 count = (rng_next() % 30) + 3;

        fill_random_unordered_quicksort_values(values, count);
        run_sort_case("QuickSort random unordered", values, count);

        fill_random_unordered_quicksortinv_values(values, count);
        run_sortinv_case("QuickSortInv random unordered", values, count);
    }
}

int main(void)
{
    fegetenv(&g_initial_fenv);

    RUN_TEST(test_bodydisplay_visible_render);
    RUN_TEST(test_bodydisplay_hidden_render);
    RUN_TEST(test_bodydisplay_alphabeta_visible_render);
    RUN_TEST(test_objectdisplay_visible_render);
    RUN_TEST(test_objectdisplay_hidden_render);
    RUN_TEST(test_objectdisplay_multigroup_visible_render);
    RUN_TEST(test_objectdisplay_multigroup_translate_render);
    RUN_TEST(test_objectdisplay_textured_solid_render);
    RUN_TEST(test_objectdisplay_textured_flat_render);
    RUN_TEST(test_objectdisplay_textured_gouraud_render);
    RUN_TEST(test_objectdisplay_textured_z_flat_render);
    RUN_TEST(test_objectdisplay_textured_quad_flat_render);
    RUN_TEST(test_objectdisplay_env_flat_render);
    RUN_TEST(test_testvisible_fixed_cases);
    RUN_TEST(test_testvisible_edge_cases);
    RUN_TEST(test_testvisible_random_stress);
    RUN_TEST(test_quicksort_fixed_cases);
    RUN_TEST(test_quicksortinv_fixed_cases);
    RUN_TEST(test_quicksort_edge_cases);
    RUN_TEST(test_quicksort_random_stress);
    RUN_TEST(test_quicksort_random_unordered);
    TEST_SUMMARY();
    return test_failures != 0;
}