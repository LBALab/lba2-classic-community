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
#include <SYSTEM/ISO9660.H>

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
int stb_vorbis_seek_start(stb_vorbis *f);
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

/* ── Background producer ───────────────────────────────────────────────── */

/* Music is fed to the device as it plays, never held whole: a track is 30 to 40
   MB of PCM (#797). One producer thread per stream pulls batches from a source
   and keeps about 64 KiB queued (Stream_QueueWaitMs), sleeping on decodeWake
   in between. WAV files, OGG files and CD audio sectors are the sources. */

static SDL_Thread *decodeThread = NULL;
static AILSemaphore *decodeWake = NULL;
static AILAtomicInt decodeStop;
static AILAtomicInt decodeActive;
static AILAtomicInt producerWaiting; // parked on a full queue (read by the tests)

/* Largest batch handed to the device at once, rounded down to whole frames. */
#define PRODUCER_PUT_BYTES 16384

/* Where the producer gets PCM. Read hands back the next batch in `spec`'s
   format, owned by the source and valid until the next Read: 1 = data,
   0 = end of track, -1 = error (SDL error set). */
class MusicSource {
  public:
    SDL_AudioSpec spec;
    MusicSource() { SDL_zero(spec); }
    virtual ~MusicSource() {}
    virtual int Read(const Uint8 **pcm, Uint32 *bytes) = 0;
};

class WavMusicSource : public MusicSource {
  public:
    explicit WavMusicSource(AILWavSource *source) : source(source), batch(NULL) {}
    ~WavMusicSource() {
        SDL_free(batch);
        AILWav_Close(source);
    }
    int Read(const Uint8 **pcm, Uint32 *bytes) {
        SDL_free(batch);
        batch = NULL;
        int result = AILWav_Read(source, &spec, &batch, bytes);
        *pcm = batch;
        return result;
    }

  private:
    AILWavSource *source;
    Uint8 *batch;
};

#define OGG_BATCH_SHORTS 4096

class OggMusicSource : public MusicSource {
  public:
    OggMusicSource(stb_vorbis *vorbis, int channels, int rate) : vorbis(vorbis), channels(channels) {
        spec.freq = rate;
        spec.format = AIL_AUDIO_S16;
        spec.channels = (Uint8)channels;
    }
    ~OggMusicSource() { stb_vorbis_close(vorbis); }
    int Read(const Uint8 **pcm, Uint32 *bytes) {
        int frames = stb_vorbis_get_samples_short_interleaved(vorbis, channels, batch,
                                                              OGG_BATCH_SHORTS);
        *pcm = (const Uint8 *)batch;
        *bytes = frames > 0 ? (Uint32)(frames * channels) * (Uint32)sizeof(short) : 0;
        return frames > 0 ? 1 : 0;
    }

  private:
    stb_vorbis *vorbis;
    int channels;
    short batch[OGG_BATCH_SHORTS];
};

struct ProducerData {
    MusicSource *source;
    const Uint8 *pcm; // the source's current batch
    Uint32 bytes, offset;
};

static Uint32 SourceFrameBytes(const MusicSource *source) {
    return AIL_AUDIO_BITSIZE(source->spec.format) / 8 * source->spec.channels;
}

static Uint32 NextPutBytes(const ProducerData *d, Uint32 frameBytes) {
    Uint32 count = d->bytes - d->offset;
    return count > PRODUCER_PUT_BYTES ? PRODUCER_PUT_BYTES / frameBytes * frameBytes : count;
}

