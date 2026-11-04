#include "C_EXTERN.H"
#include "CONFIG.H"
#include "DIRECTORIES.H"
#include "INPUT.H"
#include "JOYSTICK.H"
#include "PERSO.H" /* WriteConfigFile (persist on resolution change) */
#include "RES_MENU.H"
#include "RES_SWITCH.H" /* Res_Switch (apply on cycle) */
#include "SAVENAME_VALIDATION.H"
#include "CONSOLE/CONSOLE.H"
#ifdef USE_GPURENDERER
#include <GL/RENDER_GL.H>
#endif
#include <SYSTEM/OPENXR.H>
#include <SVGA/VIDEO.H> /* PresentCurrentFrame (renderer switch present) */
#include "MESSAGE.H"
#include "IMAGE_LOAD.H"
#include "MENU_LABELS.H"

#include <FILEIO/SAVEPNG.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/ANDROID.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>
#include <SYSTEM/MOUSE.H>

#include "GAMEMENU_MOUSE.H"
#include "GAMEMENU_CAPTURE.H"
#include "GAMEMENU_INTERNAL.H"

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/*══════════════════════════════════════════════════════════════════════════*
 *══════════════════════════════════════════════════════════════════════════*/

#define COUL_TEXT_MENU LBAWHITE // trouver blanc palette jeu
#define COUL_LETTER_INPUT LBAWHITE

#ifdef DEMO
#define COUL_TEXT_MENU_DISABLE (3 * 16 + 11) // trouver blanc palette jeu
#endif

U8 CoulTextMenu = COUL_TEXT_MENU;
S32 LargeurMenu = 550;

#ifdef DEMO
S32 FlagShowEndDemo = FALSE;
#endif

char NewGameTxt[20]; // ATTENTION: max 19 chars

U32 TimerSample;
S32 CursorActif = FALSE;
S32 CursorX, CursorY;

const char *MenuIncrustText = NULL;
S32 MenuIncrustTimer = 0;
S32 MenuIncrustSprite = -1;

/*──────────────────────────────────────────────────────────────────────────*/

/* enum MenuLocalizedLabel and LocalizedMenuLabels live in MENU_LABELS.{H,CPP}
   so the host completeness test (tests/menu_labels) links the real table.

   MENU_LABELS.H is deliberately free of engine headers (the host test links it
   with nothing else), so it declares its own width. Bind the two together here,
   where both headers are in scope: add a language to MESSAGE.H without adding a
   translation column and this fails to compile, rather than silently rendering
   the new language's menus in English. */
typedef char MenuLabelTableCoversEveryLanguage[(MENU_LANGUAGE_COUNT == NB_LANGUAGES) ? 1 : -1];

/*──────────────────────────────────────────────────────────────────────────*/

#define SIZE_FOUND_OBJ 130

static S32 GetMenuLanguageIndex(void) {
    S32 language = Language;

    if (language < 0 OR language >= GetLanguageCount()) {
        language = 1;
    }

    return language;
}

const char *GetLocalizedMenuLabel(enum MenuLocalizedLabel label) {
    /* In range by construction: GetMenuLanguageIndex() bounds against
       GetLanguageCount() == NB_LANGUAGES, which the typedef above pins equal to
       the table's own width. */
    return LocalizedMenuLabels[label].lang[GetMenuLanguageIndex()];
}

const char *GetLocalizedResDialogLabel(int which) {
    static const enum MenuLocalizedLabel kMap[RES_DIALOG_LABEL_COUNT] = {
        MENU_LABEL_RES_TITLE,
        MENU_LABEL_RES_NOW_AT,
        MENU_LABEL_RES_KEEP_Q,
        MENU_LABEL_RES_BTN_KEEP,
        MENU_LABEL_RES_BTN_REVERT_FMT,
    };
    if (which < 0 || which >= (int)RES_DIALOG_LABEL_COUNT)
        return "";
    return GetLocalizedMenuLabel(kMap[which]);
}

static void BuildCustomMenuText(S32 num, char *string) {
    // Resolution sub-page rows: id in the reserved range maps to a catalog
    // entry's label (e.g. "1280x720 (16:9)"). s_resMenuCount is set by
    // GereResolutionMenu before the page is drawn.
    if (num >= MENU_ID_RESOLUTION_BASE &&
        num < MENU_ID_RESOLUTION_BASE + s_resMenuCount) {
        CopyUtf8ToCp850(string, 256, s_resMenuEntries[num - MENU_ID_RESOLUTION_BASE].label);
        return;
    }

    // Cheat submenu rows: the label is the typed cheat word, capitalized, with
    // the current On/Off state for the toggling cheats.
    if (num >= MENU_ID_CHEAT_BASE &&
        num < MENU_ID_CHEAT_BASE + CheatGetCount()) {
        const char *name = CheatGetName(num - MENU_ID_CHEAT_BASE);
        if (name) {
            char label[32];
            S32 state = CheatGetToggleState(num - MENU_ID_CHEAT_BASE);
            if (state >= 0)
                snprintf(label, sizeof(label), "%c%s: %s",
                         (char)toupper((unsigned char)name[0]), name + 1,
                         state ? "On" : "Off");
            else
                snprintf(label, sizeof(label), "%c%s",
                         (char)toupper((unsigned char)name[0]), name + 1);
            CopyUtf8ToCp850(string, 256, label);
        }
        return;
    }

    switch (num) {
    case MENU_ID_SOUND_VOLUME:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_AUDIO));
        break;

    case MENU_ID_CHOOSE_LANGUAGE:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_LANGUAGE));
        break;

    case MENU_ID_GAMEPLAY:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_GAMEPLAY));
        break;

    case MENU_ID_MOVIE_UPSCALE:
        CopyUtf8ToCp850(string, 256,
                        GetLocalizedMenuLabel(MovieUpscale == 2 ? MENU_LABEL_MOVIE_UPSCALE_XBR : MovieUpscale == 1 ? MENU_LABEL_MOVIE_UPSCALE_SCALE2X
                                                                                                                   : MENU_LABEL_MOVIE_UPSCALE_NEAREST));
        break;

    case MENU_ID_CONTROLLER:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_CONTROLLER));
        break;

    case MENU_ID_DISPLAY:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_DISPLAY));
        break;

    case MENU_ID_DISPLAY_GRAPHICS:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_GRAPHICS));
        break;

    case MENU_ID_DISPLAY_IMAGE:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_IMAGE));
        break;

