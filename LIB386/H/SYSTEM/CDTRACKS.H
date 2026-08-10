//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_CDTRACKS
#define LIB_SYSTEM_CDTRACKS

//──────────────────────────────────────────────────────────────────────────
// Which CD track holds which music, on a disc that carries its soundtrack as
// Red Book audio rather than as files.
//
// The US retail CD ("Twinsen's Odyssey") is that disc: six of its themes are
// audio tracks 2 through 7, and the jingles are WAVs in MUSIC/. The EU release
// streams all of them from files instead, which is why nothing needs this
// mapping until a mixed-mode rip turns up.
//
// The rule is simply that the CD track number is the music index:
// track N holds SOURCES/MUSIC.CPP's ListJingle[N]. Track 2 is TADPCM2, and
// track 1 is the data track, so the ListJingle[1] slot (TADPCM1) has no CD
// track: that piece of music is not on this disc at all.
//
// Read off a real disc rather than inferred. The mounted CD's table of contents
// gives tracks 2..7, and each track's length and audio content match the named
// piece. An earlier version of this table shifted everything by one, which is
// the kind of mistake that plays perfectly and is simply the wrong song.
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
const char *CdTracks_NameForNumber(int track);

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_CDTRACKS

//──────────────────────────────────────────────────────────────────────────
