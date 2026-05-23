/*
 * PROJREC.CPP - Projection capture implementation. See LIB386/H/3D/PROJREC.H
 * for the design intent and the role this plays in the widescreen plan.
 *
 * Two output sinks, independent and composable:
 *
 *   Full text   (--capture-projection PATH): one event per line, human-
 *               readable, deterministic order. Large (hundreds of MB for a
 *               full --demo replay) but precise — diff-able for diagnosis.
 *
 *   Hash       (--projection-hash PATH): a single FNV-1a 64-bit digest
 *              computed over the same line text the full sink would have
 *              written. Tiny (one line in the output file). What CI commits
 *              and compares.
 *
 * Both sinks share the formatted line through emit(); whichever is active
 * consumes it. When neither sink is active every event is a single branch
 * (the IsActive check at the top of each Projrec_* function).
 *
 * Thread-safety: the engine is single-threaded; this module is too.
 */

#include <3D/PROJREC.H>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* FNV-1a 64-bit constants. Chosen over SHA-256 because (a) we only need
   collision-resistance against accidental drift, not adversarial input,
   and (b) it's a few lines of code with no header dependencies. */
#define PROJREC_FNV1A_OFFSET 0xcbf29ce484222325ULL
#define PROJREC_FNV1A_PRIME 0x00000100000001b3ULL

static FILE *s_fp_full = NULL;
static FILE *s_fp_hash = NULL;
static long s_event_count = 0;
static unsigned long long s_hash = PROJREC_FNV1A_OFFSET;

/* Shared formatter. Computes the line once into a local buffer, then
   dispatches to whichever sinks are active. Bounded buffer — any single
   event must fit in 256 bytes (our longest line is well under 128). */
static void emit(const char *fmt, ...) {
    if (!s_fp_full && !s_fp_hash)
        return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > (int)sizeof(buf) - 1)
        n = (int)sizeof(buf) - 1;
    if (s_fp_full)
        fwrite(buf, 1, (size_t)n, s_fp_full);
    if (s_fp_hash) {
        unsigned long long h = s_hash;
        for (int i = 0; i < n; ++i) {
            h ^= (unsigned char)buf[i];
            h *= PROJREC_FNV1A_PRIME;
        }
        s_hash = h;
    }
}

int Projrec_BeginCapture(const char *path) {
    /* Close any prior full capture; hash sink is independent and untouched. */
    if (s_fp_full) {
        fclose(s_fp_full);
        s_fp_full = NULL;
    }
    if (!path || !path[0])
        return 0;
    s_fp_full = fopen(path, "w");
    if (!s_fp_full) {
        fprintf(stderr, "[projrec] failed to open %s for writing\n", path);
        return 0;
    }
    /* Event count and hash reset only when no sink is currently active. If
       hash capture is already running, the full capture joins mid-stream and
       only mirrors events from this point — that's intentional, hash mode is
       the source of truth for what was captured. */
    if (!s_fp_hash) {
        s_event_count = 0;
        s_hash = PROJREC_FNV1A_OFFSET;
    }
    fprintf(s_fp_full, "# projrec v1\n");
    return 1;
}

int Projrec_BeginHashCapture(const char *path) {
    if (s_fp_hash) {
        fclose(s_fp_hash);
        s_fp_hash = NULL;
    }
    if (!path || !path[0])
        return 0;
    s_fp_hash = fopen(path, "w");
    if (!s_fp_hash) {
        fprintf(stderr, "[projrec] failed to open %s for hash writing\n", path);
        return 0;
    }
    if (!s_fp_full) {
        s_event_count = 0;
        s_hash = PROJREC_FNV1A_OFFSET;
    }
    return 1;
}

void Projrec_EndCapture(void) {
    if (s_fp_hash) {
        fprintf(s_fp_hash,
                "# projrec-hash v1\nSCENE events=%ld fnv1a=%016llx\n",
                s_event_count, s_hash);
        fclose(s_fp_hash);
        s_fp_hash = NULL;
    }
    if (s_fp_full) {
        fclose(s_fp_full);
        s_fp_full = NULL;
    }
    if (s_event_count > 0)
        fprintf(stderr, "[projrec] captured %ld events (fnv1a=%016llx)\n",
                s_event_count, s_hash);
    s_event_count = 0;
    s_hash = PROJREC_FNV1A_OFFSET;
}

int Projrec_IsActive(void) {
    return s_fp_full != NULL || s_fp_hash != NULL;
}

void Projrec_SetProjection(S32 xc, S32 yc, S32 clip, S32 fx, S32 fy) {
    if (!Projrec_IsActive())
        return;
    ++s_event_count;
    emit("SETPROJ seq=%ld xc=%ld yc=%ld clip=%ld fx=%ld fy=%ld\n",
         s_event_count, (long)xc, (long)yc, (long)clip, (long)fx, (long)fy);
}

void Projrec_SetIsoProjection(S32 xc, S32 yc) {
    if (!Projrec_IsActive())
        return;
    ++s_event_count;
    emit("SETISO seq=%ld xc=%ld yc=%ld\n",
         s_event_count, (long)xc, (long)yc);
}

void Projrec_LongProjectPoint3D(S32 x, S32 y, S32 z, S32 ret, S32 xp, S32 yp) {
    if (!Projrec_IsActive())
        return;
    ++s_event_count;
    emit("PROJ seq=%ld x=%ld y=%ld z=%ld -> ret=%ld xp=%ld yp=%ld\n",
         s_event_count, (long)x, (long)y, (long)z,
         (long)ret, (long)xp, (long)yp);
}

void Projrec_LongProjectPointIso(S32 x, S32 y, S32 z, S32 xp, S32 yp) {
    if (!Projrec_IsActive())
        return;
    ++s_event_count;
    emit("PROJISO seq=%ld x=%ld y=%ld z=%ld -> xp=%ld yp=%ld\n",
         s_event_count, (long)x, (long)y, (long)z,
         (long)xp, (long)yp);
}

void Projrec_ProjectList3D(S32 nbpt, S32 orgx, S32 orgy, S32 orgz) {
    if (!Projrec_IsActive())
        return;
    ++s_event_count;
    emit("PRLI seq=%ld nbpt=%ld orgx=%ld orgy=%ld orgz=%ld\n",
         s_event_count, (long)nbpt, (long)orgx, (long)orgy, (long)orgz);
}

void Projrec_ProjectListIso(S32 nbpt, S32 orgx, S32 orgy, S32 orgz) {
    if (!Projrec_IsActive())
        return;
    ++s_event_count;
    emit("PRLIISO seq=%ld nbpt=%ld orgx=%ld orgy=%ld orgz=%ld\n",
         s_event_count, (long)nbpt, (long)orgx, (long)orgy, (long)orgz);
}

void Projrec_Map2Screen(S32 x, S32 y, S32 z, S32 xscreen, S32 yscreen) {
    if (!Projrec_IsActive())
        return;
    ++s_event_count;
    emit("M2S seq=%ld x=%ld y=%ld z=%ld -> xs=%ld ys=%ld\n",
         s_event_count, (long)x, (long)y, (long)z,
         (long)xscreen, (long)yscreen);
}
