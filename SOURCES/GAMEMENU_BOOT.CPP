#include "C_EXTERN.H"
#include "CONFIG.H"
#include "DIRECTORIES.H"
#include "PERSO.H"
#include "GAMEMENU.H"
#include "GAMEMENU_CAPTURE.H"
#include "GAMEMENU_MENUS.H"
#include "MESSAGE.H"
#include "IMAGE_LOAD.H"
#include "PLAYACF.H"
#include "INPUT.H"
#include "CONSOLE/CONSOLE.H"

#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/WINDOW.H>

/*══════════════════════════════════════════════════════════════════════════*
             █▀▀▀▀ █▀▀▀█ █▄ ▄█ █▀▀▀▀       █▀▀▀█ █   █ █▀▀▀▀ █▀▀▀█
             ██ ▀█ ██▀▀█ ██▀ █ ██▀▀        ██  █ ██ ▄▀ ██▀▀  ██▀█▀
             ▀▀▀▀▀ ▀▀  ▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀   ▀▀▀▀▀ ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

#define GAMEOVER_ZOOM_TEMPO 1000
#define GAMEOVER_TEMPO 3 * 1000

#define GAMEOVER_MINZOOM 5000
#define GAMEOVER_MAXZOOM 200

void GameOver() {
    S16 *ptr3do;
    S32 zoom;
    U32 chrono;
    char tmpFilePath[ADELINE_MAX_PATH];

    HQ_StopSample();

    if (ListVarGame[160] > 0) {
        SaveTimer();

        if (!FlagBlackPal)
            FadeToBlack(PtrPal);

        PlayAcf("DELUGE");

        if (CubeMode == CUBE_INTERIEUR) {
            InitGrille(NumCube);
        }
        RazListPartFlow();
        ChoicePalette();

        RestoreTimer();
        FlagFade = TRUE;
        AffScene(AFF_ALL_NO_FLIP);
        CopyScreen(Log, Screen);
        BoxBlit();
        BoxReset();
    } else {
        BackupScreen(FALSE);
        CopyScreen(Log, Screen);
        BoxReset();
    }

    PaletteSync(PtrPal);

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, RESS_HQR_NAME);
    ptr3do = (S16 *)LoadMalloc_HQR(tmpFilePath, RESS_GAME_OVER);
    if (!ptr3do)
        return;

    // lance un sample ou music de game over ??

    zoom = GAMEOVER_MINZOOM;

    SetProjection((S32)ModeDesiredX / 2, (S32)ModeDesiredY / 2, 128, 200, 200);
    SetFollowCamera(0, 0, 0, 0, 0, 0, zoom);
    SetLightVector(0, 0, 0);

    ManageTime();
    chrono = TimerRefHR;

    // Magouille pour precharger le sample
    if (SamplesEnable) {
        HQRGetDelFunc = DelBlocHQRSample;
        GivePtrSample(SAMPLE_GAME_OVER, NULL);
        HQRGetDelFunc = NULL;
    }

    MyGetInput();

    while ((MyKey != K_ESC AND !(Input & I_MENUS)) AND(TimerRefHR - chrono <= GAMEOVER_ZOOM_TEMPO)) {
        ManageTime();
        MyGetInput();

        zoom = BoundRegleTrois(GAMEOVER_MINZOOM, GAMEOVER_MAXZOOM,
                               GAMEOVER_ZOOM_TEMPO, TimerRefHR - chrono);

        SetFollowCamera(0, 0, 0, 0, 0, 0, zoom);

        UnsetClip();

        if (BodyDisplay(0, 0, 0, 0, 0, 0, ptr3do)) {
            // Objet pas tout clippé
            SetClip(ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax);
            if (ClipXMin <= ClipXMax
                                AND ClipYMin <= ClipYMax) {
                BoxMovingAdd(ClipXMin, ClipYMin, ClipXMax, ClipYMax);
                BoxUpdate();
            }
        }
    }

    HQ_MixSample(SAMPLE_GAME_OVER, 0x1000, 2000, 1, 64, 127);

    SetFollowCamera(0, 0, 0, 0, 0, 0, zoom);

    UnsetClip();

    if (BodyDisplay(0, 0, 0, 0, 0, 0, ptr3do)) {
        // Objet pas tout clippé
        SetClip(ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax);
        if (ClipXMin <= ClipXMax
                            AND ClipYMin <= ClipYMax) {
            BoxStaticAdd(ClipXMin, ClipYMin, ClipXMax, ClipYMax);
            BoxUpdate();
        }
    }

    chrono = TimerRefHR + GAMEOVER_TEMPO;
    while ((TimerRefHR < chrono) AND(MyKey != K_ESC AND !(Input & I_MENUS))) {
        ManageTime();
        MyGetInput();
    }

    UnsetClip();
    Free(ptr3do);

    PtrInit3DView();

    FadeToBlack(PtrPal);

    PtrPal = PtrPalNormal;
    FadeMenu = TRUE;

    UnsetClip();
}

/*══════════════════════════════════════════════════════════════════════════*
                            █   █ █▀▀▀▀ █     █▀▀▀█
                            ██▀▀█ ██▀▀  ██    ██▀▀▀
                            ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
/*
void    Help( U8 flaggame )
{
        if( !flaggame ) // appelé depuis les menus
        {
                SaveTimer() ;

                FlagFade = FALSE ;

                EffectPcx( PCR_HELP/2, (U8*)Log, 0, flaggame ) ;

                WaitNoInput()   ;
                WaitInput()     ;
                FadeToBlack( PalettePcx ) ;
                CopyScreen( Screen, Log ) ;
                if( FlagShadeMenu )     ShadeBoxBlk( 0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL ) ;
                BoxStaticFullflip() ;
                FadeMenu = TRUE ;
                RestoreTimer()  ;
        }
        else    EffectPcx( PCR_HELP/2, (U8*)Screen, 0, flaggame ) ;
}
*/

