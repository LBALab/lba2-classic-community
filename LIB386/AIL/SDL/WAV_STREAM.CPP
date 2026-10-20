#include "WAV_STREAM.H"

#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/LE.H>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AILWavSource {
    FILE *file;
    iso9660_t *image;
    U32 extent, size, position, bytesRead;
    U32 cacheStart, cacheBytes;
    Uint8 cache[16384];
    Uint8 format[4096];
    U32 formatBytes, dataStart, dataBytes, consumed, blockBytes, blockFrames;
    U32 skip, decodedFrames, fact;
    bool hasFact, limitFact;
    Uint8 packet[12 + 8 + 4096 + 12 + 8 + 65536 + 2];
};

namespace {
bool Error(const char *message) {
    SDL_SetError("%s", message);
    return false;
}

bool Read(AILWavSource *s, void *buffer, U32 bytes) {
    if (s->position > s->size || bytes > s->size - s->position)
        return Error("Truncated music WAV");
    Uint8 *out = (Uint8 *)buffer;
    while (bytes) {
        if (s->position < s->cacheStart || s->position - s->cacheStart >= s->cacheBytes) {
            U32 count = s->size - s->position;
            if (count > sizeof(s->cache))
                count = sizeof(s->cache);
            int got;
            if (s->image)
                got = iso_pread(s->image, s->extent, s->size, s->position, s->cache, count);
            else {
                long offset = (long)s->position;
                if (offset < 0 || (Uint64)offset != (Uint64)s->position || fseek(s->file, offset, SEEK_SET) != 0)
                    return Error("Unable to seek music WAV");
                got = (int)fread(s->cache, 1, count, s->file);
            }
            if (got <= 0)
                return Error("Unable to read music WAV");
            s->bytesRead += (U32)got;
            s->cacheStart = s->position;
            s->cacheBytes = (U32)got;
        }
        U32 offset = s->position - s->cacheStart;
        U32 count = s->cacheBytes - offset;
        if (count > bytes)
            count = bytes;
        memcpy(out, s->cache + offset, count);
        out += count;
        s->position += count;
        bytes -= count;
    }
    return true;
}

bool Header(AILWavSource *s) {
    Uint8 header[12];
    if (!Read(s, header, sizeof(header)))
        return false;
    U32 riff = ReadLE_U32(header + 4);
    if (riff < 4 || riff > s->size - 8)
        return Error("Invalid music WAV RIFF length");
    U32 end = riff + 8;
    bool data = false;
    while (s->position <= end && end - s->position >= 8) {
        if (!Read(s, header, 8))
            return false;
        U32 length = ReadLE_U32(header + 4), start = s->position;
        if (length > end - start)
            return Error("Truncated music WAV chunk");
        if (memcmp(header, "fmt ", 4) == 0 && !s->formatBytes) {
            if (length < 16 || length > sizeof(s->format))
                return Error("Unsupported music WAV format size");
            if (!Read(s, s->format, length))
                return false;
            s->formatBytes = length;
        } else if (memcmp(header, "data", 4) == 0 && !data) {
            s->dataStart = start;
            s->dataBytes = length;
            data = true;
        } else if (memcmp(header, "fact", 4) == 0 && !s->hasFact && length >= 4) {
            if (!Read(s, header, 4))
                return false;
            s->fact = ReadLE_U32(header);
            s->hasFact = true;
        }
        s->position = start + length;
        if ((length & 1) && s->position < end)
            ++s->position;
    }
    if (!s->formatBytes || !data)
        return Error("Music WAV is missing format or data");
    U32 code = ReadLE_U16(s->format);
    if (code == 0xFFFE) {
        if (s->formatBytes < 40 || ReadLE_U16(s->format + 16) < 22)
            return Error("Invalid extensible music WAV format");
        code = ReadLE_U32(s->format + 24);
    }
    U32 channels = ReadLE_U16(s->format + 2);
    s->blockBytes = ReadLE_U16(s->format + 12);
    if (!channels || !ReadLE_U32(s->format + 4) || !s->blockBytes)
        return Error("Invalid music WAV sample geometry");
    if (code == 2 || code == 17) {
        if (s->formatBytes < 20)
            return Error("Missing music ADPCM block geometry");
        s->blockFrames = ReadLE_U16(s->format + 18);
        U32 headerBytes = channels * (code == 2 ? 7 : 4);
        if (s->blockBytes < headerBytes)
            return Error("Invalid music ADPCM block size");
        if (!s->blockFrames)
            s->blockFrames = (s->blockBytes - headerBytes) * 2 / channels + (code == 2 ? 2 : 1);
    } else if (code == 1 || code == 3 || code == 6 || code == 7) {
        s->blockFrames = 1;
    } else
        return Error("Unsupported streamed music WAV encoding");
    if (!s->blockFrames)
        return Error("Invalid music WAV frames per block");
    const char *hint = SDL_GetHint(SDL_HINT_WAVE_FACT_CHUNK);
    s->limitFact = s->hasFact && hint &&
                   (!strcmp(hint, "truncate") || !strcmp(hint, "strict") ||
                    (!strcmp(hint, "ignorezero") && s->fact != 0));
    if (hint && !strcmp(hint, "strict") && !s->hasFact && code != 1 && code != 3)
        return Error("Missing music WAV fact chunk");
    return true;
}
} // namespace

