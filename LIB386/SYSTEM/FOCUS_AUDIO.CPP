#include <SYSTEM/WINDOW.H>

#include <cstddef> // NULL

// Audio-pause-on-focus policy, kept in its own translation unit (no SDL) so the
// once-per-cycle guard can be host-tested without the window/SDL plumbing. The
// window layer calls AppFocusAudio_OnChange() from the focus events; the audio
// layer registers the hooks via SetAppFocusAudioHooks(). Mirrors the original
// PauseMiles/ResumeMiles plus the `milespaused` guard (WIN.CPP in the original
// Adeline tree): pause once on focus loss, resume once on focus gain, and ignore
// repeated edges of the same kind.

#ifdef __cplusplus
extern "C" {
#endif

static AppFocusAudioFn s_onFocusLost = NULL;
static AppFocusAudioFn s_onFocusGained = NULL;
static bool s_audioPausedByFocusLoss = false;

void SetAppFocusAudioHooks(AppFocusAudioFn onFocusLost, AppFocusAudioFn onFocusGained) {
    s_onFocusLost = onFocusLost;
    s_onFocusGained = onFocusGained;
}

void AppFocusAudio_OnChange(bool gainedFocus) {
    if (gainedFocus) {
        if (s_audioPausedByFocusLoss) {
            s_audioPausedByFocusLoss = false;
            if (s_onFocusGained)
                s_onFocusGained();
        }
    } else {
        if (!s_audioPausedByFocusLoss) {
            s_audioPausedByFocusLoss = true;
            if (s_onFocusLost)
                s_onFocusLost();
        }
    }
}

#ifdef __cplusplus
}
#endif
