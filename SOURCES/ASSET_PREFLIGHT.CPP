#include "ASSET_PREFLIGHT.H"

#include <SYSTEM/ADELINE.H> /* fixed-width types */

#include "DIRECTORIES.H"
#include "HQR_NAMES.H"
#include "LOG/LOG.H"

#include <SYSTEM/FILES.H>
#include <SYSTEM/LIMITS.H>

#include <stddef.h> /* NULL */

/* Internals in an anonymous namespace for internal linkage — the house
 * new-infrastructure style (see SOURCES/RES_PICKER.CPP). */
namespace {

/* Required data the engine loads at boot; missing -> the game cannot start.
 * These are checked at the asset root via GetResPath. */
const char *const kRequiredHqr[] = {
    RESS_HQR_NAME,
    SPRITES_HQR_NAME,
    SPRIRAW_HQR_NAME,
    ANIM_HQR_NAME,
    BODY_HQR_NAME,
    OBJFIX_HQR_NAME,
    TEXT_HQR_NAME,
    SAMPLES_HQR_NAME,
#ifndef DEMO
    ANIM3DS_HQR_NAME,
#endif
};

/* Loaded on demand (holomap screen, per-cube scenes, menu backdrops); missing
 * only matters if that feature is used — warn, don't block boot. (Making these
 * degrade gracefully at use-time is a separate change; see BOOT_LOG_PLAN.md.) */
const char *const kOptionalHqr[] = {
    HOLO_HQR_NAME,
    SCENE_HQR_NAME,
    SCREEN_HQR_NAME,
    BKG_HQR_NAME,
};

bool fileExists(const char *path) { return FileSize(path) > 0; }

/* music/ and vox/ are collections (per-track WAVs, per-language .VOX) the audio
 * modules enumerate at load time; the formats and language set vary by SKU, so
 * the robust boot check is "is the subtree present", not a specific file. */
bool dirExists(const char *path) { return ExistsFileOrDir(path); }

} // namespace

extern "C" int AssetPreflight(void) {
    char path[ADELINE_MAX_PATH];
    int missingReq = 0, missingOpt = 0;
    int i;

    const int nReq = (int)(sizeof kRequiredHqr / sizeof kRequiredHqr[0]);
    for (i = 0; i < nReq; ++i) {
        GetResPath(path, ADELINE_MAX_PATH, kRequiredHqr[i]);
        if (!fileExists(path)) {
            Log_Error("missing required asset: %s", kRequiredHqr[i]);
            ++missingReq;
        }
    }

    /* video.hqr lives in video/ (GetMoviePath), not the root. It is loaded at
     * boot by InitAcf(), which hard-fails without it — so it is required today.
     * (When cutscene playback degrades gracefully it moves to the optional set.) */
    GetMoviePath(path, ADELINE_MAX_PATH, VIDEO_HQR_NAME);
    if (!fileExists(path)) {
        Log_Error("missing required asset: %s", VIDEO_HQR_NAME);
        ++missingReq;
    }

    const int nOpt = (int)(sizeof kOptionalHqr / sizeof kOptionalHqr[0]);
    for (i = 0; i < nOpt; ++i) {
        GetResPath(path, ADELINE_MAX_PATH, kOptionalHqr[i]);
        if (!fileExists(path)) {
            Log_Warn("optional asset missing: %s", kOptionalHqr[i]);
            ++missingOpt;
        }
    }

    /* music/ (GetJinglePath) and vox/ (GetVoicePath) — presence of the subtree. */
    GetJinglePath(path, ADELINE_MAX_PATH, NULL);
    if (!dirExists(path)) {
        Log_Warn("optional asset missing: music/ (no music)");
        ++missingOpt;
    }

    GetVoicePath(path, ADELINE_MAX_PATH, NULL, false);
    if (!dirExists(path)) {
        Log_Warn("optional asset missing: vox/ (no voices)");
        ++missingOpt;
    }

    if (missingReq == 0 && missingOpt == 0)
        Log_Info("Assets     all present");
    else
        Log_Info("Assets     %d missing (%d required)", missingReq + missingOpt,
                 missingReq);

    return missingReq;
}
