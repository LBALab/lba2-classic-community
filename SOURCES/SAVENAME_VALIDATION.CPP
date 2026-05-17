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

#ifdef __cplusplus
}
#endif