#ifdef USE_GPURENDERER
    case MENU_ID_DISPLAY_SOFTSHADOWS:
        snprintf(string, 256, "Soft shadows: %s",
                 DisplaySoftShadows ? "On" : "Off");
        break;

    case MENU_ID_DISPLAY_COLORGRADE:
        snprintf(string, 256, "Colour grade: %s",
                 DisplayColorGrade == 1   ? "Warm"
                 : DisplayColorGrade == 2 ? "Cool"
                                          : "Off");
        break;

    case MENU_ID_DISPLAY_SUBPIXEL:
        snprintf(string, 256, "Sub-pixel pan: %s",
                 DisplaySubPixel ? "On" : "Off");
        break;
#endif

    case MENU_ID_DISPLAY_VR:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_VR));
        break;

    case MENU_ID_DISPLAY_WINDOW:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_WINDOW));
        break;

    case MENU_ID_CHEATS:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_CHEATS));
        break;

    case MENU_ID_RESOLUTION: {
        char value[64];
        Display_FormatCurrentLabel(value, sizeof value);
        FormatUtf8ToCp850(string, 256,
                          GetLocalizedMenuLabel(MENU_LABEL_RESOLUTION), value);
        break;
    }

    case MENU_ID_GAMEPAD_CONFIG:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_GAME_CONTROLLER));
        break;

    case MENU_ID_TEXT_LANGUAGE:
        FormatUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_TEXT_LANGUAGE), GetLanguageName(Language));
        break;

    case MENU_ID_VOICE_LANGUAGE:
#ifdef CDROM
        FormatUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_VOICE_LANGUAGE), GetLanguageName(LanguageCD));
#else
        string[0] = 0;
#endif
        break;

    case MENU_ID_STEREO_CHANNELS:
        GetMultiText(12 + ReverseStereo, string);
        break;

    case MENU_ID_MOVIE_CAMERA:
        GetMultiText(46 + AllCameras, string);
        break;

    case MENU_ID_VIDEO_SIZE:
        CopyUtf8ToCp850(string, 256,
                        GetLocalizedMenuLabel(VideoFullScreen ? MENU_LABEL_MOVIE_FIT
                                                              : MENU_LABEL_MOVIE_ORIGINAL));
        break;

    case MENU_ID_DISPLAY_FULLSCREEN:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(DisplayFullScreen ? MENU_LABEL_DISPLAY_FULLSCREEN : MENU_LABEL_DISPLAY_WINDOWED));
        break;

    case MENU_ID_DISPLAY_VSYNC:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(DisplayVSync ? MENU_LABEL_VSYNC_ON : MENU_LABEL_VSYNC_OFF));
        break;

#ifdef USE_GPURENDERER
    case MENU_ID_DISPLAY_RENDERER:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(GPURendererActive ? MENU_LABEL_RENDERER_GL : MENU_LABEL_RENDERER_SW));
        break;

    case MENU_ID_DISPLAY_TEXFILTER:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(DisplayTexFilter == 4 ? MENU_LABEL_TEXFILTER_XBRLITE : DisplayTexFilter == 3 ? MENU_LABEL_TEXFILTER_XBRTEX
                                                                                                              : DisplayTexFilter == 2   ? MENU_LABEL_TEXFILTER_SCALE2X
                                                                                                              : DisplayTexFilter        ? MENU_LABEL_TEXFILTER_BILINEAR
                                                                                                                                        : MENU_LABEL_TEXFILTER_NEAREST));
        break;

    case MENU_ID_DISPLAY_ANTIALIAS:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(DisplayAntiAlias == 3 ? MENU_LABEL_AA_MSAA4 : DisplayAntiAlias == 2 ? MENU_LABEL_AA_MSAA2
                                                                                                     : DisplayAntiAlias == 1   ? MENU_LABEL_AA_FXAA
                                                                                                                               : MENU_LABEL_AA_OFF));
        break;

    case MENU_ID_DISPLAY_DITHER:
        snprintf(string, 256, "Dithering: %s", DisplayDither ? "On" : "Off");
        break;

    case MENU_ID_DISPLAY_SHARPEN:
        snprintf(string, 256, "Sharpening: %s",
                 DisplaySharpen == 2   ? "High"
                 : DisplaySharpen == 1 ? "Low"
                                       : "Off");
        break;

    case MENU_ID_DISPLAY_STEREO3D:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(DisplayStereo3D ? MENU_LABEL_STEREO3D_ON : MENU_LABEL_STEREO3D_OFF));
        break;

    case MENU_ID_DISPLAY_VRZOOM:
        snprintf(string, 256, "VR zoom: %d%%", VrFovScale);
        break;

    case MENU_ID_DISPLAY_VRSUPERSAMPLE:
        snprintf(string, 256, "Supersampling: %s",
                 VrSuperSample == 2   ? "1.5x"
                 : VrSuperSample == 1 ? "1.25x"
                                      : "Off");
        break;

    case MENU_ID_DISPLAY_VRHOVER:
        snprintf(string, 256, "Interior hover: %s",
                 VrInteriorHover ? "On" : "Off");
        break;

    case MENU_ID_DISPLAY_VRPASSTHROUGH:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(VrPassthrough ? MENU_LABEL_INTERIOR_PASSTHROUGH_ON : MENU_LABEL_INTERIOR_PASSTHROUGH_OFF));
        break;

    case MENU_ID_DISPLAY_VRINTERIOR:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_INTERIOR));
        break;

    case MENU_ID_DISPLAY_VRBODY:
        snprintf(string, 256, "Body / hands");
        break;

    case MENU_ID_DISPLAY_VRFULLBODY:
        snprintf(string, 256, "Full body: %s",
                 VrFullBody ? "On" : "Off");
        break;

    case MENU_ID_DISPLAY_VRWORLDSCALE:
        snprintf(string, 256, "World scale: %d%%", VrWorldScale);
        break;

    case MENU_ID_DISPLAY_QUALITY:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(RenderQuality == 1 ? MENU_LABEL_QUALITY_NATIVE : RenderQuality == 2 ? MENU_LABEL_QUALITY_2X
                                                                                                                               : MENU_LABEL_QUALITY_4X));
        break;

    case MENU_ID_DISPLAY_BRIGHTNESS:
        snprintf(string, 256, "Brightness: %+d", DisplayBrightness);
        break;

    case MENU_ID_DISPLAY_CONTRAST:
        snprintf(string, 256, "Contrast: %+d", DisplayContrast);
        break;

    case MENU_ID_DISPLAY_SATURATION:
        snprintf(string, 256, "Saturation: %+d", DisplaySaturation);
        break;
