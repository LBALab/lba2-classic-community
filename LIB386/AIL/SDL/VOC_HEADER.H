#ifndef LIB_AIL_VOC_HEADER
#define LIB_AIL_VOC_HEADER

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Detect and parse a Creative Voice (.voc) header.
 *
 * Returns 1 if the buffer holds a VOC, 0 otherwise. Detection is the 19-byte signature at
 * offset 1 ("reative Voice File\x1A"), which is the real magic; the first byte is 'C' in a
 * standard VOC but varies across LBA voice banks (0x00, and 0x01 in some LBA1 VOX entries),
 * so it is not used as a discriminator.
 *
 * On success: *data / *payloadSize point at the type-0x01 sound-data block, *outHz is the
 * sample rate (defaultHz when the file's rate is absent or out of range), and *outIs8bit is
 * 1 for 8-bit unsigned PCM (0 otherwise).
 */
int ParseVocHeader(const U8 *buffer, S32 sizeBytes, const U8 **data, S32 *payloadSize,
                   S32 *outHz, int *outIs8bit, S32 defaultHz);

#ifdef __cplusplus
}
#endif

#endif /* LIB_AIL_VOC_HEADER */
