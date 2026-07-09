#include "C_EXTERN.H"
#include "DIRECTORIES.H"
#include "MUSIC.H"

#include <AIL/STREAM.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/WINDOW.H>

#define BCL_FADE 50
#define FADE_DELAY 200

/* GiveBalance (pan/volume math) lives in AUDIO_BALANCE.CPP so it can be
   unit-tested without linking the audio backend.  The DISTANCE_SAMPLE_*
   thresholds moved with it. */

#ifdef CDROM
extern S32 FlagVoiceCD;
#endif

// Tableau d'indirection des palettes se trouvant dans RESS.HQR
// (voir le fichier f:\projet\lba2\datas\ress.lst)

U8 **ListPalette[MAX_PALETTES] = {
    &PtrPalNormal,  // Palette Normal
    &PtrPalCurrent, // Palette Courante
    &PtrPalEclair,  // Palette Eclair
    &PtrPalBlack    // Palette Noire
};

S32 RestartRainSample = FALSE;

/*──────────────────────────────────────────────────────────────────────────*/

HQR_GET_CALLBACK DelBlocHQRSample;

void DelBlocHQRSample(char *name, S32 index) {
    (void)name;
    (void)index;

    /* Original called HQ_StopSample() here ("methode violente") which
           killed ALL playing voices on every HQR cache eviction.  The SDL
           backend copies sample data into each voice (malloc+memcpy), so
           voices are independent of HQR cache entries.  Removing the
           blanket stop fixes voice thrashing that silenced thunder and
           other concurrent SFX.  Rain is restarted via RestartRainSample. */
    RestartRainSample = TRUE;

    // mise à 0 du pointeur de fonction appelé par HQR_Get()
    HQRGetDelFunc = NULL;
}

/*══════════════════════════════════════════════════════════════════════════*
                █▀▀▀█ █▄ ▄█ █▀▀█   █    █▀▀▀█ ██▄ █ █▀▀▀▀ █▀▀▀▀
                ██▀▀█ ██▀ █ ██▀▀█  ██   ██▀▀█ ██▀██ ██    ██▀▀
                ▀▀  ▀ ▀▀  ▀ ▀▀▀▀▀  ▀▀   ▀▀  ▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
void InitParmSample(void) {
    ParmSampleVolume = 127; // plein pot
    ParmSampleDecalage = 0;
    ParmSampleFrequence = 0;
}

/*──────────────────────────────────────────────────────────────────────────*/

U32 HQ_MixSample(S32 numsample, S32 frequence, S32 decalage, S32 repeat, S32 pan, S32 volume) {
    U32 timesample;
    U32 retvalue = -1;
    U8 *ptr;
    S32 sampleSizeBytes = 0;

    if (SamplesEnable AND numsample != -1) {
        HQR_GET_CALLBACK *memoerrorfunc;

        timesample = IsSamplePlaying(numsample);

        if (timesample
                AND(TimerSystemHR - timesample) < SAMPLE_TIME_REPEAT) {
            InitParmSample();
            return 0;
        }
        SaveTimer();

        // tient compte du ParmSampleVolume
        if (ParmSampleVolume != 127) {
            volume = RegleTrois(0, ParmSampleVolume, 127, volume);
        }

        // tient compte du SampleVolume
        volume = RegleTrois(0, SampleVolume, 127, volume);

        if (ParmSampleFrequence)
            frequence = ParmSampleFrequence;
        if (ParmSampleDecalage)
            decalage = ParmSampleDecalage;

        if (decalage) {
            frequence = frequence - decalage / 2 + MyRnd(decalage);
        }

        // si pas assez de memoire, tant pis, on joue pas
        // le sample, mais on ne sort pas du prog !
        memoerrorfunc = HQRGetErrorFunc;
        HQRGetErrorFunc = NULL;

        HQRGetDelFunc = DelBlocHQRSample;

        if (numsample & 0xFF000000)
            numsample = (numsample >> 8) & 0xFFFF;

        // Blindage inutile
        ptr = (U8 *)GivePtrSample(numsample, &sampleSizeBytes);
        if (ptr) {
            retvalue = PlaySample(ptr, sampleSizeBytes, numsample, frequence, repeat, volume, pan);
        } else {
            LogPrintf("Can't play sample %d: not enough memory.\n", numsample);
        }

        HQRGetDelFunc = NULL;
        HQRGetErrorFunc = memoerrorfunc;

        RestoreTimer();
    }

    InitParmSample();

    return retvalue;
}

