//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/CUE.H>

#include <dirent.h>

#include <stdio.h>
#include <string.h>
#include <strings.h> // strncasecmp / strcasecmp

//──────────────────────────────────────────────────────────────────────────
// Walk the cue text line by line. Track the most recent FILE and its type; when
// an AUDIO track appears under a non-BINARY file, that file is the external
// audio we want. A BINARY file (the data image) with AUDIO tracks is in-image
// CD-DA and is skipped.
int Cue_ParseExternalAudio(const char *cueText, size_t len, char *outName, size_t outMax) {
    if (cueText == NULL || outName == NULL || outMax == 0)
        return 0;

    char curFile[512] = "";
    int curBinary = 0;

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
            int isAudio = 0;
            for (size_t k = 0; k + 5 <= llen; k++) {
                if (strncasecmp(s + k, "AUDIO", 5) == 0) {
                    isAudio = 1;
                    break;
                }
            }
            if (isAudio && curFile[0] != '\0' && !curBinary) {
                strncpy(outName, curFile, outMax - 1);
                outName[outMax - 1] = '\0';
                return 1;
            }
        }

        i = j;
        while (i < len && (cueText[i] == '\n' || cueText[i] == '\r'))
            i++;
    }
    return 0;
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
        const char *dot = strrchr(ent->d_name, '.');
        if (dot == NULL)
            continue;
        if (strcasecmp(dot, ".cue") != 0 && strcasecmp(dot, ".dat") != 0)
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
