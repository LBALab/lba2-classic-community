/* First-launch folder-picker fallback. See RES_PICKER.H for contract. */

#include "RES_PICKER.H"
#include "DIRECTORIES.H"

#include "SYSTEM/ANDROID.H"
#include "SYSTEM/LIMITS.H"
#include "SYSTEM/LOG.H"

#include <PLATFORM/PLATFORM.H>

#include <string.h>

/* Message-box bodies live as named constants (not inline in the SDL calls) so a
   host test can hold the line on the wording contract: no em-dash, recovery
   tokens present, and a length ceiling -- the "don't grow a README inside a
   modal" guard. Keep them short. The long-form setup guide is README.txt
   (shipped next to the binary; .md docs are NOT in the release), and
   docs/GAME_DATA.md for anyone reading the source. Declared in RES_PICKER.H.
   File scope (not the anonymous namespace below): extern "C" external linkage
   so the host test can reference them by name. */

/* The marker this build validates against, spelled for a message body. A demo
   build looks for RESS.HQR (FILE_VALID_RES_DIR in DIRECTORIES.CPP), and a modal
   naming a file the reader's install cannot have is what sends them renaming
   things. test_res_picker checks both bodies against Directories_GetResMarker(),
   so the two spellings cannot drift apart. */
#ifdef DEMO
#define PICKER_MARKER "RESS.HQR"
#define PICKER_INSTALL "your LBA2 demo install"
/* A demo install carries no video/ and no vox/, and spells its music folder
   MUSIC. Listing folders it cannot have would be the same wrong-file trap in
   another form. */
#define PICKER_DATA_SET "your LBA2 demo data (RESS.HQR, MUSIC/)"
#else
#define PICKER_MARKER "LBA2.HQR"
#define PICKER_INSTALL "your retail LBA2 install"
#define PICKER_DATA_SET "your retail LBA2 data (LBA2.HQR, music/, video/, vox/)"
#endif

extern "C" const char kPickerMsgDataNotFound[] =
    "Little Big Adventure 2 game data wasn't found.\n\n"
    "Pick the folder that contains " PICKER_MARKER " (" PICKER_INSTALL ").\n\n"
    "Your choice is saved for next time.";

extern "C" const char kPickerMsgInvalidPick[] =
    "That folder doesn't contain " PICKER_MARKER ".\n\n"
    "Pick " PICKER_INSTALL " folder (the one with " PICKER_MARKER ").";

extern "C" const char kPickerMsgNoPicker[] =
    "The system folder picker isn't available here.\n\n"
    "Start the game with your data folder instead:\n"
    "    lba2cc --game-dir /path/to/your/lba2/install\n"
    "or set the LBA2_GAME_DIR environment variable.\n\n"
    "See README.txt for setup help.";

extern "C" const char kPickerMsgAndroidData[] =
    "Little Big Adventure 2 game data wasn't found.\n\n"
    "Copy " PICKER_DATA_SET " to:\n"
    "    /sdcard/lba2cc/\n\n"
    "then relaunch the app.";

/* The data may be in place already: without All files access the game cannot
   see inside /sdcard/lba2cc/, so asking for a copy would send the player after
   files that are already there. Relaunching opens the Settings page again. */
extern "C" const char kPickerMsgAndroidNoAccess[] =
    "Little Big Adventure 2 can't read /sdcard/lba2cc/ because storage access "
    "wasn't granted.\n\n"
    "Relaunch the app and turn on All files access when Settings opens.";

