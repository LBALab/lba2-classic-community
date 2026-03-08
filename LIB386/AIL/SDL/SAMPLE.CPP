#include "SAMPLE_SDL.H"

#include <SDL3/SDL.h>

#include <stdlib.h>
#include <string.h>

/*
 * PlaySample() accepts RIFF WAV with PCM (tag=1) or IMA ADPCM (tag=17),
 * Creative Voice (.voc), or raw 8-bit unsigned PCM.
 */
#define SAMPLE_RATE      22050
#define NUM_VOICES       12
#define MIX_CHUNK_FRAMES 512
#define PITCHBEND_NORMAL 4096

/* ── IMA ADPCM tables and decoder ────────────────────────────────────────── */

static const S16 imaStepTable[89] = {
  7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
  73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,
  449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,
  2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,
  7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
  24623,27086,29794,32767
};

static const int imaIndexTable[16] = {
  -1,-1,-1,-1, 2,4,6,8, -1,-1,-1,-1, 2,4,6,8
};

static S16 *DecodeImaAdpcm(const U8 *adpcmData, S32 adpcmSize,
                           S32 blockAlign, S32 *outSamples) {
  if (blockAlign < 8 || adpcmSize < blockAlign) { *outSamples = 0; return NULL; }
  S32 samplesPerBlock = (blockAlign - 4) * 2 + 1;
  S32 numBlocks = adpcmSize / blockAlign;
  S32 remainder = adpcmSize % blockAlign;
  S32 totalSamples = numBlocks * samplesPerBlock;
  if (remainder >= 8)
    totalSamples += (remainder - 4) * 2 + 1;
  S16 *pcm = (S16 *)malloc((size_t)totalSamples * sizeof(S16));
  if (!pcm) { *outSamples = 0; return NULL; }
  S32 outIdx = 0;
  const U8 *src = adpcmData;
  S32 bytesLeft = adpcmSize;
  while (bytesLeft >= 8) {
    S32 blockSize = (bytesLeft >= blockAlign) ? blockAlign : bytesLeft;
    S16 predictor = (S16)(src[0] | (src[1] << 8));
    int stepIndex = src[2];
    if (stepIndex < 0) stepIndex = 0;
    if (stepIndex > 88) stepIndex = 88;
    pcm[outIdx++] = predictor;
    for (S32 i = 4; i < blockSize; i++) {
      for (int nibbleIdx = 0; nibbleIdx < 2 && outIdx < totalSamples; nibbleIdx++) {
        int nibble = (nibbleIdx == 0) ? (src[i] & 0x0F) : ((src[i] >> 4) & 0x0F);
        int step = imaStepTable[stepIndex];
        int diff = step >> 3;
        if (nibble & 1) diff += step >> 2;
        if (nibble & 2) diff += step >> 1;
        if (nibble & 4) diff += step;
        if (nibble & 8) diff = -diff;
        int sample = (int)predictor + diff;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        predictor = (S16)sample;
        pcm[outIdx++] = predictor;
        stepIndex += imaIndexTable[nibble];
        if (stepIndex < 0) stepIndex = 0;
        if (stepIndex > 88) stepIndex = 88;
      }
    }
    src += blockSize;
    bytesLeft -= blockSize;
  }
  *outSamples = outIdx;
  return pcm;
}

/* ── VOC parser ──────────────────────────────────────────────────────────── */

#define VOC_HEADER_MAGIC "reative Voice File\x1A"

