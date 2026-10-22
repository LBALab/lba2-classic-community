// Host tests for where the SDL music stream resumes after a pause
// (LIB386/AIL/SDL/STREAM.CPP), driven by a virtual clock.
//
// The real STREAM.CPP is compiled against a fake audio device (the AILAudio_*
// seam in LIB386/AIL/SDL/AUDIO_DEVICE.H). The fake keeps a byte queue per
// stream and consumes it only when the test calls Advance(ms), so playback
// time is exact and nothing depends on a sound card or the wall clock. The
// frame a stream was reopened at is read back from how many bytes it was
// handed.
//
// The menu round trip is the one the game makes on Escape: PauseMusic parks the
// track, the main menu plays its theme, StopMusic ends it on the way back, and
// ResumeMusic restores the parked track.
//
// Every case runs on three source setups. All of them feed the device through
// the one bounded producer, but each reopens the track differently:
//   wav       - track and menu theme are WAV (a disc or extracted install):
//               the WAV reader opens at a frame offset.
//   ogg       - OGG track, WAV menu theme: a restore seeks the decoder.
//   ogg-ogg   - both OGG, as in the Steam and GOG Classic releases.
// WAV loading is faked (patterned PCM of a known length for any "*.wav" name);
// OGG decoding is real, from tests/music/fixtures/silence-10s.ogg, copied into
// the build tree as track.ogg and menu.ogg. CD audio is real too, read from a
// generated two-track bin+cue (see the CD tests at the end).
#include "../sdl_main_ready.h"
#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/AUDIO_DEVICE.H"
#include "../../LIB386/AIL/SDL/WAV_STREAM.H"
#include "../../LIB386/AIL/SDL/stb_vorbis.h"

#include <AIL/STREAM.H>
#include <SYSTEM/CRASH.H>

#include <math.h>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#define MakeDir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MakeDir(path) mkdir(path, 0755)
#endif

extern "C" unsigned int stb_vorbis_stream_length_in_samples(stb_vorbis *f);

/* Playback is tested without installing the process-wide crash handler. */
void Crash_PrepareThread(void) {}

// ── Fake device ─────────────────────────────────────────────────────────────

static const int RATE = 44100;
static const int BYTES_PER_FRAME = 4; // 16-bit stereo, for WAV and the fixture alike
static const int WAV_TRACK_MS = 10000;
static const int WAV_MENU_MS = 5000;

struct FakeStream {
    int bytesPerFrame;
    int queued;
    int pushed;
    int puts; // one per chunk from the producer
    int maxQueued;
    int consumed;
    int openedFrame;
    bool wav;
    bool paused;
    bool flushed;
    bool settled; // WaitDecoded has already waited for this stream's decode thread
};

// Producers push from their own thread, so the fake locks.
static std::mutex g_lock;
static FakeStream *g_live = NULL; // the stream STREAM.CPP currently owns
static int g_opens = 0;
static bool g_openWav = false;
static int g_openFrame = 0;
static int g_sourceRate = RATE;
static int g_queueQueries = 0;
static std::vector<Uint8> *g_capturePCM = NULL;

static FakeStream *Fake(AILAudioStream *s) { return reinterpret_cast<FakeStream *>(s); }

bool AILAudio_Initialized(void) { return true; }

AILAudioStream *AILAudio_Open(const SDL_AudioSpec *spec, AILAudioCallback, void *) {
    std::lock_guard<std::mutex> hold(g_lock);
    FakeStream *s = new FakeStream();
    s->bytesPerFrame = (int)(SDL_AUDIO_BITSIZE(spec->format) / 8) * spec->channels;
    s->queued = 0;
    s->pushed = 0;
    s->puts = 0;
    s->maxQueued = 0;
    s->consumed = 0;
    s->openedFrame = g_openFrame;
    s->wav = g_openWav;
    s->paused = true; // SDL device streams open paused; FinishStream resumes them
    s->flushed = false;
    s->settled = false;
    g_live = s;
    g_opens++;
    return reinterpret_cast<AILAudioStream *>(s);
}

