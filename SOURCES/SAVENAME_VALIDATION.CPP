#include "SAVENAME_VALIDATION.H"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

S32 IsDatetimeReservedShape(const char *s) {
    if (s == NULL)
        return FALSE;
    size_t len = strlen(s);
    if (len < 19)
        return FALSE;
    const char *p = s + len - 19;
    /* If there's anything before the trailing datetime, require " - " glue —
     * otherwise "Foo-2026-05-17 14-30-00" would be a false positive. */
    if (p != s) {
        if (len < 22 || memcmp(p - 3, " - ", 3) != 0)
            return FALSE;
    }
    /* Positions in p:  0123456789012345678
     * Expected:        dddd-dd-dd dd-dd-dd */
    static const char kDigits[19] = {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1};
    static const char kSeps[19] = {0, 0, 0, 0, '-', 0, 0, '-', 0, 0, ' ', 0, 0, '-', 0, 0, '-', 0, 0};
    for (int i = 0; i < 19; i++) {
        if (kDigits[i]) {
            if (p[i] < '0' || p[i] > '9')
                return FALSE;
        } else {
            if (p[i] != kSeps[i])
                return FALSE;
        }
    }
    return TRUE;
}

/* ASCII for each CP850 byte from 0x80; "" for a symbol or box-drawing
 * character, which becomes '_'. */
static const char *const kCp850Fold[128] = {
    "C", "u", "e", "a", "a", "a", "a", "c", "e", "e", "e", "i", "i", "i", "A", "A",  /* 80 */
    "E", "ae", "AE", "o", "o", "o", "u", "u", "y", "O", "U", "o", "", "O", "", "f",  /* 90 */
    "a", "i", "o", "u", "n", "N", "a", "o", "", "", "", "", "", "", "", "",          /* A0 */
    "", "", "", "", "", "A", "A", "A", "", "", "", "", "", "", "", "",               /* B0 */
    "", "", "", "", "", "", "a", "A", "", "", "", "", "", "", "", "",                /* C0 */
    "d", "D", "E", "E", "E", "i", "I", "I", "I", "", "", "", "", "", "I", "",        /* D0 */
    "O", "ss", "O", "O", "o", "O", "u", "th", "TH", "U", "U", "U", "y", "Y", "", "", /* E0 */
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",                  /* F0 */
};

static S32 EqualsUpperAscii(const char *s, const char *upper, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (c != upper[i])
            return FALSE;
    }
    return TRUE;
}

/* Windows refuses a device name as a file name whatever follows its first
 * dot, so "CON.LBA" and "con.x.LBA" are both refused. */
static S32 IsWindowsDeviceName(const char *stem) {
    static const char *const kNames[] = {"CON", "PRN", "AUX", "NUL"};
    const char *dot = strchr(stem, '.');
    const size_t len = dot ? (size_t)(dot - stem) : strlen(stem);
    if (len == 3) {
        for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
            if (EqualsUpperAscii(stem, kNames[i], 3))
                return TRUE;
        }
    }
    /* COM1-COM9 and LPT1-LPT9. */
    return len == 4 && (EqualsUpperAscii(stem, "COM", 3) || EqualsUpperAscii(stem, "LPT", 3)) && stem[3] >= '1' && stem[3] <= '9';
}

void SaveFileStemFromName(const char *name, char *out, size_t outSize) {
    size_t n = 0;
    /* Room for the leading '_' a device name gets, and the terminator. */
    const size_t limit = outSize - 2;

    for (const U8 *p = (const U8 *)(name ? name : ""); *p && n < limit; p++) {
        const U8 c = *p;
        if (c >= 0x80) {
            const char *fold = kCp850Fold[c - 0x80];
            if (*fold == '\0')
                out[n++] = '_';
            for (; *fold && n < limit; fold++)
                out[n++] = *fold;
        } else if (c < 32 || c == 127 || strchr("<>:\"/\\|?*", c) != NULL) {
            out[n++] = '_';
        } else {
            out[n++] = (char)c;
        }
    }
    while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' '))
        n--;
    out[n] = '\0';

    if (n == 0) {
        strcpy(out, "save");
    } else if (IsWindowsDeviceName(out)) {
        memmove(out + 1, out, n + 1);
        out[0] = '_';
    }
}

#ifdef __cplusplus
}
#endif