static int ParseVocIfPresent(const U8 *buffer, S32 sizeBytes,
                             const U8 **data, S32 *payloadSize, S32 *outHz, int *outIs8bit) {
  if (sizeBytes < 26) return 0;
  if (buffer[0] != 'C' && buffer[0] != 'W' && buffer[0] != 0) return 0;
  if (memcmp(buffer + 1, VOC_HEADER_MAGIC, 19) != 0) return 0;
  *outHz = SAMPLE_RATE;
  *outIs8bit = 1;
  S32 pos = 26;
  S32 extendedRate = 0;
  while (pos + 4 <= sizeBytes) {
    U8 type = buffer[pos];
    S32 blockSize = (S32)(buffer[pos + 1] | (buffer[pos + 2] << 8) | (buffer[pos + 3] << 16));
    if (type == 0x00) break;
    if (pos + 4 + blockSize > sizeBytes) break;
    if (type == 0x08 && blockSize >= 4) {
      S32 freqDiv = (S32)(buffer[pos + 4] | (buffer[pos + 5] << 8));
      int ch = (int)(buffer[pos + 7]) + 1;
      if (ch < 1 || ch > 8) ch = 1;
      if (freqDiv != 65535 && ch > 0)
        extendedRate = 256000000 / ((65536 - freqDiv) * ch);
    }
    if (type == 0x01 && blockSize >= 2) {
      U8 freqDiv = buffer[pos + 4];
      U8 codec = buffer[pos + 5];
      if (extendedRate > 0)
        *outHz = extendedRate;
      else if (freqDiv != 255)
        *outHz = 1000000 / (256 - (S32)freqDiv);
      if (*outHz <= 0 || *outHz > 192000) *outHz = SAMPLE_RATE;
      *data = buffer + pos + 6;
      *payloadSize = blockSize - 2;
      *outIs8bit = (codec == 0) ? 1 : 0;
      return 1;
    }
    pos += 4 + blockSize;
  }
  return 0;
}

/* ── WAV parser ──────────────────────────────────────────────────────────── */

static void SkipWavHeaderIfPresent(const U8 *buffer, S32 sizeBytes,
                                   const U8 **data, S32 *payloadSize, int *hadWav,
                                   S32 *outWavHz, S32 *outWavChannels, S32 *outWavBits,
                                   S32 *outFmtTag, S32 *outBlockAlign) {
  *data = (const U8 *)buffer;
  *payloadSize = sizeBytes;
  *hadWav = 0;
  if (outWavHz) *outWavHz = SAMPLE_RATE;
  if (outWavChannels) *outWavChannels = 1;
  if (outWavBits) *outWavBits = 16;
  if (outFmtTag) *outFmtTag = 1;
  if (outBlockAlign) *outBlockAlign = 0;
  if (sizeBytes < 12) return;
  if ((memcmp(buffer, "RIFF", 4) != 0 || memcmp(buffer + 8, "WAVE", 4) != 0) &&
      (buffer[1] != 'I' || buffer[2] != 'F' || buffer[3] != 'F' || memcmp(buffer + 8, "WAVE", 4) != 0))
    return;
  S32 pos = 12;
  S32 wavHz = SAMPLE_RATE, wavChannels = 1, wavBits = 16, fmtTag = 1, blkAlign = 0;
  while (pos + 8 <= sizeBytes) {
    S32 chunkSize = (S32)(buffer[pos + 4] | (buffer[pos + 5] << 8) |
                       (buffer[pos + 6] << 16) | (buffer[pos + 7] << 24));
    if (memcmp(buffer + pos, "fmt ", 4) == 0 && chunkSize >= 16 && pos + 8 + 16 <= sizeBytes) {
      const U8 *f = buffer + pos + 8;
      fmtTag = (S32)(f[0] | (f[1] << 8));
      wavChannels = (S32)(f[2] | (f[3] << 8));
      wavHz = (S32)(f[4] | (f[5] << 8) | (f[6] << 16) | (f[7] << 24));
      blkAlign = (S32)(f[12] | (f[13] << 8));
      wavBits = (S32)(f[14] | (f[15] << 8));
      if (wavHz <= 0 || wavHz > 192000) wavHz = SAMPLE_RATE;
      if (wavChannels <= 0 || wavChannels > 8) wavChannels = 1;
      if (fmtTag == 1) {
        if (blkAlign > 0 && wavChannels > 0)
          wavBits = (blkAlign * 8) / wavChannels;
        if (wavBits != 8 && wavBits != 16) wavBits = 16;
      }
    }
    if (memcmp(buffer + pos, "data", 4) == 0 && chunkSize > 0 && pos + 8 + chunkSize <= sizeBytes) {
      *data = buffer + pos + 8;
      *payloadSize = chunkSize;
      *hadWav = 1;
      if (outWavHz) *outWavHz = wavHz;
      if (outWavChannels) *outWavChannels = wavChannels;
      if (outWavBits) *outWavBits = wavBits;
      if (outFmtTag) *outFmtTag = fmtTag;
      if (outBlockAlign) *outBlockAlign = blkAlign;
      return;
    }
    pos += 8 + ((chunkSize + 1) & ~1);
  }
}

