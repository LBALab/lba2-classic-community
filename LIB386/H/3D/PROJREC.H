/*
 * PROJREC.H - Projection capture for widescreen regression testing.
 *
 * Companion to the polyrec ASM<->CPP equivalence harness, but focused on
 * projection-pipeline state rather than draw calls. Records every
 * SetProjection / LongProjectPoint / Map2Screen event to a side-channel
 * text file; the resulting `.projrec` is byte-comparable across builds.
 *
 * Design intent: Phase 1 of the widescreen plan (docs/WIDESCREEN.md). The
 * file is the regression net that lets Phase 2 prove its 640x480 changes
 * are byte-identical, before the engine renders any wider frame. See
 * docs/widescreen/projrec.md for the workflow.
 *
 * Default: inactive (zero cost). Active only when Projrec_BeginCapture()
 * is called with a path, typically via the --capture-projection harness
 * flag in SOURCES/CONTROL.CPP.
 *
 * This PR (1.A) wires up the scaffolding and the SetProjection hook.
 * LongProjectPoint / ProjectList / Map2Screen hooks land in PR 1.B.
 */

#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Open the full-text capture file for writing. Returns 1 on success, 0
   otherwise (NULL path, empty path, fopen failure). Calling Begin while
   already active closes the prior file. Writes a `# projrec v1` header.
   Composable with Projrec_BeginHashCapture — both sinks can be active
   simultaneously and receive the same event stream. */
int Projrec_BeginCapture(const char *path);

/* Open the hash-only capture file for writing. Same shape as BeginCapture,
   but the file receives nothing during the run — only a single FNV-1a
   digest line at EndCapture time. Composable with BeginCapture; what gets
   hashed is the same line text the full sink would write. */
int Projrec_BeginHashCapture(const char *path);

/* Flush and close both sinks. Idempotent; safe to call when never begun.
   Reports the captured event count + hash to stderr if any sink was
   active. */
void Projrec_EndCapture(void);

/* Cheap branch predicate. Hot-path hook callers gate their formatting on
   this so an inactive capture costs one branch per call site. */
int Projrec_IsActive(void);

/* SetProjection capture. Called from LIB386/3D/PROJ.CPP::SetProjection (the
   exterior-scene path: 3D perspective projection with a near-clip and focal
   factors). No-op when capture is inactive. */
void Projrec_SetProjection(S32 xc, S32 yc, S32 clip, S32 fx, S32 fy);

/* SetIsoProjection capture. Called from LIB386/3D/PROJ.CPP::SetIsoProjection
   (the interior-scene path: orthographic isometric projection — no clip or
   focal factors). The two projections are mutually exclusive at any moment;
   capturing both gives a full per-scene record. No-op when inactive. */
void Projrec_SetIsoProjection(S32 xc, S32 yc);

/* Per-point projection captures. Hot path — fired once per vertex/point per
   frame. Each gates on the IsActive predicate before formatting.

   Two variants because LongProjectPoint is a function pointer that swaps
   between LongProjectPoint3D (perspective, with a far-clip return code)
   and LongProjectPointIso (orthographic, always returns -1). Phase 2's
   XCentre/YCentre change ripples through both, so both need coverage. */
void Projrec_LongProjectPoint3D(S32 x, S32 y, S32 z, S32 ret, S32 xp, S32 yp);
void Projrec_LongProjectPointIso(S32 x, S32 y, S32 z, S32 xp, S32 yp);

/* Batched projection list captures. Called once per ProjectList3DF /
   ProjectListIso invocation; the captured event includes the call's input
   (NbPt + origin) but not the per-vertex output — per-vertex coverage is
   already provided by LongProjectPoint hooks above. */
void Projrec_ProjectList3D(S32 nbpt, S32 orgx, S32 orgy, S32 orgz);
void Projrec_ProjectListIso(S32 nbpt, S32 orgx, S32 orgy, S32 orgz);

/* Map2Screen capture. Called from SOURCES/GRILLE.CPP::Map2Screen, the
   isometric world-to-screen translation that bakes the +288 / +226 origin
   offsets Phase 2 will re-derive from the framebuffer width. Captures the
   input world coords and the computed screen X/Y for byte-comparable
   regression. */
void Projrec_Map2Screen(S32 x, S32 y, S32 z, S32 xscreen, S32 yscreen);

#ifdef __cplusplus
}
#endif