/*══════════════════════════════════════════════════════════════════════════*
                   █▀▀▀▀ █▀▀▀█ █▀▀▀▀ █▀▀▀▄  █    ▀▀█▀▀ ██▀▀▀
                   ██    ██▀█▀ ██▀▀  ██  █  ██     ██  ▀▀▀▀█
                   ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀   ▀▀     ▀▀  ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

void Credits(S32 mode) {
    char tmpFilePath[ADELINE_MAX_PATH];

    SaveTimer();

    //      FadeToBlack( PtrPalNormal ) ;

    WaitNoInput();

    SaveCamera();

    /* Endgame transition can leave audio playing and PtrPal unset (issue #65).
       Mirror the safety prologue PlayAcf uses so GamePlayCredits has valid
       prerequisites regardless of caller (endgame win path or console). */
    StopMusic();
    HQ_PauseSamples();
    if (!PtrPal)
        PtrPal = PtrPalNormal;

    /* GamePlayCredits stomps several gameplay-relevant buffers because the
       original game never returned to play after credits. To make console
       invocation safe mid-game, save/restore them around the call. (issue #65)

         * PtrZBuffer's backing buffer aliases BufCube + BufferBrick (the cube
           grid and brick library — see MEM.CPP:187). Credits memsets it and
           renders Z into it, destroying the cube state.
         * BufferAnim holds animation frame state for ALL game objects.
           InitObjects() resets it; credits writes its own animation state
           into the buffer, so any in-game T_OBJ_3D with LastOfsIsPtr=1 holds
           pointers into now-stale data.

       GameInProgress is FALSE during MainLoop (set TRUE only after this
       function), so it is not a usable gate. Always save; cost is one malloc
       + memcpy round trip per buffer, ~1 ms total. */
    U8 *zbufSave = NULL;
    U32 zbufSize = (U32)ListMem[MEM_PTR_ZBUFFER].Size;
    if (zbufSize && PtrZBuffer) {
        zbufSave = (U8 *)malloc(zbufSize);
        if (zbufSave)
            memcpy(zbufSave, PtrZBuffer, zbufSize);
    }
    extern U8 *PtrLib3DBufferAnim; /* LIB386/ANIM/LIBINIT — local extern to avoid header churn */
    U8 *animSave = NULL;
    U8 *animCursorSave = NULL;
    if (BufferAnim) {
        animSave = (U8 *)malloc(SIZE_BUFFER_ANIM);
        if (animSave)
            memcpy(animSave, BufferAnim, SIZE_BUFFER_ANIM);
        animCursorSave = PtrLib3DBufferAnim;
    }

    /* Suppress console toggle for the duration of the credits sequence:
       opening the console mid-credits side-effects palette and screen state
       Credits doesn't expect, and there's no useful interaction during a
       cinematic. Mirrors the gating already in place around SlideShow(). */
    Console_SetSlideShowActive(1);

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, CREDITS_HQR_NAME);
    GamePlayCredits(tmpFilePath, mode);

    Console_SetSlideShowActive(0);

    if (zbufSave) {
        memcpy(PtrZBuffer, zbufSave, zbufSize);
        free(zbufSave);
    }
    if (animSave) {
        memcpy(BufferAnim, animSave, SIZE_BUFFER_ANIM);
        free(animSave);
        /* end_credits already re-installed the game's body/anim callbacks via
           InitObjects(BufferAnim, …) but reset the bump cursor to the start.
           Restore the cursor too, so subsequent ObjectInitAnim allocations
           don't alias frame slots already referenced by in-game T_OBJ_3D's
           with LastOfsIsPtr=1. */
        PtrLib3DBufferAnim = animCursorSave;
    }

    /* Mirror HoloMap's gameplay-restore tail (HOLOGLOB.CPP:1186-1199): re-bind
       the per-cube 3D function-pointer table (PtrWorldColBrick, PtrDoAnim, …)
       and force a full screen flip on the next frame. Without this, returning
       to MainLoop renders against credits' projection/fog/filler globals and
       the player character does not appear. */
    PtrInit3DGame();
    FlagFade = TRUE;
    FirstTime = AFF_ALL_FLIP;

    RestoreCamera();

    GameInProgress = TRUE;

    if (!mode) {
        GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
        Image_LoadStretched(tmpFilePath, Screen, PCR_MENU);
        CopyScreen(Screen, Log);
        BoxStaticFullflip();
    }

    FadeMenu = TRUE;

    WaitNoInput();

    RestoreTimer();
}

