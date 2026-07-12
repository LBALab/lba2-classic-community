#include "C_EXTERN.H"
#include "CONTROL.H"

#include "DIRECTORIES.H"

#include "IMAGE_LOAD.H"
#include "VIDEO_FIT.H"

#include <string.h>

#ifdef USE_MVIDEO_SMACKER
#include "AMBIANCE.H"
#include "INPUT.H"
#include "MESSAGE.H"
#include <AIL/VIDEO_AUDIO.H>
#include <AIL/VIDEO_AUDIO_RESAMPLE.H>
#include <FILEIO/SAVEPNG.H>
#include <SYSTEM/WINDOW.H> // AppActive

static int videoLogEnabled = 0;

void VideoLogEnable(int enable) {
    videoLogEnabled = (enable != 0) ? 1 : 0;
}

int VideoLogIsEnabled(void) {
    return videoLogEnabled;
}

#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOGPRINT.H>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "CONSOLE/CONSOLE.H"
#endif

char PathAcf[ADELINE_MAX_PATH];

char *ListAcf;
char *EndListAcf;

/* Harness one-shot screenshot — see Video_RequestCapture() in PLAYACF.H.
   When the path is non-empty, PlayAcf's playback loop fires SavePNG on the
   first composited frame and exits cleanly via the fin_play label. Stored
   at file scope unconditionally so the Video_RequestCapture symbol is
   available even in null-video-backend builds (where it's a no-op). */
static char s_video_capture_path[ADELINE_MAX_PATH] = "";

void Video_RequestCapture(const char *path) {
    if (!path || !path[0]) {
        s_video_capture_path[0] = '\0';
        return;
    }
    strncpy(s_video_capture_path, path, ADELINE_MAX_PATH - 1);
    s_video_capture_path[ADELINE_MAX_PATH - 1] = '\0';
}

#ifdef LBA_EDITOR
#define SIZE_BUFFER_SMACK 1000000L
U8 *BufferSmack;
#else
U8 *BufferSmack; // initialisé dans MEM.CPP
#endif

//---------------------------------------------------------------------------

/***************************************************************************\
|*									   *|
|*				INITS					   *|
|*									   *|
\***************************************************************************/

//---------------------------------------------------------------------------
void InitAcf(void) {
    char tmpFilePath[ADELINE_MAX_PATH];

    GetMoviePath(PathAcf, ADELINE_MAX_PATH, VIDEO_HQR_NAME);
    if (!ExistsFileOrDir(PathAcf)) {
        TheEndCheckFile(PathAcf);
    }

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    ListAcf = (char *)LoadMalloc_HQR(tmpFilePath, RESS_ACFLIST);
    if (!ListAcf) {
        TheEndCheckFile(tmpFilePath);
    }

    EndListAcf = ListAcf + LoadMallocFileSize;

#ifdef LBA_EDITOR
    // dispo Screen,ScreenAux,BufSpeak
    SmackRegisterMemory(Screen, (ModeDesiredX * ModeDesiredY + RECOVER_AREA) * 3);

    BufferSmack = (U8 *)Malloc(SIZE_BUFFER_SMACK);
    if (!BufferSmack)
        TheEnd(NOT_ENOUGH_MEM, "BufferSmack (InitAcf())");

    SmackRegisterMemory((void *)BufferSmack, SIZE_BUFFER_SMACK);
#else
    // TODO: Check if still useful when movie playback implemented
    // SmackRegisterMemory( MainBuffer, SizeOfMemAllocated ) ;
#endif
}

//---------------------------------------------------------------------------

char *PtrAcf;

char *GetAcfName(char *name) {
    char *ptr = name;
    char c;

    do /* saute cr/lf espace */
    {
        if (PtrAcf > EndListAcf) {
            PtrAcf--;
            return 0L;
        }

        c = *PtrAcf++;
    } while (c <= 32);

    do {
        *ptr++ = c;
        c = *PtrAcf++;
    } while (c > 32);

    if (PtrAcf > EndListAcf) {
        PtrAcf--;
        return 0L;
    }

    if (c == 26)
        PtrAcf--;

    *ptr++ = 0;
    return name;
}

