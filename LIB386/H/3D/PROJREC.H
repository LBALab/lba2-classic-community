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

/* Open the capture file for writing. Returns 1 on success, 0 otherwise
   (NULL path, empty path, fopen failure). Calling Begin while already
   active flushes and reopens the file. Writes a `# projrec v1` header. */
int Projrec_BeginCapture(const char *path);

/* Flush and close the capture file. Idempotent; safe to call when never
   begun. Reports the captured event count to stderr if active. */
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

#ifdef __cplusplus
}
#endif
