/* First-launch folder-picker fallback. See RES_PICKER.H for contract. */

#include "RES_PICKER.H"
#include "DIRECTORIES.H"

#include "SYSTEM/LIMITS.H"
#include "SYSTEM/LOGPRINT.H"

#include <SDL3/SDL.h>

#include <string.h>

namespace {

struct PickerState {
    bool done;
    bool valid_pick;
    char chosen[ADELINE_MAX_PATH];
};

void SDLCALL OnFolderChosen(void *userdata, const char *const *filelist,
                            int /*filter*/) {
    PickerState *state = static_cast<PickerState *>(userdata);

    if (filelist == NULL) {
        /* SDL reported an error (platform dialog failure, OOM, etc.).
         * Treat as cancel; caller falls through to the no-data exit. */
        LogPrintf("RES_PICKER: dialog error: %s\n", SDL_GetError());
        state->valid_pick = false;
    } else if (filelist[0] == NULL) {
        /* User dismissed the dialog without picking. */
        state->valid_pick = false;
    } else {
        strncpy(state->chosen, filelist[0], ADELINE_MAX_PATH - 1);
        state->chosen[ADELINE_MAX_PATH - 1] = '\0';
        state->valid_pick = true;
    }
    state->done = true;
}

void ShowInvalidPickMessage(void) {
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_WARNING,
        "Game data not found",
        "This folder doesn't contain lba2.hqr.\n\n"
        "Please pick the folder containing your retail Little Big "
        "Adventure 2 install — the one with lba2.hqr, music/, video/, "
        "vox/, etc.",
        NULL);
}

} // namespace

namespace {

/* Match the trailing-separator normalization that NormalizeAndTry in
 * RES_DISCOVERY.CPP applies to every auto-discovered path. Keeps the
 * picker's output byte-identical to what the rest of discovery would
 * have produced for the same path — same format flows into
 * InitDirectories regardless of which probe matched. */
void EnsureTrailingSeparator(char *path, U16 maxLen) {
    const size_t n = strlen(path);
    if (n == 0 || n + 2 > static_cast<size_t>(maxLen)) {
        return;
    }
    const char last = path[n - 1];
    if (last == '/' || last == '\\') {
        return;
    }
    path[n] = '/';
    path[n + 1] = '\0';
}

void ShowDiscoveryFailedMessage(void) {
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        "Game data folder not found",
        "Little Big Adventure 2 retail game data wasn't found in any of "
        "the usual places.\n\n"
        "Please pick the folder containing your LBA2 install — the one "
        "with lba2.hqr, music/, video/, vox/, etc.\n\n"
        "Your choice will be remembered for future launches.",
        NULL);
}

} // namespace

extern "C" bool PromptForResDir(char *outPath, U16 outMax) {
    /* SDL_ShowOpenFolderDialog requires SDL_INIT_VIDEO. Probe it as our
     * headless test: if video can't come up (no display, no Wayland
     * portal, --headless build, etc.), there's no way to show a dialog
     * and we fall back to the caller's exit. */
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        LogPrintf("RES_PICKER: SDL_INIT_VIDEO failed (%s); skipping picker\n",
                  SDL_GetError());
        return false;
    }

    /* Tell the user why a dialog is about to appear. Without this, the
     * folder picker pops up unannounced and feels like the app is
     * confused. The message-box-then-picker pattern matches what other
     * cross-platform games do for first-launch data discovery. */
    LogPuts("RES_PICKER: showing 'data not found' message + folder picker...");
    ShowDiscoveryFailedMessage();

    /* Default location for the picker. Home is the universal "user
     * starts here" choice — heuristic install-path detection (~/GOG
     * Games/, etc.) is a rabbit hole and most users won't have data
     * there anyway. */
    const char *defaultLoc = SDL_GetUserFolder(SDL_FOLDER_HOME);

    /* Async-to-sync adapter: SDL_ShowOpenFolderDialog returns immediately
     * and invokes the callback later. We poll until the callback fires.
     * Bounded by user interaction time (single dialog). The 16ms tick
     * matches a 60Hz event-pump cadence; tighter wastes CPU, looser
     * makes the dialog feel sluggish. */
    while (true) {
        PickerState state = {false, false, {0}};

        SDL_ShowOpenFolderDialog(OnFolderChosen, &state, NULL, defaultLoc,
                                 false);

        while (!state.done) {
            SDL_PumpEvents();
            SDL_Delay(16);
        }

        if (!state.valid_pick) {
            LogPuts("RES_PICKER: cancelled");
            return false;
        }

        /* Validation goes through the same IsValidResourceDir gate the
         * CLI/env/auto-discovery paths use — case-folding for `lba2.hqr`
         * (per Discover() in DIRECTORIES.CPP) included. The picker is
         * not a special validation path; it's the same probe with a
         * different input source. */
        if (IsValidResourceDir(state.chosen)) {
            LogPrintf("RES_PICKER: picked %s\n", state.chosen);
            strncpy(outPath, state.chosen, outMax - 1);
            outPath[outMax - 1] = '\0';
            EnsureTrailingSeparator(outPath, outMax);
            return true;
        }

        LogPrintf("RES_PICKER: invalid (%s) — re-prompting\n", state.chosen);
        ShowInvalidPickMessage();
        /* Loop: show picker again. */
    }
}
