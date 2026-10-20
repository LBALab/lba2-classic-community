// Host tests for where the SDL music stream resumes after a pause
// (LIB386/AIL/SDL/STREAM.CPP), driven by a virtual clock.
//
// The real STREAM.CPP is compiled against a fake audio device (the AILAudio_*
// seam in LIB386/AIL/SDL/AUDIO_DEVICE.H). The fake keeps a byte queue per
// stream and consumes it only when the test calls Advance(ms), so playback
// time is exact and nothing depends on a sound card, a thread or the wall clock.
// WAV loading is faked too: any "*.wav" name yields silent 44.1 kHz 16-bit
// stereo PCM of a known length, so the frame a stream was reopened at can be
// read back from how many bytes it was handed.
//
// The menu round trip is the one the game makes on Escape: PauseMusic parks the
// track, the main menu plays its theme, StopMusic ends it on the way back, and
// ResumeMusic restores the parked track.
#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/AUDIO_DEVICE.H"

#include <AIL/STREAM.H>
#include <SYSTEM/CRASH.H>

#include <string.h>

/* Playback is tested without installing the process-wide crash handler. */
void Crash_PrepareThread(void) {}

// ── Fake device ─────────────────────────────────────────────────────────────

static const int RATE = 44100;
static const int BYTES_PER_FRAME = 4; // 16-bit stereo
static const int TRACK_MS = 10000;
static const int MENU_MS = 5000;

struct FakeStream {
    int bytesPerFrame;
    int queued;
    int pushed;
    bool paused;
};

static FakeStream *g_live = NULL; // the stream STREAM.CPP currently owns
static int g_opens = 0;

static FakeStream *Fake(AILAudioStream *s) { return reinterpret_cast<FakeStream *>(s); }

bool AILAudio_Initialized(void) { return true; }

AILAudioStream *AILAudio_Open(const SDL_AudioSpec *spec, AILAudioCallback, void *) {
    FakeStream *s = new FakeStream();
    s->bytesPerFrame = (int)(SDL_AUDIO_BITSIZE(spec->format) / 8) * spec->channels;
    s->queued = 0;
    s->pushed = 0;
    s->paused = true; // SDL device streams open paused; FinishStream resumes them
    g_live = s;
    g_opens++;
    return reinterpret_cast<AILAudioStream *>(s);
}

void AILAudio_Destroy(AILAudioStream *stream) {
    FakeStream *s = Fake(stream);
    if (g_live == s)
        g_live = NULL;
    delete s;
}

void AILAudio_Lock(AILAudioStream *) {}
void AILAudio_Unlock(AILAudioStream *) {}

bool AILAudio_Put(AILAudioStream *stream, const void *, int bytes) {
    Fake(stream)->queued += bytes;
    Fake(stream)->pushed += bytes;
    return true;
}

bool AILAudio_Flush(AILAudioStream *) { return true; }
int AILAudio_Queued(AILAudioStream *stream) { return Fake(stream)->queued; }
bool AILAudio_Gain(AILAudioStream *, float) { return true; }

bool AILAudio_Resume(AILAudioStream *stream) {
    Fake(stream)->paused = false;
    return true;
}

bool AILAudio_Pause(AILAudioStream *stream) {
    Fake(stream)->paused = true;
    return true;
}

static int MsToBytes(int ms) { return (int)((long long)ms * RATE / 1000) * BYTES_PER_FRAME; }

bool AILAudio_LoadWAV(const char *path, SDL_AudioSpec *spec, Uint8 **data, Uint32 *bytes) {
    size_t n = strlen(path);
    if (n < 4 || strcmp(path + n - 4, ".wav") != 0)
        return false;
    int ms = strstr(path, "menu") ? MENU_MS : TRACK_MS;
    spec->freq = RATE;
    spec->format = AIL_AUDIO_S16;
    spec->channels = 2;
    *bytes = (Uint32)MsToBytes(ms);
    *data = (Uint8 *)SDL_calloc(1, *bytes);
    return *data != NULL;
}

bool AILAudio_LoadWAVMemory(const void *, int, SDL_AudioSpec *, Uint8 **, Uint32 *) { return false; }

