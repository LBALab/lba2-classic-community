//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/CUE.H>

#include <dirent.h>

#include <stdio.h>
#include <string.h>
#include <strings.h> // strncasecmp / strcasecmp

// Read an unsigned decimal run, stopping at the first non-digit. Returns the
// value and advances *p. A cue's numbers are all small and unsigned.
static unsigned rd_uint(const char **p, const char *end) {
    unsigned v = 0;
    while (*p < end && **p >= '0' && **p <= '9') {
        v = v * 10 + (unsigned)(**p - '0');
        (*p)++;
    }
    return v;
}

//──────────────────────────────────────────────────────────────────────────
// Parse "mm:ss:ff" into CD frames (75 per second). Returns 0 if the field is
// not in that shape, which reads the same as a track starting at the beginning
// of its file, the sane default for a malformed sheet.
static uint32_t parse_msf(const char *s, size_t len) {
    const char *p = s;
    const char *end = s + len;
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    if (p >= end || *p < '0' || *p > '9')
        return 0;
    unsigned mm = rd_uint(&p, end);
    if (p >= end || *p != ':')
        return 0;
    p++;
    unsigned ss = rd_uint(&p, end);
    if (p >= end || *p != ':')
        return 0;
    p++;
    unsigned ff = rd_uint(&p, end);
    return (uint32_t)((mm * 60 + ss) * 75 + ff);
}

//──────────────────────────────────────────────────────────────────────────
// Does `s` (a line of `len` bytes) contain `word`, case-insensitively?
static int line_has(const char *s, size_t len, const char *word) {
    size_t wl = strlen(word);
    for (size_t k = 0; k + wl <= len; k++) {
        if (strncasecmp(s + k, word, wl) == 0)
            return 1;
    }
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
// Walk the cue text line by line, tracking the FILE each TRACK belongs to.
// FILE gives the owning file and whether it is the disc image (BINARY); TRACK
// opens an entry; INDEX 01 gives its start. INDEX 00 is the pregap and is
// deliberately ignored: what a caller wants is where the audio begins, not
// where the silence before it does.
int Cue_ParseToc(const char *cueText, size_t len, CueTrack *out, int maxTracks) {
    if (cueText == NULL || out == NULL || maxTracks <= 0)
        return 0;

    char curFile[CUE_MAX_NAME] = "";
    int curBinary = 0;
    int count = 0;
    /* Tracks past the table's capacity are dropped, and so are their INDEX
       lines: without this the overflow track's INDEX would land on the last
       entry we did keep and move a real track's start. */
    int haveOpenTrack = 0;

    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && cueText[j] != '\n' && cueText[j] != '\r')
            j++;

        const char *s = cueText + i;
        size_t llen = j - i;
        while (llen > 0 && (*s == ' ' || *s == '\t')) {
            s++;
            llen--;
        }

        if (llen >= 4 && strncasecmp(s, "FILE", 4) == 0) {
            const char *q1 = (const char *)memchr(s, '"', llen);
            if (q1 != NULL) {
                size_t rem = llen - (size_t)(q1 + 1 - s);
                const char *q2 = (const char *)memchr(q1 + 1, '"', rem);
                if (q2 != NULL) {
                    size_t fl = (size_t)(q2 - q1 - 1);
                    if (fl < sizeof(curFile)) {
                        memcpy(curFile, q1 + 1, fl);
                        curFile[fl] = '\0';
                        const char *t = q2 + 1;
                        size_t tlen = llen - (size_t)(t - s);
                        while (tlen > 0 && (*t == ' ' || *t == '\t')) {
                            t++;
                            tlen--;
                        }
                        curBinary = (tlen >= 6 && strncasecmp(t, "BINARY", 6) == 0) ||
                                    (tlen >= 8 && strncasecmp(t, "MOTOROLA", 8) == 0);
                    }
                }
            }
        } else if (llen >= 5 && strncasecmp(s, "TRACK", 5) == 0) {
            haveOpenTrack = (count < maxTracks);
            if (count < maxTracks) {
                const char *p = s + 5;
                const char *end = s + llen;
                while (p < end && (*p == ' ' || *p == '\t'))
                    p++;
                CueTrack *t = &out[count];
                t->number = (int)rd_uint(&p, end);
                t->isAudio = line_has(s, llen, "AUDIO");
                t->fileIsBinary = curBinary;
                t->startFrame = 0;
                strncpy(t->file, curFile, sizeof(t->file) - 1);
                t->file[sizeof(t->file) - 1] = '\0';
                count++;
            }
        } else if (llen >= 5 && strncasecmp(s, "INDEX", 5) == 0 && count > 0 && haveOpenTrack) {
            const char *p = s + 5;
            const char *end = s + llen;
            while (p < end && (*p == ' ' || *p == '\t'))
                p++;
            unsigned idx = rd_uint(&p, end);
            if (idx == 1)
                out[count - 1].startFrame = parse_msf(p, (size_t)(end - p));
        }

        i = j;
        while (i < len && (cueText[i] == '\n' || cueText[i] == '\r'))
            i++;
    }
    return count;
}

