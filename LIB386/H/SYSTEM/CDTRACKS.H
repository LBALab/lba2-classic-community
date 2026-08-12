//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_CDTRACKS
#define LIB_SYSTEM_CDTRACKS

//──────────────────────────────────────────────────────────────────────────
// Which CD track holds which music, on a disc that carries its soundtrack as
// Red Book audio rather than as files.
//
// There is no single answer, which took three pressings to learn. Each presses a
// different number of themes, and the track numbering follows:
//
//   US Activision      6 audio tracks, 2..7   track N holds ListJingle[N]
//   Brazilian Activision  7 audio tracks, 2..8   track N holds ListJingle[N-1]
//   European EA        1 audio track          it is TADPCM6
//
// So a track number does not name a theme by itself. What does hold across all
// of them is positional: the audio tracks, in order, are the themes that have no
// file under MUSIC, in ListJingle order. The Brazilian disc presses seven and is
// missing seven; the European discs press one and are missing one; both map
// straight across. The US disc is the exception that needs the table below,
// because it is missing seven themes and presses only six: TADPCM1 is not on it
// at all, so its tracks start at the second missing theme.
//
// Read off real discs rather than inferred. Each mapping comes from a mounted
// disc's own table of contents plus a per-track match of length and audio
// content against a known recording. An early version of the US table shifted
// everything by one, which is the kind of mistake that plays perfectly and is
// simply the wrong song, and the Brazilian disc turns out to use exactly that
// shifted numbering, so the two cannot be told apart by anything but measurement.
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
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_CDTRACKS

//──────────────────────────────────────────────────────────────────────────
