//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_CDDA
#define LIB_SYSTEM_CDDA

//──────────────────────────────────────────────────────────────────────────
// Byte order of CD audio read raw out of a disc image.
//
// Red Book stores its samples MSB-first, and a ripping tool is meant to swap
// them into the little-endian order every player and mixer expects. Not all of
// them do: the European MODE2 rip of LBA2 circulating on the Internet Archive
// carries raw disc order, and played as it stands it is full-scale noise.
//
// A cue sheet has no field for this, so it has to be measured. Kept SDL-free and
// separate from the stream loader so it can be tested on its own.
//──────────────────────────────────────────────────────────────────────────
#include <stddef.h>
#include <stdint.h>

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
// Does this buffer of raw CD audio need its byte pairs swapped?
//
// Bytes in the wrong order are uniform noise, whose mean sample magnitude sits
// near half of full scale. Music has crest factor and averages far below that.
// Measured on real discs: a correctly ordered track reads 0.06 of full scale
// against 0.50 swapped, and a byte-swapped one 0.49 against 0.08. So both
// readings are taken and the quieter one wins.
//
// Digital silence contributes nothing either way, so a quiet passage cannot
// swing the result, and a level floor stops a silent lead-in deciding it. When
// the buffer is too short or too quiet to judge, the answer is "no": leaving
// audio alone is the safe failure, since a needless swap turns music into noise.
int CdDa_NeedsByteSwap(const uint8_t *pcm, size_t bytes);

//──────────────────────────────────────────────────────────────────────────
// Swap the byte pairs of `bytes` bytes of 16-bit PCM in place. A trailing odd
// byte is left as it is.
void CdDa_ByteSwap(uint8_t *pcm, size_t bytes);

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

#endif
