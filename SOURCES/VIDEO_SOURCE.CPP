#include "VIDEO_SOURCE.H"

#include <SYSTEM/FILES.H>
#include <SYSTEM/LE.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/LOG.H>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
struct Source {
    S32 file, base, index;
    char path[ADELINE_MAX_PATH];
    U32 size, packedSize, position;
    U32 cacheStart, cacheBytes;
    U8 cache[65536];
    U8 method;
    bool failed;
    U8 input[4096], history[4096];
    U32 inputPosition, inputIndex, inputBytes, decoded;
    U32 flags, flagBits, matchBytes, distance;
};

bool Fail(Source *source, const char *reason) {
    if (!source->failed)
        Log_Error("Video %s entry %d: %s", source->path, (int)source->index, reason);
    source->failed = true;
    return false;
}

bool ReadExact(S32 file, U8 *out, U32 bytes) {
    U32 done = 0;
    while (done < bytes) {
        S32 n = Read(file, out + done, bytes - done);
        if (n <= 0 || (U32)n > bytes - done)
            return false;
        done += (U32)n;
    }
    return true;
}

int EncodedByte(Source *source) {
    if (source->inputPosition >= source->packedSize) {
        Fail(source, "truncated compressed data");
        return -1;
    }
    if (source->inputIndex == source->inputBytes) {
        U32 bytes = source->packedSize - source->inputPosition;
        if (bytes > sizeof(source->input))
            bytes = sizeof(source->input);
        if (Seek(source->file, source->base + (S32)source->inputPosition, SEEK_FROM_START) !=
                source->base + (S32)source->inputPosition ||
            !ReadExact(source->file, source->input, bytes)) {
            Fail(source, "unable to read compressed data");
            return -1;
        }
        source->inputIndex = 0;
        source->inputBytes = bytes;
    }
    ++source->inputPosition;
    return source->input[source->inputIndex++];
}

int DecodedByte(Source *source) {
    int value = -1;
    if (!source->matchBytes) {
        if (!source->flagBits) {
            int flags = EncodedByte(source);
            if (flags < 0)
                return -1;
            source->flags = (U32)flags;
            source->flagBits = 8;
        }
        bool literal = (source->flags & 1) != 0;
        source->flags >>= 1;
        --source->flagBits;
        if (literal) {
            value = EncodedByte(source);
            if (value < 0)
                return -1;
        } else {
            int lo = EncodedByte(source);
            int hi = EncodedByte(source);
            if (lo < 0 || hi < 0)
                return -1;
            source->distance = (U32)((hi << 4) | (lo >> 4)) + 1;
            source->matchBytes = (U32)(lo & 15) + source->method + 1;
            if (source->distance > source->decoded) {
                Fail(source, "invalid compressed back-reference");
                return -1;
            }
        }
    }
    if (source->matchBytes) {
        value = source->history[(source->decoded - source->distance) & 4095];
        --source->matchBytes;
    }
    source->history[source->decoded & 4095] = (U8)value;
    ++source->decoded;
    return value;
}

bool Fill(Source *source) {
    U32 bytes = source->size - source->position;
    if (bytes > sizeof(source->cache))
        bytes = sizeof(source->cache);
    if (!source->method) {
        if (Seek(source->file, source->base + (S32)source->position, SEEK_FROM_START) !=
                source->base + (S32)source->position ||
            !ReadExact(source->file, source->cache, bytes))
            return Fail(source, "unable to read movie data");
    } else {
        /* Seeks move the logical cursor only. Backward reads replay LZ history;
           forward reads retain its 4 KiB overlap window. */
        if (source->position < source->decoded) {
            source->inputPosition = source->inputIndex = source->inputBytes = 0;
            source->decoded = source->flags = source->flagBits = source->matchBytes = 0;
        }
        while (source->decoded < source->position)
            if (DecodedByte(source) < 0)
                return false;
        for (U32 i = 0; i < bytes; ++i) {
            int value = DecodedByte(source);
            if (value < 0)
                return false;
            source->cache[i] = (U8)value;
        }
    }
    source->cacheStart = source->position;
    source->cacheBytes = bytes;
    return true;
}