// The device plays `ms` of whatever is live and unpaused.
static void Advance(int ms) {
    if (!g_live || g_live->paused)
        return;
    int bytes = MsToBytes(ms);
    g_live->queued = (bytes >= g_live->queued) ? 0 : g_live->queued - bytes;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

static char g_track[] = "music/track.wav";
static char g_menu[] = "music/menu.wav";

// The frame the live stream was opened at: whatever it was not handed.
static int OpenedAtMs() {
    int frames = (MsToBytes(TRACK_MS) - g_live->pushed) / BYTES_PER_FRAME;
    return (int)((long long)frames * 1000 / RATE);
}

// Escape -> main menu -> back to the game.
static void MenuRoundTrip() {
    PauseStream();
    PlayStream(g_menu);
    Advance(1000);
    StopStream();
    ResumeStream();
}

static void Reset() {
    StopStream();
    OpenStream(); // clears any parked position
    g_opens = 0;
}

// ── Tests ───────────────────────────────────────────────────────────────────

// Control: one trip mid-track resumes where it left off (worked before the fix).
static void test_one_menu_trip_resumes_in_place() {
    Reset();
    PlayStream(g_track);
    Advance(4000);
    MenuRoundTrip();
    ASSERT_TRUE(g_live != NULL);
    ASSERT_TRUE(strcmp(StreamName(), g_track) == 0);
    ASSERT_EQ_INT(4000, OpenedAtMs());
}

// Two trips in one track: the second resume continues from the first.
// Before the fix the restored stream counted from its seek point, so the second
// park saved 2 s instead of 6 s and the track jumped back.
static void test_second_menu_trip_resumes_in_place() {
    Reset();
    PlayStream(g_track);
    Advance(4000);
    MenuRoundTrip();
    Advance(2000);
    MenuRoundTrip();
    ASSERT_TRUE(g_live != NULL);
    ASSERT_EQ_INT(6000, OpenedAtMs());
}

// #427: a track that played out after an earlier trip must stay silent.
// Before the fix the spent stream was parked at "length minus first seek"
// (6 s here) and the menu return replayed it from there, mid-track.
static void test_finished_track_after_a_trip_stays_silent() {
    Reset();
    PlayStream(g_track);
    Advance(4000);
    MenuRoundTrip();
    Advance(7000); // past the end; nothing polls IsStreamPlaying, as after the queue drains
    int opensBefore = g_opens;
    MenuRoundTrip();
    ASSERT_EQ_INT(opensBefore + 1, g_opens); // the menu theme only
    ASSERT_TRUE(g_live == NULL);
    ASSERT_EQ_INT(0, IsStreamPlaying());
}

// A track that played out with no trip before also stays silent.
static void test_finished_track_stays_silent() {
    Reset();
    PlayStream(g_track);
    Advance(TRACK_MS + 1000);
    int opensBefore = g_opens;
    MenuRoundTrip();
    ASSERT_EQ_INT(opensBefore + 1, g_opens);
    ASSERT_TRUE(g_live == NULL);
}

// Control: dialogue pauses and resumes the same stream without reopening it.
static void test_dialogue_pause_unpauses_in_place() {
    Reset();
    PlayStream(g_track);
    Advance(3000);
    FakeStream *before = g_live;
    PauseStream();
    ASSERT_TRUE(g_live->paused);
    Advance(5000); // paused: the device plays nothing
    ResumeStream();
    ASSERT_TRUE(g_live == before);
    ASSERT_TRUE(!g_live->paused);
    ASSERT_EQ_INT(1, g_opens);
    ASSERT_EQ_INT(MsToBytes(TRACK_MS - 3000), g_live->queued);
}

int main() {
#ifdef LBA2_USE_SDL2
    SDL_SetMainReady();
#endif
    OpenStream();
    RUN_TEST(test_one_menu_trip_resumes_in_place);
    RUN_TEST(test_second_menu_trip_resumes_in_place);
    RUN_TEST(test_finished_track_after_a_trip_stays_silent);
    RUN_TEST(test_finished_track_stays_silent);
    RUN_TEST(test_dialogue_pause_unpauses_in_place);
    StopStream();
    TEST_SUMMARY();
    return test_failures != 0;
}
