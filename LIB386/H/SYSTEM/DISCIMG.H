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
// Does `dir` (with trailing separator) hold a disc image whose filesystem
// contains `resMarker`? Answers the question discovery asks before any mount
// has happened, so an install whose game data exists only inside the image is
// still recognised as a valid resource directory. Probes and closes; it does
// not mount, and it does not disturb an existing mount.
bool DiscImage_DirHoldsMarker(const char *dir, const char *resMarker);

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
// Raw sector access, for the CD audio a mixed-mode disc carries outside its
// filesystem. Audio sectors are 2352 bytes of PCM and belong to no file, so
// they are reached by sector address (from the cue) rather than by path.
//
// Only a raw 2352 image has them: a cooked 2048 rip is data only, and so is any
// image whose audio the container stores at a different stride. Both report a
// sector count of 0, which is the signal to leave CD music alone.
U32 DiscImage_RawSectorCount(void);
S32 DiscImage_ReadRawSectors(U32 lba, U32 count, void *buffer);

// Is `name` (as a cue writes it, no path) the image we actually mounted? Sector
// addresses from a cue only mean anything against the file that cue describes.
bool DiscImage_IsMountedFile(const char *name);

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_DISCIMG

//──────────────────────────────────────────────────────────────────────────
