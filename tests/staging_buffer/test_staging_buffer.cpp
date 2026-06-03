/* Unit test for the persistent RGBA staging buffer used by the Android present
 * path (LIB386/SVGA/STAGING_BUFFER.CPP).
 *
 * The real risk this guards is a buffer sized too small after a resize: the
 * present loop would then write past the end. On Android 16+ MTE traps that as
 * SEGV_ACCERR; here we write the full claimed extent after every resize so
 * AddressSanitizer (the host stand-in for MTE) traps it on any platform. Run
 * under the `linux_sanitize` preset for that coverage; the size assertions
 * below also fail without a sanitizer. */
#include <SVGA/STAGING_BUFFER.H>

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Touch every byte of the claimed buffer; an undersized one traps under ASan. */
static void writeFullExtent(U8 *p, U32 w, U32 h) {
    memset(p, 0xAB, (size_t)w * (size_t)h * 4u);
}

int main(void) {
    StagingBuffer sb;
    memset(&sb, 0, sizeof(sb));

    /* First allocation. */
    U8 *a = StagingBufferEnsure(&sb, 640, 480);
    assert(a != NULL);
    assert(sb.width == 640 && sb.height == 480);
    writeFullExtent(a, 640, 480);

    /* Same dimensions: no realloc, same pointer. */
    U8 *again = StagingBufferEnsure(&sb, 640, 480);
    assert(again == a);
    writeFullExtent(again, 640, 480);

    /* Grow. */
    U8 *big = StagingBufferEnsure(&sb, 1920, 1080);
    assert(big != NULL);
    assert(sb.width == 1920 && sb.height == 1080);
    writeFullExtent(big, 1920, 1080);

    /* Shrink. */
    U8 *small = StagingBufferEnsure(&sb, 320, 200);
    assert(small != NULL);
    assert(sb.width == 320 && sb.height == 200);
    writeFullExtent(small, 320, 200);

    /* Zero dimension: NULL and reset to empty. */
    assert(StagingBufferEnsure(&sb, 0, 0) == NULL);
    assert(sb.pixels == NULL && sb.width == 0 && sb.height == 0);
    assert(StagingBufferEnsure(&sb, 100, 0) == NULL);
    assert(StagingBufferEnsure(&sb, 0, 100) == NULL);

    /* Recovers after a zero request. */
    U8 *back = StagingBufferEnsure(&sb, 800, 600);
    assert(back != NULL);
    writeFullExtent(back, 800, 600);

    StagingBufferFree(&sb);
    assert(sb.pixels == NULL && sb.width == 0 && sb.height == 0);

    /* Free is idempotent on an empty buffer. */
    StagingBufferFree(&sb);

    printf("test_staging_buffer: OK\n");
    return 0;
}