/*──────────────────────────────────────────────────────────────────────────*/

/* Harness capture state — see SlideShow_RequestCapture() in GAMEMENU_CAPTURE.H. When
   armed, SlideShow's main loop breaks out after the first frame's Dial returns
   (Dial itself does the SavePNG via Dial_RequestCapture, which we arm
   alongside this flag). Same compose pattern as Menu_*Capture / Inv_*Capture. */
static int s_slideshow_capture_active = 0;

void SlideShow_RequestCapture(const char *path) {
    if (!path || !path[0]) {
        s_slideshow_capture_active = 0;
        Dial_RequestCapture(NULL);
        return;
    }
    s_slideshow_capture_active = 1;
    /* The actual SavePNG happens inside Dial's typewriter loop — same hook
       used by `ui dialog`. We just need to break out of the slideshow loop
       after that fires so the harness doesn't iterate the remaining shots. */
    Dial_RequestCapture(path);
}

// SlideShow de screenshots
S32 SlideShow(void) {
    U8 pal[768 + RECOVER_AREA];
    S32 n;
    S32 endtimer;
    U32 nbshots;
    U32 handle;
    S32 ret = TRUE;
    S32 memofirsttime = FirstTime;
    U8 fadepal = FALSE;
    U8 memobulle = Bulle;
    U8 memocoul;
    char tmpScrshotFilePath[ADELINE_MAX_PATH];

    // recupere nombre de screen shot dans le .HQR directement
    GetResPath(tmpScrshotFilePath, ADELINE_MAX_PATH, SCRSHOT_HQR_NAME);
    handle = OpenRead(tmpScrshotFilePath);
    if (handle) {
        Read(handle, &nbshots, 4);
        nbshots = ((nbshots / 4) - 1) / 2; // /2 car il y a aussi les palettes
        Close(handle);
    } else
        return TRUE;

    InitDial(2); // pour les textes sur les screen shots

    /* Prevent opening console during slideshow frames (no stable gameplay buffers/callback flow there). */
    Console_SetSlideShowActive(1);
    Bulle = FALSE;

    /* The slideshow renders centred 4:3 PCX stills with story text on top.
       Setting Dial_Letterbox keeps the dialog box inside the picture's 4:3
       region instead of stretching into the black sidebars at wider widths. */
    S32 memoDialLetterbox = Dial_Letterbox;
    Dial_Letterbox = 1;

#ifdef DEMO
    FlagSlideShow = TRUE; // pour les tempos des messages
#endif

    FirstTime = AFF_OBJETS_NO_FLIP;

    memocoul = ListObjet[NUM_PERSO].CoulObj;
    ListObjet[NUM_PERSO].CoulObj = 0;

    NumObjDial = NUM_PERSO;

    Init3DView();

    for (n = 0; n < nbshots; n++) {
        InitWaitNoKey();

        FadeToBlack(PtrPal);

        // charge image — clear to black first so widescreen sidebars don't keep
        // stale pixels (FadeToBlack only fades the palette). No-op at 640x480.
        memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        Image_LoadCentered(tmpScrshotFilePath, Log, (n * 2));

        // on peut pas afficher le logo LBA2 car il est dans la palette
        // du jeu
        //		AffGraph( 0, 639-105, 3, HQR_Get(HQRPtrSprite,11) ) ;

        Dial(START_SLIDESHOW_TEXT + n, FALSE);

        /* Harness: SlideShow_RequestCapture arms a one-shot SavePNG in Dial
           above. After it fires, exit cleanly without iterating remaining
           shots so the harness's --exit can finish promptly. */
        if (s_slideshow_capture_active) {
            s_slideshow_capture_active = 0;
            ret = FALSE;
            break;
        }

        // charge palette
        Load_HQR(tmpScrshotFilePath, pal, (n * 2) + 1);

        // reforce en blanc la couleur 12
        pal[12 * 3] = 255;
        pal[12 * 3 + 1] = 255;
        pal[12 * 3 + 2] = 255;

        BoxStaticFullflip();
        FadeToPal(pal);

        fadepal = TRUE;
        PtrPal = pal;

        ManageTime();
        endtimer = TimerRefHR + 4 * 1000;

        while ((TimerRefHR < endtimer) AND !MyKey) {
            ManageTime();
            MyGetInput();
        }

        if (MyKey == K_ESC OR(Input & I_MENUS)) {
            ret = FALSE;
            break;
        }

        if (MyKey == K_D)
            break;
    }

    FirstTime = memofirsttime;
    Bulle = memobulle;
    ListObjet[NUM_PERSO].CoulObj = memocoul;

#ifdef DEMO
    FlagSlideShow = FALSE;
#endif

    InitDial(0);

    if (fadepal) {
        FadeToBlack(pal);
        PtrPal = PtrPalNormal;
    }

    GameInProgress = TRUE; // pour recharger la partie en cours

    /* Restore normal console availability once slideshow exits. */
    Console_SetSlideShowActive(0);
    Dial_Letterbox = memoDialLetterbox;
    return ret;
}