//──────────────────────────────────────────────────────────────────────────
// The first external (non-BINARY) AUDIO track's file. An AUDIO track under a
// BINARY file is in-image CD-DA, located by INDEX and served by the passthrough
// source, so it is not an answer to this question.
int Cue_ParseExternalAudio(const char *cueText, size_t len, char *outName, size_t outMax) {
    if (outName == NULL || outMax == 0)
        return 0;

    CueTrack toc[CUE_MAX_TRACKS];
    int n = Cue_ParseToc(cueText, len, toc, CUE_MAX_TRACKS);
    for (int i = 0; i < n; i++) {
        if (toc[i].isAudio && !toc[i].fileIsBinary && toc[i].file[0] != '\0') {
            strncpy(outName, toc[i].file, outMax - 1);
            outName[outMax - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
// Is this directory entry a cue sheet? GOG names theirs .dat, so both count.
static int is_cue_name(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot != NULL && (strcasecmp(dot, ".cue") == 0 || strcasecmp(dot, ".dat") == 0);
}

//──────────────────────────────────────────────────────────────────────────
int Cue_ImageForSheet(const char *path, char *outImagePath, size_t outMax) {
    if (path == NULL || outImagePath == NULL || outMax == 0)
        return 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    char text[8192];
    size_t got = fread(text, 1, sizeof(text), f);
    fclose(f);

    CueTrack toc[CUE_MAX_TRACKS];
    int n = Cue_ParseToc(text, got, toc, CUE_MAX_TRACKS);
    for (int i = 0; i < n; i++) {
        if (!toc[i].fileIsBinary || toc[i].file[0] == '\0')
            continue;
        size_t dirLen = 0; /* the sheet's own directory: a cue's FILE is relative to it */
        for (size_t k = 0; path[k]; k++) {
            if (path[k] == '/' || path[k] == '\\')
                dirLen = k + 1;
        }
        int w = snprintf(outImagePath, outMax, "%.*s%s", (int)dirLen, path, toc[i].file);
        return (w > 0 && (size_t)w < outMax) ? 1 : 0;
    }
    return 0;
}

//──────────────────────────────────────────────────────────────────────────
int Cue_ReadTocInDir(const char *dir, char *outCuePath, size_t cuePathMax,
                     CueTrack *out, int maxTracks) {
    if (dir == NULL || dir[0] == '\0' || out == NULL || maxTracks <= 0)
        return 0;

    DIR *d = opendir(dir);
    if (d == NULL)
        return 0;

    int count = 0;
    char cuePath[1024];
    struct dirent *ent;
    while (count == 0 && (ent = readdir(d)) != NULL) {
        if (!is_cue_name(ent->d_name))
            continue;
        int n = snprintf(cuePath, sizeof(cuePath), "%s%s", dir, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(cuePath))
            continue;

        FILE *f = fopen(cuePath, "rb");
        if (f == NULL)
            continue;
        char buf[8192];
        size_t got = fread(buf, 1, sizeof(buf), f);
        fclose(f);

        count = Cue_ParseToc(buf, got, out, maxTracks);
        if (count > 0 && outCuePath != NULL && cuePathMax > 0) {
            strncpy(outCuePath, cuePath, cuePathMax - 1);
            outCuePath[cuePathMax - 1] = '\0';
        }
    }
    closedir(d);
    return count;
}

//──────────────────────────────────────────────────────────────────────────
int Cue_ResolveInDir(const char *dir, char *outPath, size_t outMax) {
    if (dir == NULL || dir[0] == '\0' || outPath == NULL || outMax == 0)
        return 0;

    DIR *d = opendir(dir);
    if (d == NULL)
        return 0;

    int ok = 0;
    char cuePath[1024];
    char audioName[512];
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!is_cue_name(ent->d_name))
            continue;

        int n = snprintf(cuePath, sizeof(cuePath), "%s%s", dir, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(cuePath))
            continue;

        FILE *f = fopen(cuePath, "rb");
        if (f == NULL)
            continue;
        char buf[8192];
        size_t got = fread(buf, 1, sizeof(buf), f);
        fclose(f);

        if (Cue_ParseExternalAudio(buf, got, audioName, sizeof(audioName))) {
            int m = snprintf(outPath, outMax, "%s%s", dir, audioName);
            if (m > 0 && (size_t)m < outMax) {
                FILE *af = fopen(outPath, "rb");
                if (af != NULL) {
                    fclose(af);
                    ok = 1;
                    break;
                }
            }
        }
    }
    closedir(d);
    return ok;
}
