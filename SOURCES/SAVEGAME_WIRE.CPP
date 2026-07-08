/* Wire-format converters for the save payload's pointer-bearing structs.
 * Layout and rationale: SAVEGAME_WIRE.H + docs/SAVE_WIRE_PLAN.md.
 *
 * The readers (...FromWire32) decode a 32-bit wire struct into the native
 * struct; the writers (...ToWire32) are their exact inverses.  Both are unit
 * tested in isolation against hand-derived golden byte-vectors and a round-trip
 * fuzz (tests/save_wire/). */

#include "SAVEGAME_WIRE.H"

#include <stdint.h>
#include <string.h>

#ifndef EDITLBA2

void SavegameObj3dFromWire32(const T_OBJ_3D_WIRE32 *s, T_OBJ_3D *d) {
    memset(d, 0, sizeof(*d));
    d->X = s->X;
    d->Y = s->Y;
    d->Z = s->Z;
    d->Alpha = s->Alpha;
    d->Beta = s->Beta;
    d->Gamma = s->Gamma;
    d->Body.Num = s->BodyNum;
    d->NextBody.Num = s->NextBodyNum;
    d->Anim.Num = s->AnimNum;
    d->Texture = NULL;
    d->NextTexture = NULL;
    d->LastOfsIsPtr = s->LastOfsIsPtr;
    d->LastFrame = s->LastFrame;
    d->LastOfsFrame = (PTR_U32)(uintptr_t)(size_t)s->LastOfsFrame;
    d->LastTimer = s->LastTimer;
    d->LastNbGroups = s->LastNbGroups;
    d->NextFrame = s->NextFrame;
    d->NextOfsFrame = (PTR_U32)(uintptr_t)(size_t)s->NextOfsFrame;
    d->NextTimer = s->NextTimer;
    d->NextNbGroups = s->NextNbGroups;
    d->LoopFrame = s->LoopFrame;
    d->LoopOfsFrame = s->LoopOfsFrame;
    d->NbFrames = s->NbFrames;
    d->LastAnimStepX = s->LastAnimStepX;
    d->LastAnimStepY = s->LastAnimStepY;
    d->LastAnimStepZ = s->LastAnimStepZ;
    d->LastAnimStepAlpha = s->LastAnimStepAlpha;
    d->LastAnimStepBeta = s->LastAnimStepBeta;
    d->LastAnimStepGamma = s->LastAnimStepGamma;
    d->Interpolator = s->Interpolator;
    d->Time = s->Time;
    d->Status = s->Status;
    d->Master = s->Master;
    d->NbGroups = s->NbGroups;
}

void SavegameExtraFromWire32(const T_EXTRA_WIRE32 *s, T_EXTRA *d) {
    memset(d, 0, sizeof(*d));
    d->PosX = s->PosX;
    d->PosY = s->PosY;
    d->PosZ = s->PosZ;
    /* Union is 12 raw bytes either way (MOVE = 3xS32, Org = 3xS32). */
    memcpy(&d->U, &s->U_0, 12);
    d->Info = s->Info;
    d->PtrBody = NULL; /* engine refills from runtime body table */
    d->Sprite = s->Sprite;
    d->Vx = s->Vx;
    d->Vy = s->Vy;
    d->Vz = s->Vz;
    d->Flags = s->Flags;
    d->Timer = s->Timer;
    d->Body = s->Body;
    d->Beta = s->Beta;
    d->TimeOut = s->TimeOut;
    d->Divers = s->Divers;
    d->Poids = s->Poids;
    d->HitForce = s->HitForce;
    d->Owner = s->Owner;
    d->NewForce = s->NewForce;
    d->Impact = s->Impact;
    d->Scale = s->Scale;
}

void SavegameFlowFromWire32(const S_PART_FLOW_WIRE32 *s, S_PART_FLOW *d) {
    /* No memset here (unlike the Obj3d/Extra readers): S_PART_FLOW's only pointer,
     * PtrListDot, is caller-managed (stashed and restored around this call), so the
     * reader must not touch it. All 14 scalar fields are copied explicitly below;
     * the round-trip fuzz catches any reader/writer field asymmetry. */
    d->Flag = s->Flag;
    d->Owner = s->Owner;
    d->NumPoint = s->NumPoint;
    d->NbDot = s->NbDot;
    d->OrgX = s->OrgX;
    d->OrgY = s->OrgY;
    d->OrgZ = s->OrgZ;
    d->XMin = s->XMin;
    d->YMin = s->YMin;
    d->ZMin = s->ZMin;
    d->XMax = s->XMax;
    d->YMax = s->YMax;
    d->ZMax = s->ZMax;
    d->FlowTimerStart = s->FlowTimerStart;
}

