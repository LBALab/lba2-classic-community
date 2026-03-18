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

enum
{
    kPolyFlat = 1,
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
    U16 HandleEnv;
    U16 Couleur;
    U16 Normale;
    U16 Scale;
    U16 Padding;
} STRUC_POLY3_ENV;

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
#pragma pack(pop)

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
    RotateMatrix = RotateMatrixU;
    RotTransList = RotTransListF;
    ProjectList = ProjectList3DF;
    InitMatrixTrans = InitMatrixTransF;
    LightListPtr = LightList;
}

static void install_asm_compatible_3d_callbacks(void)
{
    LongRotatePoint = (Func_LongRotatePoint *)AsmAbi_LongRotatePoint_Thunk;
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

static void build_test_object_fixture(T_OBJ_3D *obj, void *body, S32 x, S32 y,
                                      S32 z, S32 alpha, S32 beta, S32 gamma)
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
    obj->NbGroups = 1;
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

static void run_objectdisplay_render_case(const char *label, S32 x, S32 y, S32 z,
                                          S32 alpha, S32 beta, S32 gamma,
                                          S32 expected_result, int expect_pixels)
{
    TEST_BODY_FIXTURE fixture;
    T_OBJ_3D cpp_object;
    T_OBJ_3D asm_object;
    RENDER_SNAPSHOT cpp_snapshot;
    RENDER_SNAPSHOT asm_snapshot;

    build_test_body_fixture(&fixture);
    build_test_object_fixture(&cpp_object, &fixture, x, y, z, alpha, beta, gamma);
    build_test_object_fixture(&asm_object, &fixture, x, y, z, alpha, beta, gamma);

    restore_cpp_3d_callbacks();
    setup_common_aff_obj_environment();
    seed_cpp_aff_obj_state();
    cpp_snapshot.Result = ObjectDisplay(&cpp_object);
    capture_cpp_snapshot(&cpp_snapshot);

    setup_common_aff_obj_environment();
    install_asm_compatible_3d_callbacks();
    seed_asm_aff_obj_state();
    asm_snapshot.Result = asm_ObjectDisplay(&asm_object);
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