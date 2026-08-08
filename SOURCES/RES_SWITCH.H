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

/* Re-derive the render size for the scene just loaded. Interiors may render
   smaller than the player's resolution (LBA2_ISO_DIV): nothing in an isometric
   scene scales with the framebuffer, so a smaller one magnifies the room
   rather than losing detail. No-op when the target matches the live size, so
   walking between rooms of the same kind costs nothing. Call once CubeMode is
   known, before the grille is loaded into the resized buffers. */
void Res_ApplySceneRenderSize(void);

/* The resolution the player chose, as opposed to whatever the current scene
   renders at. This is what gets persisted: an interior downshift is a
   rendering detail, not a preference. */
void Res_BaseDimensions(U32 *outW, U32 *outH);

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
   (320..1920 wide by 200..1080 tall, the RES_MIN/RES_MAX bounds in
   RES_CATALOG.H) and W % 8 == 0. Same check Res_Switch applies to its
   arguments. Exposed so the cfg-read path can reject corrupt or out-of-range
   stored values without first calling Res_Switch. */
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

/* Auto-detect a widescreen boot resolution from the primary display: writes
   (snapped-width, 480) and returns true when the display is wider than 4:3.
   Returns false (outputs untouched) on a 4:3 display or a failed/headless
   display query, so the caller falls back to the 640×480 default. Used only
   when neither --resolution nor a saved lba2.cfg resolution is present, so the
   auto value is never persisted (derive-each-launch). */
bool Res_AutoDetectDimensions(U32 *outW, U32 *outH);

/* One-word provenance of the last Res_LoadBootDimensions result, for the boot
   log: "CLI", "cfg", "auto", or "default". Static literal; never NULL. */
const char *Res_BootDimensionsSource(void);

/* True when the live resolution was auto-detected (or fell back to the
   compile-time default) rather than chosen by the player. WriteConfigFile uses
   this to skip persisting ResolutionX/Y, so an auto resolution re-derives from
   the display each launch instead of freezing into the cfg. Cleared by a
   successful Res_Switch and by a CLI/cfg value at boot. */
bool Res_ResolutionIsAutoDerived(void);

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
