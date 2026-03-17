#include "STREAM_SDL.H"

#include <SDL3/SDL.h>

#include "stb_vorbis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ADELINE_MAX_PATH
#define ADELINE_MAX_PATH 512
#endif

/* ── Core stream state ─────────────────────────────────────────────────── */

static SDL_AudioStream *stream = NULL;
static char streamPathName[ADELINE_MAX_PATH] = "";
static bool streamPaused = false;

S32 streamVolume = 0;
static int musicLogEnabled = 0;

/* ── Path resolution cache ─────────────────────────────────────────────── */

#define PATH_CACHE_MAX 32
static struct { char key[ADELINE_MAX_PATH]; char val[ADELINE_MAX_PATH]; } pathCache[PATH_CACHE_MAX];
static int pathCacheN = 0;

/* ── Decoded PCM cache (most recently decoded OGG) ─────────────────────── */

static struct {
  char    path[ADELINE_MAX_PATH];
  short  *pcm;
  Uint32  numBytes;
  int     sample_rate;
  int     channels;
} oggCache = { "", NULL, 0, 0, 0 };

static void ClearOggCache(void) {
  if (oggCache.pcm) { free(oggCache.pcm); oggCache.pcm = NULL; }
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
  int         channels;
  int         sample_rate;
  char        oggPath[ADELINE_MAX_PATH];
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
    if (n <= 0) break;

    int chunkShorts = n * d->channels;
    SDL_PutAudioStreamData(stream, buf, chunkShorts * (int)sizeof(short));

    if (totalShorts + chunkShorts > capacity) {
      int newCap = (capacity == 0) ? 131072 : capacity * 2;
      while (newCap < totalShorts + chunkShorts) newCap *= 2;
      short *p = (short *)realloc(fullPcm, (size_t)newCap * sizeof(short));
      if (!p) { free(fullPcm); fullPcm = NULL; break; }
      fullPcm = p;
      capacity = newCap;
    }
    memcpy(fullPcm + totalShorts, buf, (size_t)chunkShorts * sizeof(short));
    totalShorts += chunkShorts;
  }

  stb_vorbis_close(d->vorbis);

  if (!SDL_GetAtomicInt(&decodeStop) && fullPcm && totalShorts > 0) {
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

/* ── Init / Stop ───────────────────────────────────────────────────────── */

static void StopStreamInternal(void) {
  StopDecodeThread();
  if (stream) {
    SDL_DestroyAudioStream(stream);
    stream = NULL;
  }
  streamPathName[0] = '\0';
  streamPaused = false;
}

void OpenStream() {
  StopDecodeThread();
  if (stream) {
    SDL_DestroyAudioStream(stream);
    stream = NULL;
  }
  streamPathName[0] = '\0';
  streamPaused = false;
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
    if (bsep && (!sep || bsep > sep)) sep = bsep;
#endif
    char *start = sep ? sep + 1 : oggPath;
    for (char *p = start; *p; p++) {
      if (*p >= 'A' && *p <= 'Z') *p += 32;
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
  if (f) { fclose(f); return true; }
  return false;
}

static const char *PathCacheGet(const char *key) {
  for (int i = 0; i < pathCacheN; i++)
    if (strcmp(pathCache[i].key, key) == 0)
      return pathCache[i].val;
  return NULL;
}

static void PathCachePut(const char *key, const char *val) {
  if (pathCacheN >= PATH_CACHE_MAX) return;
  strncpy(pathCache[pathCacheN].key, key, ADELINE_MAX_PATH - 1);
  pathCache[pathCacheN].key[ADELINE_MAX_PATH - 1] = '\0';
  strncpy(pathCache[pathCacheN].val, val, ADELINE_MAX_PATH - 1);
  pathCache[pathCacheN].val[ADELINE_MAX_PATH - 1] = '\0';
  pathCacheN++;
}

static bool ResolveOggPath(const char *wavPath, char *resolvedPath, int pathMax) {
  static const char *exts[] = { ".ogg", ".OGG" };
  for (int lower = 0; lower <= 1; lower++) {
    for (int e = 0; e < 2; e++) {
      BuildOggPath(wavPath, resolvedPath, pathMax, exts[e], lower != 0);
      if (FileExistsCheck(resolvedPath)) return true;
    }
  }
  /* Steam/GOG fallback: TADPCM<N> files renamed to track<N>.ogg */
  const char *sep = strrchr(wavPath, '/');
#ifdef _WIN32
  const char *bsep = strrchr(wavPath, '\\');
  if (bsep && (!sep || bsep > sep)) sep = bsep;
#endif
  const char *base = sep ? sep + 1 : wavPath;
  const char *dot  = strrchr(base, '.');
  if (!dot) dot = base + strlen(base);
  const char *p = dot;
  while (p > base && p[-1] >= '0' && p[-1] <= '9') p--;
  if (p < dot) {
    int n = atoi(p);
    if (n > 0 && n < 100) {
      int dirLen = (int)(base - wavPath);
      snprintf(resolvedPath, pathMax, "%.*strack%d.ogg", dirLen, wavPath, n);
      if (FileExistsCheck(resolvedPath)) return true;
      snprintf(resolvedPath, pathMax, "%.*sTrack%d.ogg", dirLen, wavPath, n);
      if (FileExistsCheck(resolvedPath)) return true;
    }
  }
  resolvedPath[0] = '\0';
  return false;
}

/* ── PlayOggFile: cache hit -> instant, cache miss -> stream decode ───── */

static bool PlayOggFile(const char *oggPath) {
  if (oggCache.pcm && strcmp(oggCache.path, oggPath) == 0) {
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = oggCache.sample_rate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = (Uint8)oggCache.channels;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) return false;
    SDL_PutAudioStreamData(stream, oggCache.pcm, oggCache.numBytes);
    return true;
  }

  int vorbisError = 0;
  stb_vorbis *v = stb_vorbis_open_filename(oggPath, &vorbisError, NULL);
  if (!v) return false;

  stb_vorbis_info info = stb_vorbis_get_info(v);
  if (info.channels <= 0 || info.sample_rate <= 0) {
    stb_vorbis_close(v);
    return false;
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

/* ── Public API ────────────────────────────────────────────────────────── */

void PlayStream(char *name) {
  if (!name || !name[0])
    return;

  StopStreamInternal();

  SDL_AudioSpec spec;
  Uint8 *wav_data = NULL;
  Uint32 wav_data_len = 0;

  if (SDL_LoadWAV(name, &spec, &wav_data, &wav_data_len)) {
    if (musicLogEnabled)
      SDL_Log("[MUSIC] PlayStream start WAV: %s", name);
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
      SDL_Log("SDL AIL: Could not open audio stream: %s", SDL_GetError());
      SDL_free(wav_data);
      return;
    }
    SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
    SDL_free(wav_data);
  } else {
    char resolved[ADELINE_MAX_PATH];
    const char *cached = PathCacheGet(name);
    if (cached) {
      strncpy(resolved, cached, sizeof(resolved) - 1);
      resolved[sizeof(resolved) - 1] = '\0';
    } else if (ResolveOggPath(name, resolved, (int)sizeof(resolved))) {
      PathCachePut(name, resolved);
    } else {
      SDL_Log("SDL AIL: Could not find '%s' or OGG fallbacks", name);
      return;
    }
    if (!PlayOggFile(resolved)) {
      SDL_Log("SDL AIL: Failed to decode '%s'", resolved);
      return;
    }
    if (musicLogEnabled)
      SDL_Log("[MUSIC] PlayStream start OGG: %s (from %s)", resolved, name);
  }

  {
    float gain = (streamVolume <= 0) ? 0.0f : (float)streamVolume / 127.0f;
    if (gain > 1.0f) gain = 1.0f;
    SDL_SetAudioStreamGain(stream, gain);
  }

  SDL_ResumeAudioStreamDevice(stream);

  strncpy(streamPathName, name, ADELINE_MAX_PATH - 1);
  streamPathName[ADELINE_MAX_PATH - 1] = '\0';
}

void ChangeVolumeStream(S32 volume) {
  streamVolume = volume;
  if (stream) {
    float gain = (volume <= 0) ? 0.0f : (float)volume / 127.0f;
    if (gain > 1.0f) gain = 1.0f;
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
    if (musicLogEnabled)
      SDL_Log("[MUSIC] PauseStream");
    SDL_PauseAudioStreamDevice(stream);
    streamPaused = true;
  }
}

void ResumeStream() {
  if (stream && streamPaused) {
    if (musicLogEnabled)
      SDL_Log("[MUSIC] ResumeStream");
    SDL_ResumeAudioStreamDevice(stream);
    streamPaused = false;
  }
}

S32 IsStreamPlaying() {
  if (!stream) return FALSE;
  if (streamPaused) return FALSE;
  if (SDL_GetAtomicInt(&decodeActive)) return TRUE;
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