void AILAudio_Destroy(AILAudioStream *stream) {
    std::lock_guard<std::mutex> hold(g_lock);
    FakeStream *s = Fake(stream);
    if (g_live == s)
        g_live = NULL;
    delete s;
}

void AILAudio_Lock(AILAudioStream *) {}
void AILAudio_Unlock(AILAudioStream *) {}

bool AILAudio_Put(AILAudioStream *stream, const void *data, int bytes) {
    std::lock_guard<std::mutex> hold(g_lock);
    Fake(stream)->queued += bytes;
    Fake(stream)->pushed += bytes;
    Fake(stream)->puts++;
    if (Fake(stream)->queued > Fake(stream)->maxQueued)
        Fake(stream)->maxQueued = Fake(stream)->queued;
    if (g_capturePCM)
        g_capturePCM->insert(g_capturePCM->end(), (const Uint8 *)data, (const Uint8 *)data + bytes);
    return true;
}

bool AILAudio_Flush(AILAudioStream *stream) {
    std::lock_guard<std::mutex> hold(g_lock);
    Fake(stream)->flushed = true;
    return true;
}

int AILAudio_Queued(AILAudioStream *stream) {
    std::lock_guard<std::mutex> hold(g_lock);
    ++g_queueQueries;
    return Fake(stream)->queued;
}

bool AILAudio_Gain(AILAudioStream *, float) { return true; }

bool AILAudio_Resume(AILAudioStream *stream) {
    std::lock_guard<std::mutex> hold(g_lock);
    Fake(stream)->paused = false;
    return true;
}

bool AILAudio_Pause(AILAudioStream *stream) {
    std::lock_guard<std::mutex> hold(g_lock);
    Fake(stream)->paused = true;
    return true;
}

static int MsToBytes(int ms) { return (int)((long long)ms * RATE / 1000) * BYTES_PER_FRAME; }

// ── Source setups ───────────────────────────────────────────────────────────

enum Setup { SETUP_WAV,
             SETUP_OGG,
             SETUP_OGG_OGG };
static Setup g_setup = SETUP_WAV;
static const char *SetupName() {
    return g_setup == SETUP_WAV ? "wav" : g_setup == SETUP_OGG ? "ogg"
                                                               : "ogg-ogg";
}

static bool TrackIsOgg() { return g_setup != SETUP_WAV; }
static bool MenuIsOgg() { return g_setup == SETUP_OGG_OGG; }

// A WAV name loads as PCM unless this setup has an OGG for it, in which
// case it fails, as on a Classic install, and STREAM.CPP falls back to the .ogg.
bool AILAudio_LoadWAV(const char *path, SDL_AudioSpec *spec, Uint8 **data, Uint32 *bytes) {
    size_t n = strlen(path);
    if (n < 4 || strcmp(path + n - 4, ".wav") != 0)
        return false;
    bool menu = strstr(path, "menu") != NULL;
    if (menu ? MenuIsOgg() : TrackIsOgg())
        return false;
    spec->freq = g_sourceRate;
    spec->format = AIL_AUDIO_S16;
    spec->channels = 2;
    *bytes = (Uint32)MsToBytes(menu ? WAV_MENU_MS : WAV_TRACK_MS);
    *data = (Uint8 *)SDL_calloc(1, *bytes);
    return *data != NULL;
}

bool AILAudio_LoadWAVMemory(const void *, int, SDL_AudioSpec *, Uint8 **, Uint32 *) { return false; }

struct AILWavSource {
    SDL_AudioSpec spec;
    Uint32 remaining;
    Uint32 position;
};

static Uint8 PCMByte(Uint32 offset) {
    return (Uint8)((offset * 37u) ^ (offset >> 8));
}

