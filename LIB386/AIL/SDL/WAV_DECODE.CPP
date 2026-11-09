#include "WAV_DECODE.H"

#include <SYSTEM/LE.H>

#include <stdlib.h>
#include <string.h>

/* Failures set the error and return false in two steps: SDL2's SDL_SetError,
   which AIL_SetError is in the host test, returns -1. */

namespace {
const U32 WAV_PCM = 1;
const U32 WAV_IMA_ADPCM = 0x11;
const U32 WAV_EXTENSIBLE = 0xFFFE;

const int imaIndex[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
const U16 imaStep[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

/* One nibble, as SDL decodes it: the index is kept unclamped between nibbles
   and clamped when used. */
S16 ImaNibble(int *index, S16 last, U32 nibble) {
    int i = *index;
    if (i < 0)
        i = 0;
    else if (i > 88)
        i = 88;
    const int step = imaStep[i];
    *index = i + imaIndex[nibble];
    int delta = step >> 3;
    if (nibble & 4)
        delta += step;
    if (nibble & 2)
        delta += step >> 1;
    if (nibble & 1)
        delta += step >> 2;
    if (nibble & 8)
        delta = -delta;
    int sample = last + delta;
    if (sample > 32767)
        sample = 32767;
    else if (sample < -32768)
        sample = -32768;
    return (S16)sample;
}

/* A block is a 4-byte header per channel (first sample, step index), then
   4-byte groups of 8 nibbles per channel in turn. Only whole blocks are
   decoded, which is what SDL does with no truncation hint. The game's music
   fills its blocks, so every group is whole there. */
bool DecodeIma(const U8 *format, U32 formatBytes, U32 code, int channels, const U8 *data, U32 dataBytes,
               U8 **pcm, U32 *pcmBytes) {
    const U32 blockBytes = ReadLE_U16(format + 12);
    const U32 headerBytes = (U32)channels * 4;
    if (ReadLE_U16(format + 14) != 4) {
        AIL_SetError("IMA ADPCM WAV must be 4 bits per sample");
        return false;
    }
    if (blockBytes < headerBytes || blockBytes % 4) {
        AIL_SetError("Invalid IMA ADPCM block size");
        return false;
    }
    const U32 extraBytes = formatBytes >= 18 ? ReadLE_U16(format + 16) : 0;
    U32 blockFrames = 0;
    if (code == WAV_EXTENSIBLE ? formatBytes >= 40 && extraBytes >= 22 : formatBytes >= 20 && extraBytes >= 2)
        blockFrames = ReadLE_U16(format + 18);
    const U32 dataFrames = (blockBytes - headerBytes) * 2 / (U32)channels;
    if (!blockFrames)
        blockFrames = dataFrames + 1;
    if (dataFrames < blockFrames - 1) {
        AIL_SetError("Invalid number of samples per IMA ADPCM block");
        return false;
    }
    U32 blocks = dataBytes / blockBytes;
    /* SDL wants room for whole groups, 4 bytes per channel each; a block
       without it counts as truncated, and SDL then keeps no blocks. */
    if ((blockFrames - 1 + 7) / 8 * headerBytes > blockBytes - headerBytes)
        blocks = 0;
    if ((U64)blocks * blockFrames * channels * 2 > 0x7fffffff) {
        AIL_SetError("IMA ADPCM WAV is too big");
        return false;
    }
    *pcmBytes = blocks * blockFrames * (U32)channels * 2;
    S16 *out = (S16 *)malloc(*pcmBytes ? *pcmBytes : 1);
    if (!out) {
        AIL_SetError("Unable to allocate %u WAV bytes", (unsigned)*pcmBytes);
        return false;
    }
    *pcm = (U8 *)out;
    int index[2];
    for (U32 b = 0; b < blocks; ++b) {
        const U8 *block = data + (size_t)b * blockBytes;
        for (int c = 0; c < channels; ++c) {
            out[c] = (S16)ReadLE_U16(block + c * 4);
            index[c] = (signed char)block[c * 4 + 2];
        }
        const U8 *p = block + headerBytes;
        for (U32 done = 1; done < blockFrames; done += 8) {
            const U32 count = blockFrames - done < 8 ? blockFrames - done : 8;
            for (int c = 0; c < channels; ++c) {
                S16 sample = out[(done - 1) * channels + c];
                for (U32 i = 0; i < count; ++i) {
                    const U32 nibble = i & 1 ? p[i / 2] >> 4 : p[i / 2] & 0x0F;
                    sample = ImaNibble(&index[c], sample, nibble);
                    out[(done + i) * channels + c] = sample;
                }
                /* A short last group moves on by the bytes it used, as SDL's does. */
                p += (count + 1) / 2;
            }
        }
        out += (size_t)blockFrames * channels;
    }
    return true;
}
} // namespace

bool AILWav_DecodeMemory(const void *data, int bytes, AILAudioSpec *spec, U8 **pcm, U32 *pcmBytes) {
    const U8 *p = (const U8 *)data;
    *pcm = NULL;
    *pcmBytes = 0;
    if (!p || bytes < 12 || memcmp(p, "RIFF", 4) || memcmp(p + 8, "WAVE", 4)) {
        AIL_SetError("Not a WAV file");
        return false;
    }
    const U8 *format = NULL;
    const U8 *samples = NULL;
    U32 formatBytes = 0, sampleBytes = 0;
    U32 at = 12;
    while (at + 8 <= (U32)bytes) {
        U32 length = ReadLE_U32(p + at + 4);
        const U32 start = at + 8;
        if (length > (U32)bytes - start)
            length = (U32)bytes - start; // a truncated chunk keeps what is there
        if (!memcmp(p + at, "fmt ", 4) && !format) {
            format = p + start;
            formatBytes = length;
        } else if (!memcmp(p + at, "data", 4) && !samples) {
            samples = p + start;
            sampleBytes = length;
        }
        at = start + length + (length & 1);
    }
    if (!format || formatBytes < 16 || !samples) {
        AIL_SetError("WAV is missing format or data");
        return false;
    }
    const U32 tag = ReadLE_U16(format);
    U32 code = tag;
    if (tag == WAV_EXTENSIBLE && formatBytes >= 40)
        code = ReadLE_U16(format + 24);
    const int channels = ReadLE_U16(format + 2);
    const int rate = (int)ReadLE_U32(format + 4);
    const int bits = ReadLE_U16(format + 14);
    if (code == WAV_IMA_ADPCM) {
        if (channels < 1 || channels > 2 || rate <= 0) {
            AIL_SetError("IMA ADPCM WAV with %d channels is not supported on this build", channels);
            return false;
        }
        if (!DecodeIma(format, formatBytes, tag, channels, samples, sampleBytes, pcm, pcmBytes))
            return false;
        spec->format = AIL_AUDIO_S16;
        spec->channels = channels;
        spec->freq = rate;
        return true;
    }
    if (code != WAV_PCM) {
        AIL_SetError("WAV encoding %u is not supported on this build (PCM and IMA ADPCM only)",
                     (unsigned)code);
        return false;
    }
    if ((bits != 8 && bits != 16) || channels < 1 || rate <= 0) {
        AIL_SetError("WAV with %d bits and %d channels is not supported on this build", bits, channels);
        return false;
    }
    const U32 frameBytes = (U32)(bits / 8 * channels);
    sampleBytes -= sampleBytes % frameBytes;
    *pcm = (U8 *)malloc(sampleBytes ? sampleBytes : 1);
    if (!*pcm) {
        AIL_SetError("Unable to allocate %u WAV bytes", (unsigned)sampleBytes);
        return false;
    }
    memcpy(*pcm, samples, sampleBytes);
    *pcmBytes = sampleBytes;
    spec->format = bits == 8 ? AIL_AUDIO_U8 : AIL_AUDIO_S16;
    spec->channels = channels;
    spec->freq = rate;
    return true;
}
