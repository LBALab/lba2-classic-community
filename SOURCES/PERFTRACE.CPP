/*
 * Per-frame timing capture for diagnosing perf cliffs that need real user
 * input or scene state to reproduce. Ring buffer; opt-in via the `perftrace`
 * console verb. See docs/PERFTRACE.md.
 */

#include "PERFTRACE.H"

#include "C_EXTERN.H" /* CubeMode */
#include "COMMON.H"   /* CUBE_INTERIEUR / CUBE_EXTERIEUR */

#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PERFTRACE_RING_SIZE 256

typedef struct {
    U64 frame_us;
    U64 phase_us[PERFTRACE_PHASE_COUNT];
    U8 scene_tag; /* 0 = unknown / pre-load, 1 = interior/iso, 2 = exterior */
} Sample;

static Sample s_ring[PERFTRACE_RING_SIZE];
static int s_ring_head;  /* next write position */
static int s_ring_count; /* valid samples, capped at PERFTRACE_RING_SIZE */
static int s_active;

static U64 s_freq;
static U64 s_frame_start;
static U64 s_phase_start[PERFTRACE_PHASE_COUNT];
static U64 s_phase_acc[PERFTRACE_PHASE_COUNT]; /* accumulated within current frame */

/* Aggregate counters since the last Reset/Start. Survive ring rotation so
   the dump header shows the true rate across the whole capture window, not
   just the last PERFTRACE_RING_SIZE samples. */
static U64 s_capture_start_ticks;
static U64 s_capture_last_ticks;
static U64 s_total_frames;

static U64 perf_now(void) {
    return (U64)SDL_GetPerformanceCounter();
}

static U64 ticks_to_us(U64 ticks) {
    if (s_freq == 0)
        return 0;
    return (ticks * 1000000ULL) / s_freq;
}

void Perftrace_Init(void) {
    s_freq = (U64)SDL_GetPerformanceFrequency();
    s_ring_head = 0;
    s_ring_count = 0;
    s_active = 0;
    memset(s_phase_acc, 0, sizeof(s_phase_acc));
}

void Perftrace_Start(void) {
    if (s_freq == 0)
        s_freq = (U64)SDL_GetPerformanceFrequency();
    s_active = 1;
}

void Perftrace_Stop(void) {
    s_active = 0;
}

int Perftrace_IsActive(void) {
    return s_active;
}

void Perftrace_Reset(void) {
    s_ring_head = 0;
    s_ring_count = 0;
    memset(s_ring, 0, sizeof(s_ring));
    memset(s_phase_acc, 0, sizeof(s_phase_acc));
    s_capture_start_ticks = 0;
    s_capture_last_ticks = 0;
    s_total_frames = 0;
}

void Perftrace_FrameBegin(void) {
    if (!s_active)
        return;
    s_frame_start = perf_now();
    memset(s_phase_acc, 0, sizeof(s_phase_acc));
}

void Perftrace_FrameEnd(void) {
    if (!s_active || s_frame_start == 0)
        return;
    U64 dt = perf_now() - s_frame_start;
    Sample *out = &s_ring[s_ring_head];
    out->frame_us = ticks_to_us(dt);
    for (int p = 0; p < PERFTRACE_PHASE_COUNT; ++p) {
        out->phase_us[p] = ticks_to_us(s_phase_acc[p]);
    }
    if (CubeMode == CUBE_INTERIEUR)
        out->scene_tag = 1;
    else if (CubeMode == CUBE_EXTERIEUR)
        out->scene_tag = 2;
    else
        out->scene_tag = 0;

    s_ring_head = (s_ring_head + 1) % PERFTRACE_RING_SIZE;
    if (s_ring_count < PERFTRACE_RING_SIZE)
        s_ring_count++;
    s_frame_start = 0;

    U64 now_ms = (U64)SDL_GetTicks();
    if (s_total_frames == 0)
        s_capture_start_ticks = now_ms;
    s_capture_last_ticks = now_ms;
    s_total_frames++;
}

void Perftrace_PhaseBegin(int phase) {
    if (!s_active || phase < 0 || phase >= PERFTRACE_PHASE_COUNT)
        return;
    s_phase_start[phase] = perf_now();
}

void Perftrace_PhaseEnd(int phase) {
    if (!s_active || phase < 0 || phase >= PERFTRACE_PHASE_COUNT)
        return;
    if (s_phase_start[phase] == 0)
        return;
    s_phase_acc[phase] += perf_now() - s_phase_start[phase];
    s_phase_start[phase] = 0;
}

void Perftrace_PresentBegin(void) {
    Perftrace_PhaseBegin(PERFTRACE_PHASE_PRESENT);
}

void Perftrace_PresentEnd(void) {
    Perftrace_PhaseEnd(PERFTRACE_PHASE_PRESENT);
}