AILWavSource *AILWav_Open(const char *path, bool image, S32 frame, bool *recognized) {
    *recognized = false;
    g_openWav = false;
    if (image)
        return NULL;
    AILWavSource *source = new AILWavSource();
    Uint8 *pcm = NULL;
    if (!AILAudio_LoadWAV(path, &source->spec, &pcm, &source->remaining)) {
        delete source;
        return NULL;
    }
    SDL_free(pcm);
    Uint32 skip = (Uint32)frame * BYTES_PER_FRAME;
    source->position = skip;
    source->remaining -= skip < source->remaining ? skip : source->remaining;
    *recognized = true;
    g_openWav = true;
    g_openFrame = frame;
    return source;
}

int AILWav_Read(AILWavSource *source, SDL_AudioSpec *spec, Uint8 **pcm, Uint32 *bytes) {
    *pcm = NULL;
    *bytes = 0;
    if (!source->remaining)
        return 0;
    *spec = source->spec;
    *bytes = source->remaining < 8192 ? source->remaining : 8192;
    *pcm = (Uint8 *)SDL_calloc(1, *bytes);
    if (*pcm) {
        for (Uint32 i = 0; i < *bytes; ++i)
            (*pcm)[i] = PCMByte(source->position + i);
        source->position += *bytes;
    }
    source->remaining -= *bytes;
    return *pcm ? 1 : -1;
}

void AILWav_Close(AILWavSource *source) { delete source; }

// ── Clock ───────────────────────────────────────────────────────────────────

// Wait for the producer to fill the queue or finish the track, so the virtual
// clock never races it. It parks at its queue bound until the clock advances.
static void WaitDecoded() {
    for (int i = 0; i < 5000; i++) {
        {
            std::lock_guard<std::mutex> hold(g_lock);
            if (!g_live || g_live->settled)
                return;
            if (!g_live->flushed && AILStream_TestProducerWaiting())
                return;
            if (g_live->flushed) {
                g_live->settled = true;
                return;
            }
        }
        SDL_Delay(1);
    }
}

// The device plays `ms` of whatever is live and unpaused.
static void Advance(int ms) {
    int left = MsToBytes(ms);
    while (left > 0) {
        WaitDecoded();
        std::lock_guard<std::mutex> hold(g_lock);
        if (!g_live || g_live->paused || !g_live->queued)
            break;
        int count = left < g_live->queued ? left : g_live->queued;
        g_live->queued -= count;
        g_live->consumed += count;
        left -= count;
        AILStream_TestWakeProducer(); // the producer sleeps in real time; this clock is virtual
    }
    WaitDecoded();
}

// ── Helpers ─────────────────────────────────────────────────────────────────

static char g_track[] = TEST_TMP_DIR "/music/track.wav";
static char g_menu[] = TEST_TMP_DIR "/music/menu.wav";
static int g_trackBytes = 0;          // the whole track as the device receives it
static int g_oggBytes = 0;            // the silent fixture's decoded length
static std::vector<Uint8> g_chirpPcm; // the chirp fixture decoded straight through
static char g_chirp[] = TEST_TMP_DIR "/music/chirp.wav";

// Where the live stream was opened, in ms. The fake WAV reader records the
// frame it was asked for; OGG and CD audio report it through the test hook.
static int OpenedAtMs() {
    WaitDecoded();
    std::lock_guard<std::mutex> hold(g_lock);
    int frames = g_live->wav ? g_live->openedFrame : AILStream_TestBaseFrame();
    return (int)((long long)frames * 1000 / RATE);
}

static int TrackMs() { return (int)((long long)g_trackBytes / BYTES_PER_FRAME * 1000 / RATE); }

// The frame asked of the source. (A Vorbis seek then lands on the start of the
// frame holding it, up to one long block early, which this does not see.)
static void AssertOpenedAt(int expectMs) {
    int got = OpenedAtMs();
    if (got != expectMs) {
        printf("    [%s] opened at %d ms, expected %d ms\n", SetupName(), got, expectMs);
        ASSERT_TRUE(!"stream opened at the wrong place");
    } else {
        ASSERT_TRUE(true);
    }
}

// Escape -> main menu -> back to the game.
static void MenuRoundTrip() {
    PauseStream();
    PlayStream(g_menu);
    Advance(1000); // also waits for an OGG menu theme to decode, evicting the track
    StopStream();
    ResumeStream();
}

