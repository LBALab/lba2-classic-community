//--------------------------------------------------------------------------
#ifndef LIB_AIL_STREAM_PARK
#define LIB_AIL_STREAM_PARK

/* Pure, SDL-free decision logic for the stream park/resume + focus-suspend model.
 *
 * The music stream has two pause layers (see AIL/STREAM.H):
 *   - LOGICAL PauseStream/ResumeStream: parks {path, frame} so a resume can restore
 *     even if a different stream (the menu theme) played and tore this one down in
 *     between. Mirrors the MILES redbook PauseCD/ResumeCD.
 *   - DEVICE SuspendStreamOutput/ResumeStreamOutput: a window-focus pause that must
 *     NOT disturb an outer logical pause.
 *
 * The choice of what to do lives here, extracted from the SDL side effects so the
 * invariants are host-testable without an audio device. LIB386/AIL/SDL/STREAM.CPP
 * supplies the state and runs the SDL calls; tests/music/test_music_stream_park.cpp
 * exercises every state combination.
 *
 * Args are plain ints (0/1) so the header stays usable from C and C++.
 */

typedef enum {
    STREAM_RESUME_NONE = 0, /* nothing parked -> nothing to do */
    STREAM_RESUME_UNPAUSE,  /* parked stream still live -> just unpause the device in place */
    STREAM_RESUME_RESTORE   /* a different stream replaced it -> re-open + seek the parked file */
} StreamResumeAction;

/* PauseStream: park + pause only a live stream that is not already logically paused
   (pausing an already-paused stream would overwrite its parked position). */
static inline int Stream_ShouldPark(int hasStream, int streamPaused) {
    return hasStream && !streamPaused;
}

/* ResumeStream: choose fast-unpause vs restore-from-park vs nothing.
 *   hasStream       - a stream object currently exists
 *   streamPaused    - the stream is logically paused (PauseStream ran)
 *   hasRemembered   - a parked {path, frame} snapshot exists
 *   currentIsParked - the currently-loaded stream name == the parked path
 *   parkedNonEmpty  - the parked path is non-empty
 * Fast unpause only when the parked stream is still the live, paused one; otherwise a
 * remembered non-empty path means something replaced it and we restore + seek. */
static inline StreamResumeAction Stream_ResumeAction(int hasStream, int streamPaused,
                                                     int hasRemembered, int currentIsParked,
                                                     int parkedNonEmpty) {
    if (hasStream && streamPaused && hasRemembered && currentIsParked)
        return STREAM_RESUME_UNPAUSE;
    if (hasRemembered && parkedNonEmpty)
        return STREAM_RESUME_RESTORE;
    return STREAM_RESUME_NONE;
}

/* ResumeStreamOutput (window focus regain): un-pause the DEVICE only if the stream is
   not logically paused, so a focus blip during a menu/dialogue pause does not un-pause
   it. This is the nesting invariant: device-resume must respect an outer logical pause. */
static inline int Stream_ShouldResumeDeviceOnFocus(int hasStream, int streamPaused) {
    return hasStream && !streamPaused;
}

//--------------------------------------------------------------------------
#endif // LIB_AIL_STREAM_PARK

//--------------------------------------------------------------------------
