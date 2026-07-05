// Host unit tests for the stream park/resume + focus-suspend DECISION logic
// (LIB386/H/AIL/STREAM_PARK.H). Pure and SDL-free: no backend, no audio device.
//
// Verifies the fast-unpause vs restore-from-park vs do-nothing choice, and the nesting
// invariant that a window-focus regain must not un-pause a logically paused
// (menu/dialogue) stream. This is the exact layer the "ResumeMusic must not StopMusic"
// bug lived in; STREAM.CPP's PauseStream/ResumeStream/ResumeStreamOutput are now thin
// wrappers that call these functions and run the SDL side effects.
#include <AIL/STREAM_PARK.H>

#include <cassert>
#include <cstdio>

int main(void) {
    // ── Stream_ShouldPark: park a live, not-already-paused stream ──
    assert(!Stream_ShouldPark(0, 0) && "no stream -> nothing to park");
    assert(Stream_ShouldPark(1, 0) && "live and unpaused -> park it");
    assert(!Stream_ShouldPark(1, 1) && "already paused -> do not re-park (would lose the position)");

    // ── Stream_ResumeAction: fast-unpause vs restore-from-park vs nothing ──
    // Dialogue/holomap: the parked stream is still current and paused -> just unpause.
    assert(Stream_ResumeAction(1, 1, 1, /*currentIsParked*/ 1, /*parkedNonEmpty*/ 1) == STREAM_RESUME_UNPAUSE &&
           "parked stream still current -> fast unpause in place");
    // Menu theme played in between: current stream != parked -> re-open + seek.
    assert(Stream_ResumeAction(1, 0, 1, /*currentIsParked*/ 0, 1) == STREAM_RESUME_RESTORE &&
           "a different stream replaced the parked one -> restore from park");
    // Path mismatch even while flagged paused still restores (never a mis-aimed unpause).
    assert(Stream_ResumeAction(1, 1, 1, /*currentIsParked*/ 0, 1) == STREAM_RESUME_RESTORE &&
           "path mismatch takes the restore path, not a same-stream unpause");
    // Nothing parked -> nothing to do.
    assert(Stream_ResumeAction(1, 1, /*hasRemembered*/ 0, 0, 0) == STREAM_RESUME_NONE &&
           "no park -> nothing");
    // Defensive: remembered flag set but path empty -> nothing (don't restore garbage).
    assert(Stream_ResumeAction(0, 0, 1, 0, /*parkedNonEmpty*/ 0) == STREAM_RESUME_NONE &&
           "remembered but empty path -> nothing");

    // ── Stream_ShouldResumeDeviceOnFocus: the nesting invariant ──
    assert(Stream_ShouldResumeDeviceOnFocus(1, 0) &&
           "focus regain resumes the device when the stream is not logically paused");
    assert(!Stream_ShouldResumeDeviceOnFocus(1, 1) &&
           "focus regain must NOT un-pause a logically paused (menu/dialogue) stream");
    assert(!Stream_ShouldResumeDeviceOnFocus(0, 0) && "no stream -> nothing to resume");

    std::printf("test_music_stream_park: OK\n");
    return 0;
}
