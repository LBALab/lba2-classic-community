// Host regression test for the music pause/resume path in SOURCES/MUSIC.CPP.
//
// The bug this guards: ResumeMusic() used to call StopMusic() before
// ResumeCD()/ResumeStream() in the CDROM build. On a streamed backend Stop is
// destructive (it tears the audio stream down), so the resume had nothing to
// unpause and music stayed silent after the in-game Options menu / dialogue.
//
// We compile the *real* MUSIC.CPP against a spy AIL backend that records the
// ordered sequence of backend calls. The contract under test:
//
//   PauseMusic()  -> PauseCD + PauseStream        (park, never Stop)
//   ResumeMusic() -> ResumeCD + ResumeStream       (unpark, never Stop)
//
// A ResumeMusic() that emits StopCD/StopStream is the regression and fails here.
//
// No SDL, no audio device, no game assets: MUSIC.CPP's whole link surface is a
// handful of AIL/timer/dir/log symbols, all stubbed below. RegleTrois /
// BoundRegleTrois come from the real LIB386/3D/REGLE3.CPP (linked in CMake).

// We deliberately do NOT include SOURCES/C_EXTERN.H: that drags in the whole
// engine header chain, which only parses when SOURCES is the *current-file*
// dir (not an -I dir), so it can't be reached from tests/. Instead we pull the
// lightweight LIB386/H interfaces and hand-declare the few SOURCES-side symbols
// MUSIC.CPP exposes, matching their real linkage:
//   - AIL backend + timer + log + filename  -> extern "C"   (LIB386 headers)
//   - PlayMusic/Pause/Resume/Stop/Init...    -> C++ linkage  (MUSIC.H, no extern C)
//   - GetJinglePath                          -> extern "C"   (DIRECTORIES.H wraps it)
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/FILENAME.H>
#include <SYSTEM/LOG.H>
#include <AIL/CD.H>
#include <AIL/STREAM.H>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// MUSIC.CPP public API, from SOURCES/MUSIC.H (CDROM build), C++ linkage.
void PlayMusic(S32 num, S32 playit);
void StopMusic(void);
void PauseMusic(S32 fade);
void ResumeMusic(S32 fade);
void InitJingle(void);
S32 GetMusic(void);
void CheckNextMusic(void);
S32 InitCD(char *name);

// SOURCES/DIRECTORIES.H is an extern "C" header.
extern "C" void GetJinglePath(char *outPath, U16 pathMaxSize, const char *jingleFilename);

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ── Spy state ───────────────────────────────────────────────────────────────

static std::vector<std::string> g_calls; // ordered backend lifecycle calls
static std::vector<std::string> g_logs;  // captured [MUSIC] Log_Info lines

static void rec(const char *name) { g_calls.push_back(name); }
static void spy_clear() { g_calls.clear(); }
static bool called(const char *name) {
    for (size_t i = 0; i < g_calls.size(); i++)
        if (g_calls[i] == name)
            return true;
    return false;
}

// Minimal "what is playing" model so GetMusic()/PlayMusic() branch sensibly.
static bool s_cdPlaying = false;
static S32 s_cdTrack = 0;
static char s_streamName[ADELINE_MAX_PATH] = "";
static bool s_streamPaused = false;
static S32 s_cdVol = 0;
static S32 s_streamVol = 0;

// ── Spy AIL backend (matches the extern "C" decls pulled in above) ───────────

const char *OpenCD(char *volume_name) {
    (void)volume_name;
    rec("OpenCD");
    s_cdPlaying = false;
    s_cdTrack = 0;
    return "D";
}
void PlayCD(S32 track) {
    rec("PlayCD");
    s_cdPlaying = true;
    s_cdTrack = track;
}
void StopCD() {
    rec("StopCD");
    s_cdPlaying = false;
    s_cdTrack = 0;
}
void PauseCD() { rec("PauseCD"); }
void ResumeCD() { rec("ResumeCD"); }
S32 IsCDPlaying() { return s_cdPlaying ? (s_cdTrack ? s_cdTrack : 1) : 0; }
void ChangeVolumeCD(S32 volume) { s_cdVol = volume; }
S32 GetVolumeCD() { return s_cdVol; }