// Enter "canned demo" mode (SHIFT+D from the main menu, the idle attract trigger, the
// SlideDemo debug entry, and the harness --demo flag all funnel through here). The
// actual RNG-seed pinning happens in ChangeCube via Demo_RngSeed(DemoSlide, ..., NewCube)
// -- while DemoSlide is set, the cube number is the srand seed instead of TimerRefHR,
// so the reel reproduces on every host regardless of wall-clock pace. The earlier
// "zero TimerRefHR here" approach (PR #249) only worked under --fixed-dt because
// ManageTime() runs between this call and the first ChangeCube on the SHIFT+D /
// idle-attract path and banks (SDL_GetTicks() - LastTime) right back into TimerRefHR.
// The TimerRefHR = 0 line remains as cheap defense-in-depth for any other clock-derived
// state read during the reel (currently none we know of; cheap to keep). See issue #176.
void BeginDemoSlide(void) {
    TimerRefHR = 0;
    DemoSlide = TRUE;
}

void SlideDemo(int argc, char *argv[]) {
    S32 aff = TRUE;
    U32 cubedeb = 5;

#ifdef DEBUG_TOOLS
    if (argc > 1)
        cubedeb = atoi(argv[1]);
#else
    argc = argc; // pour virer warning
    argv = argv;
#endif

    BeginDemoSlide();

    FOREVER {
        if (!BumperLogo()) {
            aff = FALSE;
            break;
        }

        InitGame(1);
        NewCube = cubedeb;

        FlagFade = TRUE;

        if (!MainLoop())
            break;

        if (!SlideShow())
            break;

        //#ifdef        CDROM
        //              StopMusicCD() ;
        //#endif

        FadeToBlack(PtrPal);
        Cls();
        BoxUpdate();

        //              Credits(?) ;

        if (!AdelineLogo())
            break;
    }

    FadeToBlack(PtrPal);
    //#ifdef        CDROM
    //      StopMusicCD() ;
    //#endif

    if (aff)
        BumperLogo();
}