static void Reset() {
    StopStream();
    OpenStream(); // clears any parked position
    g_opens = 0;
}

// Start the track and learn its length from the first full push.
static void StartTrack() {
    PlayStream(g_track);
    WaitDecoded();
    std::lock_guard<std::mutex> hold(g_lock);
    g_trackBytes = g_live ? (g_live->wav ? MsToBytes(WAV_TRACK_MS) : g_oggBytes) : 0;
}

// ── Tests ───────────────────────────────────────────────────────────────────

static void test_wav_full_queue_wait_and_stop() {
    g_sourceRate = 8000;
    for (int round = 0; round < 3; ++round) {
        Reset();
        StartTrack();
        PauseStream();
        WaitDecoded();
        int queries, queued;
        {
            std::lock_guard<std::mutex> hold(g_lock);
            queries = g_queueQueries;
            queued = g_live->queued;
        }
        Uint64 started = AIL_Ticks();
        SDL_Delay(50);
        Uint64 elapsed = AIL_Ticks() - started;
        {
            std::lock_guard<std::mutex> hold(g_lock);
            ASSERT_EQ_INT(queued, g_live->queued);
            ASSERT_TRUE(queued <= 65536);
            ASSERT_TRUE(g_queueQueries - queries <= (int)(elapsed / 250) + 2);
        }
        started = AIL_Ticks();
        StopStream();
        ASSERT_TRUE(AIL_Ticks() - started < 150);
        ASSERT_TRUE(!IsStreamPlaying());
        ASSERT_TRUE(g_live == NULL);
    }
    g_sourceRate = RATE;
}

static void test_wav_full_track_pcm_is_exact() {
    Reset();
    std::vector<Uint8> actual;
    g_capturePCM = &actual;
    StartTrack();
    Advance(WAV_TRACK_MS);
    {
        std::lock_guard<std::mutex> hold(g_lock);
        ASSERT_TRUE(g_live->flushed);
    }
    StopStream();
    g_capturePCM = NULL;
    std::vector<Uint8> expected(MsToBytes(WAV_TRACK_MS));
    for (size_t i = 0; i < expected.size(); ++i)
        expected[i] = PCMByte((Uint32)i);
    ASSERT_EQ_INT((int)expected.size(), (int)actual.size());
    if (expected.size() == actual.size())
        ASSERT_MEM_EQ(&expected[0], &actual[0], expected.size());
}

// Control: one trip mid-track resumes where it left off (worked before the fix).
static void test_one_menu_trip_resumes_in_place() {
    Reset();
    StartTrack();
    ASSERT_TRUE(g_trackBytes > MsToBytes(9000));
    Advance(4000);
    MenuRoundTrip();
    ASSERT_TRUE(g_live != NULL);
    ASSERT_TRUE(strcmp(StreamName(), g_track) == 0);
    AssertOpenedAt(4000);
    ASSERT_TRUE(g_live->puts > 1); // through the producer, never one whole-track push
}

// Two trips in one track: the second resume continues from the first.
// Before the fix the restored stream counted from its seek point, so the second
// park saved 2 s instead of 6 s and the track jumped back.
static void test_second_menu_trip_resumes_in_place() {
    Reset();
    StartTrack();
    Advance(4000);
    MenuRoundTrip();
    int first = OpenedAtMs();
    Advance(2000);
    MenuRoundTrip();
    ASSERT_TRUE(g_live != NULL);
    AssertOpenedAt(first + 2000);
}

// #427: a track that played out after an earlier trip must stay silent.
// Before the fix the spent stream was parked at "length minus first seek"
// (6 s here) and the menu return replayed it from there, mid-track.
static void test_finished_track_after_a_trip_stays_silent() {
    Reset();
    StartTrack();
    Advance(4000);
    MenuRoundTrip();
    Advance(TrackMs()); // past the end; nothing polls IsStreamPlaying, as after the queue drains
    int opensBefore = g_opens;
    MenuRoundTrip();
    ASSERT_EQ_INT(opensBefore + 1, g_opens); // the menu theme only
    ASSERT_TRUE(g_live == NULL);
    ASSERT_EQ_INT(0, IsStreamPlaying());
}