void OpenStream() {
    rec("OpenStream");
    s_streamName[0] = '\0';
    s_streamPaused = false;
}
void PlayStream(char *name) {
    rec("PlayStream");
    std::strncpy(s_streamName, name ? name : "", sizeof(s_streamName) - 1);
    s_streamName[sizeof(s_streamName) - 1] = '\0';
    s_streamPaused = false;
}
void StopStream() {
    rec("StopStream");
    s_streamName[0] = '\0';
    s_streamPaused = false;
}
void PauseStream() {
    rec("PauseStream");
    s_streamPaused = true;
}
void ResumeStream() {
    rec("ResumeStream");
    s_streamPaused = false;
}
S32 IsStreamPlaying() { return (s_streamName[0] && !s_streamPaused) ? TRUE : FALSE; }
void ChangeVolumeStream(S32 volume) { s_streamVol = volume; }
S32 GetVolumeStream() { return s_streamVol; }
char *StreamName() { return s_streamName; }
int MusicLogIsEnabled(void) { return 1; } // keep tracing on so we can capture it

// ── Timer / clock stubs ──────────────────────────────────────────────────────
// ManageTime/Timer_FixedDtPump advance the virtual clock so the blocking fade
// loops (FadeIn/FadeOutVolumeMusic) converge instead of spinning forever.

volatile U32 TimerRefHR = 0;
void LockTimer() {}
void UnlockTimer() {}
void SaveTimer() {}
void RestoreTimer() {}
void ManageTime() { TimerRefHR += 16; }
void Timer_FixedDtPump() { TimerRefHR += 16; }

// ── Misc globals / helpers MUSIC.CPP reaches for ─────────────────────────────

S32 DistribVersion = 0;
S32 MasterVolume = 127;

// InitJingle registers the window focus-pause hooks; the real impl lives in
// AMBIANCE.CPP, which this host test does not link.
void InstallFocusAudioHooks(void) {}

void GetJinglePath(char *outPath, U16 pathMaxSize, const char *jingleFilename) {
    std::snprintf(outPath, pathMaxSize, "music/%s", jingleFilename ? jingleFilename : "");
}
char *GetFileName(char *pathname) {
    // Match the real LIB386 GetFileName: strip the directory *and* the extension, so
    // GetNumJingle() can round-trip a stream path ("music/JADPCM09.WAV") back to its
    // jingle index. The queue decision in PlayMusic() depends on this working.
    static char fileName[64];
    char *slash = std::strrchr(pathname, '/');
    const char *start = slash ? slash + 1 : pathname;
    std::strncpy(fileName, start, sizeof(fileName) - 1);
    fileName[sizeof(fileName) - 1] = '\0';
    char *dot = std::strrchr(fileName, '.');
    if (dot)
        *dot = '\0';
    return fileName;
}

void Log_Info(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_logs.push_back(buf);
}

// ── Assertions ───────────────────────────────────────────────────────────────

// The heart of the regression guard.
static void assert_resume_never_stops() {
    assert(!called("StopCD") && "ResumeMusic must not StopCD (destroys the stream)");
    assert(!called("StopStream") && "ResumeMusic must not StopStream (destroys the stream)");
}