size_t ReadSource(void *userdata, void *buffer, size_t bytes) {
    Source *source = (Source *)userdata;
    if (source->failed)
        return 0;
    U32 available = source->size - source->position;
    if (bytes > available)
        bytes = available;
    size_t done = 0;
    while (done < bytes) {
        if (source->position < source->cacheStart ||
            source->position - source->cacheStart >= source->cacheBytes)
            if (!Fill(source))
                break;
        U32 offset = source->position - source->cacheStart;
        size_t count = source->cacheBytes - offset;
        if (count > bytes - done)
            count = bytes - done;
        memcpy((U8 *)buffer + done, source->cache + offset, count);
        source->position += (U32)count;
        done += count;
    }
    return done;
}

int SeekSource(void *userdata, long offset, int origin) {
    Source *source = (Source *)userdata;
    long base;
    switch (origin) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = (long)source->position;
        break;
    case SEEK_END:
        base = (long)source->size;
        break;
    default:
        return -1;
    }
    if (offset < -base || offset > (long)source->size - base) {
        Fail(source, "movie frame extends outside its HQR entry");
        return -1;
    }
    source->position = (U32)(base + offset);
    return 0;
}

long TellSource(void *userdata) {
    return (long)((Source *)userdata)->position;
}

void CloseSource(void *userdata) {
    Source *source = (Source *)userdata;
    if (source->file && Close(source->file) != 0)
        Fail(source, "unable to close movie archive");
    free(source);
}
} // namespace

smk Video_OpenSource(const char *archive, S32 index) {
    if (!archive || !archive[0] || index < 0) {
        Log_Error("Invalid video source or entry index");
        return NULL;
    }
    Source *source = (Source *)calloc(1, sizeof(*source));
    if (!source) {
        Log_Error("Unable to allocate video input buffer");
        return NULL;
    }
    snprintf(source->path, sizeof(source->path), "%s", archive);
    source->index = index;
    source->file = OpenRead(archive);
    U8 raw[10];
    S32 fileSize = source->file ? Seek(source->file, 0, SEEK_FROM_END) : -1;
    bool valid = fileSize >= 4 && Seek(source->file, 0, SEEK_FROM_START) == 0 &&
                 ReadExact(source->file, raw, 4);
    U32 tableBytes = valid ? ReadLE_U32(raw) : 0;
    valid = valid && tableBytes >= 4 && tableBytes % 4 == 0 &&
            tableBytes <= (U32)fileSize && index >= 0 && (U32)index < tableBytes / 4;
    U32 entry = 0;
    if (valid && Seek(source->file, index * 4, SEEK_FROM_START) == index * 4 &&
        ReadExact(source->file, raw, 4))
        entry = ReadLE_U32(raw);
    valid = valid && entry >= tableBytes && fileSize >= 10 && entry <= (U32)fileSize - 10;
    valid = valid && Seek(source->file, (S32)entry, SEEK_FROM_START) == (S32)entry &&
            ReadExact(source->file, raw, 10);
    if (valid) {
        source->base = (S32)entry + 10;
        source->size = ReadLE_U32(raw);
        U32 method = ReadLE_U16(raw + 8);
        source->method = (U8)method;
        source->packedSize = method ? ReadLE_U32(raw + 4) : source->size;
        valid = source->size > 0 && source->size <= 0x7FFFFFFFu && method <= 2 &&
                source->packedSize > 0 && source->packedSize <= (U32)(fileSize - source->base);
    }
    if (!valid) {
        Fail(source, "invalid or truncated HQR entry");
        CloseSource(source);
        return NULL;
    }
    const smk_io io = {source, ReadSource, SeekSource, TellSource, CloseSource};
    smk result = smk_open_stream(&io);
    if (!result)
        Log_Error("Unable to open video from %s entry %d", archive, (int)index);
    return result;
}