#endif

    case MENU_ID_FOLLOW_CAMERA:
        if (CameraFirstPerson) {
            CopyUtf8ToCp850(string, 256, "First person");
        } else {
            CopyUtf8ToCp850(string, 256,
                            GetLocalizedMenuLabel(FollowCamera
                                                      ? MENU_LABEL_AUTO_CAMERA
                                                      : MENU_LABEL_CLASSIC_CAMERA));
        }
        break;

    case MENU_ID_DISPLAY_TEXT:
        GetMultiText(16 + FlagDisplayText, string);
        break;

    case MENU_ID_DISPLAY_FPS:
        CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(DebugFps ? MENU_LABEL_FPS_ON : MENU_LABEL_FPS_OFF));
        break;

    case MENU_ID_FPS_LIMIT:
        if (FpsLimit <= 0) {
            CopyUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_FPSLIMIT_OFF));
        } else {
            FormatUtf8ToCp850(string, 256, GetLocalizedMenuLabel(MENU_LABEL_FPSLIMIT_FMT), FpsLimit);
        }
        break;

    case MENU_ID_GENERAL_VOLUME:
        GetMultiText(23, string);
        break;

    default:
        GetMultiText(num, string);
        break;
    }
}

/*══════════════════════════════════════════════════════════════════════════*
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

void Init3DView(void) {
    // Iso projection centre = framebuffer centre - 9px horizontal shim. The -9 is
    // an iso-grid offset that pairs with Map2Screen's -32 origin shim (see
    // GRILLE.CPP::Map2Screen) — the two iso sites must move together to keep
    // interior scene composition coherent. Routing through ModeDesiredX/Y so
    // iso scenes stay centred at any render-space size; at 640x480 this is the
    // original (311, 240). See docs/WIDESCREEN.md Phase 2.
    SetIsoProjection((S32)ModeDesiredX / 2 - 9, (S32)ModeDesiredY / 2);
    SetPosCamera(0, 0, 0);
    SetAngleCamera(0, 0, 0);
    SetLightVector(AlphaLight, BetaLight, 0);
}

/*──────────────────────────────────────────────────────────────────────────*/
void Init3DGame() {
    Init3DView();

    // Initialise les pointeurs de fonctions
    PtrReajustPos = ReajustPos;
    PtrGetShadow = GetShadow;
    PtrDoAnim = DoAnim;
    PtrInitGrille = InitGrille;
    PtrWorldColBrick = WorldColBrick;
    PtrFullWorldColBrick = FullWorldColBrick;
    PtrWorldColBrickVisible = WorldColBrickVisible;
    PtrProjectShadow = ProjectShadow;
    PtrReajustPosExtra = ReajustPosExtra;
    PtrInit3DView = Init3DView;
}

/*──────────────────────────────────────────────────────────────────────────*/
void SetDetailLevel(void) {
    S32 oldmaxpolysea = MaxPolySea;
    U8 olddrawhorizon = FlagDrawHorizon;

    switch (DetailLevel) {
    case 0:                      // machine bof !!!!!!!!
        MaxPolySea = 0;          // Pas d'eau animée
        Shadow = 1;              // Pas d'ombres sur extras
        RainEnable = FALSE;      // Pas d'affichage de pluie
        FlagDrawHorizon = FALSE; // Pas D'affichage des cubes autour
        break;

    case 1:                     // 486 ?
        MaxPolySea = 0;         // Pas d'eau animée
        Shadow = 2;             // pas d'ombres sur impacts
        RainEnable = FALSE;     // Pas d'affichage de pluie
        FlagDrawHorizon = TRUE; // Affichage des cubes autour
        break;

    case 2:                     // Pentium de base ?
        MaxPolySea = 0;         // Pas d'eau animée
        Shadow = 3;             // full shadows
        RainEnable = TRUE;      // Affichage de la pluie
        FlagDrawHorizon = TRUE; // Affichage des cubes autour
        break;

    default: // machine de folie !!!!!
        if (PtrPolySea)
            MaxPolySea = MAX_POLY_SEA;
        Shadow = 3;             // full shadows
        RainEnable = TRUE;      // Affichage de la pluie
        FlagDrawHorizon = TRUE; // Affichage des cubes autour
        break;
    }

    if (FlagDrawHorizon != olddrawhorizon OR MaxPolySea != oldmaxpolysea) {
        FirstTime = AFF_ALL_FLIP;
    }
}

//──────────────────────────────────────────────────────────────────────────
void InitMenuIncrustText(const char *text, S32 sprite, S32 time) {
    ManageTime();
    MenuIncrustText = text;
    MenuIncrustTimer = TimerRefHR + time * 1000; // end message timer in s
    MenuIncrustSprite = sprite;
}

// Plasma colour banks: SelectPlasmaBank(n) ramps the fire bar through palette
// indices n*16+0..10. 12 is the blue/cyan cursor highlight, 4 the red volume
// slider, 8 a green band used to mark the active value distinct from the cursor
// (see GereResolutionMenu). Palette rows verified against the menu palette.
#define PLASMA_BANK_CURSOR 12
#define PLASMA_BANK_SLIDER 4
#define PLASMA_BANK_GREEN 8

