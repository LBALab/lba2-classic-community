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
#include <stdint.h>

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
// A cue sheet is small and bounded; a real disc holds at most 99 tracks.
#define CUE_MAX_TRACKS 99
#define CUE_MAX_NAME 256

//──────────────────────────────────────────────────────────────────────────
// One TRACK entry. `startFrame` is INDEX 01 in CD frames (75 per second),
// measured inside the owning FILE: for a track in the disc image that is its
// absolute sector on the disc, for an external file it is normally 0.
typedef struct {
    int number;       // as written after TRACK, not the position in the file
    int isAudio;      // 0 for a data mode (MODE1/MODE2/...)
    int fileIsBinary; // the owning FILE is BINARY/MOTOROLA, i.e. the disc image
    uint32_t startFrame;
    char file[CUE_MAX_NAME];
} CueTrack;

//──────────────────────────────────────────────────────────────────────────
// Parse the whole track table. Returns the number of tracks written to `out`.
//
// Track numbers are taken from the sheet, not inferred from position, and the
// sequence is allowed to have holes. A rip that lost a track still has to
// address the survivors by their true numbers: the engine picks its music by CD
// track number, so renumbering to close a gap would shift every theme.
int Cue_ParseToc(const char *cueText, size_t len, CueTrack *out, int maxTracks);

//──────────────────────────────────────────────────────────────────────────
// Find a cue sheet (*.cue / *.dat) in `dir` (which must end with a separator)
// and parse its track table. Returns the track count, 0 if no cue was readable.
// `outCuePath` receives the sheet's path and may be NULL.
int Cue_ReadTocInDir(const char *dir, char *outCuePath, size_t cuePathMax,
                     CueTrack *out, int maxTracks);

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
