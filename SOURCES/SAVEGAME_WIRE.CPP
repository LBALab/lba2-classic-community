/* Wire-format converters for the save payload's pointer-bearing structs.
 * Layout and rationale: SAVEGAME_WIRE.H + docs/SAVE_WIRE_PLAN.md.
 *
 * The readers (...FromWire32) moved here verbatim from SAVEGAME.CPP so they can be
 * unit-tested in isolation against hand-derived golden byte-vectors.  The
 * writers (...ToWire32) are Phase 2 and are intentionally absent for now. */

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
    /* Caller stashes/restores PtrListDot externally; everything else copies. */
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

/* Phase 2 (docs/SAVE_WIRE_PLAN.md): SavegameObj3dToWire32 / ...ExtraToWire32 /
 * ...FlowToWire32 are implemented here as exact inverses of the readers above,
 * then SaveContexte routes its three raw struct writes through them. */

#endif /* !EDITLBA2 */