int main(void) {
    char empty[] = "";

    // ── Scenario 1: CD music (US layout) survives a no-fade pause/resume ──────
    // This mirrors the in-game Options menu: PauseMusic(FALSE)/ResumeMusic(FALSE).
    DistribVersion = 2; // not UNKNOWN(0)/EA(3) -> US layout, TrackCDUS, CD tracks
    MasterVolume = 127;
    InitCD(empty);   // sets PtrTrackCD = TrackCDUS, FirstCDTrack = 0
    InitJingle();    // OpenStream
    PlayMusic(0, 1); // TrackCDUS[0] = 2 (CD, no JINGLE bit) -> PlayCD(2)
    assert(called("PlayCD") && "expected CD track to start");

    spy_clear();
    PauseMusic(0);
    assert(called("PauseCD") && "PauseMusic must PauseCD");
    assert(called("PauseStream") && "PauseMusic must PauseStream");
    assert(!called("StopCD") && !called("StopStream") && "Pause must not Stop");

    spy_clear();
    ResumeMusic(0);
    assert(called("ResumeCD") && "ResumeMusic must ResumeCD");
    assert(called("ResumeStream") && "ResumeMusic must ResumeStream");
    assert_resume_never_stops(); // <-- the bug

    // Sanity: the spy *can* observe Stops, so the absence above is meaningful.
    spy_clear();
    StopMusic();
    assert(called("StopCD") && called("StopStream") && "StopMusic must Stop both");

    // ── Scenario 2: jingle/stream (EU layout) survives pause/resume ──────────
    DistribVersion = 3; // EA_VERSION -> EU layout, TrackCD, every entry a jingle
    InitCD(empty);
    InitJingle();
    PlayMusic(1, 1); // TrackCD[1] = JINGLE|3 -> PlayJingle -> PlayStream
    assert(called("PlayStream") && "expected jingle stream to start");

    spy_clear();
    PauseMusic(0);
    assert(called("PauseStream") && !called("StopStream") && "Pause must not Stop");

    spy_clear();
    ResumeMusic(0);
    assert(called("ResumeStream") && "ResumeMusic must ResumeStream");
    assert_resume_never_stops();

    // ── Scenario 3: faded resume (dialogue path, ResumeMusic(TRUE)) ───────────
    // Exercises the FadeInVolumeMusic loop; must still never Stop.
    spy_clear();
    ResumeMusic(1);
    assert_resume_never_stops();

    // ── Scenario 4: #355 - a queued track must take over, not defer forever ───
    // Field repro: area music (jingle 16 / jadpcm09) keeps playing; a scene wants a
    // DIFFERENT track; PlayMusic(FALSE) defers it into NextMusic; CheckNextMusic() (run
    // every frame by PERSO's main loop) must eventually PLAY it instead of re-deferring
    // while the looping track never ends. Before the fix these asserts fail: B is queued
    // and never starts.
    DistribVersion = 3; // EU layout: TrackCD, every entry a jingle/stream
    InitCD(empty);
    InitJingle();

    PlayMusic(14, 0); // TrackCD[14] = JINGLE|16 -> jingle 16 (jadpcm09) starts
    assert(called("PlayStream") && "area track A should start");
    assert(GetMusic() == 16 && "A (jingle 16) is the current music");

    spy_clear();
    PlayMusic(5, 0); // TrackCD[5] = JINGLE|7 -> jingle 7, a DIFFERENT track, deferred
    assert(!called("PlayStream") && "B must be deferred while A is still playing");
    assert(GetMusic() == 16 && "A still current right after the deferred request");

    spy_clear();
    TimerRefHR += 5000; // elapse past the 2s (TEST_MUSIC_TEMPO) defer window
    CheckNextMusic();   // the per-frame queue service
    assert(called("PlayStream") && "queued track B must start once the defer elapses (#355)");
    assert(GetMusic() == 7 && "B (jingle 7) is the current music after the queue drains");

    // ── Observability: the [MUSIC] trace actually fired ──────────────────────
    bool sawResumeTrace = false;
    for (size_t i = 0; i < g_logs.size(); i++)
        if (g_logs[i].find("ResumeMusic") != std::string::npos)
            sawResumeTrace = true;
    assert(sawResumeTrace && "[MUSIC] ResumeMusic trace should be emitted when logging is on");

    std::printf("test_music_pause_resume: OK (%zu log lines captured)\n", g_logs.size());
    return 0;
}
