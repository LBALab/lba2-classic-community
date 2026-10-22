#include "STREAM_SDL.H"

#include "AUDIO_DEVICE.H"
#include "WAV_STREAM.H"

#include "stb_vorbis.h"

#include <AIL/STREAM_PARK.H> /* pure park/resume/focus decision logic (host-tested) */
#include <SYSTEM/CDDA.H>
#include <SYSTEM/CDTRACKS.H>
#include <SYSTEM/CRASH.H>
#include <SYSTEM/CUE.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/FILES.H> /* ESeekOrigin */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ADELINE_MAX_PATH
#define ADELINE_MAX_PATH 512
#endif

/* stb_vorbis seek/length entry points. The project's trimmed stb_vorbis.h only
   exposes the decode subset; the full pulldata API (compiled in stb_vorbis.c) is
   declared here so ResumeStream can re-seek a re-opened file. */
extern "C" {
int stb_vorbis_seek_frame(stb_vorbis *f, unsigned int sample_number);
unsigned int stb_vorbis_stream_length_in_samples(stb_vorbis *f);
}

/* ── Core stream state ─────────────────────────────────────────────────── */

static AILAudioStream *stream = NULL;
static char streamPathName[ADELINE_MAX_PATH] = "";
static bool streamPaused = false;

/* Geometry of the source PCM currently feeding `stream`, used to convert the
   queued-byte backlog into a frame offset (see StreamPositionFrames). */
static int streamBytesPerFrame = 0;
static AILAtomicInt streamPushedBytes; // total source bytes handed to `stream`
/* Frame of the track the stream was opened at: non-zero after a seeked restore,
   whose pushed bytes start there and not at the top of the track. */
static int streamBaseFrame = 0;

/* Parked playback position for a pause that outlives its stream. PauseStream
   snapshots {path, frame}; ResumeStream either cheap-unpauses the still-alive
   stream or re-opens the file and seeks back here. Mirrors the MILES redbook
   wrapper's Cur/End/PausedTrack (LIB386/AIL/MILES/CD.CPP): the snapshot survives
   a StopStream/PlayStream in between (e.g. the menu theme) and is cleared only on
   resume or OpenStream. */
static char rememberedPath[ADELINE_MAX_PATH] = "";
static S32 rememberedFrame = 0;
static bool hasRemembered = false;

S32 streamVolume = 0;
static int musicLogEnabled = 0;

/* ── Path resolution cache ─────────────────────────────────────────────── */

#define PATH_CACHE_MAX 32
static struct {
    char key[ADELINE_MAX_PATH];
    char val[ADELINE_MAX_PATH];
} pathCache[PATH_CACHE_MAX];
static int pathCacheN = 0;

/* ── Decoded PCM cache (most recently decoded OGG) ─────────────────────── */

static struct {
    char path[ADELINE_MAX_PATH];
    short *pcm;
    Uint32 numBytes;
    int sample_rate;
    int channels;
} oggCache = {"", NULL, 0, 0, 0};

static void ClearOggCache(void) {
    if (oggCache.pcm) {
        free(oggCache.pcm);
        oggCache.pcm = NULL;
    }
    oggCache.path[0] = '\0';
    oggCache.numBytes = 0;
}

/* ── Background decode thread ──────────────────────────────────────────── */

static SDL_Thread *decodeThread = NULL;
static AILSemaphore *decodeWake = NULL;
static AILAtomicInt decodeStop;
static AILAtomicInt decodeActive;

#define DECODE_CHUNK_SHORTS 4096

typedef struct {
    stb_vorbis *vorbis;
    int channels;
    int sample_rate;
    bool cacheResult; // false when decoding from a mid-file seek (partial PCM)
    char oggPath[ADELINE_MAX_PATH];
} DecodeThreadData;

static int SDLCALL DecodeThreadFunc(void *userdata) {
    DecodeThreadData *d = (DecodeThreadData *)userdata;
    short buf[DECODE_CHUNK_SHORTS];

    Crash_PrepareThread();

    int totalShorts = 0;
    int capacity = 0;
    short *fullPcm = NULL;

    while (!AIL_AtomicGet(&decodeStop)) {
        int n = stb_vorbis_get_samples_short_interleaved(
            d->vorbis, d->channels, buf, DECODE_CHUNK_SHORTS);
        if (n <= 0)
            break;

        int chunkShorts = n * d->channels;
        int chunkBytes = chunkShorts * (int)sizeof(short);
        AILAudio_Put(stream, buf, chunkBytes);
        AIL_AtomicAdd(&streamPushedBytes, chunkBytes);

        /* Only accumulate the full-track PCM when it will be cached (startFrame
           == 0). A seeked restore decode yields partial PCM that is never cached
           (see below), so building it here would be pure waste. */
        if (!d->cacheResult)
            continue;

        if (totalShorts + chunkShorts > capacity) {
            int newCap = (capacity == 0) ? 131072 : capacity * 2;
            while (newCap < totalShorts + chunkShorts)
                newCap *= 2;
            short *p = (short *)realloc(fullPcm, (size_t)newCap * sizeof(short));
            if (!p) {
                free(fullPcm);
                fullPcm = NULL;
                break;
            }
            fullPcm = p;
            capacity = newCap;
        }
        memcpy(fullPcm + totalShorts, buf, (size_t)chunkShorts * sizeof(short));
        totalShorts += chunkShorts;
    }

    /* All of the track's PCM has been pushed. Signal end-of-input so SDL's
       device-format converter emits its held tail; otherwise AILAudio_Queued
       sticks a few bytes above 0 forever (the device format differs from the source,
       so a converter sits in the path) and the stream never reports EOF -- which
       stalls the deferred-music switch (#406). Only on a natural finish: if we were
       stopped mid-decode the stream is being torn down anyway. */
    if (!AIL_AtomicGet(&decodeStop) && stream)
        AILAudio_Flush(stream);

    stb_vorbis_close(d->vorbis);

    if (!AIL_AtomicGet(&decodeStop) && d->cacheResult && fullPcm && totalShorts > 0) {
        short *trimmed = (short *)realloc(fullPcm, (size_t)totalShorts * sizeof(short));
        ClearOggCache();
        oggCache.pcm = trimmed ? trimmed : fullPcm;
        oggCache.numBytes = (Uint32)((size_t)totalShorts * sizeof(short));
        oggCache.sample_rate = d->sample_rate;
        oggCache.channels = d->channels;
        strncpy(oggCache.path, d->oggPath, ADELINE_MAX_PATH - 1);
        oggCache.path[ADELINE_MAX_PATH - 1] = '\0';
    } else {
        free(fullPcm);
    }

    AIL_AtomicSet(&decodeActive, 0);
    free(d);
    return 0;
}

