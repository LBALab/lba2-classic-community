/* The predictive per-object stride sniff (SaveLoadGuessObjectWireStride /
 * SaveLoadSniffNbPatchesAtStride) was retired when SaveContexte began writing the
 * canonical 32-bit wire on every host: the reader now tries that layout first and
 * only falls back to the legacy native stride on validation failure, so it no
 * longer needs a heuristic to pick a first guess. See SAVEGAME.CPP LoadContexte
 * and docs/SAVE_WIRE_PLAN.md. What remains here is the compressed-staging bounds
 * validation, which is orthogonal. */

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
