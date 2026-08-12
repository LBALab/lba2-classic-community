//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/CDTRACKS.H>

#include <ctype.h>
#include <string.h>

//──────────────────────────────────────────────────────────────────────────
// Indexed by (track - CDTRACKS_FIRST_AUDIO). See the header for where this
// order comes from and why it is stable.
static const char *const CD_TRACK_NAMES[] = {
    "TADPCM2",  // CD track 2
    "TADPCM3",  // 3
    "TADPCM4",  // 4
    "TADPCM5",  // 5
    "JADPCM01", // 6
    "TADPCM6",  // 7
};

#define CD_TRACK_COUNT ((int)(sizeof(CD_TRACK_NAMES) / sizeof(CD_TRACK_NAMES[0])))

//──────────────────────────────────────────────────────────────────────────
// Every theme, in SOURCES/MUSIC.CPP's ListJingle order (its index 0 is the
// "no track" placeholder and is not a theme, so it is not here). Note where
// JADPCM01 falls: between TADPCM5 and TADPCM6, not with the other jingles.
// tests/cdtracks reads MUSIC.CPP and fails if this drifts.
static const char *const THEME_NAMES[] = {
    "TADPCM1",
    "TADPCM2",
    "TADPCM3",
    "TADPCM4",
    "TADPCM5",
    "JADPCM01",
    "TADPCM6",
    "JADPCM02",
    "JADPCM03",
    "JADPCM04",
    "JADPCM05",
    "JADPCM06",
    "JADPCM07",
    "JADPCM08",
    "JADPCM09",
    "JADPCM10",
    "JADPCM11",
    "JADPCM12",
    "JADPCM13",
    "JADPCM14",
    "JADPCM15",
    "JADPCM16",
    "JADPCM17",
    "JADPCM18",
    "LOGADPCM",
};

#define THEME_COUNT ((int)(sizeof(THEME_NAMES) / sizeof(THEME_NAMES[0])))

//──────────────────────────────────────────────────────────────────────────
// Reduce a path to the uppercased stem of its last component, which is the form
// the table stores: "…/music/TADPCM3.WAV" becomes "TADPCM3".
static void music_stem(const char *pathOrName, char *out, size_t outMax) {
    const char *base = strrchr(pathOrName, '/');
    const char *bs = strrchr(pathOrName, '\\');
    if (bs != NULL && (base == NULL || bs > base))
        base = bs;
    base = (base != NULL) ? base + 1 : pathOrName;

    size_t i = 0;
    for (; base[i] != '\0' && base[i] != '.' && i + 1 < outMax; i++)
        out[i] = (char)toupper((unsigned char)base[i]);
    out[i] = '\0';
}

//──────────────────────────────────────────────────────────────────────────
extern "C" int CdTracks_NumberForMusicName(const char *pathOrName) {
    if (pathOrName == NULL || pathOrName[0] == '\0')
        return 0;

    char stem[32];
    music_stem(pathOrName, stem, sizeof(stem));

    for (int i = 0; i < CD_TRACK_COUNT; i++) {
        if (strcmp(stem, CD_TRACK_NAMES[i]) == 0)
            return i + CDTRACKS_FIRST_AUDIO;
    }
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" const char *CdTracks_NameForNumber(int track) {
    int i = track - CDTRACKS_FIRST_AUDIO;
    return (i >= 0 && i < CD_TRACK_COUNT) ? CD_TRACK_NAMES[i] : NULL;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" int CdTracks_ThemeCount(void) { return THEME_COUNT; }

//──────────────────────────────────────────────────────────────────────────
extern "C" const char *CdTracks_ThemeName(int index) {
    return (index >= 0 && index < THEME_COUNT) ? THEME_NAMES[index] : NULL;
}