static void StopDecodeThread(void) {
    if (decodeThread) {
        AIL_AtomicSet(&decodeStop, 1);
        if (decodeWake && !AIL_SignalSemaphore(decodeWake))
            SDL_Log("SDL AIL: unable to wake WAV producer: %s", SDL_GetError());
        SDL_WaitThread(decodeThread, NULL);
        decodeThread = NULL;
    }
    if (decodeWake) {
        SDL_DestroySemaphore(decodeWake);
        decodeWake = NULL;
    }
    AIL_AtomicSet(&decodeStop, 0);
    AIL_AtomicSet(&decodeActive, 0);
}

struct WavDecodeData {
    AILWavSource *source;
    SDL_AudioSpec spec;
    Uint8 *pcm;
    Uint32 bytes, offset;
};

static int SDLCALL WavDecodeThread(void *userdata) {
    Crash_PrepareThread();
    WavDecodeData *d = (WavDecodeData *)userdata;
    const Uint32 frameBytes = AIL_AUDIO_BITSIZE(d->spec.format) / 8 * d->spec.channels;
    bool failed = false;
    while (!AIL_AtomicGet(&decodeStop)) {
        if (d->offset == d->bytes) {
            SDL_free(d->pcm);
            d->pcm = NULL;
            int result = AILWav_Read(d->source, &d->spec, &d->pcm, &d->bytes);
            d->offset = 0;
            if (result <= 0) {
                if (result < 0) {
                    SDL_Log("SDL AIL: WAV streaming failed: %s", SDL_GetError());
                    failed = true;
                }
                break;
            }
        }
        Uint32 count = d->bytes - d->offset;
        if (count > 16384)
            count = 16384 / frameBytes * frameBytes;
        int queued = AILAudio_Queued(stream);
        if (queued < 0) {
            failed = true;
            break;
        }
        int waitMs = Stream_QueueWaitMs((Uint32)queued, count, d->spec.freq, (int)frameBytes);
        if (waitMs) {
            if (AIL_WaitSemaphoreTimeout(decodeWake, waitMs) < 0) {
                SDL_Log("SDL AIL: WAV queue wait failed: %s", SDL_GetError());
                failed = true;
                break;
            }
            continue;
        }
        if (AIL_AtomicGet(&decodeStop))
            break;
        if (!AILAudio_Put(stream, d->pcm + d->offset, (int)count)) {
            failed = true;
            break;
        }
        AIL_AtomicAdd(&streamPushedBytes, (int)count);
        d->offset += count;
    }
    if (failed)
        SDL_Log("SDL AIL: music WAV producer stopped after an error");
    SDL_free(d->pcm);
    AILWav_Close(d->source);
    free(d);
    if (!AIL_AtomicGet(&decodeStop))
        AILAudio_Flush(stream);
    AIL_AtomicSet(&decodeActive, 0);
    return 0;
}

static bool PlayWavFile(const char *name, bool image, S32 startFrame, bool *recognized) {
    Uint32 started = (Uint32)SDL_GetTicks();
    AILWavSource *source = AILWav_Open(name, image, startFrame, recognized);
    if (!source)
        return false;
    WavDecodeData *d = (WavDecodeData *)calloc(1, sizeof(*d));
    if (!d) {
        AILWav_Close(source);
        SDL_SetError("Unable to allocate WAV music producer");
        return false;
    }
    d->source = source;
    int first = AILWav_Read(source, &d->spec, &d->pcm, &d->bytes);
    if (first == 0 && startFrame > 0) {
        AILWav_Close(source);
        free(d);
        return true;
    }
    if (first > 0)
        stream = AILAudio_Open(&d->spec, NULL, NULL);
    if (first <= 0 || !stream) {
        if (first == 0)
            SDL_SetError("Music WAV has no playable frames");
        SDL_free(d->pcm);
        AILWav_Close(source);
        free(d);
        return false;
    }
    streamBytesPerFrame = AIL_AUDIO_BITSIZE(d->spec.format) / 8 * d->spec.channels;
    streamBaseFrame = startFrame > 0 ? startFrame : 0;
    d->offset = d->bytes > 16384 ? 16384 / streamBytesPerFrame * streamBytesPerFrame : d->bytes;
    if (!AILAudio_Put(stream, d->pcm, (int)d->offset)) {
        SDL_free(d->pcm);
        AILWav_Close(source);
        free(d);
        AILAudio_Destroy(stream);
        stream = NULL;
        return false;
    }
    AIL_AtomicAdd(&streamPushedBytes, (int)d->offset);
    if (musicLogEnabled)
        SDL_Log("[MUSIC] WAV prefill %u bytes in %u ms: %s",
                (unsigned)d->offset, (unsigned)((Uint32)SDL_GetTicks() - started), name);
    AIL_AtomicSet(&decodeStop, 0);
    AIL_AtomicSet(&decodeActive, 1);
    decodeWake = SDL_CreateSemaphore(0);
    decodeThread = decodeWake ? SDL_CreateThread(WavDecodeThread, "WAVDecode", d) : NULL;
    if (!decodeThread) {
        if (decodeWake) {
            SDL_DestroySemaphore(decodeWake);
            decodeWake = NULL;
        }
        AIL_AtomicSet(&decodeActive, 0);
        SDL_free(d->pcm);
        AILWav_Close(source);
        free(d);
        AILAudio_Destroy(stream);
        stream = NULL;
        return false;
    }
    return true;
}

/* Current playback offset in source frames, from the top of the track: the
   frame the stream was opened at plus everything pushed minus what is still
   queued (not yet consumed by the device). Works while the decode thread is
   still filling the stream because both sides count source bytes. The device's
   own buffer makes this a slight overestimate (tens of ms), which is fine for
   re-seeking music. */
