//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_DISCIMG
#define LIB_SYSTEM_DISCIMG

//──────────────────────────────────────────────────────────────────────────
// Disc-image resource source. Mounts a raw ISO/BIN disc image sitting in the
// install directory and lets the engine read retail assets straight off it,
// with no extraction step. FILES.CPP routes OpenRead through here as a fallback
// when a file is not on the filesystem, so the whole HQR pipeline and the asset
// preflight inherit it. Reads are streamed lazily off the image (see ISO9660.H
// iso_stat/iso_pread); nothing is buffered whole.
//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/ADELINE_TYPES.H>

#include <stddef.h>

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
// Mount a disc image found in `assetsDir`. The directory is content-probed: the
// first file that opens as an ISO9660 image (a "CD001" PVD, any extension) is
// mounted. `resMarker` (e.g. "lba2.hqr") is located inside the image to find
// its asset-root subtree, since CD masters nest the game files under a volume
// directory (the LBA2 disc puts them under /LBA2). `assetsDir` is remembered as
// the mount base: a later OpenRead path under it maps into the image by
// stripping the base and resolving the remainder beneath the asset root.
// Returns true if an image was mounted. Safe no-op (false) when none is present.
bool DiscImage_Mount(const char *assetsDir, const char *resMarker);
void DiscImage_Unmount(void);
bool DiscImage_IsMounted(void);

//──────────────────────────────────────────────────────────────────────────
// Boot-banner helper. On a mount, fills the image's filesystem path and file
// count and returns true so the caller emits one "Disc:" line. Returns false
// when nothing is mounted, leaving the banner unchanged.
bool DiscImage_GetBannerInfo(char *outPath, size_t outPathSize, S32 *outFileCount);

//──────────────────────────────────────────────────────────────────────────
// FILES.CPP resource-source seam.
//
// A disc handle is distinguishable from a real POSIX fd by DiscImage_IsHandle,
// so FILES.CPP can dispatch Read/Seek/Close to the right backend.
bool DiscImage_IsHandle(S32 handle);

// Open `filename` (an absolute path under the mount base) from the image.
// Returns a virtual handle (> 0) on success, or 0 if nothing is mounted, the
// path is not under the base, or the file is not in the image. The handle
// streams lazily and does not buffer the file. Valid for Read/Seek/Close below.
S32 DiscImage_OpenRead(const char *filename);
S32 DiscImage_Read(S32 handle, void *buffer, U32 maxBytes);
// `origin` takes the ESeekOrigin values (start/current/end); returns the new
// absolute position, or -1 on a bad handle.
S32 DiscImage_Seek(S32 handle, S32 offset, S32 origin);
S32 DiscImage_Close(S32 handle);

// Does `path` (a file or a directory) exist in the image, under the mount base?
// Backs the ExistsFileOrDir fallback (e.g. the preflight music/ and vox/ checks).
bool DiscImage_Exists(const char *path);

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_DISCIMG

//──────────────────────────────────────────────────────────────────────────