/* ---------- auto-dump ---------- */

static char s_autodump_path[512];
static int s_autodump_registered;

static void perftrace_atexit(void) {
    if (s_autodump_path[0] == '\0')
        return;
    Perftrace_Dump(s_autodump_path, PERFTRACE_RING_SIZE);
}

void Perftrace_SetAutoDumpPath(const char *path) {
    if (!path || !*path) {
        s_autodump_path[0] = '\0';
        return;
    }
    size_t n = strlen(path);
    if (n >= sizeof(s_autodump_path))
        n = sizeof(s_autodump_path) - 1;
    memcpy(s_autodump_path, path, n);
    s_autodump_path[n] = '\0';
    if (!s_autodump_registered) {
        atexit(perftrace_atexit);
        s_autodump_registered = 1;
    }
}

/* ---------- summary helpers ---------- */

static int cmp_u64(const void *a, const void *b) {
    U64 x = *(const U64 *)a, y = *(const U64 *)b;
    return (x > y) - (x < y);
}

typedef struct {
    int count;
    U64 sum;
    U64 min;
    U64 max;
    U64 p50;
    U64 p95;
    U64 p99;
    U64 sum_phase[PERFTRACE_PHASE_COUNT];
} Summary;

static void compute_summary(const U64 *vals, const U64 (*phase_vals)[PERFTRACE_PHASE_COUNT], int n,
                            Summary *out) {
    memset(out, 0, sizeof(*out));
    if (n <= 0)
        return;
    U64 *tmp = (U64 *)malloc(sizeof(U64) * (size_t)n);
    if (!tmp)
        return;
    memcpy(tmp, vals, sizeof(U64) * (size_t)n);
    qsort(tmp, (size_t)n, sizeof(U64), cmp_u64);
    out->count = n;
    out->min = tmp[0];
    out->max = tmp[n - 1];
    out->p50 = tmp[(n * 50) / 100];
    out->p95 = tmp[(n * 95) / 100];
    out->p99 = tmp[(n * 99) / 100];
    for (int i = 0; i < n; ++i) {
        out->sum += vals[i];
        for (int p = 0; p < PERFTRACE_PHASE_COUNT; ++p) {
            out->sum_phase[p] += phase_vals[i][p];
        }
    }
    free(tmp);
}

static const char *tag_name(U8 t) {
    switch (t) {
    case 1:
        return "iso";
    case 2:
        return "ext";
    default:
        return "?";
    }
}

static int slice(int max_samples, int tag_filter, U64 *out_frame,
                 U64 (*out_phase)[PERFTRACE_PHASE_COUNT]) {
    int n = s_ring_count;
    if (n <= 0)
        return 0;
    int take = (max_samples > 0 && max_samples < n) ? max_samples : n;
    int start = (s_ring_head - take + PERFTRACE_RING_SIZE) % PERFTRACE_RING_SIZE;
    int written = 0;
    for (int i = 0; i < take; ++i) {
        int idx = (start + i) % PERFTRACE_RING_SIZE;
        if (tag_filter != 0 && s_ring[idx].scene_tag != tag_filter)
            continue;
        out_frame[written] = s_ring[idx].frame_us;
        for (int p = 0; p < PERFTRACE_PHASE_COUNT; ++p) {
            out_phase[written][p] = s_ring[idx].phase_us[p];
        }
        written++;
    }
    return written;
}

static void dump_section(FILE *f, const char *label, int tag_filter, int max_samples) {
    U64 *frame = (U64 *)malloc(sizeof(U64) * PERFTRACE_RING_SIZE);
    U64(*phase)
    [PERFTRACE_PHASE_COUNT] = (U64(*)[PERFTRACE_PHASE_COUNT])malloc(
        sizeof(U64[PERFTRACE_PHASE_COUNT]) * PERFTRACE_RING_SIZE);
    if (!frame || !phase) {
        free(frame);
        free(phase);
        return;
    }
    int n = slice(max_samples, tag_filter, frame, phase);
    Summary s;
    compute_summary(frame, phase, n, &s);
    fprintf(f, "## %s  (n=%d)\n", label, n);
    if (n == 0) {
        fprintf(f, "  (no samples)\n\n");
        free(frame);
        free(phase);
        return;
    }
    double avg = (double)s.sum / (double)n;
    double scene_share = s.sum > 0 ? (100.0 * (double)s.sum_phase[PERFTRACE_PHASE_SCENE]) / (double)s.sum : 0.0;
    double present_share = s.sum > 0 ? (100.0 * (double)s.sum_phase[PERFTRACE_PHASE_PRESENT]) / (double)s.sum : 0.0;
    double avg_fps = avg > 0 ? 1000000.0 / avg : 0.0;
    fprintf(f, "  frame_us  min=%lu  avg=%.0f  p50=%lu  p95=%lu  p99=%lu  max=%lu  (~%.1f fps)\n",
            (unsigned long)s.min, avg, (unsigned long)s.p50, (unsigned long)s.p95,
            (unsigned long)s.p99, (unsigned long)s.max, avg_fps);
    fprintf(f, "  scene avg=%.0f us  (%.1f%% of frame)\n",
            n ? (double)s.sum_phase[PERFTRACE_PHASE_SCENE] / (double)n : 0.0, scene_share);
    fprintf(f, "  present avg=%.0f us  (%.1f%% of frame)\n\n",
            n ? (double)s.sum_phase[PERFTRACE_PHASE_PRESENT] / (double)n : 0.0, present_share);
    free(frame);
    free(phase);
}