/*──────────────────────────────────────────────────────────────────────────*/
U32 HQ_RestartSample(S32 numsample, S32 frequence, S32 repeat, S32 pan, S32 volume) {
    U32 retvalue = -1;
    U8 *ptr;
    S32 sampleSizeBytes = 0;

    if (SamplesEnable AND numsample != -1) {
        SaveTimer();

        HQRGetDelFunc = DelBlocHQRSample;

        // Blindage inutile
        ptr = (U8 *)GivePtrSample(numsample, &sampleSizeBytes);
        if (ptr) {
            retvalue = PlaySample(ptr, sampleSizeBytes, numsample, frequence, repeat, volume, pan);
        }

        HQRGetDelFunc = NULL;

        RestoreTimer();
    }

    InitParmSample();

    return retvalue;
}

/*══════════════════════════════════════════════════════════════════════════*/
void HQ_StopSample(void) {
    StopSamples();    // stop all samples
    FadeInSamples(0); // clear fadeout flag
}

/*══════════════════════════════════════════════════════════════════════════*/
void HQ_PauseSamples(void) {
    while (FadeOutSamples(200))
        ; // 1/5 s
    PauseSamples();
}
/*══════════════════════════════════════════════════════════════════════════*/
void HQ_ResumeSamples(void) {
    ResumeSamples();
    while (FadeInSamples(200) != 200)
        ; // 1/5 s
}

/*══════════════════════════════════════════════════════════════════════════*/
/* Pause all audio while the game window is not focused (samples, music, and a
   playing cutscene), mirroring the original PauseMiles/ResumeMiles which paused
   the whole output. Registered with the window layer (InstallFocusAudioHooks),
   which fires these on the focus edge with a once-per-cycle guard.

   These are device-level pauses, NOT the engine's logical PauseMusic/PauseSamples
   pause. SuspendStreamOutput pauses the music output without touching the parked
   resume position, so losing focus while an outer PauseStream is active (e.g.
   gameplay music parked while the Options menu theme plays) does not clobber that
   snapshot; ResumeStreamOutput resumes only if the stream is not logically paused.
   Samples use the ref-counted Pause/ResumeSamples, which compose with a menu that
   already paused them. A playing cutscene is handled in its own loop (PLAYACF.CPP
   freezes the frame and its audio on focus loss), not here. Fading is deliberately
   avoided: focus loss locks the timer first (TIMER.CPP runs before WINDOW.CPP in
   the event pump), so a fade that waits on timer progress would never complete. */
static void FocusLoseAudio(void) {
    PauseSamples();
    SuspendStreamOutput();
}

static void FocusGainAudio(void) {
    ResumeStreamOutput();
    ResumeSamples();
}

void InstallFocusAudioHooks(void) {
    SetAppFocusAudioHooks(FocusLoseAudio, FocusGainAudio);
}
/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
/* GiveBalance moved to AUDIO_BALANCE.CPP (see test_ambiance_balance). */

/*──────────────────────────────────────────────────────────────────────────*/

U32 HQ_3D_MixSample(S32 numsample, S32 frequence, S32 decalage,
                    S32 repeat,
                    S32 x, S32 y, S32 z) {
    U32 pan;
    S32 volume, distance;
    S32 retvalue = 0;

    if (SamplesEnable) {
        PtrProjectPoint(x, y, z);

        if (CubeMode == CUBE_INTERIEUR)
            distance = Distance2D((S32)ModeDesiredX / 2, (S32)ModeDesiredY / 2, Xp, Yp);
        else
            distance = Distance3D(x, y, z, CameraX, CameraY, CameraZ);

        //              if( distance<DISTANCE_SAMPLE_MAX_EXT )
        {
            if (GiveBalance(Xp, Yp, distance, &pan, &volume)) {
                retvalue = HQ_MixSample(numsample, frequence, decalage, repeat, pan, volume);
            }
        }
    }

    if (!retvalue AND !repeat) // sample Always
    {
        retvalue = numsample << 8;
    }

    InitParmSample();

    return (retvalue);
}