/* ── writers (native -> wire), exact inverses of the readers above ──────────
 *
 * Pointer-slot policy mirrors the read side field-by-field:
 *   - Slots the reader DISCARDS (Texture, NextTexture -> NULL; PtrBody -> NULL;
 *     PtrListDot -> restored externally) are written as 0.  This makes our
 *     output deterministic and byte-stable across write->read->write, at the
 *     cost of not byte-matching a 32-bit build that memcpy'd a live pointer's
 *     low word here.  See docs/SAVE_WIRE_PLAN.md decision 4.
 *   - Slots the reader PRESERVES (LastOfsFrame, NextOfsFrame; cast wire<->ptr)
 *     are written as the low 32 bits of the native value, so the offset the
 *     game needs after load round-trips exactly.
 */

void SavegameObj3dToWire32(const T_OBJ_3D *s, T_OBJ_3D_WIRE32 *d) {
    d->X = s->X;
    d->Y = s->Y;
    d->Z = s->Z;
    d->Alpha = s->Alpha;
    d->Beta = s->Beta;
    d->Gamma = s->Gamma;
    d->BodyNum = s->Body.Num;
    d->NextBodyNum = s->NextBody.Num;
    d->AnimNum = s->Anim.Num;
    d->Texture = 0;     /* discarded on read (refilled from runtime tables) */
    d->NextTexture = 0; /* discarded on read */
    d->LastOfsIsPtr = s->LastOfsIsPtr;
    d->LastFrame = s->LastFrame;
    d->LastOfsFrame = (U32)(uintptr_t)s->LastOfsFrame; /* preserved on read */
    d->LastTimer = s->LastTimer;
    d->LastNbGroups = s->LastNbGroups;
    d->NextFrame = s->NextFrame;
    d->NextOfsFrame = (U32)(uintptr_t)s->NextOfsFrame; /* preserved on read */
    d->NextTimer = s->NextTimer;
    d->NextNbGroups = s->NextNbGroups;
    d->LoopFrame = s->LoopFrame;
    d->LoopOfsFrame = s->LoopOfsFrame; /* plain U32, not a pointer */
    d->NbFrames = s->NbFrames;
    d->LastAnimStepX = s->LastAnimStepX;
    d->LastAnimStepY = s->LastAnimStepY;
    d->LastAnimStepZ = s->LastAnimStepZ;
    d->LastAnimStepAlpha = s->LastAnimStepAlpha;
    d->LastAnimStepBeta = s->LastAnimStepBeta;
    d->LastAnimStepGamma = s->LastAnimStepGamma;
    d->Interpolator = s->Interpolator;
    d->Time = s->Time;
    d->Status = s->Status;
    d->Master = s->Master;
    d->NbGroups = s->NbGroups;
}

void SavegameExtraToWire32(const T_EXTRA *s, T_EXTRA_WIRE32 *d) {
    d->PosX = s->PosX;
    d->PosY = s->PosY;
    d->PosZ = s->PosZ;
    /* Union is 12 raw bytes either way (MOVE = 3xS32, Org = 3xS32). */
    memcpy(&d->U_0, &s->U, 12);
    d->Info = s->Info;
    d->PtrBody_w32 = 0; /* discarded on read (refilled from runtime body table) */
    d->Sprite = s->Sprite;
    d->Vx = s->Vx;
    d->Vy = s->Vy;
    d->Vz = s->Vz;
    d->Flags = s->Flags;
    d->Timer = s->Timer;
    d->Body = s->Body;
    d->Beta = s->Beta;
    d->TimeOut = s->TimeOut;
    d->Divers = s->Divers;
    d->Poids = s->Poids;
    d->HitForce = s->HitForce;
    d->Owner = s->Owner;
    d->NewForce = s->NewForce;
    d->Impact = s->Impact;
    d->Scale = s->Scale;
}

void SavegameFlowToWire32(const S_PART_FLOW *s, S_PART_FLOW_WIRE32 *d) {
    d->Flag = s->Flag;
    d->Owner = s->Owner;
    d->NumPoint = s->NumPoint;
    d->NbDot = s->NbDot;
    d->OrgX = s->OrgX;
    d->OrgY = s->OrgY;
    d->OrgZ = s->OrgZ;
    d->XMin = s->XMin;
    d->YMin = s->YMin;
    d->ZMin = s->ZMin;
    d->XMax = s->XMax;
    d->YMax = s->YMax;
    d->ZMax = s->ZMax;
    d->FlowTimerStart = s->FlowTimerStart;
    d->PtrListDot_w32 = 0; /* restored externally on read */
}

#endif /* !EDITLBA2 */
