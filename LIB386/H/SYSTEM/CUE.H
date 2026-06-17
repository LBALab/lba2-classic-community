//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_CUE
#define LIB_SYSTEM_CUE

//──────────────────────────────────────────────────────────────────────────
// Minimal CUE-sheet reader. Just enough to locate the external audio track a
// disc image ships alongside its data track: GOG's LBA2 cue (LBA2.DAT) lists
// the data image plus one standalone audio file (LBA2.OGG = music track 6),
//
//     FILE "LBA2.GOG" BINARY
//       TRACK 01 MODE1/2352
//     FILE "LBA2.OGG" MP3
//       TRACK 02 AUDIO
//
// An AUDIO track under a BINARY file is in-image CD-DA (located by INDEX) and is
// intentionally NOT reported here; that is a separate passthrough source.
//──────────────────────────────────────────────────────────────────────────
#include <stddef.h>

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
// Parse cue-sheet text for the first external (non-BINARY) AUDIO track's file
// name. Returns 1 and fills `outName` on success, 0 otherwise.
int Cue_ParseExternalAudio(const char *cueText, size_t len, char *outName, size_t outMax);

//──────────────────────────────────────────────────────────────────────────
// Find a cue sheet (*.cue / *.dat) in `dir` (which must end with a separator),
// parse its first external AUDIO track, and build the full path to that file if
// it exists on disk. Returns 1 and fills `outPath` on success, 0 otherwise.
int Cue_ResolveInDir(const char *dir, char *outPath, size_t outMax);

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_CUE

//──────────────────────────────────────────────────────────────────────────