// A track that played out with no trip before also stays silent.
static void test_finished_track_stays_silent() {
    Reset();
    StartTrack();
    Advance(TrackMs() + 1000);
    int opensBefore = g_opens;
    MenuRoundTrip();
    ASSERT_EQ_INT(opensBefore + 1, g_opens);
    ASSERT_TRUE(g_live == NULL);
}

// Control: dialogue pauses and resumes the same stream without reopening it.
static void test_dialogue_pause_unpauses_in_place() {
    Reset();
    StartTrack();
    Advance(3000);
    FakeStream *before = g_live;
    PauseStream();
    ASSERT_TRUE(g_live->paused);
    Advance(5000); // paused: the device plays nothing
    ResumeStream();
    ASSERT_TRUE(g_live == before);
    ASSERT_TRUE(!g_live->paused);
    ASSERT_EQ_INT(1, g_opens);
    ASSERT_EQ_INT(MsToBytes(3000), g_live->consumed);
}

// Escape parks the track, then the player loads a game from the menu, which
// stops the music without resuming it. The loaded scene's own track plays out,
// and a dialogue pauses and resumes. The old game's track must not come back.
// (The menu file stands in for the loaded scene's track.)
static void test_load_from_menu_forgets_the_old_track() {
    Reset();
    StartTrack();
    Advance(4000);
    PauseStream();      // Escape
    PlayStream(g_menu); // main menu theme
    Advance(1000);
    StopStream();       // Load game: StopMusic, no ResumeMusic
    PlayStream(g_menu); // the loaded scene's track
    Advance(TrackMs() + 1000);
    int opensBefore = g_opens;
    PauseStream(); // dialogue
    ResumeStream();
    ASSERT_EQ_INT(opensBefore, g_opens);
    ASSERT_TRUE(g_live == NULL || strcmp(StreamName(), g_track) != 0);
}

// #797: an OGG track is fed as it plays. The queue never holds more than the
// producer's bound, and the whole track still arrives, exactly, then the flush.
// The chirp, not the silent fixture, so the comparison sees content and order.
static void test_ogg_queue_stays_bounded() {
    Reset();
    std::vector<Uint8> actual;
    g_capturePCM = &actual;
    PlayStream(g_chirp);
    WaitDecoded();
    const int bytes = (int)g_chirpPcm.size();
    {
        std::lock_guard<std::mutex> hold(g_lock);
        ASSERT_TRUE(g_live != NULL);
        if (!g_live) {
            g_capturePCM = NULL;
            return;
        }
        ASSERT_TRUE(!g_live->flushed);
        ASSERT_TRUE(g_live->pushed < bytes / 4);
    }
    Advance(bytes / BYTES_PER_FRAME * 1000 / RATE + 1000);
    {
        std::lock_guard<std::mutex> hold(g_lock);
        ASSERT_TRUE(g_live != NULL);
        if (g_live) {
            ASSERT_TRUE(g_live->maxQueued <= 65536);
            ASSERT_EQ_INT(bytes, g_live->pushed);
            ASSERT_TRUE(g_live->flushed);
        }
    }
    StopStream();
    g_capturePCM = NULL;
    ASSERT_EQ_INT(bytes, (int)actual.size());
    if ((int)actual.size() == bytes && bytes > 0)
        ASSERT_MEM_EQ(&g_chirpPcm[0], &actual[0], actual.size());
}

// ── CD audio ────────────────────────────────────────────────────────────────

// A rip with one BINARY file per disc that the mount does not cover: track 2 is
// sectors 0..299 (4 s), track 3 follows. TADPCM2 is CD track 2 on the US table.
static const int CD_SECTOR = 2352;
static const int CD_TRACK_SECTORS = 300;
static const int CD_FILE_SECTORS = 400;
static char g_cdTrack[] = TEST_TMP_DIR "/cd/music/TADPCM2.WAV";

