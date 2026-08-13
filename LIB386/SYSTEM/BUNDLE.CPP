#include <SYSTEM/BUNDLE.H>

#include <SYSTEM/LOG.H>

#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
/** True when \p path sits inside \p root, on a component boundary, so a mount
 *  at /tmp/.mount_a does not claim an executable under /tmp/.mount_abc. */
static S32 PathIsInside(const char *path, const char *root) {
    const size_t rootLen = strlen(root);
    if (rootLen == 0 || strncmp(path, root, rootLen) != 0) {
        return 0;
    }
    const char after = path[rootLen];
    if (after == '\0') {
        return 1; // the root itself
    }
    /* A root already ending in a separator has consumed the boundary. */
    const char last = root[rootLen - 1];
    return ((after == '/') || (after == '\\') || (last == '/') || (last == '\\')) ? 1 : 0;
}

/* One place for "the executable is not where the user thinks it is", so the
   callers asking that question stay free of packaging formats, the way the
   game-data probe chain already is for Android. A second packaging needing the
   same correction extends this function rather than the callers.

   Today that is the AppImage, which mounts itself and runs the executable from
   /tmp/.mount_XXXX/bin/ inside a read-only image. The file the player actually
   placed somewhere is named by $APPIMAGE, exported by the runtime alongside
   APPDIR and OWD. */
S32 Bundle_GetLauncherDir(const char *execDir, char *out, U16 maxLen,
                          const char **outKindName) {
    if ((out == NULL) || (maxLen == 0)) {
        return 0;
    }

    const char *launched = getenv("APPIMAGE");
    if ((launched == NULL) || (launched[0] == '\0')) {
        return 0; // an ordinary run, which is nearly all of them
    }

    /* The runtime exports APPIMAGE into the process and every child of it, so
       finding it set says nothing about who is running: a shell inside an
       AppImage terminal hands it to a distribution build of this engine
       launched from that shell, and answering there would point discovery at
       an unrelated application's folder and claim in the banner to be a
       packaging this is not.

       What settles it is whether this executable is the one inside the image.
       APPDIR names the mount; the binary has to be under it. */
    const char *mount = getenv("APPDIR");
    if ((mount == NULL) || (mount[0] == '\0') || (execDir == NULL) ||
        !PathIsInside(execDir, mount)) {
        Log_Info("Bundle: $APPIMAGE is set but this executable is not the one "
                 "inside it; ignoring it");
        return 0;
    }

    /* Back to the last separator, keeping it, so the result composes the same
       way an already-terminated directory does. Both spellings are accepted:
       the value comes from the environment, not from a path this built. */
    size_t len = strlen(launched);
    while (len > 0) {
        const char c = launched[len - 1];
        if ((c == '/') || (c == '\\')) {
            break;
        }
        len--;
    }
    if (len == 0) {
        return 0; // a bare filename names no directory
    }

    /* The directory is measured, not the whole value: a long filename under a
       short directory is a folder that fits and should be probed. Refused only
       when the answer itself cannot be held, and said out loud, because a
       silent skip drops the folder from the "tried" list the failure prints
       and that is the one folder the player is looking straight at. */
    if (len >= (size_t)maxLen) {
        Log_Warn("Bundle: the folder holding '%s' is too long to hold (>= %d); "
                 "not probing it for game data",
                 launched, (int)maxLen);
        return 0;
    }

    memcpy(out, launched, len);
    out[len] = '\0';

    if (outKindName != NULL) {
        *outKindName = "AppImage";
    }
    return 1;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
