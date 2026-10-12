#include "PRIVATE.H"

#include <assert.h>
#include <string.h>

struct PlatformFolderDialog {
    SDL_AtomicInt done;
    PlatformDialogResult result;
    char *path;
    char error[1024];
};

extern "C" {
void SDLCALL SDL3_FolderChosen(void *userdata, const char *const *files, int) {
    PlatformFolderDialog *dialog = (PlatformFolderDialog *)userdata;
    if (files == NULL) {
        dialog->result = PLATFORM_DIALOG_ERROR;
        SDL_strlcpy(dialog->error, SDL_GetError(), sizeof(dialog->error));
    } else if (files[0] == NULL || files[0][0] == '\0') {
        dialog->result = PLATFORM_DIALOG_CANCELLED;
    } else {
        dialog->path = SDL_strdup(files[0]);
        if (dialog->path) {
            dialog->result = PLATFORM_DIALOG_SELECTED;
        } else {
            dialog->result = PLATFORM_DIALOG_ERROR;
            SDL_strlcpy(dialog->error, "Unable to allocate folder dialog result", sizeof(dialog->error));
        }
    }
    /* The callback may run on an OS worker. Publish its data only after every
       field is written; the engine consumes it on the event-polling thread. */
    SDL_SetAtomicInt(&dialog->done, 1);
}

const char *SDL3_HomePath(void) { return SDL_GetUserFolder(SDL_FOLDER_HOME); }

bool SDL3_MessageBox(PlatformMessageKind kind, const char *title, const char *message) {
    SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION;
    if (kind == PLATFORM_MESSAGE_WARNING)
        flags = SDL_MESSAGEBOX_WARNING;
    else if (kind == PLATFORM_MESSAGE_ERROR)
        flags = SDL_MESSAGEBOX_ERROR;
    bool shown = SDL_ShowSimpleMessageBox(flags, title, message, NULL);
    if (!shown)
        Platform_Log(PLATFORM_LOG_WARN, "Unable to show message box: %s", SDL_GetError());
    return shown;
}

PlatformFolderDialog *SDL3_AllocateFolderDialog(void) {
    PlatformFolderDialog *dialog = (PlatformFolderDialog *)SDL_calloc(1, sizeof(PlatformFolderDialog));
    if (dialog == NULL) {
        SDL_SetError("Unable to allocate folder dialog");
        return NULL;
    }
    dialog->result = PLATFORM_DIALOG_PENDING;
    return dialog;
}

PlatformFolderDialog *SDL3_BeginFolderDialog(const char *initialPath) {
    PlatformFolderDialog *dialog = SDL3_AllocateFolderDialog();
    if (dialog)
        SDL_ShowOpenFolderDialog(SDL3_FolderChosen, dialog, NULL, initialPath, false);
    return dialog;
}

PlatformDialogResult SDL3_PollFolderDialog(PlatformFolderDialog *dialog, const char **path) {
    assert(dialog != NULL);
    if (!SDL_GetAtomicInt(&dialog->done))
        return PLATFORM_DIALOG_PENDING;
    if (path)
        *path = dialog->path;
    if (dialog->result == PLATFORM_DIALOG_ERROR)
        SDL_SetError("%s", dialog->error);
    return dialog->result;
}

void SDL3_EndFolderDialog(PlatformFolderDialog *dialog) {
    if (dialog == NULL)
        return;
    if (!SDL_GetAtomicInt(&dialog->done)) {
        Platform_Log(PLATFORM_LOG_ERROR, "Cannot release a pending folder dialog");
        return;
    }
    SDL_free(dialog->path);
    SDL_free(dialog);
}
}