/*──────────────────────────────────────────────────────────────────────────*/

S32 AdelineLogo(void) {
    char tmpFilePath[ADELINE_MAX_PATH];
    U32 endtimer;
    S32 ret = TRUE;
    //      S32     volume ;

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
    // FillFromEdge: the Adeline logo PCX has a white background that
    // should extend across the framebuffer at widescreen, not be
    // black-cropped to the centred 640. Sampling the PCX's top-left
    // pixel picks up its white-index regardless of which palette
    // entry holds white. No-op at 640x480.
    if (!Image_LoadCenteredFillFromEdge(tmpFilePath, Log, PCR_LOGO))
        TheEndCheckFile(tmpFilePath);

    if (!Load_HQR(tmpFilePath, PalettePcx, PCR_LOGO + 1))
        TheEndCheckFile(tmpFilePath);

    PtrPal = PalettePcx;

    Palette(PalettePcx);

    //      volume = RegleTrois( 0, SampleVolume, 127, 127 ) ;
    // ATTENTION: déborde sur ScreenAux (voir MEM.CPP) !!!!!!!
    //      GetResPath(tmpFilePath, ADELINE_MAX_PATH, SAMPLES_HQR_NAME);
    //      if( Load_HQR( tmpFilePath, PtrZBuffer, SAMPLE_LOGO ) )
    {
        S32 timer;

        // pause permettant d'attendre (a peu près) la synchro ecran !
        ManageTime();
        timer = TimerRefHR + 1000;

        while (TimerRefHR <= timer) {
            ManageTime();
        }

        //              PlaySample( PtrZBuffer, SAMPLE_LOGO, 0x1000, 1, volume, 64 ) ;
    }

    InitWaitNoKey();

    PlayMusic(MUSIC_LOGO, TRUE);

    BoxStaticFullflip();

    ManageTime();
    endtimer = TimerRefHR + 3 * 1000; // 3s
    while ((TimerRefHR <= endtimer OR IsStreamPlaying()) AND !MyKey) {
        ManageTime();
        MyGetInput();
        if (FlagMenuMouse && (Click & 1))
            break;
    }
    if (MyKey == K_ESC OR(Input & I_MENUS))
        ret = FALSE;

    FlagBlackPal = FALSE;
    FadeToBlack(PalettePcx);

    ManageTime();
    endtimer = TimerRefHR + 1 * 1000;
    while (TimerRefHR <= endtimer AND !MyKey) {
        ManageTime();
        MyGetInput();
        if (FlagMenuMouse && (Click & 1))
            break;
    }
    if (MyKey == K_ESC OR(Input & I_MENUS))
        ret = FALSE;

    FadeOutVolumeMusic();
    StopMusic();

#ifdef CDROM
    SetVolumeCD(CDVolume);
#endif
    SetVolumeJingle(JingleVolume);

    //      HQ_StopSample() ;
    //      StopMusic() ;

    // ATTENTION: La music du logo est stoppée plus tard (Dans MainGameMenu())

    return ret;
}

/*──────────────────────────────────────────────────────────────────────────*/

