// Host regression tests for the music state machine in SOURCES/MUSIC.CPP (pause/resume,
// the PlayMusic decision matrix, the NextMusic queue, GetMusic, and the volume fades).
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
extern S32 StopLastMusic; // MUSIC.CPP global (declared in MUSIC.H), C++ linkage
void FadeOutVolumeMusic(void);
void FadeInVolumeMusic(void);
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

// Simulate the current stream reaching its natural end. The real SDL backend's
// IsStreamPlaying() detaches a finished stream (clears StreamName) once the
// device buffer drains, so model EOF the same way: the stream stops "playing"
// and StreamName() goes empty. Used to exercise the deferred-switch queue
// waiting for the current track to finish before starting the next one (#406).
static void spy_end_stream() {
    s_streamName[0] = '\0';
    s_streamPaused = false;
}

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
// A stream plays until it is stopped, paused, or reaches EOF (spy_end_stream).
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

    // ── Scenario 4: #406/#355 - the deferred switch waits for the current track ──
    // Retail parity (#406): area music (jingle 16 / jadpcm09) is playing; a scene wants
    // a DIFFERENT track; PlayMusic(FALSE) defers it into NextMusic. CheckNextMusic() (run
    // every frame by PERSO's main loop) must NOT hard-cut the current track once the
    // defer window elapses -- it lets the current track play out, then starts the queued
    // one (#355: which must eventually play, not defer forever).
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
    CheckNextMusic();   // window elapsed, but A is still playing -> must not cut it (#406)
    assert(!called("PlayStream") && "the queued track must not cut the current one off (#406)");
    assert(GetMusic() == 16 && "A keeps playing until it finishes on its own");

    spy_clear();
    spy_end_stream(); // A reaches its natural end (EOF detaches the stream)
    CheckNextMusic(); // A has finished -> the queued track finally takes over
    assert(called("PlayStream") && "queued track B starts once the current track ends (#355)");
    assert(GetMusic() == 7 && "B (jingle 7) is the current music after the queue drains");
    // The queue must drain SOFT (playit=FALSE): the queued track is an ordinary cube
    // jingle, so it must leave StopLastMusic clear. A forced drain would mark it as
    // script/menu music, and ChangeCube's `if (StopLastMusic) StopMusic()` would then
    // hard-cut it on the next area change -- the deferred chain would break after one
    // hop (#406).
    assert(StopLastMusic == FALSE && "the drained cube jingle stays soft so the next ChangeCube won't cut it");

    // ── Scenario 5: PlayMusic decision matrix - nothing / same / different (soft) ──
    DistribVersion = 3;
    InitCD(empty);
    InitJingle();
    spy_clear();
    PlayMusic(14, 0); // jingle 16 into silence -> plays now
    assert(called("PlayStream") && GetMusic() == 16 && "soft play into silence starts immediately");
    spy_clear();
    PlayMusic(14, 0); // SAME track, soft -> no-op (no restart, no stop)
    assert(!called("PlayStream") && !called("StopStream") && "re-requesting the playing track is a no-op");
    assert(GetMusic() == 16 && "still the same track after a same-track request");
    spy_clear();
    PlayMusic(5, 0); // DIFFERENT track, soft -> deferred, not played now
    assert(!called("PlayStream") && GetMusic() == 16 && "a different soft request defers and keeps the current track");

    // ── Scenario 6: force-play (playit=TRUE) replaces now, but never restarts the same track ──
    DistribVersion = 3;
    InitCD(empty);
    InitJingle();
    PlayMusic(14, 1); // jingle 16 playing
    spy_clear();
    PlayMusic(5, 1); // force a DIFFERENT track -> replaces immediately
    assert(called("StopStream") && called("PlayStream") && "a forced different track replaces immediately");
    assert(GetMusic() == 7 && "the forced track is now current");
    spy_clear();
    PlayMusic(5, 1); // force the SAME track already playing -> no restart
    assert(!called("PlayStream") && "forcing the already-playing track does not restart it");

    // ── Scenario 7: StopLastMusic - a soft request right after a force plays now, not deferred ──
    DistribVersion = 3;
    InitCD(empty);
    InitJingle();
    PlayMusic(14, 1); // force leaves StopLastMusic = TRUE
    spy_clear();
    PlayMusic(5, 0); // soft, but StopLastMusic short-circuits cur to 0 -> plays now
    assert(called("PlayStream") && GetMusic() == 7 && "a soft request right after a force plays immediately");

    // ── Scenario 8: CheckNextMusic timing + empty-queue edges ──
    DistribVersion = 3;
    InitCD(empty);
    InitJingle();
    PlayMusic(14, 0); // A playing (soft -> StopLastMusic = FALSE)
    PlayMusic(5, 0);  // B deferred
    spy_clear();
    CheckNextMusic(); // still inside the defer window -> must not swap yet
    assert(!called("PlayStream") && GetMusic() == 16 && "the queue must not drain before the defer window");
    TimerRefHR += 5000;
    CheckNextMusic(); // window elapsed, but A still playing -> queue waits for it (#406)
    assert(!called("PlayStream") && GetMusic() == 16 && "the queue waits for the current track to finish");
    spy_end_stream(); // A ends
    CheckNextMusic(); // A finished -> B takes over
    assert(GetMusic() == 7 && "B takes over once the current track finishes");
    spy_clear();
    CheckNextMusic(); // NextMusic is now -1
    assert(!called("PlayStream") && "CheckNextMusic with an empty queue does nothing");

    // ── Scenario 9: GetMusic() reports CD track / jingle index / silence ──
    DistribVersion = 2; // US layout: index 0 is a redbook CD track, not a jingle
    InitCD(empty);
    InitJingle();
    PlayMusic(0, 1); // TrackCDUS[0] = 2 (no JINGLE) -> PlayCD(2)
    assert(called("PlayCD") && GetMusic() == 2 && "GetMusic reports the CD track (num + FirstCDTrack)");
    StopMusic();
    assert(GetMusic() == 0 && "GetMusic is 0 when nothing is playing");
    DistribVersion = 3;
    InitCD(empty);
    InitJingle();
    PlayMusic(14, 1);
    assert(GetMusic() == 16 && "GetMusic reports the jingle index for a playing stream");

    // ── Scenario 10: volume fades converge to their targets ──
    DistribVersion = 3;
    InitCD(empty);
    InitJingle();
    PlayMusic(14, 1);
    FadeOutVolumeMusic();
    assert(GetVolumeStream() == 0 && "FadeOutVolumeMusic drives the stream volume to 0");
    FadeInVolumeMusic();
    assert(GetVolumeStream() == 127 && "FadeInVolumeMusic restores the stream volume to JingleVolume");

    // ── Observability: the [MUSIC] trace actually fired ──────────────────────
    bool sawResumeTrace = false;
    for (size_t i = 0; i < g_logs.size(); i++)
        if (g_logs[i].find("ResumeMusic") != std::string::npos)
            sawResumeTrace = true;
    assert(sawResumeTrace && "[MUSIC] ResumeMusic trace should be emitted when logging is on");

    std::printf("test_music_pause_resume: OK (%zu log lines captured)\n", g_logs.size());
    return 0;
}