int Perftrace_Dump(const char *path, int max_samples) {
    if (!path || !*path)
        return -1;
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;

    fprintf(f, "# perftrace dump\n");
    fprintf(f, "# active=%d  ring_count=%d  ring_capacity=%d\n", s_active, s_ring_count,
            PERFTRACE_RING_SIZE);

    U64 elapsed_ms = (s_capture_last_ticks > s_capture_start_ticks)
                         ? (s_capture_last_ticks - s_capture_start_ticks)
                         : 0;
    double samples_per_sec =
        elapsed_ms > 0 ? (1000.0 * (double)s_total_frames) / (double)elapsed_ms : 0.0;
    fprintf(f, "# capture_window=%lu ms  total_frames=%lu  rate=%.1f FrameEnd/s\n\n",
            (unsigned long)elapsed_ms, (unsigned long)s_total_frames, samples_per_sec);

    dump_section(f, "All frames", 0, max_samples);
    dump_section(f, "Interior / iso", 1, max_samples);
    dump_section(f, "Exterior", 2, max_samples);

    fprintf(f, "## Per-frame samples (oldest first, microseconds)\n");
    fprintf(f, "# idx  tag    frame   scene  present\n");
    int n = s_ring_count;
    int take = (max_samples > 0 && max_samples < n) ? max_samples : n;
    int start = (s_ring_head - take + PERFTRACE_RING_SIZE) % PERFTRACE_RING_SIZE;
    for (int i = 0; i < take; ++i) {
        int idx = (start + i) % PERFTRACE_RING_SIZE;
        const Sample *r = &s_ring[idx];
        fprintf(f, "%5d  %-4s  %6lu  %6lu  %6lu\n", i, tag_name(r->scene_tag),
                (unsigned long)r->frame_us, (unsigned long)r->phase_us[PERFTRACE_PHASE_SCENE],
                (unsigned long)r->phase_us[PERFTRACE_PHASE_PRESENT]);
    }

    fclose(f);
    return 0;
}

void Perftrace_PrintStatus(Perftrace_PrintFn sink) {
    if (!sink)
        return;
    char line[160];
    if (s_ring_count == 0) {
        snprintf(line, sizeof(line), "perftrace: active=%d  no samples yet", s_active);
        sink(line);
        return;
    }
    int n = s_ring_count < 60 ? s_ring_count : 60;
    int start = (s_ring_head - n + PERFTRACE_RING_SIZE) % PERFTRACE_RING_SIZE;
    U64 sum_frame = 0, sum_scene = 0, sum_present = 0;
    int iso_n = 0, ext_n = 0;
    U64 iso_sum = 0, ext_sum = 0;
    for (int i = 0; i < n; ++i) {
        int idx = (start + i) % PERFTRACE_RING_SIZE;
        const Sample *r = &s_ring[idx];
        sum_frame += r->frame_us;
        sum_scene += r->phase_us[PERFTRACE_PHASE_SCENE];
        sum_present += r->phase_us[PERFTRACE_PHASE_PRESENT];
        if (r->scene_tag == 1) {
            iso_n++;
            iso_sum += r->frame_us;
        } else if (r->scene_tag == 2) {
            ext_n++;
            ext_sum += r->frame_us;
        }
    }
    double avg = (double)sum_frame / (double)n;
    double iso_avg = iso_n > 0 ? (double)iso_sum / (double)iso_n : 0.0;
    double ext_avg = ext_n > 0 ? (double)ext_sum / (double)ext_n : 0.0;
    snprintf(line, sizeof(line),
             "perftrace: active=%d n=%d frame_avg=%.0fus (~%.1f fps)  iso=%.0fus(n=%d)  "
             "ext=%.0fus(n=%d)",
             s_active, n, avg, avg > 0 ? 1000000.0 / avg : 0.0, iso_avg, iso_n, ext_avg, ext_n);
    sink(line);
    snprintf(line, sizeof(line), "  scene_avg=%.0fus  present_avg=%.0fus  (ring_capacity=%d)",
             (double)sum_scene / (double)n, (double)sum_present / (double)n, PERFTRACE_RING_SIZE);
    sink(line);
}