/* ── Voices and mixer ────────────────────────────────────────────────────── */

struct Voice {
  const U8 *data;
  S32 sizeBytes;
  bool is8bit;
  S32 sourceRateHz;
  S32 sourceChannels;
  float posSamples;
  float stepSamples;
  S32 volume;
  S32 pan;
  S32 repeat;
  S32 repeatCount;
  U32 handle;
  bool active;
  void *ownedBuf;
};

static SDL_AudioStream *sampleStream = NULL;
static Voice voices[NUM_VOICES];
static U32 nextHandle = 0x1000000;
static S32 samplesPaused = 0;
static S32 inverseStereo = 0;

S32 Sample_Driver_Enabled = FALSE;
S32 UseWaveMixer = FALSE;
S32 sampleVolume = 0;

static void FreeVoiceBuf(Voice *V) {
  if (V->ownedBuf) { free(V->ownedBuf); V->ownedBuf = NULL; }
}

static int VoiceIndexFromHandle(U32 sample) {
  if (sample < 0x1000000) {
    for (int i = 0; i < NUM_VOICES; i++) {
      if (voices[i].active && ((voices[i].handle >> 8) & 0xFFFF) == sample)
        return i;
    }
  } else {
    int slot = sample & 0xFF;
    if (slot < NUM_VOICES && voices[slot].active && voices[slot].handle == sample)
      return slot;
  }
  return -1;
}

static void MixVoices(S16 *out, int numFrames) {
  memset(out, 0, (size_t)numFrames * 2 * sizeof(S16));
  for (int v = 0; v < NUM_VOICES; v++) {
    Voice *V = &voices[v];
    if (!V->active || !V->data) continue;
    float pos = V->posSamples;
    float step = V->stepSamples;
    int maxSamples = V->is8bit ? V->sizeBytes : (V->sizeBytes / 2);
    const int fadeLen = 64;
    float leftGain = (V->pan <= 64) ? 1.0f : (127 - V->pan) / 63.0f;
    float rightGain = (V->pan >= 64) ? 1.0f : (float)V->pan / 64.0f;
    if (inverseStereo) { float t = leftGain; leftGain = rightGain; rightGain = t; }
    float vol = (float)(sampleVolume * V->volume) / (127.0f * 127.0f);
    if (vol > 1.0f) vol = 1.0f;
    leftGain *= vol;
    rightGain *= vol;
    for (int i = 0; i < numFrames; i++) {
      int idx = (int)pos;
      if (idx >= maxSamples) {
        if (V->repeat == 0) { pos = 0; idx = 0; }
        else {
          V->repeatCount++;
          if (V->repeatCount >= V->repeat) {
            V->active = false;
            FreeVoiceBuf(V);
            break;
          }
          pos = 0; idx = 0;
        }
      }
      float frac = pos - (float)idx;
      int idx1 = (idx + 1 < maxSamples) ? idx + 1 : idx;
      float s;
      if (V->is8bit) {
        float s0 = (float)((int)V->data[idx] - 128) * 256.0f;
        float s1 = (float)((int)V->data[idx1] - 128) * 256.0f;
        s = s0 + frac * (s1 - s0);
      } else {
        const S16 *src = (const S16 *)V->data;
        float s0 = (float)src[idx];
        float s1 = (float)src[idx1];
        s = s0 + frac * (s1 - s0);
      }
      int samplesLeft = maxSamples - idx;
      if (V->repeat != 0 && samplesLeft < fadeLen) {
        float fade = (float)samplesLeft / (float)fadeLen;
        s *= fade;
      }
      out[i * 2]     += (S16)(s * leftGain);
      out[i * 2 + 1] += (S16)(s * rightGain);
      pos += step;
    }
    V->posSamples = pos;
  }
}

