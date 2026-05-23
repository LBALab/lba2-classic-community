/*
 * PROJREC.CPP - Projection capture implementation. See LIB386/H/3D/PROJREC.H
 * for the design intent and the role this plays in the widescreen plan.
 *
 * File format (v1, text, line-oriented, one event per line):
 *
 *   # projrec v1
 *   SETPROJ seq=N xc=... yc=... clip=... fx=... fy=...
 *   ...
 *
 * Sequence numbers are 1-based and incremented per event so the consumer
 * can spot dropped or reordered events without needing wall-clock state.
 *
 * Thread-safety: the engine is single-threaded; this module is too.
 */

#include <3D/PROJREC.H>

#include <stdio.h>
#include <string.h>

static FILE *s_fp = NULL;
static long s_event_count = 0;

int Projrec_BeginCapture(const char *path) {
    if (s_fp) {
        /* Close the previous capture before opening a new one. Idempotent
           Begin/End sequencing keeps the caller from leaking a file handle
           if it forgets to End between runs. */
        Projrec_EndCapture();
    }
    if (!path || !path[0])
        return 0;
    s_fp = fopen(path, "w");
    if (!s_fp) {
        fprintf(stderr, "[projrec] failed to open %s for writing\n", path);
        return 0;
    }
    s_event_count = 0;
    fprintf(s_fp, "# projrec v1\n");
    return 1;
}

void Projrec_EndCapture(void) {
    if (!s_fp)
        return;
    fprintf(stderr, "[projrec] captured %ld events\n", s_event_count);
    fclose(s_fp);
    s_fp = NULL;
    s_event_count = 0;
}

int Projrec_IsActive(void) {
    return s_fp != NULL;
}

void Projrec_SetProjection(S32 xc, S32 yc, S32 clip, S32 fx, S32 fy) {
    if (!s_fp)
        return;
    ++s_event_count;
    fprintf(s_fp, "SETPROJ seq=%ld xc=%ld yc=%ld clip=%ld fx=%ld fy=%ld\n",
            s_event_count, (long)xc, (long)yc, (long)clip, (long)fx, (long)fy);
}

void Projrec_SetIsoProjection(S32 xc, S32 yc) {
    if (!s_fp)
        return;
    ++s_event_count;
    fprintf(s_fp, "SETISO seq=%ld xc=%ld yc=%ld\n",
            s_event_count, (long)xc, (long)yc);
}

void Projrec_LongProjectPoint3D(S32 x, S32 y, S32 z, S32 ret, S32 xp, S32 yp) {
    if (!s_fp)
        return;
    ++s_event_count;
    fprintf(s_fp,
            "PROJ seq=%ld x=%ld y=%ld z=%ld -> ret=%ld xp=%ld yp=%ld\n",
            s_event_count, (long)x, (long)y, (long)z,
            (long)ret, (long)xp, (long)yp);
}

void Projrec_LongProjectPointIso(S32 x, S32 y, S32 z, S32 xp, S32 yp) {
    if (!s_fp)
        return;
    ++s_event_count;
    fprintf(s_fp,
            "PROJISO seq=%ld x=%ld y=%ld z=%ld -> xp=%ld yp=%ld\n",
            s_event_count, (long)x, (long)y, (long)z,
            (long)xp, (long)yp);
}

void Projrec_ProjectList3D(S32 nbpt, S32 orgx, S32 orgy, S32 orgz) {
    if (!s_fp)
        return;
    ++s_event_count;
    fprintf(s_fp,
            "PRLI seq=%ld nbpt=%ld orgx=%ld orgy=%ld orgz=%ld\n",
            s_event_count, (long)nbpt, (long)orgx, (long)orgy, (long)orgz);
}

void Projrec_ProjectListIso(S32 nbpt, S32 orgx, S32 orgy, S32 orgz) {
    if (!s_fp)
        return;
    ++s_event_count;
    fprintf(s_fp,
            "PRLIISO seq=%ld nbpt=%ld orgx=%ld orgy=%ld orgz=%ld\n",
            s_event_count, (long)nbpt, (long)orgx, (long)orgy, (long)orgz);
}

void Projrec_Map2Screen(S32 x, S32 y, S32 z, S32 xscreen, S32 yscreen) {
    if (!s_fp)
        return;
    ++s_event_count;
    fprintf(s_fp,
            "M2S seq=%ld x=%ld y=%ld z=%ld -> xs=%ld ys=%ld\n",
            s_event_count, (long)x, (long)y, (long)z,
            (long)xscreen, (long)yscreen);
}