/*──────────────────────────────────────────────────────────────────────────*/

void HQ_ChangePanSample(S32 numsample, S32 volume, U32 pan) {
    if (SamplesEnable) {
        if (ParmSampleVolume != 127) {
            volume = RegleTrois(0, ParmSampleVolume, 127, volume);
            ParmSampleVolume = 127;
        }

        // tient compte du SampleVolume
        volume = RegleTrois(0, SampleVolume, 127, volume);

        ChangeVolumePanSample(numsample, volume, pan);
    }

    InitParmSample();
}

/*──────────────────────────────────────────────────────────────────────────*/

void HQ_3D_ChangePanSample(S32 numsample, S32 x, S32 y, S32 z) {
    U32 pan;
    S32 volume, distance;

    if (SamplesEnable) {
        PtrProjectPoint(x, y, z);

        if (CubeMode == CUBE_INTERIEUR)
            distance = Distance2D((S32)ModeDesiredX / 2, (S32)ModeDesiredY / 2, Xp, Yp);
        else
            distance = Distance3D(x, y, z, CameraX, CameraY, CameraZ);

        volume = 0;

        GiveBalance(Xp, Yp, distance, &pan, &volume);

        if (volume > 0) {
            HQ_ChangePanSample(numsample, volume, pan);
        } else
            HQ_StopOneSample(numsample);
    }

    InitParmSample();
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/

void LaunchAmbiance(void) {
    S32 numsample;
    S32 n;
    S32 decal;
    S32 freq;
    S32 volume;
    S32 repeat;

    if (!SamplesEnable)
        return;

#ifdef DEMO
    if (NumCube == NUM_CUBE_PHANTOM)
        return;
#endif

    for (n = 0; n < 4; n++) {
        numsample = SampleAmbiance[n];
        repeat = SampleRepeat[n];

        if (numsample != -1               // si defini
                         AND repeat == 0) // on lance immediatement les repeat infini
        {
            decal = SampleRnd[n];
            freq = SampleFreq[n];
            volume = SampleVol[n];

            InitParmSample();

            if (!IsSamplePlaying(numsample)) {
                HQ_MixSample(numsample,
                             freq, decal,
                             repeat, 64, volume);
            }
        }
    }
}

/*──────────────────────────────────────────────────────────────────────────*/

void GereAmbiance(void) {
    S32 sample;
    S32 numsample;
    S32 n;
    S32 decal;
    S32 repeat;
    S32 freq;
    S32 volume;

    if (!SamplesEnable)
        return;

#ifdef DEMO
    if (NumCube == NUM_CUBE_PHANTOM)
        return;
#endif

    if (TimerRefHR >= TimerNextAmbiance) {
        sample = MyRnd(4); /* 0 1 2 3 */

        for (n = 0; n < 4; n++) {
            if (!(SamplePlayed & (1 << sample))) /* si pas joue */
            {
                SamplePlayed |= (1 << sample); /* marque le joué */
                if (SamplePlayed == 15)        /* tous joue */
                    SamplePlayed = 0;

                numsample = SampleAmbiance[sample];
                repeat = SampleRepeat[sample];

                if (numsample != -1               // si defini
                                 AND repeat != 0) // on ne relance jamais 1 repeat infini
                {
                    decal = SampleRnd[sample];
                    freq = SampleFreq[sample];
                    volume = SampleVol[sample];

                    InitParmSample();

                    if (!IsSamplePlaying(numsample)) {
                        HQ_MixSample(numsample,
                                     freq, decal,
                                     repeat, MyRnd(127), volume);
                    }
                    break;
                }
            }
            sample++;
            sample &= 3;
        }

        TimerNextAmbiance = TimerRefHR +
                            (MyRnd(SecondEcart) + SecondMin) * 1000;
    }

    // on s'assure de ne jamais en jouer un autre
    if (SecondMin < 0)
        TimerNextAmbiance = TimerRefHR + 200000;
}

/*──────────────────────────────────────────────────────────────────────────*/

// Appelée au changement de cube pour couper les ambiance du cube en cours
// Rain stop added: since DelBlocHQRSample no longer kills all voices,
// rain must be explicitly stopped on cube change to prevent carry-over.
void ClearAmbiance(void) {
    S32 n;

    if (IsSamplePlaying(SAMPLE_RAIN))
        HQ_StopOneSample(SAMPLE_RAIN);

    for (n = 0; n < 4; n++) {
        if (SampleAmbiance[n] != -1) /* si defini */
        {
            HQ_StopOneSample(SampleAmbiance[n]);
        }
    }
}

/*══════════════════════════════════════════════════════════════════════════*
                   █▀▀▀█ █▀▀▀█ █     █▀▀▀▀ ▀▀█▀▀ ▀▀█▀▀ █▀▀▀▀
                   ██▀▀▀ ██▀▀█ ██    ██▀▀    ██    ██  ██▀▀
                   ▀▀    ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀   ▀▀    ▀▀  ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
void InitPalette(void) {
    char tmpFilePath[ADELINE_MAX_PATH];

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);

    PtrPalNormal = (U8 *)LoadMalloc_HQR(tmpFilePath, RESS_PAL);
    if (!PtrPalNormal)
        TheEndCheckFile(tmpFilePath);

    PtrPalEclair = (U8 *)LoadMalloc_HQR(tmpFilePath, RESS_PAL_ECLAIR);
    if (!PtrPalEclair)
        TheEndCheckFile(tmpFilePath);

    PtrPalBlack = (U8 *)LoadMalloc_HQR(tmpFilePath, RESS_PAL_BLACK);
    if (!PtrPalBlack)
        TheEndCheckFile(tmpFilePath);
}

/*──────────────────────────────────────────────────────────────────────────*/
void ChoicePalette(void) {
    char tmpFilePath[ADELINE_MAX_PATH];

    if (CubeMode == CUBE_INTERIEUR
                        OR(Island == 0 AND TEMPETE_FINIE)) {
        IdxPalette = RESS_XPL00; // citabau
    } else {
        IdxPalette = RESS_XPL0 + Island;
    }

    if (IdxPalette != LastIdxPalette) {
        GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
        if (!Load_HQR(tmpFilePath, PtrXplPalette, IdxPalette))
            TheEnd(ERROR_NOT_FOUND_FILE, tmpFilePath);

        LastIdxPalette = IdxPalette;
    }

    PtrXplHeader = (XPL_HEADER *)PtrXplPalette;

    PtrPalCurrent = PtrXplPalette + PtrXplHeader->OffsetPalette;
    PtrNuances = PtrXplPalette + PtrXplHeader->OffsetFog;
    PtrTransPal = PtrXplPalette + PtrXplHeader->OffsetTransp;
    FogCoul = PtrXplHeader->FogCoul;
    PalLevel = PtrXplHeader->ShadeNormalLevel;

    // Fixe Palette
    if (CubeMode == CUBE_INTERIEUR OR TEMPETE_FINIE) {
        if (IsSamplePlaying(SAMPLE_RAIN))
            HQ_StopOneSample(SAMPLE_RAIN);
    }

    RestartRainSample = TRUE;
    PtrPal = PtrPalCurrent;
    PtrCLUTGouraud = PtrCLUTFog = PtrNuances;
}

/*──────────────────────────────────────────────────────────────────────────*/

// Island guard: rain is only intended for Citadel Island (Island 0). Without this,
// loading or warping to Desert/other exterior cubes would incorrectly play rain
// because TEMPETE_ACTIVE is global (chapter-based) and StartRainSample did not
// check Island.
void StartRainSample(void) {
    if (SamplesEnable
            AND Island == 0 AND CubeMode == CUBE_EXTERIEUR AND !TEMPETE_FINIE AND !IsSamplePlaying(SAMPLE_RAIN)) {
        HQ_MixSample(SAMPLE_RAIN,
                     0x1000, 300, // frequence, decalage
                     0, 64, VOLUME_RAIN);
    }

    RestartRainSample = FALSE;
}

//──────────────────────────────────────────────────────────────────────────
void FadePal(U8 r, U8 v, U8 b, U8 *ptrpal, S32 scale) {
    U8 workpal[768];
    S32 n;

    for (n = 0; n < 256; n++) {
        workpal[n * 3 + 0] = (U8)RegleTrois(r, ptrpal[n * 3 + 0], FADE_DELAY, scale);
        workpal[n * 3 + 1] = (U8)RegleTrois(v, ptrpal[n * 3 + 1], FADE_DELAY, scale);
        workpal[n * 3 + 2] = (U8)RegleTrois(b, ptrpal[n * 3 + 2], FADE_DELAY, scale);
    }

    PaletteSync(workpal);
    // Re-present the composited Log at the ramped palette. On DOS the palette
    // write alone re-tinted the live framebuffer, so the fade acted on whatever
    // was on screen (terrain + actors). Under SDL PaletteSync does not repaint,
    // so we re-blit the whole frame. Use BoxBlit (present only), NOT BoxUpdate:
    // BoxUpdate also runs BoxClean, whose Screen->Log restore wipes the moving
    // (actor/object) boxes out of Log, so every fade would drop the object layer
    // and only fade the terrain -- actors vanish at fade-out start and pop in
    // after fade-in. The next real AffScene recycles the box lists as usual.
    BoxStaticAdd(0, 0, (S32)(ModeDesiredX - 1), (S32)(ModeDesiredY - 1));
    BoxBlit();
}

//──────────────────────────────────────────────────────────────────────────
void FadeToBlack(U8 *ptrpal) {
    S32 starttime, delta;

    if (FlagBlackPal)
        return;

    SaveTimer();

    starttime = TimerSystemHR;

    do {
        Timer_FixedDtPump(); // non-presenting palette-fade wait: step the virtual clock under fixed-dt
        ManageTime();

        delta = starttime + FADE_DELAY - TimerSystemHR;

        if (delta < 0)
            delta = 0;

        FadePal(0, 0, 0, ptrpal, delta);
    } while (delta);

    FlagBlackPal = TRUE;

    RestoreTimer();
}

//──────────────────────────────────────────────────────────────────────────
void WhiteFade() {
    U8 workpal[768];
    S32 starttime, delta;

    SaveTimer();

    memset(workpal, 255, 768);

    starttime = TimerSystemHR;

    do {
        Timer_FixedDtPump(); // non-presenting palette-fade wait: step the virtual clock under fixed-dt
        ManageTime();

        delta = TimerSystemHR - starttime;

        if (delta > FADE_DELAY)
            delta = FADE_DELAY;

        FadePal(0, 0, 0, workpal, delta);
    } while (delta != FADE_DELAY);

    FlagBlackPal = FALSE;

    RestoreTimer();
}

//──────────────────────────────────────────────────────────────────────────
void FadeWhiteToPal(U8 *ptrpal) {
    S32 starttime, delta;

    SaveTimer();

    starttime = TimerSystemHR;

    do {
        Timer_FixedDtPump(); // non-presenting palette-fade wait: step the virtual clock under fixed-dt
        ManageTime();

        delta = TimerSystemHR - starttime;

        if (delta > FADE_DELAY)
            delta = FADE_DELAY;

        FadePal(255, 255, 255, ptrpal, delta);
    } while (delta != FADE_DELAY);

    FlagBlackPal = FALSE;

    RestoreTimer();
}

//──────────────────────────────────────────────────────────────────────────
void FadeToPal(U8 *ptrpal) {
    S32 starttime, delta;

    SaveTimer();

    starttime = TimerSystemHR;

    do {
        Timer_FixedDtPump(); // non-presenting palette-fade wait: step the virtual clock under fixed-dt
        ManageTime();

        delta = TimerSystemHR - starttime;

        if (delta > FADE_DELAY)
            delta = FADE_DELAY;

        FadePal(0, 0, 0, ptrpal, delta);
    } while (delta != FADE_DELAY);

    FlagBlackPal = FALSE;

    RestoreTimer();
}

//──────────────────────────────────────────────────────────────────────────
void FadeToBlackAndSamples(U8 *ptrpal) {
    S32 starttime, delta;

    SaveTimer();

    if (!FlagBlackPal) {
        // Visual fade timed off the managed game clock (TimerSystemHR), like
        // FadeToBlack/FadeToPal -- NOT off the audio sample-fade countdown, whose
        // SDL_GetTicks() baseline goes stale across the cube load and collapses the
        // fade to a single step (#353). FadeOutSamples() still ramps the SFX volume;
        // its return value is deliberately ignored so audio state cannot break the
        // visual fade.
        starttime = TimerSystemHR;

        do {
            Timer_FixedDtPump();
            ManageTime();

            delta = starttime + FADE_DELAY - TimerSystemHR;

            if (delta < 0)
                delta = 0;

            FadeOutSamples(FADE_DELAY);
            FadePal(0, 0, 0, ptrpal, delta);
        } while (delta);
    }

    if ((CubeMode == CUBE_INTERIEUR OR TEMPETE_FINIE)
            AND IsSamplePlaying(SAMPLE_RAIN)) {
        HQ_StopOneSample(SAMPLE_RAIN);
    }

    PauseSamples();

    FlagBlackPal = TRUE;

    RestoreTimer();
}

//──────────────────────────────────────────────────────────────────────────
void FadeToPalAndSamples(U8 *ptrpal) {
    S32 starttime, delta;

    SaveTimer();

    ResumeSamples();

    if ((CubeMode == CUBE_INTERIEUR OR TEMPETE_FINIE)
            AND IsSamplePlaying(SAMPLE_RAIN)) {
        HQ_StopOneSample(SAMPLE_RAIN);
    }

    // Visual fade timed off the managed game clock (TimerSystemHR); see the note
    // in FadeToBlackAndSamples. FadeInSamples() still ramps the SFX volume.
    starttime = TimerSystemHR;

    do {
        Timer_FixedDtPump();
        ManageTime();

        delta = TimerSystemHR - starttime;

        if (delta > FADE_DELAY)
            delta = FADE_DELAY;

        FadeInSamples(FADE_DELAY);
        FadePal(0, 0, 0, ptrpal, delta);
    } while (delta != FADE_DELAY);

    FlagBlackPal = FALSE;

    RestoreTimer();
}

//──────────────────────────────────────────────────────────────────────────
void SetBlackPal() {
    S32 n;

    for (n = 0; n < 256; n++) {
        PalOne(n, 0, 0, 0);
    }
    FlagBlackPal = TRUE;
}

//──────────────────────────────────────────────────────────────────────────
void FadePalToPal(U8 *ptrpal, U8 *ptrpal1) {
    U8 workpal[768];
    S32 starttime, delta;

    SaveTimer();

    starttime = TimerSystemHR;

    do {
        S32 n;

        Timer_FixedDtPump(); // step the virtual clock under fixed-dt (as the other fade loops do)
        ManageTime();

        delta = TimerSystemHR - starttime;

        if (delta > FADE_DELAY)
            delta = FADE_DELAY;

        for (n = 0; n < 256; n++) {
            workpal[n * 3 + 0] = (U8)RegleTrois(ptrpal[n * 3 + 0], ptrpal1[n * 3 + 0], FADE_DELAY, delta);
            workpal[n * 3 + 1] = (U8)RegleTrois(ptrpal[n * 3 + 1], ptrpal1[n * 3 + 1], FADE_DELAY, delta);
            workpal[n * 3 + 2] = (U8)RegleTrois(ptrpal[n * 3 + 2], ptrpal1[n * 3 + 2], FADE_DELAY, delta);
        }

        // Present each ramp step, like FadePal. PaletteSync alone only rebuilds the
        // palette LUT; under SDL nothing repaints until the next BoxBlit, so without
        // this the lit-to-lit crossfade is invisible and snaps to the target palette
        // on the next unrelated present. BoxBlit (present only, no BoxClean) keeps the
        // composited Log intact, matching the DOS palette-write behaviour.
        PaletteSync(workpal);
        BoxStaticAdd(0, 0, (S32)(ModeDesiredX - 1), (S32)(ModeDesiredY - 1));
        BoxBlit();
    } while (delta != FADE_DELAY);

    RestoreTimer();
}

//──────────────────────────────────────────────────────────────────────────
void FadeToPalIndex(U8 num) {
    if (num >= MAX_PALETTES)
        return;

    if (num == 3) // palette noire demandee ?
        FadeToBlackAndSamples(PtrPal);
    else if (PtrPal == PtrPalBlack)
        FadeToPalAndSamples(*ListPalette[num]);
    else
        FadePalToPal(PtrPal, *ListPalette[num]);

    PtrPal = *ListPalette[num];
}

//──────────────────────────────────────────────────────────────────────────
void PaletteIndex(U8 num) {
    if (num >= MAX_PALETTES)
        return;

    SaveTimer();

    PaletteSync(*ListPalette[num]);

    PtrPal = *ListPalette[num];

    RestoreTimer();
}

/*══════════════════════════════════════════════════════════════════════════*
                            █▄ ▄█  █    █▀▀▀▄  █
                            ██▀ █  ██   ██  █  ██
                            ▀▀  ▀  ▀▀   ▀▀▀▀   ▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

void ReadVolumeSettings() {
    //---------------------------------------------------------------
    //      Read config file (LBA2.CFG)
    //---------------------------------------------------------------
    SampleVolume = DefFileBufferReadValueDefault("WaveVolume", DEF_SAMPLE_VOLUME);

    if (SampleVolume < 0)
        SampleVolume = 0;
    if (SampleVolume > 127)
        SampleVolume = 127;

    //---------------------------------------------------------------
    VoiceVolume = DefFileBufferReadValueDefault("VoiceVolume", DEF_VOICE_VOLUME);

    if (VoiceVolume < 0)
        VoiceVolume = 0;
    if (VoiceVolume > 127)
        VoiceVolume = 127;

    //---------------------------------------------------------------
    JingleVolume = DefFileBufferReadValueDefault("MusicVolume", DEF_JINGLE_VOLUME);

    if (JingleVolume > 127) {
        JingleVolume = 127;
    }

    //---------------------------------------------------------------
#ifdef CDROM
    CDVolume = DefFileBufferReadValueDefault("CDVolume", DEF_CD_VOLUME);

    if (CDVolume > 127)
        CDVolume = 127;
#endif

    //---------------------------------------------------------------
    MasterVolume = DefFileBufferReadValueDefault("MasterVolume", DEF_MASTER_VOLUME);

    if (MasterVolume < 0)
        MasterVolume = 0;
    if (MasterVolume > 127)
        MasterVolume = 127;

    SetMasterVolumeSample(MasterVolume);
    SetVolumeJingle(JingleVolume);
#ifdef CDROM
    SetVolumeCD(CDVolume);
#endif
}

/*──────────────────────────────────────────────────────────────────────────*/

void WriteVolumeSettings() {
    DefFileBufferWriteValue("WaveVolume", SampleVolume);
    DefFileBufferWriteValue("VoiceVolume", VoiceVolume);
    DefFileBufferWriteValue("MusicVolume", JingleVolume);
#ifdef CDROM
    DefFileBufferWriteValue("CDVolume", CDVolume);
#endif
    DefFileBufferWriteValue("MasterVolume", MasterVolume);
}

/*──────────────────────────────────────────────────────────────────────────*/
