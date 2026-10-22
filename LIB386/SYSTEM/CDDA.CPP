//──────────────────────────────────────────────────────────────────────────
#include <SYSTEM/CDDA.H>

// Sampled rather than swept: a track is tens of megabytes, and the statistic
// separates the two orders by roughly an order of magnitude, so a scattering of
// windows settles it. Spreading them means a long silent passage cannot decide
// the answer on its own. The window size and count are CDDA_PROBE_WINDOW and
// CDDA_PROBE_COUNT in the header, so a streaming reader can fetch the windows.

// Below this mean magnitude there is not enough signal to tell the orders apart.
// A quarter of full scale sits well above any music (0.06 measured) and well
// below noise (0.50), so it only rejects buffers that are near silent.
#define CDDA_MIN_LEVEL (32768 / 4)

//──────────────────────────────────────────────────────────────────────────
extern "C" size_t CdDa_ProbeOffset(size_t bytes, int probe) {
    size_t stride = (bytes - CDDA_PROBE_WINDOW) / CDDA_PROBE_COUNT;
    return (stride * (size_t)probe) & ~(size_t)1; // stay on a sample boundary
}

//──────────────────────────────────────────────────────────────────────────
static int JudgeWindows(const uint8_t *const windows[CDDA_PROBE_COUNT]) {
    uint64_t sumLE = 0, sumBE = 0, n = 0;
    for (int p = 0; p < CDDA_PROBE_COUNT; p++) {
        const uint8_t *w = windows[p];
        for (size_t i = 0; i + 1 < CDDA_PROBE_WINDOW; i += 2) {
            int32_t le = (int16_t)((uint16_t)w[i] | ((uint16_t)w[i + 1] << 8));
            int32_t be = (int16_t)(((uint16_t)w[i] << 8) | (uint16_t)w[i + 1]);
            sumLE += (uint64_t)(le < 0 ? -le : le);
            sumBE += (uint64_t)(be < 0 ? -be : be);
            n++;
        }
    }

    if (n == 0 || sumLE / n < (uint64_t)CDDA_MIN_LEVEL)
        return 0;
    // A decisive margin, not a nudge: the two orders differ by about 6x on a
    // real disc, so requiring a factor of two cannot fire on a close call.
    return (sumBE * 2 < sumLE) ? 1 : 0;
}

//──────────────────────────────────────────────────────────────────────────
extern "C" int CdDa_NeedsByteSwap(const uint8_t *pcm, size_t bytes) {
    if (pcm == NULL || bytes < (size_t)CDDA_PROBE_WINDOW * 2)
        return 0;
    const uint8_t *windows[CDDA_PROBE_COUNT];
    for (int p = 0; p < CDDA_PROBE_COUNT; p++)
        windows[p] = pcm + CdDa_ProbeOffset(bytes, p);
    return JudgeWindows(windows);
}

//──────────────────────────────────────────────────────────────────────────
extern "C" int CdDa_NeedsByteSwapWindows(const uint8_t *windows, size_t bytes) {
    if (windows == NULL || bytes < (size_t)CDDA_PROBE_WINDOW * 2)
        return 0;
    const uint8_t *w[CDDA_PROBE_COUNT];
    for (int p = 0; p < CDDA_PROBE_COUNT; p++)
        w[p] = windows + (size_t)p * CDDA_PROBE_WINDOW;
    return JudgeWindows(w);
}

//──────────────────────────────────────────────────────────────────────────
extern "C" void CdDa_ByteSwap(uint8_t *pcm, size_t bytes) {
    if (pcm == NULL)
        return;
    for (size_t i = 0; i + 1 < bytes; i += 2) {
        uint8_t t = pcm[i];
        pcm[i] = pcm[i + 1];
        pcm[i + 1] = t;
    }
}
