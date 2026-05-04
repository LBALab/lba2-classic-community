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

/*
 * Heuristic: which per-object stride (bytes) matches NbPatches after the object array.
 * Returns 278 (32-bit wire), stride64_native (from caller), or 0 if ambiguous / invalid or
 * both sniffs failed without a raw word matching the scene hint (caller then uses native stride).
 * `after_nb_objets` = PtrSave immediately after the NbObjets S32 has been consumed.
 * stride64_native = 142 + (sizeof(T_OBJ_3D) - sizeof(CurrentFrame)) on the host build.
 * scene_nb_patches_hint: NbPatches from the loaded scene (disk) before save stream overwrites it;
 *   pass -1 to disable. When both 32- and 64-bit wire layouts look plausible, matching this hint
 *   picks the correct stride so patch data stays aligned.
 */
S32 SaveLoadGuessObjectWireStride(S32 nb_objets, const U8 *after_nb_objets, const U8 *stream_end,
                                  S32 stride64_native, S32 scene_nb_patches_hint);

/* Sniffed NbPatches after `nb_objets * stride_bytes` from `after_nb_objets`, or -1000000 if invalid. */
S32 SaveLoadSniffNbPatchesAtStride(S32 nb_objets, const U8 *after_nb_objets, const U8 *stream_end,
                                   U32 stride_bytes);

#ifdef __cplusplus
}
#endif

#endif
