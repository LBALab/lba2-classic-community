#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LE.H>
#include <ANIM/LIBINIT.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
    S16 Type;
    S16 Alpha;
    S16 Beta;
    S16 Gamma;
} T_GROUP_INFO;
#pragma pack(pop)

#pragma pack(push, 1)
// The game stores a body or animation number through Ptr and reads it back as
// Num. On a 64-bit big-endian CPU an S32 Num would overlap the pointer's high
// half, so there it is as wide as the pointer.
typedef union {
    void *Ptr;
#if LBA_BIG_ENDIAN && defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
    S64 Num;
#else
    S32 Num;
#endif
} T_PTR_NUM;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    S32 X; // world coor
    S32 Y;
    S32 Z;

    S32 Alpha; // local angles
    S32 Beta;
    S32 Gamma;

    T_PTR_NUM Body;
    ; // ->.O3D
    T_PTR_NUM NextBody;
    ;               // ->.O3D
    T_PTR_NUM Anim; // ->.A3D

    void *Texture;     //
    void *NextTexture; //

    U32 LastOfsIsPtr;

    S32 LastFrame;
    PTR_U32 LastOfsFrame;
    U32 LastTimer; // last TimerRef
    U32 LastNbGroups;

    S32 NextFrame;
    PTR_U32 NextOfsFrame;
    U32 NextTimer; // TimerRef when frame reached
    U32 NextNbGroups;

    S32 LoopFrame;
    U32 LoopOfsFrame;

    U32 NbFrames;

    S32 LastAnimStepX;
    S32 LastAnimStepY;
    S32 LastAnimStepZ;

    S32 LastAnimStepAlpha;
    S32 LastAnimStepBeta;
    S32 LastAnimStepGamma;

    U32 Interpolator; // lib internal

    U32 Time; // TimerRef last modif

    U32 Status; // BitField FLAG_*

    U32 Master; // BitField ANIM_MASTER_ROT

    U32 NbGroups;

    T_GROUP_INFO CurrentFrame[30];
} T_OBJ_3D;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    S16 X;
    S16 Y;
    S16 Z;
    S16 Group;
} T_OBJ_POINT;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    S32 Info;
    S16 SizeHeader;
    S16 Dummy;
    S32 XMin;
    S32 XMax;
    S32 YMin;
    S32 YMax;
    S32 ZMin;
    S32 ZMax;
    S32 NbGroupes;
    S32 OffGroupes;
    S32 NbPoints;
    S32 OffPoints;
    S32 NbNormales;
    S32 OffNormales;
    S32 NbNormFaces;
    S32 OffNormFaces;
    S32 NbPolys;
    S32 OffPolys;
    S32 NbLines;
    S32 OffLines;
    S32 NbSpheres;
    S32 OffSpheres;
    S32 NbTextures;
    S32 OffTextures;
} T_BODY_HEADER;
#pragma pack(pop)

// -----------------------------------------------------------------------------
#define TYPE_ROTATE 0
#define TYPE_TRANSLATE 1
#define MASK_OBJECT_VERSION (0xff) //; Dans Obj_Info
#define MASK_OBJECT_ANIMATED (1 << 8)
#define MASK_OBJECT_NO_SORT (1 << 9)
#define MASK_OBJECT_TRANSPARENT (1 << 10)

// -----------------------------------------------------------------------------
extern U8 *ObjPtrMap; // Ptr on the current texture page
extern T_OBJ_POINT Obj_ListRotatedPoints[];

extern Func_TransNumPtr *TransFctBody; // Pointer to a conversion fct that converts a body number into its address

// -----------------------------------------------------------------------------
extern S32 ObjectDisplay(T_OBJ_3D *obj);
extern S32 BodyDisplay(S32 x, S32 y, S32 z, S32 alpha, S32 beta, S32 gamma,
                       void *obj);
extern S32 BodyDisplay_AlphaBeta(S32 x, S32 y, S32 z, S32 alpha, S32 beta,
                                 S32 gamma, void *obj);

// Converts a freshly loaded body from little-endian to the CPU's order, in place
extern void ObjectBodyFromLE(void *body, S32 size);

// =============================================================================
#ifdef __cplusplus
}
#endif
