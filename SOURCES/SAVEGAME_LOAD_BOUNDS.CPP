#include "SAVEGAME_LOAD_BOUNDS.H"

#include <SYSTEM/LZ.H>

#include <stdlib.h>
#include <string.h>

U32 SaveLoadScreenBufferBytes(void) {
    return (U32)(640UL * 480UL + (U32)RECOVER_AREA);
}

U32 SaveLoadSubbufferBytesFromLeadingSkip(U32 leading_skip) {
    U32 cap = SaveLoadScreenBufferBytes();
    if (leading_skip >= cap)
        return 0;
    return cap - leading_skip;
}

S32 SaveLoadValidateCompressedStaging(const U8 *screen_base, U32 screen_cap,
                                      const U8 *compressed_ptr, U32 compressed_size,
                                      S32 sizefile) {
    if (!screen_base || screen_cap == 0)
        return FALSE;
    if (sizefile < 0)
        return FALSE;
    if ((U32)sizefile > screen_cap)
        return FALSE;
    if (compressed_ptr < screen_base)
        return FALSE;
    if (compressed_ptr >= screen_base + screen_cap)
        return FALSE;
    /* ExpandLZ output occupies [compressed_ptr, compressed_ptr + (U32)sizefile) */
    if ((U32)sizefile > (U32)(screen_base + screen_cap - compressed_ptr))
        return FALSE;
    {
        const U8 *ptrdecomp = compressed_ptr + (U32)sizefile + (U32)RECOVER_AREA;
        if (ptrdecomp < compressed_ptr)
            return FALSE; /* overflow */
        if (ptrdecomp > screen_base + screen_cap)
            return FALSE;
        if (compressed_size > (U32)(screen_base + screen_cap - ptrdecomp))
            return FALSE;
    }
    return TRUE;
}

#define SAVEGAME_MAX_PATCHES 500
#define SAVEGAME_MAX_OBJETS 100
#define SAVEGAME_PER_OBJECT_PREFIX 142u
#define SAVEGAME_OBJ_SUFFIX_32 136u
#define SAVEGAME_STRIDE32 (SAVEGAME_PER_OBJECT_PREFIX + SAVEGAME_OBJ_SUFFIX_32)

static S32 sniff_nb_patches(S32 nb_objets, const U8 *after_nb, size_t stride,
                            const U8 *stream_end) {
    if (nb_objets < 0 || nb_objets > SAVEGAME_MAX_OBJETS)
        return -1000000;
    if (!after_nb || !stream_end || stream_end < after_nb)
        return -1000000;
    if ((size_t)(stream_end - after_nb) < (size_t)nb_objets * stride + 4u)
        return -1000000;
    {
        const U8 *p = after_nb + (size_t)nb_objets * stride;
        S32 nb_patches;
        memcpy(&nb_patches, p, sizeof(S32));
        if (nb_patches < 0 || nb_patches > SAVEGAME_MAX_PATCHES)
            return -1000000;
        return nb_patches;
    }
}

S32 SaveLoadSniffNbPatchesAtStride(S32 nb_objets, const U8 *after_nb_objets, const U8 *stream_end,
                                   U32 stride_bytes) {
    return sniff_nb_patches(nb_objets, after_nb_objets, (size_t)stride_bytes, stream_end);
}

S32 SaveLoadGuessObjectWireStride(S32 nb_objets, const U8 *after_nb_objets, const U8 *stream_end,
                                  S32 stride64_native, S32 scene_nb_patches_hint) {
    /* First-guess only: the LoadContexte caller validates after the read and retries the
     * other stride on failure, so this just needs to pick the more likely candidate.
     * Strategy:
     *   1. If exactly one of the two offsets has a valid NbPatches sniff, pick that stride.
     *   2. If both are valid and exactly one matches the scene's NbPatches, pick that one.
     *   3. Otherwise return 0 — caller falls back to native and the retry handles the rest. */
    S32 s32 = sniff_nb_patches(nb_objets, after_nb_objets, SAVEGAME_STRIDE32, stream_end);
    S32 s64 = sniff_nb_patches(nb_objets, after_nb_objets, (size_t)stride64_native, stream_end);
    int ok32 = (s32 > -100000);
    int ok64 = (s64 > -100000);

    if (ok32 && !ok64)
        return (S32)SAVEGAME_STRIDE32;
    if (ok64 && !ok32)
        return stride64_native;

    if (ok32 && ok64 && scene_nb_patches_hint >= 0 && scene_nb_patches_hint <= SAVEGAME_MAX_PATCHES) {
        if (s32 == scene_nb_patches_hint && s64 != scene_nb_patches_hint)
            return (S32)SAVEGAME_STRIDE32;
        if (s64 == scene_nb_patches_hint && s32 != scene_nb_patches_hint)
            return stride64_native;
    }

    return 0;
}
