#ifndef HQR_NAMES_H
#define HQR_NAMES_H

/* Retail HQR / data filenames, resolved relative to the asset root by the
 * Get*Path helpers in DIRECTORIES.H. Split out of COMMON.H so lightweight,
 * standalone consumers (e.g. ASSET_PREFLIGHT) and their host tests can use
 * them without pulling the whole game-wide prelude. Included by COMMON.H. */

#define SCRSHOT_HQR_NAME "scrshot.hqr"
#define CREDITS_HQR_NAME "lba2.hqr"
#define HOLO_HQR_NAME "holomap.hqr"
#define RESS_HQR_NAME "ress.hqr"
#define SCREEN_HQR_NAME "screen.hqr"
#define BKG_HQR_NAME "lba_bkg.hqr"
#define VIDEO_HQR_NAME "video.hqr"
#define SAMPLES_HQR_NAME "samples.hqr"
#define SPRITES_HQR_NAME "sprites.hqr"
#define SPRIRAW_HQR_NAME "spriraw.hqr"
#define ANIM3DS_HQR_NAME "anim3ds.hqr"
#define ANIM_HQR_NAME "anim.hqr"
#define BODY_HQR_NAME "body.hqr"
#define OBJFIX_HQR_NAME "objfix.hqr"
#define SCENE_HQR_NAME "scene.hqr"
#define TEXT_HQR_NAME "text.hqr"

#endif /* HQR_NAMES_H */