/* ── SDL3 audio callback ─────────────────────────────────────────────────── */

static S16 mixChunkBuf[MIX_CHUNK_FRAMES * 2];

static void SDLCALL SampleStreamCallback(void *userdata, SDL_AudioStream *stream,
                                          int additional_amount, int total_amount) {
  (void)userdata;
  (void)total_amount;
  if (samplesPaused || additional_amount <= 0) return;
  int bytesLeft = additional_amount;
  while (bytesLeft > 0) {
    int frames = bytesLeft / (2 * (int)sizeof(S16));
    if (frames > MIX_CHUNK_FRAMES) frames = MIX_CHUNK_FRAMES;
    if (frames <= 0) break;
    MixVoices(mixChunkBuf, frames);
    SDL_PutAudioStreamData(stream, mixChunkBuf, frames * 2 * (int)sizeof(S16));
    bytesLeft -= frames * 2 * (int)sizeof(S16);
  }
}

/* ── Driver init / control ───────────────────────────────────────────────── */

S32 InitSampleDriver(char *driver_name) {
  (void)driver_name;
  if (Sample_Driver_Enabled) return TRUE;
  memset(voices, 0, sizeof(voices));
  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.freq = SAMPLE_RATE;
  spec.format = SDL_AUDIO_S16;
  spec.channels = 2;
  sampleStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, SampleStreamCallback, NULL);
  if (!sampleStream) {
    SDL_Log("SDL AIL: Could not open sample stream: %s", SDL_GetError());
    return FALSE;
  }
  SDL_ResumeAudioStreamDevice(sampleStream);
  Sample_Driver_Enabled = TRUE;
  return TRUE;
}

void SetMasterVolumeSample(S32 volume) {
  sampleVolume = volume;
}

S32 FadeOutSamples(S32 delay) {
  (void)delay;
  return 0;
}

S32 FadeInSamples(S32 delay) {
  (void)delay;
  return delay;
}

void InverseStereoSample(S32 inverse) {
  inverseStereo = (inverse != 0);
}

/* ── PlaySample ──────────────────────────────────────────────────────────── */