S32 ShowLogo(S32 numscr, S32 nsec) {
    U32 endtimer;
    S32 ret = TRUE;
    char tmpFilePath[ADELINE_MAX_PATH];

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
    // Widescreen sidebars treatment: the Adeline studio bumper PCX has
    // a white background that should extend across the framebuffer, so
    // the fade-in reads as one contiguous frame instead of a centred
    // white logo with black wings. Sampling the PCX's top-left pixel
    // via Image_LoadCenteredFillFromEdge handles this without hard-
    // coding the palette's white-index. All other PCRs (distributor
    // logos: Activision / EA / Virgin / publisher bumpers) pre-clear
    // Log to 0 so the sidebars are explicit black instead of whatever
    // leftover bytes the previous frame left behind. Both branches are
    // no-ops at 640x480 (image fully overwrites the buffer).
    U32 loaded;
    if (numscr == PCR_BUMPER) {
        loaded = Image_LoadCenteredFillFromEdge(tmpFilePath, Log, numscr);
    } else {
        memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        loaded = Image_LoadCentered(tmpFilePath, Log, numscr);
    }
    if (loaded
            AND Load_HQR(tmpFilePath, PalettePcx, numscr + 1)) {
        WaitNoInput();

        PtrPal = PalettePcx;

        BoxStaticFullflip();
        FadeToPal(PalettePcx);

        ManageTime();
        endtimer = TimerRefHR + nsec * 1000; // wait nsec

        while (TimerRefHR <= endtimer AND !MyKey) {
            ManageTime();
            MyGetInput();
            if (FlagMenuMouse && (Click & 1))
                break;
        }

        if (MyKey == K_ESC OR(Input & I_MENUS))
            ret = FALSE;

        FadeToBlack(PalettePcx);
    }

    return ret;
}

/*──────────────────────────────────────────────────────────────────────────*/

void DistribLogo(void) {
#ifdef DEMO
    // Bumper 3 Distributeurs
    ShowLogo(3 * 2, 5);
#else
    // depend de la version
    switch (DistribVersion) {
    case ACTIVISION_VERSION:
    case ACTIVISION_SUD_VERSION:
        ShowLogo(PCR_ACTIVISION, 2);
        break;

    case EA_VERSION:
        ShowLogo(PCR_EA, 2);
        break;

    case VIRGIN_VERSION:
    case VIRGIN_ASIA_VERSION:
        ShowLogo(PCR_VIRGIN, 2);
        break;
    }
#endif
}

/*──────────────────────────────────────────────────────────────────────────*/

S32 BumperLogo(void) {
    /*	U32     endtimer ;
	S32     ret = TRUE ;
	chat    tmpFilePath[ADELINE_MAX_PATH];

	GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
	if( !Load_HQR( tmpFilePath, Log, PCR_BUMPER ) )
		TheEndCheckFile( tmpFilePath ) ;

	if( !Load_HQR( tmpFilePath, PalettePcx, PCR_BUMPER+1) )
		TheEndCheckFile( tmpFilePath ) ;

	WaitNoInput() ;

	PtrPal = PalettePcx ;

	BoxStaticFullflip() ;
	FadeToPal( PalettePcx ) ;

	ManageTime() ;
	endtimer = TimerRefHR + 3*1000 ;

	while( TimerRefHR<=endtimer AND !MyKey )
	{
		ManageTime() ;
		MyGetInput() ;
	}

	if( MyKey==K_ESC OR (Input&I_MENUS) )   ret = FALSE ;

	FadeToBlack( PalettePcx ) ;

	return ret ;
*/
    return (ShowLogo(PCR_BUMPER, 5));
}

/*──────────────────────────────────────────────────────────────────────────*/