//──────────────────────────────────────────────────────────────────────────
void DrawFireBar(S32 xmin, S32 ymin, S32 xmax, S32 ymax, S32 coul) {
    Struc_Point TabPoint[4];

    TabPoint[0].Pt_XE = (S16)xmin;
    TabPoint[0].Pt_YE = (S16)ymin;
    TabPoint[0].Pt_MapU = (U16)0;
    TabPoint[0].Pt_MapV = (U16)0;

    TabPoint[1].Pt_XE = (S16)xmin;
    TabPoint[1].Pt_YE = (S16)ymax;
    TabPoint[1].Pt_MapU = (U16)0; //(16<<8);
    TabPoint[1].Pt_MapV = (U16)((63 << 8) + 250);

    TabPoint[2].Pt_XE = (S16)xmax;
    TabPoint[2].Pt_YE = (S16)ymax;
    TabPoint[2].Pt_MapU = (U16)(2 * ((63 << 8) + 250)); //(16<<8);
    TabPoint[2].Pt_MapV = (U16)((63 << 8) + 250);

    TabPoint[3].Pt_XE = (S16)xmax;
    TabPoint[3].Pt_YE = (S16)ymin;
    TabPoint[3].Pt_MapU = (U16)(2 * ((63 << 8) + 250));
    TabPoint[3].Pt_MapV = (U16)0;

    RepMask = 0x3f3f; // repmask x=32 y=32

    switch (coul) {
    case FIRE_SELECT_MENU:
    case FIRE_INPUT_NAME:
        SelectPlasmaBank(PLASMA_BANK_CURSOR);
        break;

    case FIRE_SLIDER_MENU:
        SelectPlasmaBank(PLASMA_BANK_SLIDER);
        break;

    case FIRE_CURRENT_MENU:
        SelectPlasmaBank(PLASMA_BANK_GREEN);
        break;
    }

    DoTextureAnimation();

    FixePlasmaPtrMap();

    Fill_Poly(POLY_TEXTURE, 0, 4, TabPoint);
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*
             ██▀▀▀ █▀▀▀█ █   █ █▀▀▀▀       █     █▀▀▀█ █▀▀▀█ █▀▀▀▄
             ▀▀▀▀█ ██▀▀█ ██ ▄▀ ██▀▀        ██    ██  █ ██▀▀█ ██  █
             ▀▀▀▀▀ ▀▀  ▀ ▀▀▀   ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

S32 HQR_GiveOffset(T_HQR_HEADER *header, S32 index) {
    S32 handle;
    S32 nbbloc;

    handle = OpenRead(header->Name);
    if (!handle)
        return 0;

    index *= 4;
    Read(handle, &nbbloc, 4);

    if (index >= nbbloc) {
        Close(handle);
        return 0;
    }

    Seek(handle, index, SEEK_FROM_START);
    Read(handle, &index, 4);

    Close(handle);
    return index;
}

////////*************

/*══════════════════════════════════════════════════════════════════════════*
                            █▄ ▄█ █▀▀▀▀ ██▄ █ █   █
                            ██▀ █ ██▀▀  ██▀██ ██  █
                            ▀▀  ▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

S32 LastSampleVolume;
S32 SampleMenu;
S32 LastVoiceVolume;
S32 VoiceMenu;
U32 PanMenu;

S32 WayChoice;
S32 DeltaChoice;
MOVE MoveChoice;

/*──────────────────────────────────────────────────────────────────────────*/
void InitShakeChoice() {
    WayChoice = -1; //     To the Left First
    DeltaChoice = 0;

    InitMove(&MoveChoice, 48);
}
/*──────────────────────────────────────────────────────────────────────────*/

void DrawOneChoice(S32 x, S32 y, S32 type, S32 num, S32 select) {
    S32 x2, x0, y0, x1, y1;
    S32 dx;
    char string[256];
    S32 value;
    S32 drawslider = FALSE;
    S32 sliderRange = 127; // volume sliders read 0..127
    U8 *samplePtr;
    S32 sampleSizeBytes;

    x0 = x - LargeurMenu / 2;
    x1 = x + LargeurMenu / 2;

    y0 = y - HAUTEUR_STANDARD / 2;
    y1 = y + HAUTEUR_STANDARD / 2;

    // fond

    BackupAngles(x0, y0, x1, y1);

    // select is a bitmask: bit 0 = cursor row (blue plasma, the usual
    // highlight), bit 1 = active-value marker (green plasma, e.g. the current
    // resolution in GereResolutionMenu). Cursor wins when a row is both.
    if (select & 1) {
        switch (type) {
        case 2: // sample volume
            value = SampleVolume;
            drawslider = TRUE;
            break;

        case 3: // voice volume
            value = VoiceVolume;
            drawslider = TRUE;
            break;

        case 4: // Jingle volume
            value = JingleVolume;
            drawslider = TRUE;
            break;

        case 5: // CD volume
#ifdef CDROM
            value = CDVolume;
#else
            value = 127;
#endif
            drawslider = TRUE;
            break;

        case 6: // master volume
            value = MasterVolume;
            drawslider = TRUE;
            break;

        case 7: // Niveau de détails
            x2 = RegleTrois(x0, x1, MAX_DETAIL_LEVEL, DetailLevel);
            DrawFire(y0, FIRE_SLIDER_MENU);
            Box(x2, y0, x1, y1, COUL_SELECT_MENU);
            break;

        case MENU_TYPE_IMAGE_BRIGHTNESS:
            value = DisplayBrightness + 20; // signed -20..20 -> zero-centred 0..40
            sliderRange = 40;
            drawslider = TRUE;
            break;

        case MENU_TYPE_IMAGE_CONTRAST:
            value = DisplayContrast + 20;
            sliderRange = 40;
            drawslider = TRUE;
            break;

        case MENU_TYPE_IMAGE_SATURATION:
            value = DisplaySaturation + 20;
            sliderRange = 40;
            drawslider = TRUE;
            break;

        default:
            DrawFire(y0, FIRE_SELECT_MENU);
        }

        if (drawslider) {
            x2 = RegleTrois(x0, x1, sliderRange, value);
            DrawFire(y0, FIRE_SLIDER_MENU);
            Box(x2, y0, x1, y1, COUL_SELECT_MENU);

            if (SamplesEnable) {
                switch (type) {
                case 2: // sample volume
                case 6: // Master volume
                    // bruitons un peu la situation (violement)

                    if ((TimerRefHR - TimerSample) > 35 * 20 AND !IsSamplePlaying(SampleMenu)) {
                        TimerSample = TimerRefHR;

                        do {
                            SampleMenu = 0 + MyRnd(28);
                        } while (!HQR_GiveOffset(HQR_Samples, SampleMenu));

                        PanMenu = 20 + MyRnd(88);

                        HQ_MixSample(SampleMenu, 0x1000, 600, 1, PanMenu, 127);
                    } else if (SampleVolume != LastSampleVolume) {
                        HQ_ChangePanSample(SampleMenu, 127, PanMenu);
                        LastSampleVolume = SampleVolume;
                    }
                    break;

#ifndef DEMO
                case 3: // voice volume
                    // bruitons un peu la situation (violement)

                    if ((TimerRefHR - TimerSample) > 5000 AND !IsSamplePlaying(VoiceMenu)) {
                        TimerSample = TimerRefHR;

                        VoiceMenu = SAMPLE_VOICE_MENU + LanguageCD;

                        PanMenu = 20 + MyRnd(88);

                        samplePtr = (U8 *)GivePtrSample(VoiceMenu, &sampleSizeBytes);
                        PlaySample(samplePtr, sampleSizeBytes, VoiceMenu, 0x1000, 1, VoiceVolume, PanMenu);
                    } else if (VoiceVolume != LastVoiceVolume) {
                        ChangeVolumePanSample(VoiceMenu, VoiceVolume, PanMenu);
                        LastVoiceVolume = VoiceVolume;
                    }
                    break;
#endif

                case 4: // Music Volume
                    if ((TimerRefHR - TimerSample) > 1000) {
                        TimerSample = TimerRefHR;
#ifdef CDROM
                        if (IsCDPlaying())
                            StopMusic();
#endif
                        PlayMusic(JINGLE_TRACK_MENU, TRUE);
                    }
                    break;

                case 5: // CD Volume
                    if ((TimerRefHR - TimerSample) > 1000) {
                        TimerSample = TimerRefHR;
#ifdef CDROM
                        if (IsStreamPlaying())
                            StopMusic();
                        PlayMusic(CD_TRACK_MENU, TRUE);
#endif
                    }
                    break;
                }
            }
        }

    } else if (select & 2) {
        // Active-value marker (not the cursor): green plasma bar.
        DrawFire(y0, FIRE_CURRENT_MENU);
    } else {
        CopyBlock(x0, y0, x1, y1, Screen, x0, y0, Log);
        ShadeBoxBlk(x0, y0, x1, y1, MENU_SHADE_LVL);
    }

    // cadre
    DrawCadre(x0, y0, x1, y1, ALL_ANGLES);
    RestoreAngles(x0, y0, x1, y1);

    // text

    ColorFont(CoulTextMenu);
    BuildCustomMenuText(num, string);

    //----------------------------------------------------------------------------
    MemoClip();
    SetClip(x0 + 1, y0, x1 - 1, y1);

    dx = SizeFont(string);

    if (dx > LargeurMenu) //       Biggest than window
    {
        if (!select)
            Font(x0 + 8, y - 18, string); // To the Left
        else {                            //     Shake Baby, shake...
            if (WayChoice == -1)
                DeltaChoice -= GetDeltaMove(&MoveChoice);
            else
                DeltaChoice += GetDeltaMove(&MoveChoice);

            if (DeltaChoice < LargeurMenu - dx - 8) {
                DeltaChoice = LargeurMenu - dx - 8;
                WayChoice = 1; // Right
            }
            if (DeltaChoice > 8) {
                DeltaChoice = 8;
                WayChoice = -1; // Left
            }
            Font(x0 + DeltaChoice, y - 18, string);
        }
    } else
        Font(x - SizeFont(string) / 2, y - 18, string);

    RestoreClip();
    //----------------------------------------------------------------------------

    // flip

    BoxStaticAdd(x0, y0, x1, y1);
}

/*──────────────────────────────────────────────────────────────────────────*/

void DrawGameMenu(U16 *ptrmenu, S32 justone) {
    S32 n, y;
    S32 nb;
    S32 selected, type, num;
#ifdef DEMO
    U8 mainmenu = FALSE;

    if (ptrmenu == GameMainMenu)
        mainmenu = TRUE;
#endif

    selected = *ptrmenu++;

    nb = *ptrmenu++;

    y = *ptrmenu++;

    ptrmenu++; // dial num

    if (y == 0) // depuis le haut
    {
        // Top-anchored list (the ask-choice menu, GameChoiceMenu, the only
        // y==0 user). Float with the canvas so the choices track the rest of
        // the UI on a taller framebuffer instead of staying stranded at the top
        // edge while the question bubble (MyDial) bottom-anchors. No-op at 480.
        y = HAUTEUR_STANDARD / 2 + 10 + UI_VCENTER_OFS;
    } else // centre sur y
    {
        // The table y-centre is authored on the 480-tall canvas; float it to
        // the vertical middle of a taller framebuffer (no-op at 480).
        y += UI_VCENTER_OFS;
        y -= (HAUTEUR_STANDARD * nb + (nb - 1) * MENU_SPACE) / 2;
    }

    for (n = 0; n < nb; n++) {
        type = *ptrmenu++;
        num = *ptrmenu++;

#ifdef DEMO
        if (mainmenu AND num == MENU_ID_MAIN_LOAD OR num == MENU_ID_MAIN_SAVE) // save or load
        {
            CoulTextMenu = COUL_TEXT_MENU_DISABLE;
        } else {
            CoulTextMenu = COUL_TEXT_MENU;
        }
#endif

        // Bit 0: cursor row. Bit 1: the green active-value marker row (only the
        // resolution sub-page sets s_menuMarkRow; -1 elsewhere so this is a no-op).
        S32 sel = (n == selected) ? 1 : 0;
        if (n == s_menuMarkRow)
            sel |= 2;

        if (justone) {
            // Per-frame redraw animates the plasma. Refresh the cursor row and,
            // when set, the green marker row, so the marker persists and animates
            // between full redraws instead of being drawn once and cleared.
            if (n == selected || n == s_menuMarkRow) {
                DrawOneChoice((S32)ModeDesiredX / 2, y, type, num, sel);
            }
        } else {
            DrawOneChoice((S32)ModeDesiredX / 2, y, type, num, sel);
        }

        y += HAUTEUR_STANDARD + MENU_SPACE;
    }

#ifdef DEMO
    if (mainmenu)
        CoulTextMenu = COUL_TEXT_MENU;
#endif

    BoxUpdate();
}

/*──────────────────────────────────────────────────────────────────────────*/

#define VOLUME_TIMER_KEY 5

static void GameMenuApplyMouseWheelDoGameMenu(U16 *ptrmenu, S16 selected, S32 wy, S32 *timerkey, S32 *flag) {
    S32 t;
    S32 n;
    S32 dir;
    S32 i;
    S32 applied;

    if (wy == 0 || !FlagMenuMouse || MenuMouse_GetRefCount() <= 0)
        return;

    t = ptrmenu[4 + 0 + selected * 2];
    dir = (wy > 0) ? 1 : -1;
    n = (wy > 0) ? wy : -wy;
    if (n > 16)
        n = 16;

    for (i = 0; i < n; i++) {
        if (TimerRefHR <= *timerkey)
            break;
        *timerkey = TimerRefHR + VOLUME_TIMER_KEY;
        applied = 0;

        switch (t) {
        case 2:
            if (dir < 0 AND SampleVolume > 0) {
                SampleVolume -= 1;
                applied = 1;
            } else if (dir > 0 AND SampleVolume < 127) {
                SampleVolume += 1;
                applied = 1;
            }
            break;

        case 3:
            if (dir < 0 AND VoiceVolume > 0) {
                VoiceVolume -= 1;
                applied = 1;
            } else if (dir > 0 AND VoiceVolume < 127) {
                VoiceVolume += 1;
                applied = 1;
            }
            break;

        case 4:
            if (dir < 0 AND JingleVolume > 0) {
                JingleVolume -= 1;
                SetVolumeJingle(JingleVolume);
                applied = 1;
            } else if (dir > 0 AND JingleVolume < 127) {
                JingleVolume += 1;
                SetVolumeJingle(JingleVolume);
                applied = 1;
            }
            break;

        case 5:
#ifdef CDROM
            if (dir < 0 AND CDVolume > 0) {
                CDVolume -= 1;
                SetVolumeCD(CDVolume);
                SetVolumeJingle(CDVolume);
                applied = 1;
            } else if (dir > 0 AND CDVolume < 127) {
                CDVolume += 1;
                SetVolumeCD(CDVolume);
                SetVolumeJingle(CDVolume);
                applied = 1;
            }
#endif
            break;

        case 6:
            if (dir < 0 AND MasterVolume > 0) {
                MasterVolume -= 1;
                SetMasterVolumeSample(MasterVolume);
                SetVolumeJingle(JingleVolume);
#ifdef CDROM
                SetVolumeCD(CDVolume);
#endif
                applied = 1;
            } else if (dir > 0 AND MasterVolume < 127) {
                MasterVolume += 1;
                SetMasterVolumeSample(MasterVolume);
                SetVolumeJingle(JingleVolume);
#ifdef CDROM
                SetVolumeCD(CDVolume);
#endif
                applied = 1;
            }
            break;

        case 7:
            if (dir < 0 AND DetailLevel > 0) {
                DetailLevel--;
                InitWaitNoInput(I_LEFT);
                InitWaitNoKey();
                applied = 1;
            } else if (dir > 0 AND DetailLevel < MAX_DETAIL_LEVEL) {
                DetailLevel++;
                InitWaitNoInput(I_RIGHT);
                InitWaitNoKey();
                applied = 1;
            }
            break;

        case MENU_TYPE_IMAGE_BRIGHTNESS:
            if (dir<0 AND DisplayBrightness> - 20) {
                DisplayBrightness -= 1;
                applied = 1;
            } else if (dir > 0 AND DisplayBrightness < 20) {
                DisplayBrightness += 1;
                applied = 1;
            }
            break;

        case MENU_TYPE_IMAGE_CONTRAST:
            if (dir<0 AND DisplayContrast> - 20) {
                DisplayContrast -= 1;
                applied = 1;
            } else if (dir > 0 AND DisplayContrast < 20) {
                DisplayContrast += 1;
                applied = 1;
            }
            break;

        case MENU_TYPE_IMAGE_SATURATION:
            if (dir<0 AND DisplaySaturation> - 20) {
                DisplaySaturation -= 1;
                applied = 1;
            } else if (dir > 0 AND DisplaySaturation < 20) {
                DisplaySaturation += 1;
                applied = 1;
            }
            break;

        case MENU_TYPE_TEXT_LANGUAGE:
            if (dir < 0)
                CycleTextLanguage(-1);
            else
                CycleTextLanguage(1);
            *flag = 1;
            InitWaitNoInput(dir < 0 ? I_LEFT : I_RIGHT);
            InitWaitNoKey();
            applied = 1;
            break;

#ifdef CDROM
        case MENU_TYPE_VOICE_LANGUAGE:
            if (dir < 0)
                CycleVoiceLanguage(-1);
            else
                CycleVoiceLanguage(1);
            *flag = 1;
            InitWaitNoInput(dir < 0 ? I_LEFT : I_RIGHT);
            InitWaitNoKey();
            applied = 1;
            break;
#endif

        default:
            break;
        }

        if (!applied)
            break;
    }
}

/* Harness capture state — see Menu_RequestCapture() in GAMEMENU_CAPTURE.H.  When the
   path is non-empty, DoGameMenu's modal loop counts down per iteration;
   on reaching zero it SavePNGs and returns 1000 (the "ESC" sentinel) so the
   wrapping menu loop (OptionsMenu, MainGameMenu, ...) sees a clean exit.
   The countdown gives the menu's fade-in and any per-frame text/sprite
   animations time to settle into a steady visual state. */
static char s_menu_capture_path[ADELINE_MAX_PATH] = "";
static int s_menu_capture_countdown = 0;

void Menu_RequestCapture(const char *path) {
    if (!path || !path[0]) {
        s_menu_capture_path[0] = '\0';
        s_menu_capture_countdown = 0;
        return;
    }
    strncpy(s_menu_capture_path, path, ADELINE_MAX_PATH - 1);
    s_menu_capture_path[ADELINE_MAX_PATH - 1] = '\0';
    s_menu_capture_countdown = 60; /* iterations -- ~1 s at fixed-dt 16, past any
                                      menu fade-in and dynamic-item initial draws */
}

S32 Menu_IsCaptureArmed(void) {
    return s_menu_capture_path[0] != '\0';
}

S32 DoGameMenu(U16 *ptrmenu) {
    S32 flag = 1;
    U32 chrono;
    S32 timerkey;
    // Gamepad D-pad / stick fallback: edge-only, matching the keyboard-arrow
    // feel (one step per press, no auto-repeat). Primed below so a stick
    // already deflected on menu entry does not auto-fire.
    S32 padNavDir = 0;
    S16 nb, selected;

    selected = ptrmenu[0];
    nb = ptrmenu[1];

    ManageTime();
    chrono = TimerRefHR;

    InitWaitNoKey();
    InitWaitNoInput(I_JOY | I_FIRE | I_MENUS | I_ACTION);

    MenuIncrustText = NULL;
    MenuIncrustSprite = -1;

    timerkey = TimerRefHR + VOLUME_TIMER_KEY;
    padNavDir = JoystickMenuNavOnly();

    InitShakeChoice(); //     NEW

    AsciiMode = TRUE;

    while (TRUE) {
        ManageTime();
        MyGetInput();

        // Gamepad D-pad / left stick feed the same codes as keyboard arrows.
        // Edge-only: fire one K_GRAY_* on the direction change so a held
        // stick/dpad behaves like a held keyboard arrow (one step, stay).
        if (!MyKey) {
            S32 rawDir = JoystickMenuNavOnly();
            if (rawDir != padNavDir) {
                padNavDir = rawDir;
                if (rawDir == K_UP)
                    MyKey = K_GRAY_UP;
                else if (rawDir == K_DOWN)
                    MyKey = K_GRAY_DOWN;
                else if (rawDir == K_LEFT)
                    MyKey = K_GRAY_LEFT;
                else if (rawDir == K_RIGHT)
                    MyKey = K_GRAY_RIGHT;
            }
        }

        if (FlagMenuMouse && MenuMouse_GetRefCount() > 0) {
            S32 mret = GameMenuMouseProcessDoGameMenu(ptrmenu, &selected, &flag);

            if (mret >= 0) {
                WaitNoInput();
                AsciiMode = FALSE;
                return mret;
            }
        }

#ifdef DEBUG_TOOLS
        CheckSaveLogPcx(PtrPalNormal);
#endif

        // rustine Démo non jouable ...
        if (ptrmenu == GameMainMenu) {
            if ((TimerRefHR - chrono > 50 * 20 * 60)
                    OR(CheckKey(K_D) AND(CheckKey(K_SHIFT) OR CheckKey(K_SHIFT_LEFT) OR CheckKey(K_SHIFT_RIGHT)))) {
#ifdef DEMO
                //                                if( SlideShow() )
                //					DemoBumper() ;
                SlideShow();

                char tmpFilePathScreen[ADELINE_MAX_PATH];
                GetResPath(tmpFilePathScreen, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
                Image_LoadStretched(tmpFilePathScreen, Log, PCR_MENU);
                CopyScreen(Log, Screen);
                BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

                FlagShowEndDemo = TRUE;
#else
                BeginDemoSlide();

                InitGame(1);
                NewCube = 193;

                FlagFade = TRUE;

                if (MainLoop())
                    if (SlideShow())
                        Credits(FALSE);

                UnsetClipWindow();

                // MainLoop's render path overwrote Log with gameplay frames
                // during the attract reel; restore the menu plate before
                // CopyScreen / BoxReset / FadeToBlack below so the menu UI
                // loop has the right background. Same image as the menu's
                // own boot a few hundred lines down (BuildGameMainMenu /
                // Image_LoadStretched into Screen + CopyScreen to Log).
                // Affects every exit from MainLoop on the cold-start attract
                // path: stuck-cube watchdog timeout (#176), mid-reel ESC,
                // LM_THE_END natural reel end.
                {
                    char tmpFilePathScreen[ADELINE_MAX_PATH];
                    GetResPath(tmpFilePathScreen, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
                    Image_LoadStretched(tmpFilePathScreen, Log, PCR_MENU);
                }

                CopyScreen(Log, Screen);
                BoxReset();
#endif
                PtrPal = PtrPalNormal;
                if (!FlagBlackPal)
                    FadeToBlack(PtrPal);

                FadeMenu = TRUE;
                AsciiMode = FALSE;
                return MENU_RET_DEMO_REEL;
            }
        }

        if (Input & I_DOWN OR MyKey == K_GRAY_DOWN) {
#ifdef DEMO
            if (ptrmenu == GameMainMenu) {
                do {
                    selected++;
                } while (selected < nb
                                        AND(ptrmenu[MENU_HEADER_WORDS + 1 + selected * 2] == MENU_ID_MAIN_LOAD OR ptrmenu[MENU_HEADER_WORDS + 1 + selected * 2] == MENU_ID_MAIN_SAVE));
            } else
#endif
            {
                selected++;
            }

            if (selected >= nb)
                selected = 0;
            flag = 1;
        }

        if (Input & I_UP OR MyKey == K_GRAY_UP) {
#ifdef DEMO
            if (ptrmenu == GameMainMenu) {
                do {
                    selected--;
                } while (selected >= 0 AND(ptrmenu[MENU_HEADER_WORDS + 1 + selected * 2] == MENU_ID_MAIN_LOAD OR ptrmenu[MENU_HEADER_WORDS + 1 + selected * 2] == MENU_ID_MAIN_SAVE));
            } else
#endif
            {
                selected--;
            }

            if (selected < 0)
                selected = (U16)(nb - 1);
            flag = 1;
        }

        switch (ptrmenu[4 + 0 + selected * 2]) // type
        {
        case 0: // text normal
            break;

        case 2: // volume sample

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND SampleVolume > 0) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                SampleVolume -= 1;
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND SampleVolume < 127) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                SampleVolume += 1;
            }
            break;

        case 3: // volume voix (jouer 1 voix ?)

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND VoiceVolume > 0) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                VoiceVolume -= 1;
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND VoiceVolume < 127) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                VoiceVolume += 1;
            }
            break;

        case 4: // volume musiques

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND JingleVolume > 0) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                JingleVolume -= 1;
                SetVolumeJingle(JingleVolume);
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND JingleVolume < 127) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                JingleVolume += 1;
                SetVolumeJingle(JingleVolume);
            }
            break;

        case 5: // volume cd