static S32 StreamPositionFrames(void) {
    if (!stream)
        return 0;
    return (S32)Stream_PositionFrames(streamBaseFrame, AIL_AtomicGet(&streamPushedBytes),
                                      (int)AILAudio_Queued(stream), streamBytesPerFrame);
}

/* ── Init / Stop ───────────────────────────────────────────────────────── */

static void StopStreamInternal(void) {
    StopDecodeThread();
    if (stream) {
        AILAudio_Destroy(stream);
        stream = NULL;
    }
    streamPathName[0] = '\0';
    streamPaused = false;
    streamBytesPerFrame = 0;
    streamBaseFrame = 0;
    AIL_AtomicSet(&streamPushedBytes, 0);
    /* Deliberately keep rememberedPath/Frame: a StopStream here may be the menu
       theme replacing parked game music, which ResumeStream must still restore. */
}

void OpenStream() {
    StopDecodeThread();
    if (stream) {
        AILAudio_Destroy(stream);
        stream = NULL;
    }
    streamPathName[0] = '\0';
    streamPaused = false;
    streamBytesPerFrame = 0;
    streamBaseFrame = 0;
    AIL_AtomicSet(&streamPushedBytes, 0);
    rememberedPath[0] = '\0';
    rememberedFrame = 0;
    hasRemembered = false;
    pathCacheN = 0;
    ClearOggCache();
    if (musicLogEnabled)
        SDL_Log("[MUSIC] OpenStream init");
}

/* ── OGG path helpers ──────────────────────────────────────────────────── */

static void BuildOggPath(const char *wavPath, char *oggPath, int pathMax,
                         const char *ext, bool lowerName) {
    strncpy(oggPath, wavPath, pathMax - 1);
    oggPath[pathMax - 1] = '\0';

    if (lowerName) {
        char *sep = strrchr(oggPath, '/');
#ifdef _WIN32
        char *bsep = strrchr(oggPath, '\\');
        if (bsep && (!sep || bsep > sep))
            sep = bsep;
#endif
        char *start = sep ? sep + 1 : oggPath;
        for (char *p = start; *p; p++) {
            if (*p >= 'A' && *p <= 'Z')
                *p += 32;
        }
    }

    char *dot = strrchr(oggPath, '.');
    if (dot) {
        strcpy(dot, ext);
    } else {
        int len = (int)strlen(oggPath);
        if (len + 5 <= pathMax)
            strcpy(oggPath + len, ext);
    }
}