// Music-like 16-bit stereo: a sine well below a quarter of full scale, the
// level the byte-order check treats as correctly ordered.
static std::vector<Uint8> CdPcm() {
    std::vector<Uint8> pcm((size_t)CD_FILE_SECTORS * CD_SECTOR);
    for (size_t i = 0; i < pcm.size() / 4; ++i) {
        short v = (short)(8000.0 * sin((double)i * 2.0 * 3.14159265358979 * 441.0 / 44100.0));
        for (int c = 0; c < 2; ++c) {
            pcm[i * 4 + c * 2] = (Uint8)(v & 0xff);
            pcm[i * 4 + c * 2 + 1] = (Uint8)((v >> 8) & 0xff);
        }
    }
    return pcm;
}

static bool WriteCdDisc(bool bigEndian) {
    std::vector<Uint8> pcm = CdPcm();
    if (bigEndian) {
        for (size_t i = 0; i + 1 < pcm.size(); i += 2) {
            Uint8 t = pcm[i];
            pcm[i] = pcm[i + 1];
            pcm[i + 1] = t;
        }
    }
    FILE *bin = fopen(TEST_TMP_DIR "/cd/disc.bin", "wb");
    if (!bin)
        return false;
    bool ok = fwrite(&pcm[0], 1, pcm.size(), bin) == pcm.size();
    ok = (fclose(bin) == 0) && ok;
    FILE *cue = fopen(TEST_TMP_DIR "/cd/disc.cue", "wb");
    if (!cue)
        return false;
    fputs("FILE \"disc.bin\" BINARY\n"
          "  TRACK 02 AUDIO\n"
          "    INDEX 01 00:00:00\n"
          "  TRACK 03 AUDIO\n"
          "    INDEX 01 00:04:00\n",
          cue);
    return (fclose(cue) == 0) && ok;
}

static int CdTrackMs() { return CD_TRACK_SECTORS * CD_SECTOR / BYTES_PER_FRAME * 1000 / RATE; }

// Plays the track to its end and checks what the device got against `expected`,
// from byte `from` of the track.
static void PlayCdToEndAndCompare(std::vector<Uint8> &actual, size_t from) {
    Advance(CdTrackMs() + 1000);
    {
        std::lock_guard<std::mutex> hold(g_lock);
        ASSERT_TRUE(g_live != NULL);
        if (g_live) {
            ASSERT_TRUE(g_live->flushed);
            ASSERT_TRUE(g_live->maxQueued <= 65536);
        }
    }
    StopStream();
    g_capturePCM = NULL;
    std::vector<Uint8> pcm = CdPcm();
    size_t trackBytes = (size_t)CD_TRACK_SECTORS * CD_SECTOR;
    ASSERT_EQ_INT((int)(trackBytes - from), (int)actual.size());
    if (actual.size() == trackBytes - from)
        ASSERT_MEM_EQ(&pcm[from], &actual[0], actual.size());
}

static void test_cd_track_streams_exact_pcm() {
    for (int bigEndian = 0; bigEndian <= 1; ++bigEndian) {
        ASSERT_TRUE(WriteCdDisc(bigEndian != 0));
        Reset();
        std::vector<Uint8> actual;
        g_capturePCM = &actual;
        PlayStream(g_cdTrack);
        WaitDecoded();
        {
            std::lock_guard<std::mutex> hold(g_lock);
            ASSERT_TRUE(g_live != NULL);
            if (!g_live) {
                g_capturePCM = NULL;
                return;
            }
            ASSERT_TRUE(!g_live->flushed); // fed as it plays, not pushed whole
        }
        // A big-endian rip comes out byte-swapped into the same samples.
        PlayCdToEndAndCompare(actual, 0);
    }
}