#ifdef CDROM
            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND CDVolume > 0) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                CDVolume -= 1;
                SetVolumeCD(CDVolume);
                SetVolumeJingle(CDVolume);
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND CDVolume < 127) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                CDVolume += 1;
                SetVolumeCD(CDVolume);
                SetVolumeJingle(CDVolume);
            }
#endif
            break;

        case 6: // volume master

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND MasterVolume > 0) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                MasterVolume -= 1;
                SetMasterVolumeSample(MasterVolume);
                SetVolumeJingle(JingleVolume);
#ifdef CDROM
                SetVolumeCD(CDVolume);
#endif
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND MasterVolume < 127) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                MasterVolume += 1;
                SetMasterVolumeSample(MasterVolume);
                SetVolumeJingle(JingleVolume);
#ifdef CDROM
                SetVolumeCD(CDVolume);
#endif
            }
            break;

        case 7: // Niveau détails

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND(DetailLevel > 0)) {
                DetailLevel--;
                InitWaitNoInput(I_LEFT);
                InitWaitNoKey();
            }

            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND(DetailLevel < MAX_DETAIL_LEVEL)) {
                DetailLevel++;
                InitWaitNoInput(I_RIGHT);
                InitWaitNoKey();
            }
            break;

        case MENU_TYPE_IMAGE_BRIGHTNESS:

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND DisplayBrightness > -20) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                DisplayBrightness -= 1;
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND DisplayBrightness < 20) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                DisplayBrightness += 1;
            }
            break;

        case MENU_TYPE_IMAGE_CONTRAST:

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND DisplayContrast > -20) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                DisplayContrast -= 1;
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND DisplayContrast < 20) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                DisplayContrast += 1;
            }
            break;

        case MENU_TYPE_IMAGE_SATURATION:

            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)
                    AND TimerRefHR > timerkey AND DisplaySaturation > -20) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                DisplaySaturation -= 1;
            }
            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)
                    AND TimerRefHR > timerkey AND DisplaySaturation < 20) {
                timerkey = TimerRefHR + VOLUME_TIMER_KEY;
                DisplaySaturation += 1;
            }
            break;

        case MENU_TYPE_TEXT_LANGUAGE:
            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)) {
                CycleTextLanguage(-1);
                flag = 1;
                InitWaitNoInput(I_LEFT);
                InitWaitNoKey();
            }

            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)) {
                CycleTextLanguage(1);
                flag = 1;
                InitWaitNoInput(I_RIGHT);
                InitWaitNoKey();
            }
            break;