static int SDLCALL ProducerThread(void *userdata) {
    Crash_PrepareThread();
    ProducerData *d = (ProducerData *)userdata;
    const Uint32 frameBytes = SourceFrameBytes(d->source);
    bool failed = false;
    while (!AIL_AtomicGet(&decodeStop)) {
        if (d->offset == d->bytes) {
            int result = d->source->Read(&d->pcm, &d->bytes);
            d->offset = 0;
            if (result <= 0) {
                if (result < 0) {
                    SDL_Log("SDL AIL: music streaming failed: %s", SDL_GetError());
                    failed = true;
                }
                break;
            }
        }
        Uint32 count = NextPutBytes(d, frameBytes);
        int queued = AILAudio_Queued(stream);
        if (queued < 0) {
            failed = true;
            break;
        }
        int waitMs = Stream_QueueWaitMs((Uint32)queued, count, d->source->spec.freq, (int)frameBytes);
        if (waitMs) {
            AIL_AtomicSet(&producerWaiting, 1);
            int waited = AIL_WaitSemaphoreTimeout(decodeWake, waitMs);
            AIL_AtomicSet(&producerWaiting, 0);
            if (waited < 0) {
                SDL_Log("SDL AIL: music queue wait failed: %s", SDL_GetError());
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
        SDL_Log("SDL AIL: music producer stopped after an error");
    delete d->source;
    free(d);
    /* All of the track has been pushed. Signal end-of-input so SDL's device-format
       converter emits its held tail; otherwise AILAudio_Queued sticks a few bytes
       above 0 forever and the stream never reports EOF, which stalls the
       deferred-music switch (#406). Not after a stop: the stream is going away. */
    if (!AIL_AtomicGet(&decodeStop))
        AILAudio_Flush(stream);
    AIL_AtomicSet(&decodeActive, 0);
    return 0;
}

static void StopDecodeThread(void) {
    if (decodeThread) {
        AIL_AtomicSet(&decodeStop, 1);
        if (decodeWake && !AIL_SignalSemaphore(decodeWake))
            SDL_Log("SDL AIL: unable to wake music producer: %s", SDL_GetError());
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

#ifdef LBA2_STREAM_TESTS
void AILStream_TestWakeProducer(void) {
    if (decodeWake) {
        AIL_AtomicSet(&producerWaiting, 0); // until it looks at the queue again
        AIL_SignalSemaphore(decodeWake);
    }
}

int AILStream_TestProducerWaiting(void) {
    return AIL_AtomicGet(&producerWaiting);
}

int AILStream_TestBaseFrame(void) {
    return stream ? streamBaseFrame : 0;
}
#endif

/* Open the device stream for `source`, queue its first batch and start the
   producer on the rest. `baseFrame` is the frame of the track the source starts
   at (non-zero on a resume); `started` is SDL_GetTicks from before the source
   was opened, for the log. Takes ownership of `source` either way.

   Returns true with no stream when a resume lands at or past the end of the
   track: there is nothing left to play, which is not a failure. */
static bool StartProducer(MusicSource *source, S32 baseFrame, const char *name, Uint32 started) {
    ProducerData *d = (ProducerData *)calloc(1, sizeof(*d));
    if (!d) {
        delete source;
        SDL_SetError("Unable to allocate music producer");
        return false;
    }
    d->source = source;
    int first = source->Read(&d->pcm, &d->bytes);
    if (first == 0 && baseFrame > 0) {
        delete source;
        free(d);
        return true;
    }
    if (first > 0)
        stream = AILAudio_Open(&source->spec, NULL, NULL);
    if (first <= 0 || !stream) {
        if (first == 0)
            SDL_SetError("Music track has no playable frames");
        delete source;
        free(d);
        return false;
    }
    streamBytesPerFrame = (int)SourceFrameBytes(source);
    streamBaseFrame = baseFrame > 0 ? baseFrame : 0;
    d->offset = NextPutBytes(d, (Uint32)streamBytesPerFrame);
    if (!AILAudio_Put(stream, d->pcm, (int)d->offset)) {
        delete source;
        free(d);
        AILAudio_Destroy(stream);
        stream = NULL;
        return false;
    }
    AIL_AtomicAdd(&streamPushedBytes, (int)d->offset);
    if (musicLogEnabled)
        SDL_Log("[MUSIC] opened and prefilled %u bytes in %u ms: %s",
                (unsigned)d->offset, (unsigned)((Uint32)SDL_GetTicks() - started), name);
    AIL_AtomicSet(&decodeStop, 0);
    AIL_AtomicSet(&decodeActive, 1);
    decodeWake = SDL_CreateSemaphore(0);
    decodeThread = decodeWake ? SDL_CreateThread(ProducerThread, "MusicStream", d) : NULL;
    if (!decodeThread) {
        if (decodeWake) {
            SDL_DestroySemaphore(decodeWake);
            decodeWake = NULL;
        }
        AIL_AtomicSet(&decodeActive, 0);
        delete source;
        free(d);
        AILAudio_Destroy(stream);
        stream = NULL;
        return false;
    }
    return true;
}

static bool PlayWavFile(const char *name, bool image, S32 startFrame, bool *recognized) {
    Uint32 started = (Uint32)SDL_GetTicks();
    AILWavSource *source = AILWav_Open(name, image, startFrame, recognized);
    if (!source)
        return false;
    return StartProducer(new WavMusicSource(source), startFrame, name, started);
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

/* ── PlayOggFile ───────────────────────────────────────────────────────── */
/* startFrame > 0 resumes mid-file (ResumeStream restore path) by seeking the
   decoder. Opening and decoding the first batch costs about half a millisecond
   on a phone, so a replayed track needs no decoded copy kept around (#797). */

static bool PlayOggFile(const char *oggPath, S32 startFrame) {
    Uint32 started = (Uint32)SDL_GetTicks();
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
        if (!stb_vorbis_seek_frame(v, seekFrame)) {
            /* The decoder's position is unknown after a failed seek: play the
               track from the top rather than from somewhere unknown. */
            SDL_Log("SDL AIL: could not seek '%s' to frame %u, playing it from the start",
                    oggPath, seekFrame);
            seekFrame = 0;
            if (!stb_vorbis_seek_start(v)) {
                stb_vorbis_close(v);
                return false;
            }
        }
    }

    return StartProducer(new OggMusicSource(v, info.channels, (int)info.sample_rate),
                         (S32)seekFrame, oggPath, started);
}

/* ── WAV + disc-image helpers ──────────────────────────────────────────── */

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

#define CD_SECTOR_BYTES 2352
#define CD_BATCH_SECTORS 14 // about 190 ms of audio per read

/* Sectors a byte-order probe window touches when it starts `within` bytes into
   its first sector. */
static uint32_t ProbeSpanSectors(size_t within) {
    return (uint32_t)((within + CDDA_PROBE_WINDOW + CD_SECTOR_BYTES - 1) / CD_SECTOR_BYTES);
}

/* Stream one CD audio track, from the mounted image or from a BINARY file of
   its own. Audio sectors are already 16-bit stereo 44.1 kHz PCM, so there is
   nothing to decode beyond putting the bytes in the order the mixer wants.
   The image is read through a handle of its own, never the asset loader's. */
class CdAudioMusicSource : public MusicSource {
  public:
    CdAudioMusicSource(iso9660_t *iso, FILE *file, uint32_t startSector, uint32_t endSector)
        : iso(iso), file(file), start(startSector), next(startSector), end(endSector),
          skipBytes(0), swap(false) {
        spec.freq = 44100;
        spec.format = AIL_AUDIO_S16LE;
        spec.channels = 2;
    }
    ~CdAudioMusicSource() {
        if (iso)
            iso_close(iso);
        if (file)
            fclose(file);
    }

    /* Measure the byte order and move to `skipFrames` into the track. */
    void Prepare(S32 skipFrames) {
        /* Red Book is MSB-first and a ripper is meant to swap it; some do not, so
           the order is measured rather than assumed, over the same windows of the
           whole track as before. See LIB386/SYSTEM/CDDA.CPP. */
        swap = MeasureNeedsSwap();
        if (swap && musicLogEnabled)
            SDL_Log("[MUSIC] CD audio at sector %u is big-endian, byte-swapping it",
                    (unsigned)start);
        if (skipFrames > 0) {
            uint64_t skip = (uint64_t)skipFrames * 4;
            uint64_t total = (uint64_t)(end - start) * CD_SECTOR_BYTES;
            if (skip >= total) {
                next = end;
            } else {
                next = start + (uint32_t)(skip / CD_SECTOR_BYTES);
                skipBytes = (Uint32)(skip % CD_SECTOR_BYTES);
            }
        }
    }

    int Read(const Uint8 **pcm, Uint32 *bytes) {
        *pcm = batch;
        *bytes = 0;
        if (next >= end)
            return 0;
        uint32_t want = end - next;
        if (want > CD_BATCH_SECTORS)
            want = CD_BATCH_SECTORS;
        int got = ReadSectors(next, want, batch);
        if (got < 0) {
            SDL_SetError("CD audio read failed at sector %u", (unsigned)next);
            return -1;
        }
        /* A short read is the end of what the image holds; play what came. */
        next = ((uint32_t)got < want) ? end : next + (uint32_t)got;
        Uint32 n = (Uint32)got * CD_SECTOR_BYTES;
        if (swap)
            CdDa_ByteSwap(batch, n);
        Uint32 skip = skipBytes < n ? skipBytes : n;
        skipBytes = 0;
        *pcm = batch + skip;
        *bytes = n - skip;
        return *bytes > 0 ? 1 : 0;
    }

  private:
    iso9660_t *iso;
    FILE *file;
    uint32_t start, next, end;
    Uint32 skipBytes;
    bool swap;
    Uint8 batch[CD_BATCH_SECTORS * CD_SECTOR_BYTES];

    /* Whole sectors read, or -1. */
    int ReadSectors(uint32_t lba, uint32_t count, Uint8 *out) {
        if (iso)
            return iso_read_raw(iso, lba, count, out);
        if (fseek(file, (long)lba * CD_SECTOR_BYTES, SEEK_SET) != 0)
            return -1;
        return (int)(fread(out, 1, (size_t)count * CD_SECTOR_BYTES, file) / CD_SECTOR_BYTES);
    }

    bool MeasureNeedsSwap() {
        size_t total = (size_t)(end - start) * CD_SECTOR_BYTES;
        if (total < (size_t)CDDA_PROBE_WINDOW * 2)
            return false;
        const uint32_t spanSectors = ProbeSpanSectors(CD_SECTOR_BYTES - 1); // the most any window needs
        Uint8 *windows = (Uint8 *)malloc((size_t)CDDA_PROBE_COUNT * CDDA_PROBE_WINDOW);
        Uint8 *span = (Uint8 *)malloc((size_t)spanSectors * CD_SECTOR_BYTES);
        bool ok = windows && span;
        for (int p = 0; ok && p < CDDA_PROBE_COUNT; p++) {
            size_t off = CdDa_ProbeOffset(total, p);
            uint32_t sector = (uint32_t)(off / CD_SECTOR_BYTES);
            size_t within = off % CD_SECTOR_BYTES;
            uint32_t count = ProbeSpanSectors(within);
            ok = ReadSectors(start + sector, count, span) == (int)count;
            if (ok)
                memcpy(windows + (size_t)p * CDDA_PROBE_WINDOW, span + within, CDDA_PROBE_WINDOW);
        }
        /* An unreadable track is left alone: a needless swap turns music into noise. */
        bool needs = ok && CdDa_NeedsByteSwapWindows(windows, total);
        free(span);
        free(windows);
        return needs;
    }
};

static bool PlayCdAudio(iso9660_t *iso, FILE *file, uint32_t startSector, uint32_t endSector,
                        S32 skipFrames, const char *name, Uint32 started) {
    CdAudioMusicSource *source = new CdAudioMusicSource(iso, file, startSector, endSector);
    source->Prepare(skipFrames);
    return StartProducer(source, skipFrames, name, started);
}

static bool PlayCdAudioFromImage(uint32_t startSector, uint32_t endSector, S32 skipFrames) {
    if (endSector <= startSector)
        return false;
    Uint32 started = (Uint32)SDL_GetTicks();
    iso9660_t *reader = DiscImage_OpenRawIndependent();
    if (!reader)
        return false;
    return PlayCdAudio(reader, NULL, startSector, endSector, skipFrames, "CD audio (image)", started);
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
            sectors = (uint32_t)(size / CD_SECTOR_BYTES);
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
    Uint32 started = (Uint32)SDL_GetTicks();
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    return PlayCdAudio(NULL, f, startSector, endSector, skipFrames, path, started);
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
          on the filesystem (install root), so decode it directly: WAV through
          the WAV reader, otherwise OGG via PlayOggFile. */
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
    /* The producer has pushed and flushed the whole track (ProducerThread), so
       once the device has consumed everything the queue reaches 0 and the track
       is done. Without the flush the device-format converter would
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