#ifdef DEMO
void DemoLogo(void) {
    char string[30];
    S32 x0, y0, x1, y1;
    S32 endtimer;

    FlagBlackPal = FALSE;

    FadeToBlackAndSamples(PtrPal);
    BoxReset();
    Cls();

    strcpy(string, "      Demo  Version      ");
    ColorFont(LBAWHITE);

    // cadre
    x0 = (S32)ModeDesiredX / 2 - SizeFont(string) / 2 - 50;
    x1 = (S32)ModeDesiredX / 2 + SizeFont(string) / 2 + 50;
    y0 = (S32)ModeDesiredY / 2 - 25;
    y1 = (S32)ModeDesiredY / 2 + 25;
    //      ShadeBoxBlk( 0, 0, 639, 479, SCREEN_SHADE_LVL ) ;
    BackupAngles(x0, y0, x1, y1);
    ShadeBoxBlk(x0, y0, x1, y1, MENU_SHADE_LVL);
    DrawCadre(x0, y0, x1, y1, ALL_ANGLES);
    RestoreAngles(x0, y0, x1, y1);
    Font(x0 + 50, y0 + 7, string);
    BoxStaticFullflip();

    FadeToPal(PtrPal);

    InitWaitNoKey();

    ManageTime();
    endtimer = TimerRefHR + 2 * 1000;

    // wait a key or tempo
    while (TimerRefHR <= endtimer AND !MyKey) {
        ManageTime();
        MyGetInput();
    }

    FadeToBlack(PtrPal);
    Cls();
    BoxUpdate();
    FlagBlackPal = FALSE;
}

/*──────────────────────────────────────────────────────────────────────────*/

void DemoBumper(void) {
    /*
	U8      pal[768+RECOVER_AREA] ;
	char    tmpFilePath[ADELINE_MAX_PATH];

	GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
	if( Load_HQR( tmpFilePath, Log, PCR_BUMPER )
	AND Load_HQR( tmpFilePath, pal, PCR_BUMPER+1 ) )
	{
		S32     endtimer ;

		WaitNoInput() ;

		FadeToBlack( PtrPal ) ;
		BoxStaticFullflip() ;
		FadeToPal( pal ) ;

		PtrPal = PtrPalNormal ;

		ManageTime() ;
		endtimer = TimerRefHR + 2*1000 ;

		while( (TimerRefHR<endtimer) AND !MyKey )
		{
			ManageTime() ;
			MyGetInput() ;
		}

		FadeToBlack( pal ) ;

		Load_HQR( tmpFilePath, Log, PCR_MENU ) ;
	}
*/
    ShowLogo(PCR_BUMPER, 5);
    Load_HQR(tmpFilePath, Log, PCR_MENU);
}
#endif // DEMO

/*══════════════════════════════════════════════════════════════════════════*
                               █▀▀▀█ █▀▀▀▀ ▀▄ ▄▀
                               ██▀▀▀ ██     ▄▀▄
                               ▀▀    ▀▀▀▀▀ ▀   ▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
void EffectPcx(U8 numpcx, U8 *screen, U8 effect, U8 flagbcl) {
    char tmpFilePath[ADELINE_MAX_PATH];

    /* PCX cinematic: any dialog drawn over the centred PCX should letterbox to
       stay inside the picture's 4:3 region. The flag is scoped to this call —
       AffScene at the end (in the flagbcl branch) re-renders the gameplay
       scene, and subsequent Dial calls during normal play will see the
       restored original value. */
    S32 memoDialLetterbox = Dial_Letterbox;
    Dial_Letterbox = 1;

    SaveTimer();

    numpcx *= 2; // car il y a les palettes intercalées

    if (!FlagFade)
        FadeToBlackAndSamples(PtrPal);

    BoxReset(); // pour virer les boites de la scene

    // charge palette qui se trouve derriere dans le .HQR
    GetResPath(tmpFilePath, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
    if (!Load_HQR(tmpFilePath, PalettePcx, numpcx + 1))
        TheEndCheckFile(tmpFilePath);

    switch (effect) {
    case PCX_FADE:
        /* Clear the whole framebuffer to black first: Image_LoadCentered only
           writes the centred 640x480 region, so at widescreen the sidebars
           keep stale pixels that FadeToPal then re-indexes under the PCX
           palette (garbage margins). No-op at 640x480 — the image fills it. */
        memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        if (!Image_LoadCentered(tmpFilePath, Log, numpcx))
            TheEndCheckFile(tmpFilePath);

        BoxStaticFullflip();
        FadeToPal(PalettePcx);
        break;

    case PCX_V_SHADE: {
        /* Venetian-blind reveal: load the PCX into <screen> with centering +
           proper stride, then copy bands of rows from <screen> to Log row by
           row to animate the reveal. CopyBlock src/dst coords are shifted by
           the centering offsets so the reveal lines up with the centred PCX
           content; the sidebars stay whatever was already in Log. */
        S32 x, y;
        const S32 xOff = ((S32)ModeDesiredX - IMAGE_AUTHORED_WIDTH) / 2;
        const S32 yOff = ((S32)ModeDesiredY - IMAGE_AUTHORED_HEIGHT) / 2;

        if (!Image_LoadCentered(tmpFilePath, screen, numpcx))
            TheEndCheckFile(tmpFilePath);

        PaletteSync(PalettePcx);

        /* The reveal below only copies the centred PCX region from <screen>
           into Log, so clear Log to black first or the widescreen sidebars
           show stale pixels under the new palette. At 640x480 the final frame
           is unchanged (the reveal fills the whole frame); the un-revealed
           bands now wipe in from black instead of the previous frame. */
        memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);

        for (y = 0; y <= IMAGE_AUTHORED_HEIGHT - 1; y++) {
            CopyBlock(xOff, yOff + y, xOff + IMAGE_AUTHORED_WIDTH - 1, yOff + y + 4,
                      screen, xOff, yOff + y, Log);
            y += 5;

            for (x = y; x <= IMAGE_AUTHORED_HEIGHT - 1; x++) {
                CopyBlock(xOff, yOff + y, xOff + IMAGE_AUTHORED_WIDTH - 1, yOff + y,
                          screen, xOff, yOff + x, Log);
            }

            BoxStaticAdd(0, yOff + y - 5, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1);
            WaitVideoSync();
            BoxUpdate();
        }
    }; break;
    }

    if (flagbcl) {
        WaitNoInput();
        WaitInput();
        FadeToBlack(PalettePcx);
        RestoreTimer();
        AffScene(AFF_ALL_FLIP);
        FlagFade = TRUE;
    } else
        RestoreTimer();

    Dial_Letterbox = memoDialLetterbox;
}