#ifdef CDROM
        case MENU_TYPE_VOICE_LANGUAGE:
            if ((Input & I_LEFT OR MyKey == K_GRAY_LEFT)) {
                CycleVoiceLanguage(-1);
                flag = 1;
                InitWaitNoInput(I_LEFT);
                InitWaitNoKey();
            }

            if ((Input & I_RIGHT OR MyKey == K_GRAY_RIGHT)) {
                CycleVoiceLanguage(1);
                flag = 1;
                InitWaitNoInput(I_RIGHT);
                InitWaitNoKey();
            }
            break;
#endif
        }

        if (FlagMenuMouse && MenuMouse_GetRefCount() > 0) {
            S32 wy = ConsumeMouseWheelY();

            if (wy != 0)
                GameMenuApplyMouseWheelDoGameMenu(ptrmenu, selected, wy, &timerkey, &flag);
        }

        if (flag == 1) {
            InitShakeChoice(); //     NEW
            TimerSample = 0;
            ptrmenu[0] = selected;
            DrawGameMenu(ptrmenu, FALSE);

            if (ptrmenu == GameMainMenu) {
                U8 memonumversion = NumVersion;
                char memoplayername[256];
                char memogamepathname[256];
                U8 *ptr;

                strcpy(memoplayername, PlayerName);
                strcpy(memogamepathname, GamePathname);

                GetSavePath(GamePathname, ADELINE_MAX_PATH, CURRENTSAVE_FILENAME);

                ptr = LoadGameScreen();
                if (ptr)
                    // Main-menu current-save preview box. Float it with the menu
                    // list (both use UI_VCENTER_OFS) so the box-to-menu gap stays
                    // as authored on a taller framebuffer. No-op at 480.
                    DrawScreenSave(ptr, SAVE_THUMB_X, 10 + UI_VCENTER_OFS);

                strcpy(GamePathname, memogamepathname);
                strcpy(PlayerName, memoplayername);
                NumVersion = memonumversion;
                NewCube = -1;
            }

            if (FadeMenu) {
                FadeToPal(PtrPalNormal);
                FadeMenu = FALSE;
            }

            do {
                DrawGameMenu(ptrmenu, TRUE);
                MyGetInput();
            } while (MyKey OR Input);

            flag = 0;
        } else {
            DrawGameMenu(ptrmenu, TRUE);
            flag = 0;
        }

        if (MenuIncrustText) {
            if (TimerRefHR > MenuIncrustTimer) {
                CopyBlock(0, 0, 150, 19, Screen, 0, 0, Log);
                BoxStaticAdd(0, 0, 150, 19);
                if (FlagShadeMenu)
                    ShadeBoxBlk(0, 0, 150, 19, SCREEN_SHADE_LVL);
                MenuIncrustText = NULL;
                MenuIncrustSprite = -1;
            } else {
                S32 x;

                if ((TimerRefHR & 0xFF) >= 85) // aff 2/3 eff 1/3
                {
                    if (MenuIncrustSprite != -1) {
                        if (MenuIncrustSprite >= 100) {
                            SpriteX = 10 + PtrZvExtra[MenuIncrustSprite * 8 + 0];
                            SpriteY = 10 + PtrZvExtra[MenuIncrustSprite * 8 + 1];
                        } else {
                            SpriteX = 10 + PtrZvExtraRaw[MenuIncrustSprite * 8 + 0];
                            SpriteY = 10 + PtrZvExtraRaw[MenuIncrustSprite * 8 + 1];
                        }

                        ScaleFactorSprite = DEF_SCALE_FACTOR;
                        PtrAffGraph(SpriteX, SpriteY, MenuIncrustSprite);
                        BoxStaticAdd(ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax);
                        x = ScreenXMax + 5;
                    } else
                        x = 5;

                    CoulText(LBAWHITE, -1);
                    GraphPrintf(FALSE, x, 0, MenuIncrustText);
                    BoxStaticAdd(TextXMin, TextYMin, TextXMax, TextYMax);
                } else {
                    CopyBlock(0, 0, 150, 19, Screen, 0, 0, Log);
                    BoxStaticAdd(0, 0, 150, 19);
                    if (FlagShadeMenu)
                        ShadeBoxBlk(0, 0, 150, 19, SCREEN_SHADE_LVL);
                }
            }
        }

        /* Quest Touch: A confirms and B goes back.  Keep the desktop
           gamepad/keyboard meaning (B/Enter confirm, Start/ESC back). */
        S32 menuConfirm = Input & I_ACTION_M;
        S32 menuBack = Input & I_MENUS;
        if (OpenXR_IsActive()) {
            menuConfirm |= Input & I_ACTION_ALWAYS;
            menuBack |= Input & I_RETURN;
            /* Physical B is the camera-mode key on Quest; in a menu it is
               the back key.  Right-stick click still arrives as I_RETURN. */
            if (CheckKey(K_GAMEPAD_RSTICK_BTN))
                menuBack = 1;
        } else {
            menuConfirm |= Input & I_RETURN;
        }

        if (menuConfirm OR MyKey == K_ENTER OR MyKey == K_NUMPAD_ENTER) {
            WaitNoInput();
            AsciiMode = FALSE;
            return ptrmenu[MENU_HEADER_WORDS + 1 + selected * 2]; // num mess
        }

        if (MyKey == K_ESC OR menuBack) {
            AsciiMode = FALSE;
            return MENU_RET_ESC;
        }

        /* Harness one-shot capture: count down per iteration; on reaching
           zero SavePNG the rendered menu and return the ESC sentinel so
           the wrapping menu loop exits cleanly.  Read Log (back buffer),
           not Screen -- DrawGameMenu composites the menu into Log, and the
           BoxUpdate inside only flushes dirty regions to Screen; without a
           prior frame populating Screen, the unflushed regions stay black.
           The third surface to need this (after holomap and dialog). */
        if (s_menu_capture_path[0] && --s_menu_capture_countdown <= 0) {
            SavePNG(s_menu_capture_path, Log, (S32)ModeDesiredX,
                    (S32)ModeDesiredY, PtrPal);
            s_menu_capture_path[0] = '\0';
            AsciiMode = FALSE;
            return MENU_RET_ESC;
        }

#ifdef DEBUG_TOOLS
        if (ptrmenu == GameMainMenu) {
            // chargement d'un bug
            if (MyKey == K_L) {
                AsciiMode = FALSE;
                return 2000;
            }

            // sauvegarde d'un bug
            if (MyKey == K_G) {
                AsciiMode = FALSE;
                return 2001;
            }
        }
#endif

        if (!DemoSlide AND MyKey)
            GereCheatCode();
    }
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
