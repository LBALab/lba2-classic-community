/*
 * Runtime resolution switching — orchestrator.
 *
 * See docs/RUNTIME_RESOLUTION.md for the design.  Res_Switch tears down the
 * framebuffer-sized buffers and the SDL surface, reinitialises both at the new
 * dimensions, and reloads the current cube's grille data (Bricks / BlockTable /
 * MaskBrick) so the next AffScene() can present a complete frame.  On any
 * rebuild failure the orchestrator falls back to the previous (W, H).
 *
 * This is the engine-side primitive; the console verb and the keep / revert
 * dialog described in the doc are user-facing layers that drive it.
 */

#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns true on success, false if newW/newH is outside the engine's
   supported range, isn't a multiple of 8 in width, or if the rebuild failed
   and the previous resolution had to be restored. */
bool Res_Switch(U32 newW, U32 newH);

/* Safety gate. Returns NULL if it's safe to call Res_Switch right now, else
   a short user-facing reason string. Refuses while a cinematic is playing
   (FlagPlayAcf), no scene is loaded, or the holomap is open (HoloMode). The
   string is a static literal — do not free. */
const char *Res_SwitchAllowedReason(void);

/* Begin the keep-or-revert countdown after a successful Res_Switch. The
   modal pops up on the next MainLoop tick via Res_RunRevertDialogIfPending().
   Caller is responsible for closing the console (or any other layer) that
   would otherwise occlude the modal. (prevW, prevH) is what gets restored
   on Revert or timeout. */
void Res_BeginRevertCountdown(U32 prevW, U32 prevH);

/* Returns true if a revert dialog is pending (caller about to draw should
   defer to the modal). */
bool Res_HasPendingRevert(void);

/* Run the revert modal if one is pending; otherwise no-op. Called from the
   top of MainLoop. Drives its own tick loop (ManageTime + MyGetInput) until
   the user picks Keep/Revert or the timeout expires; on Revert calls
   Res_Switch back to (prevW, prevH). Returns the choice the user made
   (RES_REVERT_NONE if no dialog was pending). */
typedef enum {
    RES_REVERT_NONE = 0,
    RES_REVERT_KEEP,
    RES_REVERT_REVERT
} ResRevertChoice;
ResRevertChoice Res_RunRevertDialogIfPending(void);

/* Cancel any pending revert (e.g. on game state change that would make the
   dialog stale). Idempotent; no-op when nothing is pending. */
void Res_CancelPendingRevert(void);

/* True if (w, h) is a valid render resolution: within the engine's range
   (320..1920 × 200..1024) and W % 8 == 0. Same check Res_Switch applies to
   its arguments. Exposed so the cfg-read path can reject corrupt or
   out-of-range stored values without first calling Res_Switch. */
bool Res_ValidateDimensions(U32 w, U32 h);

/* Boot-time resolution resolver: writes the effective (W, H) the engine
   should boot at, following the precedence
       --resolution CLI  >  lba2.cfg ResolutionX/Y  >  compile-time default.
   Reads lba2.cfg via a transient DefFileBuffer init against an internal
   static buffer; never allocates from the engine heap, so it's safe to
   call BEFORE Mem_ConfigureScreenBuffers / InitMainBuffer. Requires
   PathConfigFile to be resolved first (call GetCfgPath); silently falls
   back to the compile-time default otherwise. Logs a one-line warning if
   the cfg held an invalid value so the player can spot a corrupted key. */
void Res_LoadBootDimensions(U32 *outW, U32 *outH);

/* Read the boot fullscreen choice (lba2.cfg DisplayFullScreen) the same
   transient, heap-free way as Res_LoadBootDimensions, so the window can be
   created fullscreen from the start instead of created windowed then flipped
   by the later ReadConfigFile pass. Returns false (windowed) when the key is
   absent/invalid or the cfg can't be read. Requires PathConfigFile resolved
   first (call GetCfgPath). */
bool Res_LoadBootFullscreen(void);

#ifdef __cplusplus
}
#endif