AILWavSource *AILWav_Open(const char *path, bool image, S32 frame, bool *recognized) {
    *recognized = false;
    AILWavSource *s = (AILWavSource *)calloc(1, sizeof(*s));
    if (!s) {
        *recognized = true;
        Error("Unable to allocate music WAV decoder");
        return NULL;
    }
    if (image) {
        s->image = DiscImage_OpenIndependent(path, &s->extent, &s->size);
        if (!s->image) {
            AILWav_Close(s);
            return NULL;
        }
    } else {
        s->file = fopen(path, "rb");
        if (!s->file) {
            AILWav_Close(s);
            return NULL;
        }
        long size = fseek(s->file, 0, SEEK_END) == 0 ? ftell(s->file) : -1;
        if (size < 0 || (unsigned long)size > UINT_MAX) {
            *recognized = true;
            Error("Unable to size music WAV");
            AILWav_Close(s);
            return NULL;
        }
        s->size = (U32)size;
    }
    Uint8 signature[12];
    if (s->size < sizeof(signature) || !Read(s, signature, sizeof(signature)) ||
        memcmp(signature, "RIFF", 4) || memcmp(signature + 8, "WAVE", 4)) {
        AILWav_Close(s);
        return NULL;
    }
    *recognized = true;
    s->position = 0;
    if (!Header(s)) {
        AILWav_Close(s);
        return NULL;
    }
    U32 start = frame > 0 ? (U32)frame : 0;
    U32 block = start / s->blockFrames;
    s->consumed = block > s->dataBytes / s->blockBytes ? s->dataBytes : block * s->blockBytes;
    s->decodedFrames = block * s->blockFrames;
    s->skip = start % s->blockFrames;
    return s;
}

int AILWav_Read(AILWavSource *s, SDL_AudioSpec *spec, Uint8 **pcm, Uint32 *bytes) {
    *pcm = NULL;
    *bytes = 0;
    if (s->consumed == s->dataBytes || (s->limitFact && s->decodedFrames >= s->fact))
        return 0;
    U32 blocks = 2048 / s->blockFrames;
    if (!blocks)
        blocks = 1;
    if (blocks > 65536 / s->blockBytes)
        blocks = 65536 / s->blockBytes;
    U32 count = blocks * s->blockBytes;
    if (count > s->dataBytes - s->consumed)
        count = s->dataBytes - s->consumed;
    /* ADPCM predictors restart at block boundaries. Rewrapping complete blocks
       preserves SDL's decoder and its fact/truncation policies. */
    Uint8 *p = s->packet;
    memcpy(p, "RIFF", 4);
    memcpy(p + 8, "WAVEfmt ", 8);
    WriteLE_U32(p + 16, s->formatBytes);
    memcpy(p + 20, s->format, s->formatBytes);
    U32 offset = 20 + s->formatBytes;
    if (offset & 1)
        p[offset++] = 0;
    if (s->hasFact) {
        memcpy(p + offset, "fact", 4);
        WriteLE_U32(p + offset + 4, 4);
        U32 frames = blocks * s->blockFrames;
        U32 left = s->fact > s->decodedFrames ? s->fact - s->decodedFrames : 0;
        WriteLE_U32(p + offset + 8, count == s->dataBytes - s->consumed || left < frames ? left : frames);
        offset += 12;
    }
    memcpy(p + offset, "data", 4);
    WriteLE_U32(p + offset + 4, count);
    offset += 8;
    s->position = s->dataStart + s->consumed;
    if (!Read(s, p + offset, count))
        return -1;
    s->consumed += count;
    U32 length = offset + count;
    if (length & 1)
        p[length++] = 0;
    WriteLE_U32(p + 4, length - 8);
    if (!AILAudio_LoadWAVMemory(p, (int)length, spec, pcm, bytes))
        return -1;
    U32 frameBytes = AIL_AUDIO_BITSIZE(spec->format) / 8 * spec->channels;
    if (!frameBytes) {
        SDL_free(*pcm);
        *pcm = NULL;
        Error("Invalid decoded music WAV geometry");
        return -1;
    }
    s->decodedFrames += *bytes / frameBytes;
    U32 skip = s->skip;
    if (skip > *bytes / frameBytes)
        skip = *bytes / frameBytes;
    s->skip -= skip;
    if (skip) {
        *bytes -= skip * frameBytes;
        memmove(*pcm, *pcm + skip * frameBytes, *bytes);
    }
    return 1;
}

void AILWav_Close(AILWavSource *s) {
    if (!s)
        return;
    if (s->file && fclose(s->file) != 0)
        SDL_Log("SDL AIL: unable to close music WAV: %s", strerror(errno));
    if (s->image)
        iso_close(s->image);
    free(s);
}
#ifdef LBA2_MUSIC_TESTS
U32 AILWav_TestBytesRead(const AILWavSource *s) { return s->bytesRead; }
#endif