U32 PlaySample(void *buffer, S32 sizeBytes, U32 userhandle, S32 pitchbend,
               S32 repeat, S32 volume, S32 pan) {
  if (!Sample_Driver_Enabled || !buffer || sizeBytes <= 0) return 0xFFFFFFFF;
  const U8 *data = NULL;
  S32 payloadSize = 0;
  int hadVoc = 0;
  S32 vocHz = SAMPLE_RATE;
  int vocIs8bit = 1;
  int hadWav = 0;
  S32 wavHz = SAMPLE_RATE, wavChannels = 1, wavBits = 16, fmtTag = 1, blkAlign = 0;
  hadVoc = ParseVocIfPresent((const U8 *)buffer, sizeBytes, &data, &payloadSize, &vocHz, &vocIs8bit);
  if (!hadVoc)
    SkipWavHeaderIfPresent((const U8 *)buffer, sizeBytes, &data, &payloadSize,
                           &hadWav, &wavHz, &wavChannels, &wavBits, &fmtTag, &blkAlign);
  if (!data || payloadSize <= 0) return 0xFFFFFFFF;
  if (pitchbend <= 0) pitchbend = PITCHBEND_NORMAL;
  int slot = -1;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].active) { slot = i; break; }
  }
  if (slot < 0) {
    slot = 0;
    voices[0].active = false;
  }
  Voice *V = &voices[slot];
  FreeVoiceBuf(V);

  if (hadWav && fmtTag == 17 && blkAlign > 0) {
    S32 decodedSamples = 0;
    S16 *pcm = DecodeImaAdpcm(data, payloadSize, blkAlign, &decodedSamples);
    if (!pcm || decodedSamples <= 0) return 0xFFFFFFFF;
    V->ownedBuf = pcm;
    V->data = (const U8 *)pcm;
    V->sizeBytes = decodedSamples * 2;
    V->is8bit = false;
    V->sourceRateHz = wavHz;
    V->sourceChannels = wavChannels;
  } else {
    void *copy = malloc((size_t)payloadSize);
    if (!copy) return 0xFFFFFFFF;
    memcpy(copy, data, (size_t)payloadSize);
    V->ownedBuf = copy;
    V->data = (const U8 *)copy;
    V->sizeBytes = payloadSize;
    V->is8bit = hadVoc ? vocIs8bit : (hadWav ? (wavBits == 8) : 1);
    V->sourceRateHz = hadVoc ? vocHz : (hadWav ? wavHz : SAMPLE_RATE);
    V->sourceChannels = hadWav ? wavChannels : 1;
  }

  V->posSamples = 0;
  {
    float rateRatio = (float)V->sourceRateHz / (float)SAMPLE_RATE;
    int ch = (V->sourceChannels >= 2) ? 2 : 1;
    V->stepSamples = rateRatio * (float)ch * (float)pitchbend / (float)PITCHBEND_NORMAL;
  }
  V->volume = (volume > 127) ? 127 : volume;
  V->pan = (pan > 127) ? 127 : pan;
  V->repeat = repeat;
  V->repeatCount = 0;
  V->handle = nextHandle | ((userhandle & 0xFFFF) << 8) | (slot & 0xFF);
  nextHandle += 0x1000000;
  if (nextHandle == 0) nextHandle = 0x1000000;
  V->active = true;
  return V->handle;
}

void ChangePitchbendSample(U32 sample, S32 pitchbend) {
  int i = VoiceIndexFromHandle(sample);
  if (i >= 0 && pitchbend > 0) {
    Voice *V = &voices[i];
    float rateRatio = (float)V->sourceRateHz / (float)SAMPLE_RATE;
    int ch = (V->sourceChannels >= 2) ? 2 : 1;
    V->stepSamples = rateRatio * (float)ch * (float)pitchbend / (float)PITCHBEND_NORMAL;
  }
}

void ChangeVolumePanSample(U32 sample, S32 volume, S32 pan) {
  int i = VoiceIndexFromHandle(sample);
  if (i >= 0) {
    voices[i].volume = (volume > 127) ? 127 : volume;
    voices[i].pan = (pan > 127) ? 127 : pan;
  }
}

void StopOneSample(U32 sample) {
  int i = VoiceIndexFromHandle(sample);
  if (i >= 0) {
    FreeVoiceBuf(&voices[i]);
    voices[i].active = false;
  }
}

void StopSamples() {
  for (int i = 0; i < NUM_VOICES; i++) {
    FreeVoiceBuf(&voices[i]);
    voices[i].active = false;
  }
}

void PauseSamples() {
  samplesPaused++;
  if (samplesPaused == 1 && sampleStream)
    SDL_PauseAudioStreamDevice(sampleStream);
}

void ResumeSamples() {
  if (samplesPaused <= 0) return;
  samplesPaused--;
  if (samplesPaused == 0 && sampleStream)
    SDL_ResumeAudioStreamDevice(sampleStream);
}

U32 IsSamplePlaying(U32 sample) {
  int i = VoiceIndexFromHandle(sample);
  return (i >= 0) ? sample : (U32)FALSE;
}
