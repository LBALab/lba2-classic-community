#include "STREAM_SDL.H"

#include <SDL3/SDL.h>

#include "stb_vorbis.h"

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

static SDL_AudioStream *stream = NULL;
static char streamPathName[ADELINE_MAX_PATH] = "";
static bool streamPaused = false;

/* Geometry of the source PCM currently feeding `stream`, used to convert the
   queued-byte backlog into a frame offset (see StreamPositionFrames). */
static int streamBytesPerFrame = 0;
static SDL_AtomicInt streamPushedBytes; // total source bytes handed to `stream`

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
static SDL_AtomicInt decodeStop;
static SDL_AtomicInt decodeActive;

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

    int totalShorts = 0;
    int capacity = 0;
    short *fullPcm = NULL;

    while (!SDL_GetAtomicInt(&decodeStop)) {
        int n = stb_vorbis_get_samples_short_interleaved(
            d->vorbis, d->channels, buf, DECODE_CHUNK_SHORTS);
        if (n <= 0)
            break;

        int chunkShorts = n * d->channels;
        int chunkBytes = chunkShorts * (int)sizeof(short);
        SDL_PutAudioStreamData(stream, buf, chunkBytes);
        SDL_AddAtomicInt(&streamPushedBytes, chunkBytes);

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

    stb_vorbis_close(d->vorbis);

    if (!SDL_GetAtomicInt(&decodeStop) && d->cacheResult && fullPcm && totalShorts > 0) {
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

    SDL_SetAtomicInt(&decodeActive, 0);
    free(d);
    return 0;
}

static void StopDecodeThread(void) {
    if (decodeThread) {
        SDL_SetAtomicInt(&decodeStop, 1);
        SDL_WaitThread(decodeThread, NULL);
        decodeThread = NULL;
    }
    SDL_SetAtomicInt(&decodeStop, 0);
    SDL_SetAtomicInt(&decodeActive, 0);
}

/* Current playback offset in source frames: everything pushed to the stream
   minus what is still queued (not yet consumed by the device). Works while the
   decode thread is still filling the stream because both sides count source
   bytes. The device's own buffer makes this a slight overestimate (tens of ms),
   which is fine for re-seeking music. */
static S32 StreamPositionFrames(void) {
    if (!stream || streamBytesPerFrame <= 0)
        return 0;
    int pushed = SDL_GetAtomicInt(&streamPushedBytes);
    int queued = (int)SDL_GetAudioStreamQueued(stream);
    int consumed = pushed - queued;
    if (consumed < 0)
        consumed = 0;
    return (S32)(consumed / streamBytesPerFrame);
}

/* ── Init / Stop ───────────────────────────────────────────────────────── */

static void StopStreamInternal(void) {
    StopDecodeThread();
    if (stream) {
        SDL_DestroyAudioStream(stream);
        stream = NULL;
    }
    streamPathName[0] = '\0';
    streamPaused = false;
    streamBytesPerFrame = 0;
    SDL_SetAtomicInt(&streamPushedBytes, 0);
    /* Deliberately keep rememberedPath/Frame: a StopStream here may be the menu
       theme replacing parked game music, which ResumeStream must still restore. */
}

void OpenStream() {
    StopDecodeThread();
    if (stream) {
        SDL_DestroyAudioStream(stream);
        stream = NULL;
    }
    streamPathName[0] = '\0';
    streamPaused = false;
    streamBytesPerFrame = 0;
    SDL_SetAtomicInt(&streamPushedBytes, 0);
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
        spec.format = SDL_AUDIO_S16;
        spec.channels = (Uint8)oggCache.channels;
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
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
        SDL_PutAudioStreamData(stream, oggCache.pcm + offsetBytes / sizeof(short), playBytes);
        SDL_AddAtomicInt(&streamPushedBytes, (int)playBytes);
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

    if (startFrame > 0) {
        /* Clamp a slightly-overestimated resume frame (StreamPositionFrames can
           read a device-buffer ahead of true playback) so a seek past end-of-file
           cannot leave the decoder at an undefined position or replay from 0. */
        unsigned int lenFrames = stb_vorbis_stream_length_in_samples(v);
        unsigned int seekFrame = (unsigned int)startFrame;
        if (lenFrames > 0 && seekFrame >= lenFrames)
            seekFrame = lenFrames - 1;
        stb_vorbis_seek_frame(v, seekFrame);
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = (int)info.sample_rate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = (Uint8)info.channels;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        stb_vorbis_close(v);
        return false;
    }
    streamBytesPerFrame = (int)info.channels * (int)sizeof(short);

    DecodeThreadData *d = (DecodeThreadData *)malloc(sizeof(DecodeThreadData));
    if (!d) {
        stb_vorbis_close(v);
        SDL_DestroyAudioStream(stream);
        stream = NULL;
        return false;
    }
    d->vorbis = v;
    d->channels = (int)info.channels;
    d->sample_rate = (int)info.sample_rate;
    d->cacheResult = (startFrame == 0); // a seeked decode yields partial PCM; don't cache it
    strncpy(d->oggPath, oggPath, ADELINE_MAX_PATH - 1);
    d->oggPath[ADELINE_MAX_PATH - 1] = '\0';

    SDL_SetAtomicInt(&decodeStop, 0);
    SDL_SetAtomicInt(&decodeActive, 1);
    decodeThread = SDL_CreateThread(DecodeThreadFunc, "OGGDecode", d);
    if (!decodeThread) {
        SDL_SetAtomicInt(&decodeActive, 0);
        stb_vorbis_close(v);
        free(d);
        SDL_DestroyAudioStream(stream);
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
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, spec, NULL, NULL);
    if (!stream) {
        SDL_Log("SDL AIL: Could not open audio stream: %s", SDL_GetError());
        return false;
    }
    streamBytesPerFrame = (int)SDL_AUDIO_BYTESIZE(spec->format) * spec->channels;
    Uint32 offsetBytes = 0;
    if (startFrame > 0 && streamBytesPerFrame > 0) {
        offsetBytes = (Uint32)startFrame * (Uint32)streamBytesPerFrame;
        if (offsetBytes > wavLen)
            offsetBytes = wavLen;
    }
    SDL_PutAudioStreamData(stream, wavData + offsetBytes, wavLen - offsetBytes);
    SDL_AddAtomicInt(&streamPushedBytes, (int)(wavLen - offsetBytes));
    return true;
}

/* Apply the current volume, start playback, and record the source path. Common
   tail shared by every successful source. */
static void FinishStream(const char *name) {
    float gain = (streamVolume <= 0) ? 0.0f : (float)streamVolume / 127.0f;
    if (gain > 1.0f)
        gain = 1.0f;
    SDL_SetAudioStreamGain(stream, gain);

    SDL_ResumeAudioStreamDevice(stream);

    strncpy(streamPathName, name, ADELINE_MAX_PATH - 1);
    streamPathName[ADELINE_MAX_PATH - 1] = '\0';
}

/* Read a whole file out of the mounted disc image into a malloc'd buffer (caller
   frees). Returns NULL if no image is mounted or the file is not in it. */
static Uint8 *ReadWholeFromImage(const char *name, Uint32 *outLen) {
    S32 h = DiscImage_OpenRead(name);
    if (!h)
        return NULL;
    S32 size = DiscImage_Seek(h, 0, SEEK_FROM_END);
    DiscImage_Seek(h, 0, SEEK_FROM_START);
    if (size <= 0) {
        DiscImage_Close(h);
        return NULL;
    }
    Uint8 *buf = (Uint8 *)malloc((size_t)size);
    if (!buf) {
        DiscImage_Close(h);
        return NULL;
    }
    S32 got = 0;
    while (got < size) {
        S32 n = DiscImage_Read(h, buf + got, (U32)(size - got));
        if (n <= 0)
            break;
        got += n;
    }
    DiscImage_Close(h);
    if (got != size) {
        free(buf);
        return NULL;
    }
    *outLen = (Uint32)size;
    return buf;
}

/* Disc-image fallback: read the file's bytes through the disc seam (the
   filesystem lookups above already missed) and decode the WAV from memory. This
   reaches music that ships only inside a mounted image, e.g. LBA2-GOG's in-BIN
   JADPCM and TADPCM WAV files. Returns true with `stream` set up on success. */
static bool PlayWavFromImage(const char *name, S32 startFrame) {
    Uint32 rawLen = 0;
    Uint8 *raw = ReadWholeFromImage(name, &rawLen);
    if (!raw)
        return false;

    SDL_IOStream *io = SDL_IOFromConstMem(raw, rawLen);
    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_data_len = 0;
    /* closeio=true closes `io` whether or not the load succeeds; it does not own
       `raw`, which we free below once SDL has copied the PCM into wav_data. */
    bool loaded = io && SDL_LoadWAV_IO(io, true, &spec, &wav_data, &wav_data_len);
    free(raw);
    if (!loaded)
        return false; // present in the image but not a WAV we can decode

    if (musicLogEnabled)
        SDL_Log("[MUSIC] PlayStream start WAV (disc): %s", name);
    bool ok = SetupWavStream(&spec, wav_data, wav_data_len, startFrame);
    SDL_free(wav_data);
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
    i--; /* step before the separator that precedes the filename */
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

/* ── Public API ────────────────────────────────────────────────────────── */

static void PlayStreamInternal(const char *name, S32 startFrame) {
    if (!name || !name[0])
        return;

    StopStreamInternal();

    /* 1) Filesystem WAV. PlayJingle/PlayCD request "*.WAV"; some installs ship it. */
    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_data_len = 0;
    if (SDL_LoadWAV(name, &spec, &wav_data, &wav_data_len)) {
        if (musicLogEnabled)
            SDL_Log("[MUSIC] PlayStream start WAV: %s", name);
        bool ok = SetupWavStream(&spec, wav_data, wav_data_len, startFrame);
        SDL_free(wav_data);
        if (!ok)
            return;
        FinishStream(name);
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

    /* 3) Disc image: read bytes through the seam and decode the WAV from memory.
          Reaches music that lives only inside a mounted disc image. */
    if (PlayWavFromImage(name, startFrame)) {
        FinishStream(name);
        return;
    }

    /* 4) Cue external audio: a redbook track shipped as a standalone file the
          data image does not contain (GOG's LBA2.OGG = music track 6, listed in
          LBA2.DAT). Only for redbook "track" requests, not jingles. The file is
          on the filesystem (install root), so decode it directly: WAV via
          SDL_LoadWAV, otherwise OGG via PlayOggFile. */
    if (IsRedbookTrackName(name)) {
        char installDir[ADELINE_MAX_PATH];
        char cueAudio[ADELINE_MAX_PATH];
        if (InstallDirFromMusicPath(name, installDir, (int)sizeof(installDir)) &&
            Cue_ResolveInDir(installDir, cueAudio, sizeof(cueAudio))) {
            SDL_AudioSpec cspec;
            Uint8 *cwav = NULL;
            Uint32 cwavLen = 0;
            bool played;
            if (SDL_LoadWAV(cueAudio, &cspec, &cwav, &cwavLen)) {
                played = SetupWavStream(&cspec, cwav, cwavLen, startFrame);
                SDL_free(cwav);
            } else {
                played = PlayOggFile(cueAudio, startFrame);
            }
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

    SDL_Log("SDL AIL: Could not find '%s' (filesystem, OGG fallback, disc image, or cue audio)", name);
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
        SDL_SetAudioStreamGain(stream, gain);
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
    if (stream && !streamPaused) {
        /* Park the position so a later Resume can restore even if a different
           stream (e.g. the in-game menu theme) plays and tears this one down in
           the meantime. MILES does the same in PauseCD (save Cur/End/track). */
        strncpy(rememberedPath, streamPathName, ADELINE_MAX_PATH - 1);
        rememberedPath[ADELINE_MAX_PATH - 1] = '\0';
        rememberedFrame = StreamPositionFrames();
        hasRemembered = true;
        if (musicLogEnabled)
            SDL_Log("[MUSIC] PauseStream park=%s frame=%d", rememberedPath, (int)rememberedFrame);
        SDL_PauseAudioStreamDevice(stream);
        streamPaused = true;
    }
}

void ResumeStream() {
    /* Fast path: the parked stream is still alive and paused -> just unpause the
       device. This is the dialogue/holomap case (nothing played in between). */
    if (stream && streamPaused && hasRemembered && strcmp(streamPathName, rememberedPath) == 0) {
        if (musicLogEnabled)
            SDL_Log("[MUSIC] ResumeStream unpause %s", streamPathName);
        SDL_ResumeAudioStreamDevice(stream);
        streamPaused = false;
        hasRemembered = false;
        return;
    }

    /* Restore path: a different stream replaced the parked one (the menu theme
       played between Pause and Resume), so re-open the parked file and seek back
       to where it was. Mirrors MILES ResumeCD replaying from the saved Cur. */
    if (hasRemembered && rememberedPath[0]) {
        char path[ADELINE_MAX_PATH];
        strncpy(path, rememberedPath, ADELINE_MAX_PATH - 1);
        path[ADELINE_MAX_PATH - 1] = '\0';
        S32 frame = rememberedFrame;
        hasRemembered = false;
        if (musicLogEnabled)
            SDL_Log("[MUSIC] ResumeStream restore %s frame=%d", path, (int)frame);
        PlayStreamInternal(path, frame);
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
        SDL_PauseAudioStreamDevice(stream);
    }
}

void ResumeStreamOutput() {
    /* Re-resume the device only if the stream is not logically paused; otherwise
       a focus blip during a dialogue/menu music pause would un-pause it. */
    if (stream && !streamPaused) {
        if (musicLogEnabled)
            SDL_Log("[MUSIC] ResumeStreamOutput");
        SDL_ResumeAudioStreamDevice(stream);
    }
}

S32 IsStreamPlaying() {
    if (!stream)
        return FALSE;
    if (streamPaused)
        return FALSE;
    if (SDL_GetAtomicInt(&decodeActive))
        return TRUE;
    int queued = SDL_GetAudioStreamQueued(stream);
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
