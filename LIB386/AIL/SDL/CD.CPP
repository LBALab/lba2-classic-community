#include "CD_SDL.H"
#include "STREAM_SDL.H"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#ifndef ADELINE_MAX_PATH
#define ADELINE_MAX_PATH 512
#endif

/* European layout: first CD track is 6 (Track06.wav). US layout: first is 2 (Track02.wav). */
#define FIRST_CD_TRACK_EU 6
#define FIRST_CD_TRACK_US 2

static S32 cdVolume = 0;
static bool cdPlaying = false;

static bool FileExists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

/* Try multiple case variations of the music directory for case-sensitive
   filesystems.  Returns true and fills outPath if a match is found. */
static bool BuildCDTrackPath(char *outPath, S32 pathMax, S32 trackOneBased) {
    static const char *musicDirs[] = {"music", "Music", "MUSIC"};
    char *cwd = SDL_GetCurrentDirectory();
    if (!cwd) {
        outPath[0] = '\0';
        return false;
    }
    for (int i = 0; i < 3; i++) {
        int n = snprintf(outPath, pathMax, "%s%s/Track%02d.wav", cwd, musicDirs[i], trackOneBased);
        if (n >= 0 && n < pathMax && FileExists(outPath)) {
            SDL_free(cwd);
            return true;
        }
    }
    SDL_free(cwd);
    outPath[0] = '\0';
    return false;
}

const char *OpenCD(char *volume_name) {
    (void)volume_name;
    cdVolume = 127;
    cdPlaying = false;
    return "D";
}

void PlayCD(S32 track) {
    char path[ADELINE_MAX_PATH];
    /* track is 0-based from the game (PtrTrackCD[num]&MUSIC - FirstCDTrack). */
    /* European: track 0 -> Track06.wav (6), track 1 -> 7, etc. US: track 0 -> Track02.wav (2). */
    if (BuildCDTrackPath(path, sizeof(path), FIRST_CD_TRACK_EU + track)) {
        if (MusicLogIsEnabled())
            SDL_Log("[CD] PlayCD track=%d path=%s", track, path);
        PlayStream(path);
        ChangeVolumeStream(cdVolume);
        cdPlaying = true;
        return;
    }
    if (BuildCDTrackPath(path, sizeof(path), FIRST_CD_TRACK_US + track)) {
        if (MusicLogIsEnabled())
            SDL_Log("[CD] PlayCD track=%d path=%s", track, path);
        PlayStream(path);
        ChangeVolumeStream(cdVolume);
        cdPlaying = true;
    }
}

void ChangeVolumeCD(S32 volume) {
    cdVolume = volume;
    if (MusicLogIsEnabled())
        SDL_Log("[CD] ChangeVolumeCD vol=%d cdPlaying=%d", volume, cdPlaying ? 1 : 0);
    if (cdPlaying)
        ChangeVolumeStream(cdVolume);
}

S32 GetVolumeCD() {
    return cdVolume;
}

void StopCD() {
    if (MusicLogIsEnabled())
        SDL_Log("[CD] StopCD");
    StopStream();
    cdPlaying = false;
}

void PauseCD() {
    if (MusicLogIsEnabled())
        SDL_Log("[CD] PauseCD");
    PauseStream();
}

void ResumeCD() {
    if (MusicLogIsEnabled())
        SDL_Log("[CD] ResumeCD");
    ResumeStream();
}

S32 IsCDPlaying() {
    return (cdPlaying && IsStreamPlaying()) ? TRUE : FALSE;
}
