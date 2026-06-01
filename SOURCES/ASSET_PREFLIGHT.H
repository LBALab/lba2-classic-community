#ifndef ASSET_PREFLIGHT_H
#define ASSET_PREFLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Boot asset preflight: report which expected retail data files are present,
 * using the correct path resolver per category (root .hqr via GetResPath,
 * video.hqr via GetMoviePath, music/ and vox/ via GetJinglePath/GetVoicePath).
 * Required missing -> error (logged); optional missing -> warning.
 *
 * Returns the number of *required* assets missing. The caller should abort the
 * boot when this is non-zero (fail fast with a clear log, rather than dying
 * later at an arbitrary load). Graceful runtime degradation for the optional
 * set is a separate change (see docs/BOOT_LOG_PLAN.md).
 *
 * Call once at boot, after InitDirectories and Log_Init. */
int AssetPreflight(void);

#ifdef __cplusplus
}
#endif

#endif /* ASSET_PREFLIGHT_H */
