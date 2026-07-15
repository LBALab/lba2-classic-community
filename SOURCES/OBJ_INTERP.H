#ifndef OBJ_INTERP_H
#define OBJ_INTERP_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

// Engine-side render interpolation (issue #412, docs/RENDER_INTERP_PLAN.md). The fixed-timestep sim
// (#358) advances at ~60 Hz; on a faster display the extra rendered frames re-present a frozen pose,
// so held movement judders. This snapshots each interpolated object's last two sim-step positions and,
// when RenderInterp is on, draws it between them every rendered frame -- smooth motion while the sim
// stays byte-exact. Built on the pure Render_LerpCoord/Render_InterpNum core (LIB386/3D/RENDERINTERP.H).
// Phase 2 interpolates the hero only; later phases widen the active set.

// Snapshot the interpolated objects' positions after a completed sim step (prev = cur; cur = Obj).
// Call once per simulated sub-step, after the object loop. A one-step jump larger than a teleport
// threshold (a Life SetPos, cube change, or respawn) collapses prev = cur = Obj, so the object is
// drawn at its destination rather than lerped across the map. Never touches sim state.
void ObjInterp_Step(void);

// Overwrite each interpolated object's drawn position with lerp(prev, cur, num/den) and save the true
// position, for the render only. No-op (renders the true position) when RenderInterp is off, den == 0
// (throttle off), or fullFlip (a full-scene redraw frame, where a background bake would capture the
// drawn position). Call right before AffScene, after computing num via Render_InterpNum.
void ObjInterp_Apply(U32 num, U32 den, int fullFlip);

// Restore the true positions saved by ObjInterp_Apply, so the sim never sees an interpolated value.
// Call right after AffScene.
void ObjInterp_Restore(void);

#ifdef __cplusplus
}
#endif

#endif /* OBJ_INTERP_H */
