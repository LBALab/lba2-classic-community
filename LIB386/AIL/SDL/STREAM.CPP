#include "STREAM_SDL.H"

#include <SDL3/SDL.h>

#include "stb_vorbis.h"

#include <stdlib.h>
#include <string.h>

#ifndef ADELINE_MAX_PATH
#define ADELINE_MAX_PATH 512
#endif

static SDL_AudioStream *stream = NULL;
static char streamPathName[ADELINE_MAX_PATH] = "";
static bool streamPaused = false;

S32 streamVolume = 0;

void OpenStream() {
  /* State initialized on first PlayStream; ensure no stale stream. */
  if (stream) {
    SDL_DestroyAudioStream(stream);
    stream = NULL;
  }
  streamPathName[0] = '\0';
  streamPaused = false;
}

static void StopStreamInternal(void) {
  if (stream) {
    SDL_DestroyAudioStream(stream);
    stream = NULL;
  }
  streamPathName[0] = '\0';
  streamPaused = false;
}

/* Build path with extension replaced and optional filename case change.
   When lowerName is true the filename portion (after last separator) is
   lowercased.  ext should be ".ogg" or ".OGG". */
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

static bool TryPlayOgg(const char *oggPath) {
  int channels = 0, sample_rate = 0;
  short *pcm = NULL;
  int samplesPerChannel = stb_vorbis_decode_filename(oggPath, &channels, &sample_rate, &pcm);
  if (samplesPerChannel <= 0 || !pcm || channels <= 0 || sample_rate <= 0) {
    if (pcm) free(pcm);
    return false;
  }
  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.freq = sample_rate;
  spec.format = SDL_AUDIO_S16;
  spec.channels = (Uint8)channels;
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
  if (!stream) {
    free(pcm);
    return false;
  }
  Uint32 numBytes = (Uint32)(samplesPerChannel * channels * sizeof(short));
  SDL_PutAudioStreamData(stream, pcm, numBytes);
  free(pcm);
  return true;
}

void PlayStream(char *name) {
  if (!name || !name[0])
    return;

  StopStreamInternal();

  SDL_AudioSpec spec;
  Uint8 *wav_data = NULL;
  Uint32 wav_data_len = 0;

  if (SDL_LoadWAV(name, &spec, &wav_data, &wav_data_len)) {
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
      SDL_Log("SDL AIL: Could not open audio stream: %s", SDL_GetError());
      SDL_free(wav_data);
      return;
    }
    SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
    SDL_free(wav_data);
  } else {
    /* Try OGG with several case variations for case-sensitive filesystems. */
    char oggPath[ADELINE_MAX_PATH];
    bool found = false;
    static const char *exts[] = { ".ogg", ".OGG" };
    for (int lower = 0; lower <= 1 && !found; lower++) {
      for (int e = 0; e < 2 && !found; e++) {
        BuildOggPath(name, oggPath, (int)sizeof(oggPath), exts[e], lower != 0);
        found = TryPlayOgg(oggPath);
      }
    }
    if (!found) {
      SDL_Log("SDL AIL: Could not load WAV '%s' or OGG", name);
      return;
    }
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
  }
}

S32 GetVolumeStream() {
  return streamVolume;
}

void StopStream() {
  StopStreamInternal();
}

void PauseStream() {
  if (stream && !streamPaused) {
    SDL_PauseAudioStreamDevice(stream);
    streamPaused = true;
  }
}

void ResumeStream() {
  if (stream && streamPaused) {
    SDL_ResumeAudioStreamDevice(stream);
    streamPaused = false;
  }
}

S32 IsStreamPlaying() {
  if (!stream) return FALSE;
  if (streamPaused) return FALSE;
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
