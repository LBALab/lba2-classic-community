//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_CDTRACKS
#define LIB_SYSTEM_CDTRACKS

//──────────────────────────────────────────────────────────────────────────
// Which CD track holds which music, on a disc that carries its soundtrack as
// Red Book audio rather than as files.
//
// There is no single answer. Three known artifacts, three layouts:
//
//   Brazilian Activision  7 audio tracks, 2..8   TADPCM1..5, JADPCM01, TADPCM6
//   European EA           1 audio track          it is TADPCM6
//   TWINSEN.mdx rip       6 audio tracks, 2..7   the same seven less TADPCM1
//
// So a track number does not name a theme by itself. What holds across all of
// them is positional: the audio tracks, in order, are the themes that have no
// file under MUSIC, in ListJingle order. The Brazilian disc presses seven and is
// missing seven; the European discs press one and are missing one; both map
// straight across, and the Brazilian result reproduces SOURCES/MUSIC.CPP's own
// TrackCDUS exactly, which is a useful check on the rule.
//
// The table below is for the third case, and it is worth being precise about
// what it describes. TrackCDUS clears the JINGLE flag on its first seven
// entries, values 2 to 8, so the shipped game expects seven audio tracks and
// track 2 to be TADPCM1. Both retail executables carry that table byte for byte,
// the Brazilian one and the one on the six-track rip alike. That rip therefore
// disagrees with the binary sitting beside it on the same image: its track 2 is
// TADPCM2, measured at 0.985 against a known recording and 0.33 against the real
// TADPCM1, and its audio region is short by almost exactly one TADPCM1.
//
// The likeliest reading is that the rip lost its first audio track and renumbered
// what was left, rather than that Activision pressed a disc missing a theme its
// own executable asks for. Either way the table stays, because that image is the
// one in circulation and its six tracks have to be named for what they actually
// hold. Do not "correct" it to match TrackCDUS: it is not describing the same
// disc. If a seven-track US rip turns up, the positional rule names it with no
// table at all.
//
// Read off real discs rather than inferred. Each mapping comes from a mounted
// disc's own table of contents plus a per-track match of length and audio
// content against a known recording. Measurement is the only way to tell these
// layouts apart: the Brazilian numbering and a one-off-by-one error in this
// table are the same numbering, and both play perfectly while being the wrong
// song throughout.
//
// It lives here, beside the disc sources, because it describes the layout of a
// disc rather than the engine's music indirection, the same way CD.CPP keeps
// its first-track constants. Retail data does not change, so neither does this.
//──────────────────────────────────────────────────────────────────────────

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
#define CDTRACKS_FIRST_AUDIO 2
#define CDTRACKS_LAST_AUDIO 7

//──────────────────────────────────────────────────────────────────────────
// The CD track carrying the music named by `pathOrName`, or 0 when that music
// is a file on every release. Accepts a bare name, a filename with extension,
// or a full path ("…/music/TADPCM3.WAV"), and is case-insensitive.
int CdTracks_NumberForMusicName(const char *pathOrName);

//──────────────────────────────────────────────────────────────────────────
// The music name a CD track carries ("TADPCM2" for track 2), or NULL if the
// track number is outside the audio range. For diagnostics and for walking the
// mapping without duplicating its bounds.
//
// This is the US disc's numbering. Use it only as the fallback for a disc whose
// pressed-theme count does not match its audio-track count; otherwise the
// positional rule above is what names a track.
const char *CdTracks_NameForNumber(int track);

//──────────────────────────────────────────────────────────────────────────
// Every theme the game can ask for, in SOURCES/MUSIC.CPP's ListJingle order.
// That order is load-bearing rather than cosmetic: the positional rule walks it
// to work out which themes a disc did not ship as files, and JADPCM01 sits
// between TADPCM5 and TADPCM6 there. Sorting the names instead would swap those
// two on any disc that presses both.
//
// `CdTracks_ThemeName` returns NULL for an index outside 0..count-1.
int CdTracks_ThemeCount(void);
const char *CdTracks_ThemeName(int index);

//──────────────────────────────────────────────────────────────────────────
// Where `pathOrName` sits among the themes that have no file, and how many such
// themes there are. `present` holds one flag per theme in CdTracks_ThemeName
// order, so deciding what counts as "has a file" stays with the caller and the
// arithmetic that names a track stays testable on its own.
//
// Returns the 0-based position among the missing, or -1 when the name is not a
// theme or does have a file. `outMissing` may be NULL.
//
// The caller applies the rule: when the count of missing themes equals the
// number of audio tracks the disc presses, the track at this position is this
// theme. When they differ, the correspondence is not established and the track
// table above is the fallback.
int CdTracks_MissingThemeIndex(const char *pathOrName, const unsigned char *present,
                               int presentCount, int *outMissing);

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_CDTRACKS

//──────────────────────────────────────────────────────────────────────────
