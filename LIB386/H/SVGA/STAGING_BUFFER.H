#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

/* A lazily-(re)allocated RGBA scratch buffer keyed on (width, height).
 *
 * The Android present path stages its 8-bit -> ARGB conversion through a
 * buffer it owns (SDL_LockTexture's pointer can carry a stale memory tag on
 * Android 16+ MTE). Reusing one persistent buffer avoids a per-frame malloc;
 * the buffer is only reallocated when the frame dimensions change.
 *
 * Pure: no SDL or platform dependency, so the sizing logic is unit-testable on
 * the host (where ASan stands in for MTE — both catch a buffer sized too small
 * for the requested dimensions). */
typedef struct {
    U8 *pixels; /* width*height*4 bytes, or NULL when unallocated */
    U32 width;
    U32 height;
} StagingBuffer;

/* Ensure sb holds a width*height*4 RGBA buffer, reallocating only when the
 * dimensions differ from the last call. Returns sb->pixels, or NULL on a zero
 * dimension or allocation failure (sb is reset to empty so the next call
 * retries cleanly). */
U8 *StagingBufferEnsure(StagingBuffer *sb, U32 width, U32 height);

/* Free sb's buffer and reset it to empty. Safe on an already-empty buffer. */
void StagingBufferFree(StagingBuffer *sb);

// =============================================================================
#ifdef __cplusplus
}
#endif
