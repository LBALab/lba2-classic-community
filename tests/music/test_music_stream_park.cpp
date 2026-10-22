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
    assert(Stream_QueueWaitMs(0, 16384, 22050, 4) == 0);
    assert(Stream_QueueWaitMs(65535, 1, 22050, 4) == 0);
    assert(Stream_QueueWaitMs(65536, 1, 22050, 4) == 1);
    assert(Stream_QueueWaitMs(65536, 8192, 22050, 4) == 93);
    assert(Stream_QueueWaitMs(65536, 8192, 44100, 4) == 47);
    assert(Stream_QueueWaitMs(65536, 8192, 8000, 4) == 250);
    assert(Stream_QueueWaitMs(65536, 8192, 8000, 1) == 250);
    assert(Stream_QueueWaitMs(65536, 8192, 2147483647, 32) == 1);
    assert(Stream_QueueWaitMs(0xffffffffu, 16384, 48000, 4) == 250);

    // ── Stream_ShouldPark: park a live, not-already-paused stream with audio left ──
    assert(!Stream_ShouldPark(0, 0, 0) && "no stream -> nothing to park");
    assert(Stream_ShouldPark(1, 0, 1) && "live and unpaused -> park it");
    assert(!Stream_ShouldPark(1, 1, 1) && "already paused -> do not re-park (would lose the position)");
    assert(!Stream_ShouldPark(1, 0, 0) &&
           "played out but not detached -> nothing to park (a resume would replay it)");

    // ── Stream_PositionFrames: absolute position, counted from the track start ──
    // 4 bytes per frame (16-bit stereo).
    assert(Stream_PositionFrames(0, 400, 0, 4) == 100 && "fresh stream, all consumed");
    assert(Stream_PositionFrames(0, 400, 100, 4) == 75 && "fresh stream, part still queued");
    // Restored at frame 1000 and then played 100 frames: position is 1100, not 100.
    assert(Stream_PositionFrames(1000, 400, 0, 4) == 1100 &&
           "a restored stream counts from its seek point");
    assert(Stream_PositionFrames(1000, 0, 0, 4) == 1000 && "restored, nothing played yet");
    assert(Stream_PositionFrames(0, 100, 400, 4) == 0 && "queue above pushed clamps to 0");
    assert(Stream_PositionFrames(1000, 400, 0, 0) == 0 && "no geometry -> 0");
    assert(Stream_PositionFrames(-5, 400, 0, 4) == 100 && "a negative base is treated as 0");
    // Two menu trips in one track: pause at 1000, restore, play 500 more, pause again.
    {
        int first = Stream_PositionFrames(0, 1000 * 4, 0, 4);
        int second = Stream_PositionFrames(first, 500 * 4, 0, 4);
        assert(first == 1000 && second == 1500 && "the second park continues from the first");
    }

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
