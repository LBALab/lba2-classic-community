#include <SVGA/STAGING_BUFFER.H>

#include <stdlib.h>

/* 4 bytes per pixel (ARGB8888). */
#define STAGING_BPP 4u

U8 *StagingBufferEnsure(StagingBuffer *sb, U32 width, U32 height) {
    if (sb->pixels != NULL && sb->width == width && sb->height == height)
        return sb->pixels;

    /* Dimensions changed (or first call): drop the old buffer and reset, so a
     * failure below leaves sb empty and the next call retries. */
    free(sb->pixels);
    sb->pixels = NULL;
    sb->width = 0;
    sb->height = 0;

    if (width == 0 || height == 0)
        return NULL;

    /* size_t math guards the multiply against U32 overflow on large modes. */
    size_t bytes = (size_t)width * (size_t)height * STAGING_BPP;
    U8 *pixels = (U8 *)malloc(bytes);
    if (pixels == NULL)
        return NULL;

    sb->pixels = pixels;
    sb->width = width;
    sb->height = height;
    return sb->pixels;
}

void StagingBufferFree(StagingBuffer *sb) {
    free(sb->pixels);
    sb->pixels = NULL;
    sb->width = 0;
    sb->height = 0;
}
