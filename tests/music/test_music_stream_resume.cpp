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
// Every case runs on three source setups, because each reopens the track
// through a different path in STREAM.CPP:
//   wav       - track and menu theme are WAV (a disc or extracted install):
//               SetupWavStream from a byte offset.
//   ogg       - OGG track, WAV menu theme: the track stays in the one-slot
//               decoded-PCM cache, so a restore is PlayOggFile's cache hit.
//   ogg-ogg   - both OGG, as in the Steam and GOG Classic releases: decoding
//               the menu theme evicts the track, so a restore re-decodes it
//               from a stb_vorbis seek on the background thread.
// WAV loading is faked (silent PCM of a known length for any "*.wav" name);
// OGG decoding is real, from tests/music/fixtures/silence-10s.ogg, copied into
// the build tree as track.ogg and menu.ogg.
#include "../sdl_main_ready.h"
#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/AUDIO_DEVICE.H"

#include <AIL/STREAM.H>
#include <SYSTEM/CRASH.H>

#include <mutex>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MakeDir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MakeDir(path) mkdir(path, 0755)
#endif

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
    int puts; // one for a WAV or an OGG cache hit, one per chunk from the decoder
    bool paused;
    bool flushed;
    bool settled; // WaitDecoded has already waited for this stream's decode thread
};

// The OGG decode thread pushes from its own thread, so the fake locks.
static std::mutex g_lock;
static FakeStream *g_live = NULL; // the stream STREAM.CPP currently owns
static int g_opens = 0;

static FakeStream *Fake(AILAudioStream *s) { return reinterpret_cast<FakeStream *>(s); }

bool AILAudio_Initialized(void) { return true; }

AILAudioStream *AILAudio_Open(const SDL_AudioSpec *spec, AILAudioCallback, void *) {
    std::lock_guard<std::mutex> hold(g_lock);
    FakeStream *s = new FakeStream();
    s->bytesPerFrame = (int)(SDL_AUDIO_BITSIZE(spec->format) / 8) * spec->channels;
    s->queued = 0;
    s->pushed = 0;
    s->puts = 0;
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

bool AILAudio_Put(AILAudioStream *stream, const void *, int bytes) {
    std::lock_guard<std::mutex> hold(g_lock);
    Fake(stream)->queued += bytes;
    Fake(stream)->pushed += bytes;
    Fake(stream)->puts++;
    return true;
}

bool AILAudio_Flush(AILAudioStream *stream) {
    std::lock_guard<std::mutex> hold(g_lock);
    Fake(stream)->flushed = true;
    return true;
}

int AILAudio_Queued(AILAudioStream *stream) {
    std::lock_guard<std::mutex> hold(g_lock);
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

// A WAV name loads as silent PCM unless this setup has an OGG for it, in which
// case it fails, as on a Classic install, and STREAM.CPP falls back to the .ogg.
bool AILAudio_LoadWAV(const char *path, SDL_AudioSpec *spec, Uint8 **data, Uint32 *bytes) {
    size_t n = strlen(path);
    if (n < 4 || strcmp(path + n - 4, ".wav") != 0)
        return false;
    bool menu = strstr(path, "menu") != NULL;
    if (menu ? MenuIsOgg() : TrackIsOgg())
        return false;
    spec->freq = RATE;
    spec->format = AIL_AUDIO_S16;
    spec->channels = 2;
    *bytes = (Uint32)MsToBytes(menu ? WAV_MENU_MS : WAV_TRACK_MS);
    *data = (Uint8 *)SDL_calloc(1, *bytes);
    return *data != NULL;
}

bool AILAudio_LoadWAVMemory(const void *, int, SDL_AudioSpec *, Uint8 **, Uint32 *) { return false; }

// ── Clock ───────────────────────────────────────────────────────────────────

// Wait for the OGG decode thread to push the whole track and exit, so the
// virtual clock never races it. The flush is its last call on the stream, but
// the thread then stores the decoded track in the cache, and a PlayStream that
// joined it first would stop it short of that and turn the next cache hit into
// a decode. So a decoded stream (more than one put) also gets a settle delay,
// once. WAV and cache hits push and flush before PlayStream returns.
static void WaitDecoded() {
    bool decoded = false;
    for (int i = 0; i < 5000; i++) {
        {
            std::lock_guard<std::mutex> hold(g_lock);
            if (!g_live || g_live->settled)
                return;
            if (g_live->flushed) {
                decoded = g_live->puts > 1;
                g_live->settled = true;
                break;
            }
        }
        SDL_Delay(1);
    }
    if (decoded)
        SDL_Delay(100);
}

// The device plays `ms` of whatever is live and unpaused.
static void Advance(int ms) {
    WaitDecoded();
    std::lock_guard<std::mutex> hold(g_lock);
    if (!g_live || g_live->paused)
        return;
    int bytes = MsToBytes(ms);
    g_live->queued = (bytes >= g_live->queued) ? 0 : g_live->queued - bytes;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

static char g_track[] = TEST_TMP_DIR "/music/track.wav";
static char g_menu[] = TEST_TMP_DIR "/music/menu.wav";
static int g_trackBytes = 0; // the whole track as the device receives it

// Where the live stream was opened, in ms: whatever of the track it was not handed.
static int OpenedAtMs() {
    WaitDecoded();
    std::lock_guard<std::mutex> hold(g_lock);
    int frames = (g_trackBytes - g_live->pushed) / BYTES_PER_FRAME;
    return (int)((long long)frames * 1000 / RATE);
}

static int TrackMs() { return (int)((long long)g_trackBytes / BYTES_PER_FRAME * 1000 / RATE); }

// A decoder seek lands on the start of the Vorbis frame holding the target, up
// to one long block (2048 samples, 46 ms) early. WAV and the cache are exact.
static void AssertOpenedAt(int expectMs) {
    int got = OpenedAtMs();
    int slack = (g_setup == SETUP_OGG_OGG) ? 50 : 0;
    if (got < expectMs - slack || got > expectMs) {
        printf("    [%s] opened at %d ms, expected %d ms (-%d)\n", SetupName(), got, expectMs, slack);
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
    g_trackBytes = g_live ? g_live->pushed : 0;
}

// ── Tests ───────────────────────────────────────────────────────────────────

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
    // Pin the path each setup claims to cover: only ogg-ogg re-decodes.
    if (g_setup == SETUP_OGG_OGG)
        ASSERT_TRUE(g_live->puts > 1);
    else
        ASSERT_EQ_INT(1, g_live->puts);
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
    ASSERT_EQ_INT(g_trackBytes - MsToBytes(3000), g_live->queued);
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
        !CopyFile(TEST_FIXTURE_OGG, TEST_TMP_DIR "/music/menu.ogg")) {
        printf("could not stage %s\n", TEST_FIXTURE_OGG);
        return 1;
    }

    OpenStream();
    const Setup setups[] = {SETUP_WAV, SETUP_OGG, SETUP_OGG_OGG};
    for (int i = 0; i < 3; i++) {
        g_setup = setups[i];
        printf("# setup: %s\n", SetupName());
        RunAll();
    }
    StopStream();
    TEST_SUMMARY();
    return test_failures != 0;
}
