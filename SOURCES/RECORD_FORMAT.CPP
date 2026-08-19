#include "RECORD_FORMAT.H"

#include <stdio.h>  /* snprintf, sscanf */
#include <string.h> /* strstr, strchr, strlen */

//-------------------------------------------------------------------------
/* The value of `key`, or NULL when the header does not carry it. Searching for the
   key and stepping over it here is what keeps the offset and the key in one place. */
static const char *field(const char *hdr, const char *key) {
    const char *p;

    if (hdr == NULL OR key == NULL)
        return NULL;

    p = strstr(hdr, key);
    if (p == NULL)
        return NULL;

    return p + strlen(key);
}

//-------------------------------------------------------------------------
S32 RecFmt_WriteBindings(char *out, size_t n, const T_DEF_KEY *tab, S32 slots,
                         const char *key) {
    size_t at;
    S32 i;

    if (out == NULL OR n == 0 OR tab == NULL OR key == NULL OR slots <= 0)
        return 0;

    out[0] = 0;
    at = (size_t)snprintf(out, n, "%s", key);
    if (at >= n)
        return 0;

    for (i = 0; i < slots; i++) {
        /* snprintf returns what it would have written, so a return that runs past the
           end is how truncation is detected rather than by comparing lengths after.
           The room is held in its own variable and the two failures tested apart
           because `OR (size_t)x` reformats into `OR(size_t) x`: the OR macro reads as
           a call once a cast follows it. */
        size_t room = n - at;
        int wrote = snprintf(out + at, room, "%s%u:%u", i ? "," : "",
                             (unsigned)tab[i].Key1, (unsigned)tab[i].Key2);
        if (wrote < 0)
            return 0;
        if ((size_t)wrote >= room)
            return 0;
        at += (size_t)wrote;
    }

    if (at + 1 >= n)
        return 0;
    out[at] = '\n';
    out[at + 1] = 0;
    return 1;
}

//-------------------------------------------------------------------------
S32 RecFmt_ParseBindings(const char *hdr, const char *key, T_DEF_KEY *tab, S32 slots) {
    const char *p = field(hdr, key);
    const char *end;
    S32 i;

    if (p == NULL OR tab == NULL OR slots <= 0)
        return 0;

    /* The table is one line. Bound the walk to it up front: a short binding line
       followed by another `k:v` field would otherwise read on into that field and
       come back with a full table built partly from someone else's numbers. */
    end = strchr(p, '\n');
    if (end == NULL)
        end = p + strlen(p);

    for (i = 0; i < slots; i++) {
        unsigned a = 0, b = 0;
        const char *comma;

        if (p >= end OR sscanf(p, "%u:%u", &a, &b) != 2)
            return 0;
        tab[i].Key1 = (U32)a;
        tab[i].Key2 = (U32)b;

        comma = strchr(p, ',');
        if (comma == NULL OR comma >= end) {
            /* Ran out of pairs on this line. Only the last slot may end without a
               comma; anywhere earlier and the table is short, which is a different
               set of bindings rather than a partial one. */
            return i == slots - 1;
        }
        p = comma + 1;
    }
    return 1;
}

//-------------------------------------------------------------------------
S32 RecFmt_ReadU32(const char *hdr, const char *key, U32 *out) {
    const char *p = field(hdr, key);
    unsigned v = 0;

    if (p == NULL OR out == NULL)
        return 0;
    if (sscanf(p, "%u", &v) != 1)
        return 0;

    *out = (U32)v;
    return 1;
}

//-------------------------------------------------------------------------
S32 RecFmt_ReadInt(const char *hdr, const char *key, S32 *out) {
    const char *p = field(hdr, key);
    int v = 0;

    if (p == NULL OR out == NULL)
        return 0;
    if (sscanf(p, "%d", &v) != 1)
        return 0;

    *out = (S32)v;
    return 1;
}

//-------------------------------------------------------------------------
S32 RecFmt_ReadStr(const char *hdr, const char *key, char *out, size_t n) {
    const char *p = field(hdr, key);
    const char *nl;
    size_t len;

    if (p == NULL OR out == NULL OR n == 0)
        return 0;

    nl = strchr(p, '\n');
    len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
    if (len >= n)
        return 0; /* the caller's buffer cannot hold the value; a cut path is a wrong path */

    memcpy(out, p, len);
    out[len] = 0;
    return 1;
}
