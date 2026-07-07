#ifndef SAVEGAME_LOAD_BOUNDS_H
#define SAVEGAME_LOAD_BOUNDS_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Same allocation as MEM.CPP ListMem Screen / BufSpeak: 640*480 + RECOVER_AREA */
U32 SaveLoadScreenBufferBytes(void);

/* Max bytes that fit when the save stream starts at `leading_skip` inside that buffer. */
U32 SaveLoadSubbufferBytesFromLeadingSkip(U32 leading_skip);

/*
 * Validate ExpandLZ staging used by SAVEGAME.CPP (compressed tail memcpy + output window).
 * compressed_ptr = PtrSave after reading declared uncompressed size (sizefile).
 * compressed_size = bytes of compressed tail still in the file buffer.
 */
S32 SaveLoadValidateCompressedStaging(const U8 *screen_base, U32 screen_cap,
                                      const U8 *compressed_ptr, U32 compressed_size,
                                      S32 sizefile);

/* The per-object stride sniff that used to live here was retired with the
 * canonical-first reader (SAVEGAME.CPP LoadContexte): the reader reads the
 * canonical 32-bit wire first and falls back to the legacy native stride only on
 * validation failure, so no first-guess heuristic is needed. See
 * SAVEGAME_LOAD_BOUNDS.CPP and docs/SAVE_WIRE_PLAN.md. */

#ifdef __cplusplus
}
#endif

#endif
