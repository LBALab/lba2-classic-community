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

/* Build path with extension replaced by .ogg (caller provides buffer of size pathMax). */
static void PathToOgg(const char *wavPath, char *oggPath, int pathMax) {
  strncpy(oggPath, wavPath, pathMax - 1);
  oggPath[pathMax - 1] = '\0';
  char *dot = strrchr(oggPath, '.');
  if (dot) {
    strcpy(dot, ".ogg");
  } else {
    int len = (int)strlen(oggPath);
    if (len + 5 <= pathMax)
      strcpy(oggPath + len, ".ogg");
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
    /* Try .ogg (e.g. Steam ships OGG instead of WAV). */
    char oggPath[ADELINE_MAX_PATH];
    PathToOgg(name, oggPath, (int)sizeof(oggPath));
    if (!TryPlayOgg(oggPath)) {
      SDL_Log("SDL AIL: Could not load WAV '%s' or OGG '%s'", name, oggPath);
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
  /* Preserve a paused stream so ResumeStream can unpause it later.
     PlayStream calls StopStreamInternal directly, so new music still works. */
  if (streamPaused)
    return;
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
  return (stream != NULL) ? TRUE : FALSE;
}

char *StreamName() {
  return streamPathName;
}
