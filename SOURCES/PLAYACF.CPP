#include "C_EXTERN.H"
#include "CONTROL.H"

#include "DIRECTORIES.H"

#include "IMAGE_LOAD.H"

#include <string.h>

#ifdef USE_MVIDEO_SMACKER
#include "AMBIANCE.H"
#include "INPUT.H"
#include "MESSAGE.H"
#include <AIL/VIDEO_AUDIO.H>
#include <AIL/VIDEO_AUDIO_RESAMPLE.H>
#include <FILEIO/SAVEPNG.H>

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
/* Decode and upscale one Smacker frame (320×200 source, nearest-neighbour
   2× in both axes, 400 rows of video vertically centred inside a 480-row
   "image area" — 40-row black letterbox top and bottom).

   destOrigin / destStride identify the top-left of the 640×480 image area
   inside a wider host buffer (Log).  Writing directly into Log with the
   correct stride keeps the wider sidebars untouched.  At ModeDesiredX=640
   this collapses to "destStride==640", matching the historical layout
   when the buffer was a malloc'd 640×480 scratch. */
static void ReadNextVideoFrame(smk smkObject, U8 *destOrigin, U32 destStride, U8 **pal) {
    const U32 width = 320;
    const U32 height = 200;
    const U32 imageWidth = IMAGE_AUTHORED_WIDTH;   // 640 — upscaled video span
    const U32 imageHeight = IMAGE_AUTHORED_HEIGHT; // 480 — image-area height
    const U32 videoStartY = 40;
    U8 *frame = (U8 *)smk_get_video(smkObject);
    *pal = (U8 *)smk_get_palette(smkObject);

    /* Clear the 640×480 image area each frame so the letterbox bands
       (rows 0..39 and 440..479) stay black, and any sidebar slop from
       previous frame painting is wiped before the upscale fills the
       central 640×400. Sidebars outside this rectangle are pre-cleared
       once in PlayAcf before the playback loop. */
    for (U32 y = 0; y < imageHeight; y++) {
        memset(destOrigin + y * destStride, 0, imageWidth);
    }

    U8 *targetRow = destOrigin + videoStartY * destStride;
    for (U32 y = 0; y < height; y++) {
        const U8 *sourceRow = frame + y * width;
        for (U32 x = 0; x < width; x++) {
            targetRow[2 * x] = sourceRow[x];
            targetRow[2 * x + 1] = sourceRow[x];
        }
        memcpy(targetRow + destStride, targetRow, imageWidth);
        targetRow += destStride * 2;
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

        /* Save current screen and palette so we can restore after playback (fix black screen). */
        const U32 logSize = (U32)(ModeDesiredX * ModeDesiredY);
        const U32 palSize = 768;
        U8 *savedLog = (U8 *)malloc(logSize + palSize);
        if (!savedLog) {
            if (VideoLogIsEnabled())
                SDL_Log("[VIDEO] PlayAcf: malloc failed for screen save");
            HQ_ResumeSamples();
            s_playacf_playing = 0;
            return 0;
        }
        U8 *savedPal = savedLog + logSize;
        if (Log && PtrPal) {
            memcpy(savedLog, Log, logSize);
            memcpy(savedPal, PtrPal, palSize);
        }

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

        /* Cinematic letterbox: paint the wider Log to black once so the
           sidebars at widths > 640 don't leak whatever scene rendered
           before the cutscene started. Then each frame writes the 320×200
           Smacker upscale (2× → 640×400) directly into Log at the centred
           image-area origin, with proper Log row-stride. No intermediate
           malloc'd buffer / CopyBlock — earlier versions blitted from a
           640-stride scratch into a wider Log via CopyBlock, but
           CopyBlock assumes the same stride for src and dst (uses
           TabOffLine[1] both sides) and would have produced a diagonal
           smear at any width > 640. At MDX=640 this is byte-identical
           to the old path. */
        const S32 cineX0 = ((S32)ModeDesiredX - IMAGE_AUTHORED_WIDTH) / 2;
        const S32 cineY0 = ((S32)ModeDesiredY - IMAGE_AUTHORED_HEIGHT) / 2;
        memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        U8 *cineOrigin = (U8 *)Log + (size_t)cineY0 * (size_t)ModeDesiredX + (size_t)cineX0;

        U8 *pal = NULL;
        U32 videoStartTicks = 0;
        static int s_wasConsoleOpen = 0;
        for (unsigned long c = 0; c < f; c++) {
            /* Push this frame's audio (skip for c < prebufFrames, already in stream from pre-fill) */
            if (numTracks > 0 && c >= prebufFrames) {
                PushCurrentFrameVideoAudio(smkObject, tracksToPlay, numTracks, a_d, a_r, a_c, usf);
            }

            ReadNextVideoFrame(smkObject, cineOrigin, ModeDesiredX, &pal);
            if (pal)
                PaletteSync(pal);

            /* Harness one-shot capture — saves the first composited frame
               and exits cleanly via fin_play. The cleanup at fin_play
               restores Log from savedLog, but the on-disk PNG already
               holds the cinematic frame. */
            if (s_video_capture_path[0]) {
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
                /* Pause video while console is open: keep presenting current frame until they close it.
         * Pause video audio so the buffer doesn't drain and cause a gap when console closes. */
                if (Console_IsOpen()) {
                    if (!s_wasConsoleOpen && numTracks > 0) {
                        PauseVideoAudio();
                        if (VideoLogIsEnabled())
                            SDL_Log("[VIDEO] Audio paused (console open)");
                        s_wasConsoleOpen = 1;
                    }
                    BoxStaticAdd(0, 0, (S32)(ModeDesiredX - 1), (S32)(ModeDesiredY - 1));
                    BoxUpdate();
                    continue;
                }
                if (s_wasConsoleOpen && numTracks > 0) {
                    ResumeVideoAudio();
                    if (VideoLogIsEnabled())
                        SDL_Log("[VIDEO] Audio resumed (console closed)");
                    s_wasConsoleOpen = 0;
                }
                now = SDL_GetTicks();
                if (now >= nextAt)
                    break;
                SDL_Delay(1); /* sleep 1ms to avoid busy-spin, re-check input each time */
            }

            smk_next(smkObject);
        }

    fin_play:
        s_wasConsoleOpen = 0;
        StopVideoAudio();
        smk_close(smkObject);
        Free(decompbuf);

        /* Restore screen and palette so we don't leave a black screen. */
        if (savedLog && Log && PtrPal) {
            memcpy(Log, savedLog, logSize);
            memcpy(PtrPal, savedLog + logSize, palSize);
            PaletteSync(PtrPal);
        }
        if (savedLog)
            free(savedLog);

        BoxStaticFullflip();
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