/*──────────────────────────────────────────────────────────────────────────*/
void EffectPcxMess(U8 numpcx, U8 *screen, U8 effect, S32 mess) {
#ifdef CDROM
    S32 memoflagdisplaytext;
#endif

    /* PCX cinematic with a dialog message overlay (scripted scene-message
       opcodes, end-of-island stills, etc.). Letterbox the dialog so it stays
       inside the centred PCX picture instead of bleeding into the black
       sidebars at wider framebuffers. Encompasses EffectPcx + Dial; AffScene
       at the end clears the cinematic and the restore returns to gameplay
       dialog framing. */
    S32 memoDialLetterbox = Dial_Letterbox;
    Dial_Letterbox = 1;

    SaveTimer();

    EffectPcx(numpcx, screen, effect, FALSE);

    FirstTime = AFF_OBJETS_FLIP;
    FlagFade = FALSE;

#ifdef CDROM
    memoflagdisplaytext = FlagDisplayText;
    FlagDisplayText = TRUE;
#endif

    HQ_StopSample();

    Dial(mess, FALSE);

    RestartRainSample = TRUE;

#ifdef CDROM
    FlagDisplayText = memoflagdisplaytext;
#endif

    FadeToBlack(PalettePcx);
    RestoreTimer();
    FlagFade = FALSE;
    AffScene(AFF_ALL_FLIP);
    FlagFade = TRUE;

    Dial_Letterbox = memoDialLetterbox;
}

/*══════════════════════════════════════════════════════════════════════════*
     █    ██▄ █ ▀▀█▀▀ █▀▀▀█ █▀▀▀█ █▀▀▀▄ █   █ █▀▀▀▀ ▀▀█▀▀  █    █▀▀▀█ ██▄ █
     ██   ██▀██   ██  ██▀█▀ ██  █ ██  █ ██  █ ██      ██   ██   ██  █ ██▀██
     ▀▀   ▀▀  ▀   ▀▀  ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀  ▀▀▀▀▀ ▀▀▀▀▀   ▀▀   ▀▀   ▀▀▀▀▀ ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

void Introduction(void) {
    PlayAcf("INTRO");
    FlagPlayAcf = FALSE;
}
