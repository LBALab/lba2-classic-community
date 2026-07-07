#ifndef SAVEGAME_WIRE_H
#define SAVEGAME_WIRE_H

/* Fixed 32-bit-ABI wire mirrors of the three pointer-bearing structs the save
 * payload serializes: T_OBJ_3D (per-object tail), T_EXTRA, and S_PART_FLOW.
 * These pin the on-disk layout the 32-bit retail engine wrote, independent of
 * host word size, so a 64-bit build reads and writes the same bytes a 32-bit
 * build did.  See docs/SAVE_WIRE_PLAN.md (derivation + section map) and
 * docs/ABI.md (the "never sizeof(T)-as-stride for fat structs" rule).
 *
 * Retail 32-bit wire sizes: T_OBJ_3D_WIRE32 = 136, T_EXTRA_WIRE32 = 68,
 * S_PART_FLOW_WIRE32 = 60.  The per-object stride is 140 (the field-by-field
 * scalar prefix) + 136 = 276.  The original game structs are frozen; all layout
 * logic lives here.
 */

/* Minimal, individually guarded type headers only - never the guardless
 * DEFINES.H / C_EXTERN.H chain, so this header is safe to include in a TU that
 * already pulled them (e.g. SAVEGAME.CPP) without redefinition. */
#include <SYSTEM/ADELINE_TYPES.H>
#include <OBJECT/AFF_OBJ.H> /* T_OBJ_3D, T_PTR_NUM, PTR_U32 */
#include <3D/MOVE.H>        /* MOVE - used by T_EXTRA's union in COMMON.H */
#include "COMMON.H"         /* T_EXTRA */
#include "FLOW.H"           /* S_PART_FLOW, S_ONE_DOT */

#ifndef EDITLBA2

/* ── T_OBJ_3D wire (136 B, pack(1), the native struct minus CurrentFrame) ── */
#pragma pack(push, 1)
typedef struct {
    S32 X;
    S32 Y;
    S32 Z;
    S32 Alpha;
    S32 Beta;
    S32 Gamma;
    S32 BodyNum;
    S32 NextBodyNum;
    S32 AnimNum;
    U32 Texture;
    U32 NextTexture;
    U32 LastOfsIsPtr;
    S32 LastFrame;
    U32 LastOfsFrame;
    U32 LastTimer;
    U32 LastNbGroups;
    S32 NextFrame;
    U32 NextOfsFrame;
    U32 NextTimer;
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
    U32 Interpolator;
    U32 Time;
    U32 Status;
    U32 Master;
    U32 NbGroups;
} T_OBJ_3D_WIRE32;
#pragma pack(pop)

/* ── T_EXTRA wire (68 B, pack(1)) ── */
#pragma pack(push, 1)
typedef struct {
    S32 PosX, PosY, PosZ;                /* 12 */
    S32 U_0, U_1, U_2;                   /* 12  union { MOVE | Org } as opaque 12B */
    S32 Info;                            /*  4 */
    U32 PtrBody_w32;                     /*  4  32-bit pointer slot (value discarded) */
    S16 Sprite;                          /*  2 */
    S16 Vx, Vy, Vz;                      /*  6 */
    U32 Flags;                           /*  4 */
    U32 Timer;                           /*  4 */
    S16 Body, Beta;                      /*  4 */
    S16 TimeOut, Divers;                 /*  4 */
    U8 Poids, HitForce, Owner, NewForce; /* 4 */
    S32 Impact;                          /*  4 */
    S32 Scale;                           /*  4 */
} T_EXTRA_WIRE32;                        /* total: 68 bytes */
#pragma pack(pop)

/* ── S_PART_FLOW wire (60 B, pack(1)) ── */
#pragma pack(push, 1)
typedef struct {
    S32 Flag;
    S32 Owner;
    S32 NumPoint;
    S32 NbDot;
    S32 OrgX, OrgY, OrgZ;
    S32 XMin, YMin, ZMin;
    S32 XMax, YMax, ZMax;
    U32 FlowTimerStart;
    U32 PtrListDot_w32; /* 32-bit pointer slot (value discarded) */
} S_PART_FLOW_WIRE32;   /* total: 60 bytes */
#pragma pack(pop)

/* Readers: decode a 32-bit wire struct into the native struct.  Pointer slots
 * are dropped (Texture/PtrBody refilled from runtime tables; PtrListDot restored
 * by the caller).  Implemented in SAVEGAME_WIRE.CPP. */
void SavegameObj3dFromWire32(const T_OBJ_3D_WIRE32 *s, T_OBJ_3D *d);
void SavegameExtraFromWire32(const T_EXTRA_WIRE32 *s, T_EXTRA *d);
void SavegameFlowFromWire32(const S_PART_FLOW_WIRE32 *s, S_PART_FLOW *d);

/* Writers: encode the native struct into a 32-bit wire struct, the exact inverse
 * of the readers above (pointer slots written as 0).  Phase 2 of
 * docs/SAVE_WIRE_PLAN.md - intentionally not yet defined, so the round-trip test
 * (tests/save_wire/test_save_wire_to32.cpp) fails to link until they exist. */
void SavegameObj3dToWire32(const T_OBJ_3D *s, T_OBJ_3D_WIRE32 *d);
void SavegameExtraToWire32(const T_EXTRA *s, T_EXTRA_WIRE32 *d);
void SavegameFlowToWire32(const S_PART_FLOW *s, S_PART_FLOW_WIRE32 *d);

#endif /* !EDITLBA2 */

#endif /* SAVEGAME_WIRE_H */
