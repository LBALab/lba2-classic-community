//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_ISO9660
#define LIB_SYSTEM_ISO9660

//──────────────────────────────────────────────────────────────────────────
// Minimal ISO9660 reader for raw (2352-byte Mode-1) or cooked (2048) CD images.
// Just enough to list a directory and pull a file out by path: the disc-image
// resource source reads retail assets straight off a mounted image through this.
//──────────────────────────────────────────────────────────────────────────
#include <stddef.h>
#include <stdint.h>

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
typedef struct iso9660 iso9660_t;

//──────────────────────────────────────────────────────────────────────────
// Open a CD image. Auto-detects 2352-raw vs 2048-cooked by locating the
// ISO9660 "CD001" primary volume descriptor at logical sector 16. Returns NULL
// if the file is not an ISO9660 image we understand.
iso9660_t *iso_open(const char *path);
void iso_close(iso9660_t *iso);

//──────────────────────────────────────────────────────────────────────────
// List the regular files in a directory (path uses '/', e.g. "LBA/FLA").
// Names are reported without the ";1" ISO version suffix.
typedef void (*iso_list_cb)(void *ud, const char *name, uint32_t size);
int iso_list(iso9660_t *iso, const char *dirpath, iso_list_cb cb, void *ud);

//──────────────────────────────────────────────────────────────────────────
// Recursively visit every regular file in the image, reporting each as a full
// '/'-rooted path (version suffixes stripped, e.g. "/SEQUENCE/BIGINTRO.ACF").
typedef void (*iso_walk_cb)(void *ud, const char *path, uint32_t size);
int iso_walk(iso9660_t *iso, iso_walk_cb cb, void *ud);

//──────────────────────────────────────────────────────────────────────────
// Read a file by path (e.g. "LBA/FLA/INTROD.FLA") into a freshly malloc'd
// buffer. Caller frees *out. Returns 0 on success.
int iso_read(iso9660_t *iso, const char *filepath, uint8_t **out, size_t *outsize);

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_ISO9660

//──────────────────────────────────────────────────────────────────────────