namespace {

struct PickerState {
    bool done;
    bool valid_pick;
    /* dialog_failed=true means SDL itself couldn't open the folder
     * dialog (no backend available — common on WSL without zenity, on
     * minimal Linux containers, etc.). Distinct from "user clicked
     * Cancel" so the caller can offer a helpful recovery hint instead
     * of silently exiting. */
    bool dialog_failed;
    char chosen[ADELINE_MAX_PATH];
};

/* Strip trailing whitespace/newlines from a path. Some dialog backends
 * (notably zenity in some configurations) return the path with a
 * trailing newline; without trimming, IsValidResourceDir would look for
 * `LBA2.HQR` at the wrong-named directory and silently fail. */
void TrimTrailingWhitespace(char *s) {
    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == '\n' || s[len - 1] == '\r' ||
            s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

void OnFolderChosen(PickerState *state, PlatformDialogResult result, const char *path) {
    /* Diagnostic: log exactly what the dialog backend handed back so
     * platform-specific quirks (trailing newlines, empty-string-on-cancel,
     * etc.) are visible in the log when something goes wrong. */
    if (result == PLATFORM_DIALOG_ERROR) {
        Log_Warn("RES_PICKER: folder dialog failed: %s", g_platform.system.get_error());
    } else if (result == PLATFORM_DIALOG_CANCELLED || path == NULL) {
        Log_Debug("RES_PICKER: folder dialog cancelled");
    } else {
        Log_Debug("RES_PICKER: folder dialog path='%s' (len=%zu)", path, strlen(path));
    }

    if (result == PLATFORM_DIALOG_ERROR) {
        /* The backend reported an error (dialog service missing, OOM,
         * permission denied, etc.). Capture for the caller to surface a
         * recovery hint. */
        state->dialog_failed = true;
        state->valid_pick = false;
    } else if (result == PLATFORM_DIALOG_CANCELLED || path == NULL || path[0] == '\0') {
        /* An empty selected path is cancellation, on the same terms as an
           explicit cancelled result. */
        state->dialog_failed = false;
        state->valid_pick = false;
    } else {
        strncpy(state->chosen, path, ADELINE_MAX_PATH - 1);
        state->chosen[ADELINE_MAX_PATH - 1] = '\0';
        TrimTrailingWhitespace(state->chosen);
        if (state->chosen[0] == '\0') {
            /* Whitespace-only string after trimming — same as empty. */
            state->dialog_failed = false;
            state->valid_pick = false;
        } else {
            state->dialog_failed = false;
            state->valid_pick = true;
        }
    }
    state->done = true;
}

void ShowInvalidPickMessage(void) {
    g_platform.host.message_box(PLATFORM_MESSAGE_WARNING, "Game data not found",
                                kPickerMsgInvalidPick);
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
    g_platform.host.message_box(PLATFORM_MESSAGE_INFORMATION,
                                "Game data folder not found", kPickerMsgDataNotFound);
}

/* The picker fired but the OS couldn't actually open a folder dialog: no
 * backend installed (common on WSL without zenity, on minimal Linux
 * containers, on locked-down corporate setups). Point at the universal
 * escape hatch and README.txt rather than dumping the per-distro install
 * matrix into a modal -- that matrix lives in README.txt / docs/GAME_DATA.md.
 * The advice is platform-neutral (no apt/pacman/dnf) so it reads correctly on
 * every desktop, not just Linux. */
void ShowDialogUnavailableMessage(void) {
    g_platform.host.message_box(PLATFORM_MESSAGE_WARNING, "Couldn't open folder picker",
                                kPickerMsgNoPicker);
}

/* Android has no usable folder picker, so instead of browsing the user is told
 * exactly where to copy their data. Shown in place of the picker flow above. */
void ShowAndroidDataMessage(void) {
    g_platform.host.message_box(PLATFORM_MESSAGE_INFORMATION, "Game data not found",
                                kPickerMsgAndroidData);
}

void ShowAndroidNoAccessMessage(void) {
    g_platform.host.message_box(PLATFORM_MESSAGE_WARNING, "Storage access needed",
                                kPickerMsgAndroidNoAccess);
}

} // namespace

extern "C" bool PromptForResDir(char *outPath, U16 outMax) {
    /* Android has no usable folder picker: SDL's file dialog there returns
     * content:// document URIs, not filesystem paths IsValidResourceDir can
     * probe. So instead of browsing, tell the user where to copy their data
     * and fall through to the caller's exit. Android_GetExternalFilesDir is a
     * no-op returning 0 off-Android, so desktop builds skip this and reach the
     * picker below -- the seam keeps this caller #ifdef-free. */
    char androidProbe[ADELINE_MAX_PATH];
    if (Android_GetExternalFilesDir(androidProbe, ADELINE_MAX_PATH)) {
        if (!Android_HasExternalStoragePermission()) {
            Log_Info("RES_PICKER: Android - no storage access; showing permission hint");
            ShowAndroidNoAccessMessage();
        } else {
            Log_Info("RES_PICKER: Android - no folder picker; showing data-copy hint");
            ShowAndroidDataMessage();
        }
        return false;
    }

    /* Tell the user why a dialog is about to appear. Without this, the
     * folder picker pops up unannounced and feels like the app is
     * confused. The message-box-then-picker pattern matches what other
     * cross-platform games do for first-launch data discovery. */
    Log_Info("RES_PICKER: showing 'data not found' message + folder picker...");
    ShowDiscoveryFailedMessage();

    /* Default location for the picker. Home is the universal "user
     * starts here" choice — heuristic install-path detection (~/GOG
     * Games/, etc.) is a rabbit hole and most users won't have data
     * there anyway. */
    const char *defaultLoc = g_platform.host.home_path();

    /* Async-to-sync adapter: SDL_ShowOpenFolderDialog returns immediately
     * and invokes the callback later. We poll until the callback fires.
     * Bounded by user interaction time (single dialog). The 16ms tick
     * matches a 60Hz event-pump cadence; tighter wastes CPU, looser
     * makes the dialog feel sluggish. */
    while (true) {
        PickerState state = {false, false, false, {0}};

        PlatformFolderDialog *dialog = g_platform.host.begin_folder_dialog(defaultLoc);
        if (dialog == NULL) {
            OnFolderChosen(&state, PLATFORM_DIALOG_ERROR, NULL);
        } else {
            while (!state.done) {
                const char *path = NULL;
                PlatformDialogResult result = g_platform.host.poll_folder_dialog(dialog, &path);
                if (result == PLATFORM_DIALOG_PENDING) {
                    g_platform.events.pump();
                    g_platform.system.delay_ms(16);
                } else {
                    OnFolderChosen(&state, result, path);
                }
            }
            g_platform.host.end_folder_dialog(dialog);
        }

        if (!state.valid_pick) {
            if (state.dialog_failed) {
                /* Backend missing — surface recovery hint graphically
                 * (the "data not found" box already explained what was
                 * needed; this one explains why the picker couldn't help
                 * and what to do instead). */
                Log_Warn("RES_PICKER: dialog backend unavailable");
                ShowDialogUnavailableMessage();
            } else {
                Log_Info("RES_PICKER: cancelled");
            }
            return false;
        }

        /* Apply trailing-separator normalization BEFORE validation.
         * Discover() in DIRECTORIES.CPP does `snprintf("%s%s", baseDir,
         * checkPath)` without injecting a separator, so a baseDir
         * without a trailing slash gives "CommonLBA2.HQR"-style joined
         * paths that always miss. The auto-discovery probe chain calls
         * EnsureTrailingSeparator pre-validation in NormalizeAndTry;
         * the picker has to do the same. Without this, picking a
         * legitimate retail folder via the dialog silently fails
         * validation while the same path passed through --game-dir
         * (which does go through NormalizeAndTry) succeeds. */
        EnsureTrailingSeparator(state.chosen, ADELINE_MAX_PATH);

        /* Validation goes through the same gate the CLI and env overrides
         * use, case-folding for `LBA2.HQR` (per Discover() in
         * DIRECTORIES.CPP) included. The picker is not a special validation
         * path; it's the same probe with a different input source.
         *
         * The image-aware variant, because a folder somebody browsed to is as
         * explicit as a path they typed, and a retail CD rip is one .bin and a
         * .cue with no extracted LBA2.HQR to find. The cheap check is for the
         * speculative candidate sweep, which this is not. */
        if (IsValidResourceDirOrImage(state.chosen)) {
            Log_Info("RES_PICKER: picked %s", state.chosen);
            strncpy(outPath, state.chosen, outMax - 1);
            outPath[outMax - 1] = '\0';
            return true;
        }

        Log_Warn("RES_PICKER: invalid (%s) - re-prompting", state.chosen);
        ShowInvalidPickMessage();
        /* Loop: show picker again. */
    }
}