// Escape part-way into a CD track, then back: the track picks up mid-sector at
// the frame it left, which is where its first bytes come from.
static void test_cd_track_resumes_mid_sector() {
    ASSERT_TRUE(WriteCdDisc(false));
    Reset();
    PlayStream(g_cdTrack);
    Advance(1100); // 48510 frames: sector 82 and a half
    PauseStream();
    PlayStream(g_menu);
    Advance(500);
    StopStream();
    std::vector<Uint8> actual;
    g_capturePCM = &actual;
    ResumeStream();
    AssertOpenedAt(1100);
    PlayCdToEndAndCompare(actual, (size_t)MsToBytes(1100));
}

// ── Fixture files ───────────────────────────────────────────────────────────

static bool CopyFile(const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    if (!in)
        return false;
    FILE *out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[4096];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        ok = ok && fwrite(buf, 1, n, out) == n;
    fclose(in);
    return (fclose(out) == 0) && ok;
}

static void RunAll() {
    RUN_TEST(test_one_menu_trip_resumes_in_place);
    RUN_TEST(test_second_menu_trip_resumes_in_place);
    RUN_TEST(test_finished_track_after_a_trip_stays_silent);
    RUN_TEST(test_finished_track_stays_silent);
    RUN_TEST(test_dialogue_pause_unpauses_in_place);
    RUN_TEST(test_load_from_menu_forgets_the_old_track);
}

int main() {
    TEST_SDL_MAIN_READY();
#ifdef LBA2_USE_SDL2
    SDL_SetMainReady();
#endif
    // track.ogg and menu.ogg sit next to the requested .wav names, where
    // STREAM.CPP's .ogg fallback looks for them.
    MakeDir(TEST_TMP_DIR "/music"); // may already exist from an earlier run
    if (!CopyFile(TEST_FIXTURE_OGG, TEST_TMP_DIR "/music/track.ogg") ||
        !CopyFile(TEST_FIXTURE_OGG, TEST_TMP_DIR "/music/menu.ogg") ||
        !CopyFile(TEST_FIXTURE_CHIRP_OGG, TEST_TMP_DIR "/music/chirp.ogg")) {
        printf("could not stage %s\n", TEST_FIXTURE_OGG);
        return 1;
    }

    int vorbisError = 0;
    stb_vorbis *fixture = stb_vorbis_open_filename(TEST_FIXTURE_OGG, &vorbisError, NULL);
    if (!fixture) {
        printf("could not open %s\n", TEST_FIXTURE_OGG);
        return 1;
    }
    g_oggBytes = (int)stb_vorbis_stream_length_in_samples(fixture) * BYTES_PER_FRAME;
    stb_vorbis_close(fixture);
    stb_vorbis *chirp = stb_vorbis_open_filename(TEST_FIXTURE_CHIRP_OGG, &vorbisError, NULL);
    if (!chirp) {
        printf("could not open %s\n", TEST_FIXTURE_CHIRP_OGG);
        return 1;
    }
    short batch[4096];
    int frames;
    while ((frames = stb_vorbis_get_samples_short_interleaved(chirp, 2, batch, 4096)) > 0)
        g_chirpPcm.insert(g_chirpPcm.end(), (const Uint8 *)batch, (const Uint8 *)(batch + frames * 2));
    stb_vorbis_close(chirp);
    MakeDir(TEST_TMP_DIR "/cd");

    OpenStream();
    RUN_TEST(test_wav_full_queue_wait_and_stop);
    RUN_TEST(test_wav_full_track_pcm_is_exact);
    const Setup setups[] = {SETUP_WAV, SETUP_OGG, SETUP_OGG_OGG};
    for (int i = 0; i < 3; i++) {
        g_setup = setups[i];
        printf("# setup: %s\n", SetupName());
        RunAll();
    }
    g_setup = SETUP_OGG;
    RUN_TEST(test_ogg_queue_stays_bounded);
    g_setup = SETUP_WAV;
    RUN_TEST(test_cd_track_streams_exact_pcm);
    RUN_TEST(test_cd_track_resumes_mid_sector);
    StopStream();
    TEST_SUMMARY();
    return test_failures != 0;
}
