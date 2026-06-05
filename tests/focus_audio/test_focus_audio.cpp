/* Host-only regression test for the focus-audio pause guard
 * (LIB386/SYSTEM/FOCUS_AUDIO.CPP).
 *
 * Audio is paused while the game window is not focused, mirroring the original
 * Adeline `PauseMiles`/`ResumeMiles` + `milespaused` guard (WIN.CPP). The
 * invariant under test: exactly one pause per focus-loss edge and one resume per
 * focus-gain edge, with repeated edges of the same kind ignored, so the
 * ref-counted sample pause and the music pause never go out of balance.
 *
 * FOCUS_AUDIO.CPP is deliberately SDL-free so this links without the window /
 * SDL plumbing. See docs/AUDIO.md "Audio pauses when the window loses focus".
 */

#include <SYSTEM/WINDOW.H>

#include <cassert>
#include <cstddef> // NULL
#include <cstdio>

static int pauses = 0;
static int resumes = 0;
static void onPause(void) { pauses++; }
static void onResume(void) { resumes++; }

int main() {
    SetAppFocusAudioHooks(onPause, onResume);

    // Focus lost -> pause exactly once.
    AppFocusAudio_OnChange(false);
    assert(pauses == 1 && resumes == 0);

    // Repeated focus-lost (no intervening gain) must NOT pause again.
    AppFocusAudio_OnChange(false);
    assert(pauses == 1 && resumes == 0);

    // Focus gained -> resume exactly once.
    AppFocusAudio_OnChange(true);
    assert(pauses == 1 && resumes == 1);

    // Repeated focus-gain must NOT resume again.
    AppFocusAudio_OnChange(true);
    assert(pauses == 1 && resumes == 1);

    // A second full cycle balances again.
    AppFocusAudio_OnChange(false);
    AppFocusAudio_OnChange(true);
    assert(pauses == 2 && resumes == 2);

    // NULL hooks must be safe (guard still advances).
    SetAppFocusAudioHooks(NULL, NULL);
    AppFocusAudio_OnChange(false);
    AppFocusAudio_OnChange(true);
    assert(pauses == 2 && resumes == 2);

    std::printf("test_focus_audio: ok\n");
    return 0;
}