static bool FileExistsCheck(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

static const char *PathCacheGet(const char *key) {
    for (int i = 0; i < pathCacheN; i++)
        if (strcmp(pathCache[i].key, key) == 0)
            return pathCache[i].val;
    return NULL;
}

static void PathCachePut(const char *key, const char *val) {
    if (pathCacheN >= PATH_CACHE_MAX)
        return;
    strncpy(pathCache[pathCacheN].key, key, ADELINE_MAX_PATH - 1);
    pathCache[pathCacheN].key[ADELINE_MAX_PATH - 1] = '\0';
    strncpy(pathCache[pathCacheN].val, val, ADELINE_MAX_PATH - 1);
    pathCache[pathCacheN].val[ADELINE_MAX_PATH - 1] = '\0';
    pathCacheN++;
}

static bool ResolveOggPath(const char *wavPath, char *resolvedPath, int pathMax) {
    static const char *exts[] = {".ogg", ".OGG"};
    for (int lower = 0; lower <= 1; lower++) {
        for (int e = 0; e < 2; e++) {
            BuildOggPath(wavPath, resolvedPath, pathMax, exts[e], lower != 0);
            if (FileExistsCheck(resolvedPath))
                return true;
        }
    }
    /* Steam/GOG fallback: TADPCM<N> files renamed to track<N>.ogg */
    const char *sep = strrchr(wavPath, '/');
#ifdef _WIN32
    const char *bsep = strrchr(wavPath, '\\');
    if (bsep && (!sep || bsep > sep))
        sep = bsep;
#endif
    const char *base = sep ? sep + 1 : wavPath;
    const char *dot = strrchr(base, '.');
    if (!dot)
        dot = base + strlen(base);
    const char *p = dot;
    while (p > base && p[-1] >= '0' && p[-1] <= '9')
        p--;
    if (p < dot) {
        int n = atoi(p);
        if (n > 0 && n < 100) {
            int dirLen = (int)(base - wavPath);
            snprintf(resolvedPath, pathMax, "%.*strack%d.ogg", dirLen, wavPath, n);
            if (FileExistsCheck(resolvedPath))
                return true;
            snprintf(resolvedPath, pathMax, "%.*sTrack%d.ogg", dirLen, wavPath, n);
            if (FileExistsCheck(resolvedPath))
                return true;
        }
    }
    resolvedPath[0] = '\0';
    return false;
}

/* ── PlayOggFile: cache hit -> instant, cache miss -> stream decode ───── */
/* startFrame > 0 resumes mid-file (ResumeStream restore path): the cache hit
   pushes from a byte offset; the cache miss seeks the decoder and skips caching
   the resulting partial PCM. */

static bool PlayOggFile(const char *oggPath, S32 startFrame) {
    if (oggCache.pcm && strcmp(oggCache.path, oggPath) == 0) {
        SDL_AudioSpec spec;
        SDL_zero(spec);
        spec.freq = oggCache.sample_rate;
        spec.format = AIL_AUDIO_S16;
        spec.channels = (Uint8)oggCache.channels;
        stream = AILAudio_Open(&spec, NULL, NULL);
        if (!stream)
            return false;
        streamBytesPerFrame = oggCache.channels * (int)sizeof(short);
        Uint32 offsetBytes = (startFrame > 0)
                                 ? (Uint32)startFrame * (Uint32)streamBytesPerFrame
                                 : 0;
        /* Leave at least one frame so a slightly-overestimated resume near EOF
           replays a short tail instead of pushing zero bytes (matches the
           decode-path seek clamp). */
        Uint32 bpf = (Uint32)streamBytesPerFrame;
        if (bpf > 0 && oggCache.numBytes >= bpf && offsetBytes > oggCache.numBytes - bpf)
            offsetBytes = oggCache.numBytes - bpf;
        else if (offsetBytes > oggCache.numBytes)
            offsetBytes = oggCache.numBytes;
        Uint32 playBytes = oggCache.numBytes - offsetBytes;
        streamBaseFrame = (bpf > 0) ? (int)(offsetBytes / bpf) : 0;
        AILAudio_Put(stream, oggCache.pcm + offsetBytes / sizeof(short), playBytes);
        AIL_AtomicAdd(&streamPushedBytes, (int)playBytes);
        AILAudio_Flush(stream); // all input pushed -> let the converter drain to 0 at EOF (#406)
        return true;
    }

    int vorbisError = 0;
    stb_vorbis *v = stb_vorbis_open_filename(oggPath, &vorbisError, NULL);
    if (!v)
        return false;

    stb_vorbis_info info = stb_vorbis_get_info(v);
    if (info.channels <= 0 || info.sample_rate <= 0) {
        stb_vorbis_close(v);
        return false;
    }

    unsigned int seekFrame = 0;
    if (startFrame > 0) {
        /* Clamp a slightly-overestimated resume frame (StreamPositionFrames can
           read a device-buffer ahead of true playback) so a seek past end-of-file
           cannot leave the decoder at an undefined position or replay from 0. */
        unsigned int lenFrames = stb_vorbis_stream_length_in_samples(v);
        seekFrame = (unsigned int)startFrame;
        if (lenFrames > 0 && seekFrame >= lenFrames)
            seekFrame = lenFrames - 1;
        stb_vorbis_seek_frame(v, seekFrame);
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = (int)info.sample_rate;
    spec.format = AIL_AUDIO_S16;
    spec.channels = (Uint8)info.channels;
    stream = AILAudio_Open(&spec, NULL, NULL);
    if (!stream) {
        stb_vorbis_close(v);
        return false;
    }
    streamBytesPerFrame = (int)info.channels * (int)sizeof(short);
    streamBaseFrame = (int)seekFrame;

    DecodeThreadData *d = (DecodeThreadData *)malloc(sizeof(DecodeThreadData));
    if (!d) {
        stb_vorbis_close(v);
        AILAudio_Destroy(stream);
        stream = NULL;
        return false;
    }
    d->vorbis = v;
    d->channels = (int)info.channels;
    d->sample_rate = (int)info.sample_rate;
    d->cacheResult = (startFrame == 0); // a seeked decode yields partial PCM; don't cache it
    strncpy(d->oggPath, oggPath, ADELINE_MAX_PATH - 1);
    d->oggPath[ADELINE_MAX_PATH - 1] = '\0';

    AIL_AtomicSet(&decodeStop, 0);
    AIL_AtomicSet(&decodeActive, 1);
    decodeThread = SDL_CreateThread(DecodeThreadFunc, "OGGDecode", d);
    if (!decodeThread) {
        AIL_AtomicSet(&decodeActive, 0);
        stb_vorbis_close(v);
        free(d);
        AILAudio_Destroy(stream);
        stream = NULL;
        return false;
    }

    return true;
}

/* ── WAV + disc-image helpers ──────────────────────────────────────────── */

/* Open the device stream for a decoded WAV and queue its PCM from `startFrame`.
   Returns false (and logs) if the device stream cannot open. Does not free
   `wavData` (the caller owns it). */
static bool SetupWavStream(const SDL_AudioSpec *spec, const Uint8 *wavData,
                           Uint32 wavLen, S32 startFrame) {
    stream = AILAudio_Open(spec, NULL, NULL);
    if (!stream) {
        SDL_Log("SDL AIL: Could not open audio stream: %s", SDL_GetError());
        return false;
    }
    streamBytesPerFrame = (int)(AIL_AUDIO_BITSIZE(spec->format) / 8) * spec->channels;
    Uint32 offsetBytes = 0;
    if (startFrame > 0 && streamBytesPerFrame > 0) {
        offsetBytes = (Uint32)startFrame * (Uint32)streamBytesPerFrame;
        if (offsetBytes > wavLen)
            offsetBytes = wavLen;
    }
    streamBaseFrame = (streamBytesPerFrame > 0) ? (int)(offsetBytes / (Uint32)streamBytesPerFrame) : 0;
    AILAudio_Put(stream, wavData + offsetBytes, wavLen - offsetBytes);
    AIL_AtomicAdd(&streamPushedBytes, (int)(wavLen - offsetBytes));
    AILAudio_Flush(stream); // all input pushed -> let the converter drain to 0 at EOF (#406)
    return true;
}

/* Apply the current volume, start playback, and record the source path. Common
   tail shared by every successful source. */
static void FinishStream(const char *name) {
    if (!stream)
        return;
    float gain = (streamVolume <= 0) ? 0.0f : (float)streamVolume / 127.0f;
    if (gain > 1.0f)
        gain = 1.0f;
    AILAudio_Gain(stream, gain);

    AILAudio_Resume(stream);

    strncpy(streamPathName, name, ADELINE_MAX_PATH - 1);
    streamPathName[ADELINE_MAX_PATH - 1] = '\0';
}

/* Disc-image WAVs use a private cursor so the producer cannot move the
   asset loader's seek position. */
static bool PlayWavFromImage(const char *name, S32 startFrame, bool *recognized) {
    bool ok = PlayWavFile(name, true, startFrame, recognized);
    if (*recognized && !ok)
        SDL_Log("SDL AIL: could not stream image WAV '%s': %s", name, SDL_GetError());
    if (ok && musicLogEnabled)
        SDL_Log("[MUSIC] PlayStream start WAV (disc): %s", name);
    return ok;
}

/* ── Cue external-audio fallback ───────────────────────────────────────── */

/* The install dir is the parent of the music/ subdir: Get*Path builds music as
   "<install>/music/<file>", so strip the last two path components. Returns false
   if `name` lacks a "<dir>/music/<file>" shape. outDir keeps the trailing sep. */
static bool InstallDirFromMusicPath(const char *name, char *outDir, int max) {
    int i = (int)strlen(name) - 1;
    while (i >= 0 && name[i] != '/' && name[i] != '\\') /* skip filename */
        i--;
    if (i < 0)
        return false;
    i--;                                                /* step before the separator that precedes the filename */
    while (i >= 0 && name[i] != '/' && name[i] != '\\') /* skip "music" component */
        i--;
    if (i < 0)
        return false;
    int len = i + 1; /* include the separator: install dir with trailing sep */
    if (len <= 0 || len >= max)
        return false;
    memcpy(outDir, name, (size_t)len);
    outDir[len] = '\0';
    return true;
}

/* ── Redbook tracks (a mixed-mode retail disc) ─────────────────────────── */

/* Stream one CD audio track straight out of the mounted image. Audio sectors
   are already 16-bit stereo 44.1 kHz PCM, so there is nothing to decode beyond
   putting the bytes in the order the mixer wants: read the sector range and hand
   it to the same stream setup a WAV uses, which is what gives it the existing
   volume, pause and resume behaviour. */
static bool PlayCdAudioBuffer(Uint8 *pcm, size_t pcmBytes, uint32_t startSector, S32 skipFrames) {
    /* Red Book is MSB-first and a ripper is meant to swap it; some do not, so
       the order is measured rather than assumed. See LIB386/SYSTEM/CDDA.CPP. */
    if (CdDa_NeedsByteSwap(pcm, pcmBytes)) {
        CdDa_ByteSwap(pcm, pcmBytes);
        if (musicLogEnabled)
            SDL_Log("[MUSIC] CD audio at sector %u is big-endian, byte-swapped it",
                    (unsigned)startSector);
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = 44100;
    spec.format = AIL_AUDIO_S16LE;
    spec.channels = 2;
    return SetupWavStream(&spec, pcm, (Uint32)pcmBytes, skipFrames);
}

static bool PlayCdAudioFromImage(uint32_t startSector, uint32_t endSector, S32 skipFrames) {
    if (endSector <= startSector)
        return false;
    uint32_t sectors = endSector - startSector;
    Uint8 *pcm = (Uint8 *)malloc((size_t)sectors * 2352);
    if (!pcm)
        return false;

    S32 got = DiscImage_ReadRawSectors(startSector, sectors, pcm);
    if (got <= 0) {
        free(pcm);
        return false;
    }

    bool ok = PlayCdAudioBuffer(pcm, (size_t)got * 2352, startSector, skipFrames);
    free(pcm);
    return ok;
}

/* How many whole 2352-byte audio sectors a file holds. */
static uint32_t RawSectorsInFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    uint32_t sectors = 0;
    if (fseek(f, 0, SEEK_END) == 0) {
        long size = ftell(f);
        if (size > 0)
            sectors = (uint32_t)(size / 2352);
    }
    fclose(f);
    return sectors;
}

/* Raw audio sectors out of a BINARY file the cue names but the mount does not
   cover. A rip that splits every track into its own file is still bin+cue, and
   is what DiscImageCreator and EAC produce by default; the bytes are the same
   raw sectors an in-image track holds, addressed in a file of their own. Track
   addresses in such a cue are relative to the file that carries the track. */
static bool PlayCdAudioFromFile(const char *path, uint32_t startSector, uint32_t endSector,
                                S32 skipFrames) {
    if (endSector <= startSector)
        return false;
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    size_t want = (size_t)(endSector - startSector) * 2352;
    Uint8 *pcm = (Uint8 *)malloc(want);
    if (!pcm) {
        fclose(f);
        return false;
    }
    bool read = (fseek(f, (long)startSector * 2352, SEEK_SET) == 0) &&
                (fread(pcm, 1, want, f) == want);
    fclose(f);
    if (!read) {
        free(pcm);
        return false;
    }

    bool ok = PlayCdAudioBuffer(pcm, want, startSector, skipFrames);
    free(pcm);
    return ok;
}

/* True for the redbook "track" music (TADPCM<N> / Track<N>), the kind a cue's
   external audio track stands in for. Excludes JADPCM jingles, which are never
   the cue's redbook track. */
static bool IsRedbookTrackName(const char *name) {
    const char *base = strrchr(name, '/');
#ifdef _WIN32
    const char *bs = strrchr(name, '\\');
    if (bs && (!base || bs > base))
        base = bs;
#endif
    base = base ? base + 1 : name;
    char up[64];
    int i = 0;
    for (; base[i] && i < (int)sizeof(up) - 1; i++)
        up[i] = (char)toupper((unsigned char)base[i]);
    up[i] = '\0';
    return strstr(up, "TADPCM") != NULL || strstr(up, "TRACK") != NULL;
}

/* What the cue's table of contents had to say about a music request.

   The distinction that matters is HANDLED versus NO_TOC. Once a cue is a real
   table of contents it is the authority on where the music lives, so a track it
   does not list is absent, full stop. Falling back to "play the first external
   audio file" there would put the wrong song on: on a rip that mixes in-image
   and standalone tracks, a request for a track the disc lost would play some
   other track's file. */
typedef enum {
    REDBOOK_NO_TOC,  /* no disc table of contents here; later rules may apply */
    REDBOOK_PLAYED,  /* playing */
    REDBOOK_HANDLED, /* the table of contents owns this track and it did not play */
} RedbookOutcome;

/* Read the disc's table of contents. The cue that describes a disc sits beside
   that disc, so look there first. Normally the image is in the install directory
   and these are the same place; they part company when --disc names an image
   elsewhere, and then the image's own cue is the one that means anything.
   Falling back to the install directory covers the reverse oddity, a cue kept
   with the game rather than with the image. */
static int LoadDiscToc(const char *name, char *tocDir, size_t tocDirMax, CueTrack *toc,
                       int maxTracks) {
    char installDir[ADELINE_MAX_PATH];
    if (!InstallDirFromMusicPath(name, installDir, (int)sizeof(installDir)))
        return 0;
    int n = 0;
    if (DiscImage_GetImageDir(tocDir, tocDirMax))
        n = Cue_ReadTocInDir(tocDir, NULL, 0, toc, maxTracks);
    if (n == 0) {
        strncpy(tocDir, installDir, tocDirMax - 1);
        tocDir[tocDirMax - 1] = '\0';
        n = Cue_ReadTocInDir(tocDir, NULL, 0, toc, maxTracks);
    }
    return n;
}

/* Play one track the table of contents names, wherever it lives: a standalone
   file the cue points at, or audio sectors inside the mounted image. `label` is
   only for the log line, since a track's number does not name its music. */
static RedbookOutcome PlayTocTrack(const CueTrack *t, const CueTrack *toc, int n,
                                   const char *tocDir, const char *name, const char *label,
                                   S32 startFrame) {
    if (!t->fileIsBinary) {
        /* A cue's FILE is relative to the sheet, not to the install. */
        char path[ADELINE_MAX_PATH];
        int w = snprintf(path, sizeof(path), "%s%s", tocDir, t->file);
        if (w < 0 || (size_t)w >= sizeof(path))
            return REDBOOK_HANDLED;
        bool recognized = false;
        bool played = PlayWavFile(path, false, startFrame, &recognized);
        if (!played && !recognized)
            played = PlayOggFile(path, startFrame);
        if (!played)
            SDL_Log("SDL AIL: could not stream cue track '%s': %s", path, SDL_GetError());
        if (played && musicLogEnabled)
            SDL_Log("[MUSIC] PlayStream start cue track %s: %s (from %s)", label, path, name);
        return played ? REDBOOK_PLAYED : REDBOOK_HANDLED;
    }

    /* A BINARY file the mount does not cover: a rip that gave every track its own
       file, which DiscImageCreator and EAC both do by default. Still bin+cue, and
       the bytes are the same raw sectors. Addresses in such a cue are relative to
       the file carrying the track, so a track ends at the next track in that same
       file, or at its end. */
    if (!DiscImage_IsMountedFile(t->file)) {
        char path[ADELINE_MAX_PATH];
        int w = snprintf(path, sizeof(path), "%s%s", tocDir, t->file);
        if (w < 0 || (size_t)w >= sizeof(path))
            return REDBOOK_HANDLED;
        uint32_t fileSectors = RawSectorsInFile(path);
        if (fileSectors == 0 || t->startFrame >= fileSectors)
            return REDBOOK_HANDLED;
        uint32_t stop = fileSectors;
        for (int i = 0; i < n; i++) {
            if (strcmp(toc[i].file, t->file) == 0 && toc[i].startFrame > t->startFrame &&
                toc[i].startFrame < stop)
                stop = toc[i].startFrame;
        }
        bool played = PlayCdAudioFromFile(path, t->startFrame, stop, startFrame);
        if (played && musicLogEnabled) {
            SDL_Log("[MUSIC] PlayStream start CD-DA track %s: sectors %u..%u of %s (from %s)",
                    label, (unsigned)t->startFrame, (unsigned)stop, t->file, name);
        }
        return played ? REDBOOK_PLAYED : REDBOOK_HANDLED;
    }

    /* Inside the image. The cue's sector addresses only mean anything against
       the file that cue describes, and only a raw image has audio sectors. */
    uint32_t total = DiscImage_RawSectorCount();
    if (total == 0 || t->startFrame >= total)
        return REDBOOK_HANDLED;

    /* Audio cannot begin inside the data track. A cue that says it does is
       wrong, and honouring it would read the filesystem as PCM and play it at
       music volume: full-scale noise, and an allocation the size of the disc.
       The filesystem's own volume size says where the data ends. */
    uint32_t dataEnd = DiscImage_DataSectorCount();
    if (t->startFrame < dataEnd) {
        SDL_Log("SDL AIL: cue puts CD track %s at sector %u, inside the data track (ends %u); "
                "ignoring it",
                label, (unsigned)t->startFrame, (unsigned)dataEnd);
        return REDBOOK_HANDLED;
    }

    /* The cue states where a track starts, never where it ends: a track runs to
       the next one, and the last runs to the end of the disc. */
    uint32_t end = total;
    for (int i = 0; i < n; i++) {
        if (toc[i].fileIsBinary && toc[i].startFrame > t->startFrame && toc[i].startFrame < end)
            end = toc[i].startFrame;
    }

    bool ok = PlayCdAudioFromImage(t->startFrame, end, startFrame);
    if (ok && musicLogEnabled) {
        SDL_Log("[MUSIC] PlayStream start CD-DA track %s: sectors %u..%u (from %s)", label,
                (unsigned)t->startFrame, (unsigned)end, name);
    }
    return ok ? REDBOOK_PLAYED : REDBOOK_HANDLED;
}

/* Which themes this install has no file for, and where `name` sits among them.
   The arithmetic lives in CdTracks_MissingThemeIndex, which tests/cdtracks pins;
   what belongs here is only the question of what counts as having a file.

   Cached, because it is one existence check per theme and several of them go to
   the disc image, while the answer cannot change while the game is running. The
   cache is keyed on the music directory, so pointing at a different install
   inside one process recomputes rather than answering from the old one. */
static int ThemePositionAmongMissing(const char *name, const char *musicDir, int *outMissing) {
    static char cachedDir[ADELINE_MAX_PATH] = "";
    static unsigned char cachedPresent[64];
    static bool cachedValid = false;

    int themes = CdTracks_ThemeCount();
    if (themes > (int)sizeof(cachedPresent))
        return -1;

    if (!cachedValid || strcmp(cachedDir, musicDir) != 0) {
        for (int i = 0; i < themes; i++) {
            char path[ADELINE_MAX_PATH];
            int w = snprintf(path, sizeof(path), "%s%s.WAV", musicDir, CdTracks_ThemeName(i));
            /* A path too long to build is treated as present: claiming a theme is
               missing is what moves a track onto it, so the safe answer is no. */
            cachedPresent[i] =
                (w > 0 && (size_t)w < sizeof(path)) ? (ExistsFileOrDir(path) ? 1 : 0) : 1;
        }
        strncpy(cachedDir, musicDir, sizeof(cachedDir) - 1);
        cachedDir[sizeof(cachedDir) - 1] = '\0';
        cachedValid = true;
    }

    return CdTracks_MissingThemeIndex(name, cachedPresent, themes, outMissing);
}

/* The rule that holds across every pressing: the audio tracks, in order, are the
   themes with no file, in ListJingle order.

   A track number does not name a theme by itself. The US Activision disc presses
   six themes as tracks 2..7 and its numbering is ListJingle[N]; the Brazilian
   Activision disc presses seven as tracks 2..8 and is ListJingle[N-1]; both
   European pressings press one. Reading a number on the wrong disc plays a
   plausible piece of music that is simply not the right one.

   This engages only when the two counts agree, which is what makes the
   correspondence unambiguous. The US disc is missing seven themes and presses
   six, because TADPCM1 is not on it at all, so it declines here and falls
   through to the table, which is exactly the disc that table was measured on. */
static RedbookOutcome PlayRedbookByPosition(const char *name, S32 startFrame) {
    char musicDir[ADELINE_MAX_PATH];
    int cut = (int)strlen(name) - 1;
    while (cut >= 0 && name[cut] != '/' && name[cut] != '\\')
        cut--;
    if (cut < 0 || (size_t)(cut + 1) >= sizeof(musicDir))
        return REDBOOK_NO_TOC;
    memcpy(musicDir, name, (size_t)cut + 1);
    musicDir[cut + 1] = '\0';

    char tocDir[ADELINE_MAX_PATH];
    CueTrack toc[CUE_MAX_TRACKS];
    int n = LoadDiscToc(name, tocDir, sizeof(tocDir), toc, CUE_MAX_TRACKS);
    if (n == 0)
        return REDBOOK_NO_TOC;

    const CueTrack *audio[CUE_MAX_TRACKS];
    int audioCount = 0;
    for (int i = 0; i < n; i++) {
        if (toc[i].isAudio)
            audio[audioCount++] = &toc[i];
    }
    if (audioCount == 0)
        return REDBOOK_NO_TOC;

    int missing = 0;
    int pos = ThemePositionAmongMissing(name, musicDir, &missing);
    if (pos < 0 || missing != audioCount)
        return REDBOOK_NO_TOC;

    char label[32];
    snprintf(label, sizeof(label), "%d (%d of %d pressed)", audio[pos]->number, pos + 1, missing);
    return PlayTocTrack(audio[pos], toc, n, tocDir, name, label, startFrame);
}

/* Resolve a redbook track by its CD track number, using the US disc's table.
   The fallback for a disc whose pressed count does not match its missing count,
   which in practice means the US Activision disc.

   Engages only when the cue describes two or more audio tracks, i.e. an actual
   disc TOC. A cue with a single audio entry is left to the older rule below:
   that is the GOG shape, where the one external file is music track 6 and
   matching it by number would find nothing. */
static RedbookOutcome PlayRedbookByNumber(const char *name, S32 startFrame) {
    int cdTrack = CdTracks_NumberForMusicName(name);
    if (cdTrack == 0)
        return REDBOOK_NO_TOC;

    char tocDir[ADELINE_MAX_PATH];
    CueTrack toc[CUE_MAX_TRACKS];
    int n = LoadDiscToc(name, tocDir, sizeof(tocDir), toc, CUE_MAX_TRACKS);
    int audioCount = 0;
    for (int i = 0; i < n; i++) {
        if (toc[i].isAudio)
            audioCount++;
    }
    if (audioCount < 2)
        return REDBOOK_NO_TOC;

    const CueTrack *t = NULL;
    for (int i = 0; i < n; i++) {
        if (toc[i].number == cdTrack && toc[i].isAudio) {
            t = &toc[i];
            break;
        }
    }
    if (t == NULL) {
        /* A rip that lost a track lands here. Say which one: silence with no
           explanation is what made this hard to diagnose in the first place. */
        SDL_Log("SDL AIL: '%s' is CD track %d, which this disc's cue does not list", name, cdTrack);
        return REDBOOK_HANDLED;
    }

    char label[16];
    snprintf(label, sizeof(label), "%d", cdTrack);
    return PlayTocTrack(t, toc, n, tocDir, name, label, startFrame);
}

/* ── Public API ────────────────────────────────────────────────────────── */

static void PlayStreamInternal(const char *name, S32 startFrame) {
    if (!AILAudio_Initialized())
        return;
    if (!name || !name[0])
        return;

    StopStreamInternal();

    /* 1) Filesystem WAV. PlayJingle/PlayCD request "*.WAV"; some installs ship it. */
    bool recognized = false;
    if (PlayWavFile(name, false, startFrame, &recognized)) {
        if (musicLogEnabled)
            SDL_Log("[MUSIC] PlayStream start WAV: %s", name);
        FinishStream(name);
        return;
    }
    if (recognized) {
        SDL_Log("SDL AIL: could not stream WAV '%s': %s", name, SDL_GetError());
        return;
    }

    /* 2) Filesystem OGG fallback (.ogg / .OGG / track<N>.ogg). */
    char resolved[ADELINE_MAX_PATH];
    const char *cached = PathCacheGet(name);
    if (cached || ResolveOggPath(name, resolved, (int)sizeof(resolved))) {
        if (cached) {
            strncpy(resolved, cached, sizeof(resolved) - 1);
            resolved[sizeof(resolved) - 1] = '\0';
        } else {
            PathCachePut(name, resolved);
        }
        if (!PlayOggFile(resolved, startFrame)) {
            SDL_Log("SDL AIL: Failed to decode '%s'", resolved);
            return;
        }
        if (musicLogEnabled)
            SDL_Log("[MUSIC] PlayStream start OGG: %s (from %s)", resolved, name);
        FinishStream(name);
        return;
    }

    /* 3) Disc image: bounded WAV decoding on an independent image reader. */
    if (PlayWavFromImage(name, startFrame, &recognized)) {
        FinishStream(name);
        return;
    }
    if (recognized)
        return;

    /* 4) Redbook by position, the rule that holds across every pressing: the
          disc's audio tracks, in order, are the themes it did not ship as
          files. Tried before the number table because a track number means
          different music on different pressings, and this does not. */
    RedbookOutcome redbook = PlayRedbookByPosition(name, startFrame);
    if (redbook == REDBOOK_PLAYED) {
        FinishStream(name);
        return;
    }

    /* 5) Redbook by CD track number, for the one disc whose pressed count and
          missing count disagree. A real TOC is authoritative, so when it owns
          the answer the search stops here either way. */
    redbook = PlayRedbookByNumber(name, startFrame);
    if (redbook == REDBOOK_PLAYED) {
        FinishStream(name);
        return;
    }
    if (redbook == REDBOOK_HANDLED)
        return;

    /* 6) Cue external audio: a redbook track shipped as a standalone file the
          data image does not contain (GOG's LBA2.OGG = music track 6, listed in
          LBA2.DAT). Only for redbook "track" requests, not jingles. The file is
          on the filesystem (install root), so decode it directly: WAV via
          SDL_LoadWAV, otherwise OGG via PlayOggFile. */
    if (IsRedbookTrackName(name)) {
        char installDir[ADELINE_MAX_PATH];
        char cueAudio[ADELINE_MAX_PATH];
        if (InstallDirFromMusicPath(name, installDir, (int)sizeof(installDir)) &&
            Cue_ResolveInDir(installDir, cueAudio, sizeof(cueAudio))) {
            bool recognized = false;
            bool played = PlayWavFile(cueAudio, false, startFrame, &recognized);
            if (!played && !recognized)
                played = PlayOggFile(cueAudio, startFrame);
            if (!played) {
                SDL_Log("SDL AIL: Failed to decode cue audio '%s'", cueAudio);
                return;
            }
            if (musicLogEnabled)
                SDL_Log("[MUSIC] PlayStream start cue-audio: %s (from %s)", cueAudio, name);
            FinishStream(name);
            return;
        }
    }

    SDL_Log("SDL AIL: Could not find '%s' (filesystem, OGG fallback, disc image, cue audio, "
            "or the disc's own audio track)",
            name);
}

void PlayStream(char *name) {
    PlayStreamInternal(name, 0);
}

void ChangeVolumeStream(S32 volume) {
    streamVolume = volume;
    if (stream) {
        float gain = (volume <= 0) ? 0.0f : (float)volume / 127.0f;
        if (gain > 1.0f)
            gain = 1.0f;
        AILAudio_Gain(stream, gain);
        if (musicLogEnabled)
            SDL_Log("[MUSIC] ChangeVolumeStream vol=%d gain=%.3f", volume, gain);
    }
}

S32 GetVolumeStream() {
    return streamVolume;
}

void StopStream() {
    if (musicLogEnabled)
        SDL_Log("[MUSIC] StopStream");
    StopStreamInternal();
}

void PauseStream() {
    /* A stream that has played out is never detached unless something polls
       IsStreamPlaying, which nothing does once the music queue is empty. */
    int hasAudioLeft = stream != NULL &&
                       (AIL_AtomicGet(&decodeActive) || AILAudio_Queued(stream) > 0);
    if (Stream_ShouldPark(stream != NULL, streamPaused, hasAudioLeft)) {
        /* Park the position so a later Resume can restore even if a different
           stream (e.g. the in-game menu theme) plays and tears this one down in
           the meantime. MILES does the same in PauseCD (save Cur/End/track). */
        strncpy(rememberedPath, streamPathName, ADELINE_MAX_PATH - 1);
        rememberedPath[ADELINE_MAX_PATH - 1] = '\0';
        rememberedFrame = StreamPositionFrames();
        hasRemembered = true;
        if (musicLogEnabled)
            SDL_Log("[MUSIC] PauseStream park=%s frame=%d", rememberedPath, (int)rememberedFrame);
        AILAudio_Pause(stream);
        streamPaused = true;
    } else if (stream != NULL && !streamPaused) {
        /* The current track has played out, so this pause has nothing to resume.
           Drop an older park too: one left by an Escape that never resumed (the
           player loaded a game from the menu) would bring that track back here. */
        hasRemembered = false;
    }
}

void ResumeStream() {
    switch (Stream_ResumeAction(stream != NULL, streamPaused, hasRemembered,
                                strcmp(streamPathName, rememberedPath) == 0,
                                rememberedPath[0] != '\0')) {
    case STREAM_RESUME_UNPAUSE:
        /* Fast path: the parked stream is still alive and paused -> just unpause the
           device. This is the dialogue/holomap case (nothing played in between). */
        if (musicLogEnabled)
            SDL_Log("[MUSIC] ResumeStream unpause %s", streamPathName);
        AILAudio_Resume(stream);
        streamPaused = false;
        hasRemembered = false;
        break;

    case STREAM_RESUME_RESTORE: {
        /* Restore path: a different stream replaced the parked one (the menu theme
           played between Pause and Resume), so re-open the parked file and seek back
           to where it was. Mirrors MILES ResumeCD replaying from the saved Cur. */
        char path[ADELINE_MAX_PATH];
        strncpy(path, rememberedPath, ADELINE_MAX_PATH - 1);
        path[ADELINE_MAX_PATH - 1] = '\0';
        S32 frame = rememberedFrame;
        hasRemembered = false;
        if (musicLogEnabled)
            SDL_Log("[MUSIC] ResumeStream restore %s frame=%d", path, (int)frame);
        PlayStreamInternal(path, frame);
        break;
    }

    case STREAM_RESUME_NONE:
        break;
    }
}

void SuspendStreamOutput() {
    /* Device-level pause for window-focus loss. Unlike PauseStream this does NOT
       set streamPaused or snapshot a resume position, so it never disturbs an
       outer PauseStream's parked {path, frame} (e.g. gameplay music parked while
       the Options menu theme plays). Pausing an already-paused device is a no-op. */
    if (stream) {
        if (musicLogEnabled)
            SDL_Log("[MUSIC] SuspendStreamOutput");
        AILAudio_Pause(stream);
    }
}

void ResumeStreamOutput() {
    /* Re-resume the device only if the stream is not logically paused; otherwise
       a focus blip during a dialogue/menu music pause would un-pause it. */
    if (Stream_ShouldResumeDeviceOnFocus(stream != NULL, streamPaused)) {
        if (musicLogEnabled)
            SDL_Log("[MUSIC] ResumeStreamOutput");
        AILAudio_Resume(stream);
    }
}

S32 IsStreamPlaying() {
    if (!stream)
        return FALSE;
    if (streamPaused)
        return FALSE;
    if (AIL_AtomicGet(&decodeActive))
        return TRUE;
    /* The stream is fully pushed and flushed (see PlayOggFile / SetupWavStream /
       DecodeThreadFunc), so once the device has consumed everything the queue reaches
       0 and the track is done. Without the flush the device-format converter would
       hold a few bytes here forever and the stream would never report EOF (#406). */
    int queued = AILAudio_Queued(stream);
    if (queued <= 0) {
        StopStreamInternal();
        return FALSE;
    }
    return TRUE;
}

char *StreamName() {
    return streamPathName;
}

void MusicLogEnable(int enable) {
    musicLogEnabled = (enable != 0) ? 1 : 0;
    SDL_Log("[MUSIC] logging %s", musicLogEnabled ? "ON" : "OFF");
}

int MusicLogIsEnabled(void) {
    return musicLogEnabled;
}

#ifdef LBA2_MUSIC_TESTS
int Stream_TestQueuedBytes(void) {
    return stream ? AILAudio_Queued(stream) : 0;
}
#endif
