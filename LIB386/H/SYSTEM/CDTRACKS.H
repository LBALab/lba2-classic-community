//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_CDTRACKS
#define LIB_SYSTEM_CDTRACKS

//──────────────────────────────────────────────────────────────────────────
// Which CD track holds which music, on a disc that carries its soundtrack as
// Red Book audio rather than as files.
//
// The US retail CD ("Twinsen's Odyssey") is that disc: its first seven music
// entries are audio tracks 2 through 8, and the remaining jingles are WAVs in
// MUSIC/. The EU release streams all of them from files instead, which is why
// nothing needs this mapping until a mixed-mode rip turns up.
//
// The order is the one SOURCES/MUSIC.CPP already states: ListJingle[1..7]
// against the seven non-JINGLE entries of TrackCDUS. It lives here, beside the
// disc sources, because it describes the layout of a disc rather than the
// engine's music indirection, the same way CD.CPP keeps its first-track
// constants. Retail data does not change, so this table does not either.
//──────────────────────────────────────────────────────────────────────────

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
#define CDTRACKS_FIRST_AUDIO 2
#define CDTRACKS_LAST_AUDIO 8

//──────────────────────────────────────────────────────────────────────────
// The CD track carrying the music named by `pathOrName`, or 0 when that music
// is a file on every release. Accepts a bare name, a filename with extension,
// or a full path ("…/music/TADPCM3.WAV"), and is case-insensitive.
int CdTracks_NumberForMusicName(const char *pathOrName);

//──────────────────────────────────────────────────────────────────────────
// The music name a CD track carries ("TADPCM1" for track 2), or NULL if the
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