//---------------------------------------------------------------------------
void ResetAcfList(void) {
    PtrAcf = ListAcf;
}

//---------------------------------------------------------------------------
// TODO: New implementation was not tested yet
S32 GetNumAcf(const char *name) {
    S32 n = 0;

    const char *nameDotAt = strchr(name, '.');
    const size_t nameNoDotLen = (nameDotAt)
                                    ? (nameDotAt - name)
                                    : strlen(name);

    char getname[15];
    PtrAcf = ListAcf;
    while (GetAcfName(getname)) {
        const char *getNameDotAt = strchr(getname, '.');
        const size_t getNameNoDotLen = (getNameDotAt)
                                           ? (getNameDotAt - getname)
                                           : strlen(getname);

        const bool sameLen = (nameNoDotLen == getNameNoDotLen);
        if (sameLen && !strncasecmp(name, getname, nameNoDotLen)) {
            return n;
        }

        n++;
    }

    return -1;
}

//---------------------------------------------------------------------------

void PlayAllAcf(void) {
    S32 firsttime = TRUE;
    S32 n = 0;
    S32 var = FLAG_ACF;
    char getname[13];

    if (!ListAcf || !EndListAcf || ListAcf >= EndListAcf)
        return;

    PtrAcf = ListAcf;

#if defined(LBA_EDITOR) || defined(CDROM)
    FadeOutVolumeMusic();
#endif

    while (GetAcfName(getname)) {
#ifdef LBA_EDITOR
        if ((ListVarGame[var].Value & (1 << n))
#else
        if ((ListVarGame[var] & (1 << n))
#endif
                AND(strncasecmp(getname, "INTRO", 5))) // on ne rejoue pas l'intro
        {
            if (!FlagFade) {
                if (firsttime) {
                    FadeToBlackAndSamples(PtrPal);
                    firsttime = FALSE;
                } else
                    FadeToBlack(PtrPal);
            }

            FlagFade = TRUE;

            PlayAcf(getname);
            //			if( PlayAcf( getname ) == K_ESC )	break ;
        }

        n++;

        if (n >= 16) {
            var++;
            n = 0;
        }
    }

#if defined(LBA_EDITOR) || defined(CDROM)
    FadeInVolumeMusic();
#endif

    FirstTime = AFF_ALL_FLIP;
}

//---------------------------------------------------------------------------

//extern	void	CopyFrameDoubleXY( U8 *ptrsrc ) ;

/***************************************************************************\
|*									   *|
|*				MAIN					   *|
|*									   *|
\***************************************************************************/

#ifdef USE_MVIDEO_SMACKER
/* Decode one Smacker frame (320×200 source) and nearest-neighbour upscale it
   into a dstW×dstH rectangle whose top-left is destOrigin inside the host
   buffer (Log), with row stride destStride. PlayAcf sizes and positions that
   rectangle once (Video_ComputeCineRect) and pre-clears the framebuffer, so
   this only paints the video rect: everything outside it (letterbox/pillarbox
   bands, sidebars) stays black without a per-frame re-clear.

   Vertical: one divide per destination row picks the source row. Horizontal:
   a Bresenham-style accumulator advances the source column without a divide
   per pixel; this runs every video frame, so the per-pixel divide the
   image-load stretch can afford would be wasteful here. Both axes use the
   floor mapping floor(d*src/dst); at the legacy 2x (dstW=640, dstH=400) that
   duplicates every source pixel/row exactly, byte-identical to the old fixed
   2× loop. */
static void ReadNextVideoFrame(smk smkObject, U8 *destOrigin, U32 destStride, S32 dstW, S32 dstH, U8 **pal) {
    const S32 srcW = 320;
    const S32 srcH = 200;
    U8 *frame = (U8 *)smk_get_video(smkObject);
    *pal = (U8 *)smk_get_palette(smkObject);

    for (S32 dy = 0; dy < dstH; dy++) {
        const S32 srcY = (dy * srcH) / dstH;
        const U8 *sourceRow = frame + srcY * srcW;
        U8 *targetRow = destOrigin + (U32)dy * destStride;
        S32 srcX = 0;
        S32 acc = 0;
        for (S32 dx = 0; dx < dstW; dx++) {
            targetRow[dx] = sourceRow[srcX];
            acc += srcW;
            while (acc >= dstW) {
                acc -= dstW;
                srcX++;
            }
        }
    }
}

/* Push current frame's audio for requested tracks; mix if numTracks > 1. Resample to stream rate with duration-correct resampling. */
static void PushCurrentFrameVideoAudio(smk smkObject, const int *tracksToPlay, int numTracks, const U8 *a_d, const unsigned long *a_r, const U8 *a_c, double usf) {
    if (numTracks <= 0)
        return;
    const unsigned long kMaxFrameBytes = 128 * 1024;
    static U8 mixBuf[128 * 1024];
    unsigned long sizeBytes[7];
    const unsigned char *chunk[7];
    int first = tracksToPlay[0];
    unsigned long targetRate = a_r[first];
    if (targetRate == 0)
        return;
    int chans = (a_c[first] >= 2) ? 2 : 1;
    int is16 = (a_d[first] == 16) ? 1 : 0;
    /* Bytes per frame at stream rate (fallback when a frame has no audio) */
    double frameSec = (usf > 0.0) ? (usf / 1e6) : (1.0 / 30.0);
    unsigned long expectedBytes = (unsigned long)((double)targetRate * frameSec * (double)chans * (is16 ? 2.0 : 1.0));
    if (expectedBytes > kMaxFrameBytes)
        expectedBytes = kMaxFrameBytes;

    unsigned long maxSize = 0;
    for (int i = 0; i < numTracks; i++) {
        int t = tracksToPlay[i];
        sizeBytes[i] = smk_get_audio_size(smkObject, (unsigned char)t);
        chunk[i] = smk_get_audio(smkObject, (unsigned char)t);
        if (chunk[i] && sizeBytes[i] > 0) {
            unsigned long srcRate = GetEffectiveTrackRate(t, a_r, a_c, targetRate);
            unsigned long effectiveBytes = (srcRate == targetRate) ? sizeBytes[i] : (unsigned long)((double)sizeBytes[i] * (double)targetRate / (double)srcRate);
            if (effectiveBytes > maxSize)
                maxSize = effectiveBytes;
        }
    }
    /* Use chunk-based size so we push actual data; fall back to expected size if frame is silent */
    if (maxSize == 0)
        maxSize = expectedBytes;
    if (maxSize == 0)
        return;
    if (maxSize > kMaxFrameBytes)
        maxSize = kMaxFrameBytes;

    unsigned long mixSamples = maxSize / (is16 ? 2u : 1u);

    memset(mixBuf, is16 ? 0 : 128, (size_t)maxSize);
    int numAdded = 0;
    for (int i = 0; i < numTracks; i++) {
        if (!chunk[i] || sizeBytes[i] == 0)
            continue;
        ResampleAndAddToMix(GetEffectiveTrackRate(tracksToPlay[i], a_r, a_c, targetRate), targetRate, chunk[i], sizeBytes[i], mixBuf, mixSamples, is16);
        numAdded++;
    }
    /* Scale mix when multiple tracks were added to avoid clipping distortion (grain). Use rounding to reduce ticking from truncation. */
    if (numAdded > 1) {
        int half = numAdded / 2;
        if (is16) {
            short *mix = (short *)mixBuf;
            for (unsigned long j = 0; j < mixSamples; j++) {
                int s = (int)mix[j];
                s = (s + (s >= 0 ? half : -half)) / numAdded;
                if (s > 32767)
                    s = 32767;
                if (s < -32768)
                    s = -32768;
                mix[j] = (short)s;
            }
        } else {
            U8 *mix = mixBuf;
            for (unsigned long j = 0; j < mixSamples; j++) {
                int s = (int)(mix[j]) - 128;
                s = (s + (s >= 0 ? half : -half)) / numAdded + 128;
                if (s < 0)
                    s = 0;
                if (s > 255)
                    s = 255;
                mix[j] = (U8)s;
            }
        }
    }
    PushVideoAudio(mixBuf, (U32)maxSize);
}
#endif

//---------------------------------------------------------------------------

S32 PlayAcf(const char *name) {
    if (Control_IsVerbose())
        fprintf(stderr, "[control] modal: PlayAcf '%s' -- cutscene, blocks --exit\n", name);
#ifdef USE_MVIDEO_SMACKER
    {
        static int s_playacf_playing = 0;
        if (s_playacf_playing) {
            /* Re-entry guard: avoid same video starting repeatedly (e.g. track/life script every frame). */
            return 0;
        }
        s_playacf_playing = 1;

        S32 n = GetNumAcf(name);
        if (n == -1) {
            if (VideoLogIsEnabled())
                SDL_Log("[VIDEO] PlayAcf: video name '%s' not found in ACF list", name);
            s_playacf_playing = 0;
            return 0;
        }

        ListVarGame[FLAG_ACF + (n / 16)] |= (S16)(1 << (n & 15));

        StopMusic();
        HQ_PauseSamples();

        U8 *decompbuf = (U8 *)LoadMalloc_HQR(PathAcf, n);
        U32 size = LoadMallocFileSize;
        if (!size) {
            if (VideoLogIsEnabled())
                SDL_Log("[VIDEO] PlayAcf: LoadMalloc_HQR failed for index %d", (int)n);
            TheEnd(PROGRAM_OK, MessageNoCD);
        }

        smk smkObject = smk_open_memory(decompbuf, (unsigned long)size);
        if (!smkObject) {
            if (VideoLogIsEnabled())
                SDL_Log("[VIDEO] PlayAcf: smk_open_memory failed for index %d (size %u)", (int)n, (unsigned)size);
            Free(decompbuf);
            TheEnd(PROGRAM_OK, MessageNoCD);
        }

        SetBlackPal();
        BoxReset();
        Cls();
        BoxUpdate();
        InitWaitNoKey();
        InitWaitNoInput(I_JOY | I_FIRE | I_MENUS);

        unsigned long w = 0, h = 0;
        unsigned long f = 0;
        double usf = 0.0;
        smk_info_all(smkObject, NULL, &f, &usf);
        smk_info_video(smkObject, &w, &h, NULL);
        if (w != 320 || h != 200) {
            smk_close(smkObject);
            Free(decompbuf);
            TheEnd(PROGRAM_FAIL, "Unsupported video format (320x200 only)");
        }

        U8 a_t = 0, a_c[7] = {0}, a_d[7] = {0};
        unsigned long a_r[7] = {0};
        smk_info_audio(smkObject, &a_t, a_c, a_d, a_r);

        S32 smackflags = SMK_AUDIO_TRACK_0;
        if (!strncasecmp(name, "INTRO", 5) && FlagSpeak) {
            switch (LanguageCD) {
            case 0:
                smackflags |= SMK_AUDIO_TRACK_3;
                break;
            case 1:
                smackflags |= SMK_AUDIO_TRACK_1;
                break;
            case 2:
                smackflags |= SMK_AUDIO_TRACK_2;
                break;
            default:
                smackflags |= SMK_AUDIO_TRACK_3;
                break;
            }
        }

        /* .smk uses track 0 = music, tracks 1/2/3 = FR/DE/EN voice; we mix all requested tracks into one stream so intro has both music and voice. */
        int tracksToPlay[7];
        int numTracks = 0;
        for (int i = 0; i < 7; i++) {
            if ((a_t & (1 << i)) && (smackflags & (1 << i)))
                tracksToPlay[numTracks++] = i;
        }
        if (numTracks > 0) {
            int first = tracksToPlay[0];
            S32 chans = (a_c[first] >= 2) ? 2 : 1;
            S32 is16 = (a_d[first] == 16) ? 1 : 0;
            if (!StartVideoAudio((S32)(U32)a_r[first], chans, is16)) {
                /* non-fatal: continue without video audio */
                numTracks = 0;
            }
        }

        if (VideoLogIsEnabled()) {
            if (!strncasecmp(name, "INTRO", 5) && numTracks > 0) {
                static const char *voiceLang[] = {"FR", "DE", "EN"};
                const char *lang = (LanguageCD >= 0 && LanguageCD <= 2) ? voiceLang[LanguageCD] : "FR";
                if (numTracks > 1) {
                    SDL_Log("[VIDEO] PlayAcf: playing '%s' index %d from %s (LanguageCD=%d, voice=%s track %d)",
                            name, (int)n, PathAcf, LanguageCD, lang, tracksToPlay[1]);
                } else {
                    SDL_Log("[VIDEO] PlayAcf: playing '%s' index %d from %s (LanguageCD=%d, voice=off)",
                            name, (int)n, PathAcf, LanguageCD);
                }
            } else {
                SDL_Log("[VIDEO] PlayAcf: playing '%s' index %d from %s", name, (int)n, PathAcf);
            }
        }

        smk_enable_all(smkObject, SMK_VIDEO_TRACK | a_t);
        smk_first(smkObject);

        /* Frame interval in ms from file (usf = microseconds per frame) for accurate pacing */
        U32 frameMs = (U32)(usf / 1000.0);
        if (frameMs == 0)
            frameMs = 67; /* ~15 fps */

        /* Pre-fill audio buffer before starting playback (like lba2-classic-ida's SoLoud deque).
     * Stream is created paused; we fill it then resume so the device never underruns.
     * Formula: ~2s for long videos (60+ fps), ~1s for medium (30+), ~0.5s for short; avoids
     * underrun at start while not delaying playback too much on brief clips. */
        unsigned long prebufFrames = 0;
        if (numTracks > 0 && f > 8u) {
            prebufFrames = (f > 60u) ? 30u : (f > 30u) ? 25u
                                         : (f > 15u)   ? 15u
                                                       : (unsigned long)(f / 2);
            if (prebufFrames > f)
                prebufFrames = f;
            for (unsigned long p = 0; p < prebufFrames; p++) {
                PushCurrentFrameVideoAudio(smkObject, tracksToPlay, numTracks, a_d, a_r, a_c, usf);
                smk_next(smkObject);
            }
            smk_first(smkObject);
            ResumeVideoAudio();
        } else if (numTracks > 0) {
            ResumeVideoAudio();
        }

        /* Cinematic framing: size and place the video rectangle once, then
           paint the whole Log black so every band outside that rectangle
           (letterbox/pillarbox, sidebars at widths > 640) starts and stays
           black, and the per-frame blit only touches the video rect. The frame
           is written directly into Log at the centred origin with proper Log
           row-stride; no intermediate malloc'd buffer / CopyBlock (which
           assumes equal src/dst stride and would smear at any width > 640).

           VideoFullScreen selects the policy (see Video_ComputeCineRect):
           0 = legacy centred letterbox (fixed 2× → 640×400), 1 = scale the
           320×200 source up uniformly to fill the frame without distortion.
           Both collapse to the identical 640×400 layout at 640×480. */
        S32 dstW, dstH, dstX0, dstY0;
        Video_ComputeCineRect((S32)ModeDesiredX, (S32)ModeDesiredY, VideoFullScreen,
                              &dstW, &dstH, &dstX0, &dstY0);
        memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        U8 *cineOrigin = (U8 *)Log + (size_t)dstY0 * (size_t)ModeDesiredX + (size_t)dstX0;

        U8 *pal = NULL;
        U32 videoStartTicks = 0;
        int videoFrozen = 0; // console open or focus lost -> hold the frame
        int everFocused = 0; // set once the window is focused; a never-focused run
                             // (e.g. a headless capture) then never freezes
        U32 freezeStart = 0; // SDL_GetTicks when the freeze began
        for (unsigned long c = 0; c < f; c++) {
            /* Push this frame's audio (skip for c < prebufFrames, already in stream from pre-fill) */
            if (numTracks > 0 && c >= prebufFrames) {
                PushCurrentFrameVideoAudio(smkObject, tracksToPlay, numTracks, a_d, a_r, a_c, usf);
            }

            ReadNextVideoFrame(smkObject, cineOrigin, ModeDesiredX, dstW, dstH, &pal);
            if (pal) {
                // The video is on screen with its own palette; clear FlagBlackPal so
                // the engine state matches (SetBlackPal at entry left it TRUE). The
                // tail FadeToBlack() below early-returns when FlagBlackPal is set, so
                // without this the video would cut, not fade, at its end.
                FlagBlackPal = FALSE;
                PaletteSync(pal);
            }

            /* Harness one-shot capture — saves a composited frame past the
               cinematic's initial fade-in (frame 0 is uniformly black for the
               INTRO smk; we pick a later frame so the golden is meaningful
               regardless of whether audio init / pre-fill is enabled). The PNG is
               written here from Log, so the fade-to-black at fin_play doesn't
               affect it. */
            if (s_video_capture_path[0] && c >= 25u) {
                SavePNG(s_video_capture_path, Log, (S32)ModeDesiredX,
                        (S32)ModeDesiredY, pal ? pal : PtrPal);
                s_video_capture_path[0] = '\0';
                goto fin_play;
            }
            BoxStaticAdd(0, 0, (S32)(ModeDesiredX - 1), (S32)(ModeDesiredY - 1));
            BoxUpdate();

            /* Pace to real time with SDL so speed is correct and sleep reduces stutter */
            if (c == 0)
                videoStartTicks = SDL_GetTicks();
            U32 now = SDL_GetTicks();
            U32 nextAt = videoStartTicks + (c + 1) * frameMs;
            for (;;) {
                MyGetInput();
                ManageTime();
                if (MyKey == K_ESC || (Input & I_MENUS))
                    goto fin_play;
                if (AppActive)
                    everFocused = 1;
                /* Freeze the cinematic while the console is open or the window has
                   lost focus (only once it was focused: a never-focused window such
                   as a headless capture keeps playing rather than freezing at frame
                   0). Hold the current frame and pause its audio. The frame clock is
                   wall-clock based (SDL_GetTicks), so re-base it on resume, otherwise
                   playback fast-forwards to catch up and the (paused, then resumed)
                   audio lags the video. */
                if (Console_IsOpen() || (everFocused && !AppActive)) {
                    if (!videoFrozen) {
                        if (numTracks > 0)
                            PauseVideoAudio();
                        if (VideoLogIsEnabled())
                            SDL_Log("[VIDEO] frozen (console open or unfocused)");
                        freezeStart = SDL_GetTicks();
                        videoFrozen = 1;
                    }
                    BoxStaticAdd(0, 0, (S32)(ModeDesiredX - 1), (S32)(ModeDesiredY - 1));
                    BoxUpdate();
                    continue;
                }
                if (videoFrozen) {
                    U32 pausedMs = SDL_GetTicks() - freezeStart;
                    videoStartTicks += pausedMs; // shift the whole schedule forward
                    nextAt += pausedMs;
                    if (numTracks > 0)
                        ResumeVideoAudio();
                    if (VideoLogIsEnabled())
                        SDL_Log("[VIDEO] resumed (rebased +%u ms)", (unsigned)pausedMs);
                    videoFrozen = 0;
                }
                now = SDL_GetTicks();
                if (now >= nextAt)
                    break;
                SDL_Delay(1); /* sleep 1ms to avoid busy-spin, re-check input each time */
            }

            smk_next(smkObject);
        }

    fin_play:
        StopVideoAudio();

        /* `pal` points into the smk struct (smk_get_palette returns an inline member),
           so copy it out before smk_close frees that struct -- FadeToBlack below reads
           all 768 bytes. Fall back to PtrPal if the first frame never decoded. */
        U8 lastFramePal[768];
        memcpy(lastFramePal, pal ? pal : PtrPal, sizeof(lastFramePal));

        smk_close(smkObject);
        Free(decompbuf);

        /* Original contract: fade the video out to black and leave the screen black,
           so the scene load stays hidden and the caller's fade-in (FlagFade) reveals
           the next scene. The port used to restore the pre-video scene bright here
           ("so we don't leave a black screen"), which un-blacked the screen under
           every LM_PLAY_ACF / TM_PLAY_ACF / menu-FMV caller and flashed the scene one
           frame before the fade-in (#404). */
        FadeToBlack(lastFramePal);
        Cls();
        BoxStaticFullflip();
        SetBlackPal();

        HQ_ResumeSamples();
        InitWaitNoKey();
        InitWaitNoInput(I_MENUS);
        s_playacf_playing = 0;
        return MyKey;
    }
#else
    fprintf(stderr, "PlayAcf: '%s' skipped (video backend is null; build with -DMVIDEO_BACKEND=smacker for video)\n", name);
    return 0;
#endif
}

//---------------------------------------------------------------------------
