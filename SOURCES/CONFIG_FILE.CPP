#include "C_EXTERN.H"

#include "CONFIG_FILE.H"
#include "CONSOLE/CONSOLE.H" /* Console_SetToggleScancode */
#include "CONTROL.H"         /* Control_Has* / Control_Get*: the per-run overrides */
#include "DIRECTORIES.H"     /* the cfg's path */
#include "DISTRIB.H"         /* Distrib_* : the release the data says it is */
#include "FOLLOWCAM.H"       /* the Auto camera's settings */
#include "FOLLOWCAM_CFG.H"   /* and their defaults */
#include "INPUT.H"           /* Read/WriteInputConfig, Read/WriteGamepadConfig */
#include "RES_SWITCH.H"
#include <POLYGON/POLYFILT.H> /* PolyTexFilter / EnvTexFilter */
#include <SVGA/SCREEN.H>
#include <SYSTEM/KEYBOARD_KEYS.H> /* K_F12, the console toggle default */
#include <SYSTEM/LOG.H>
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>

extern S32 GfxDitherShading; /* defined in POLYFILT's caller; no header declares it */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*══════════════════════════════════════════════════════════════════════════*
                      █▀▀▀▀ █▀▀▀█ ██▄ █ █▀▀▀▀  █    █▀▀▀▀
                      ██    ██  █ ██▀██ ██▀▀   ██   ██ ▀█
                      ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀     ▀▀   ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/* What lba2.cfg held for a setting some override may be forcing for this run,
   and what the override forced (-1 when none). WriteConfigFile needs both to
   tell a per-run override apart from a genuine change.

   Every flag here says "this run only" in its own help text, and a run that
   said so must not leave the value behind for the next one: `--fixed-timestep 100`
   once would otherwise throttle every later launch, and `--language Deutsch` for
   one look at the German text would make German the setting. --resolution is
   deliberately not in this list; it is a player-facing preference that persists
   on purpose (see WriteConfigFile). */
static S32 StoredTexFilter = 0;
static S32 EnvTexFilter = -1;
static S32 StoredFixedTimestep = 16;
static S32 StoredLanguage = 0;
static S32 StoredVSync = TRUE;

/* The value to write for such a setting: the stored preference while the live
   value is still the one the override forced, otherwise the live value, because
   then something during the run changed it on purpose.

   The comparison is on the value, not on who last wrote it. Settings reached
   through cvars are registered as bare pointers with no on-change hook, so
   setting the same value from the console during an overridden run cannot be
   told from not touching it: that does not persist, while setting a different
   one does. Accepted trade for not growing the cvar API. */
void ConfigFile_NoteBootLanguage(S32 language) {
    StoredLanguage = language;
}

static S32 ValueToPersist(S32 live, S32 stored, S32 forced) {
    return (forced >= 0 && live == forced) ? stored : live;
}

/* -1 when the run was not given the flag, which is what ValueToPersist wants. */
static S32 ForcedFixedTimestep(void) {
    return Control_HasFixedTimestep() ? (S32)Control_GetFixedTimestep() : -1;
}

static S32 ForcedLanguage(void) {
    return Control_HasLanguage() ? (S32)Control_GetLanguage() : -1;
}

static S32 ForcedVSync(void) {
    return Control_HasVSync() ? (S32)Control_GetVSync() : -1;
}

void ReadConfigFile(void) {
    const char *ptr;

    ReadInputConfig();    // Lit Config Clavier dans LBA2.CFG
    ReadGamepadConfig();  // Lit Config Gamepad dans LBA2.CFG
    ReadVolumeSettings(); // Lit Volumes dans LBA2.CFG

    Version_US = DefFileBufferReadValue("Version_US");
    NumVersion = (U8)DefFileBufferReadValue("CompressSave");

    if (NumVersion)
        NumVersion = SAVE_COMPRESS;

    NumVersion |= NUM_VERSION;

    // LastSave
    ptr = DefFileBufferReadString("LastSave");
    if (ptr AND strcmp(ptr, ""))
        strcpy(PlayerName, ptr);

    Shadow = DefFileBufferReadValueDefault("Shadow", 3);
    AllCameras = DefFileBufferReadValueDefault("AllCameras", TRUE);
    /* User-facing name: "Auto camera" (menu/docs); config key FollowCamera; legacy AutoCameraCenter */
    FollowCamera = DefFileBufferReadValueDefault("FollowCamera",
                                                 DefFileBufferReadValueDefault("AutoCameraCenter", FALSE));

    if (FollowCamera < 0 OR FollowCamera > 1)
        FollowCamera = FALSE;

    ReverseStereo = DefFileBufferReadValueDefault("ReverseStereo", FALSE);

    InverseStereoSample(ReverseStereo);

    DetailLevel = DefFileBufferReadValueDefault("DetailLevel", MAX_DETAIL_LEVEL);

    if (DetailLevel < 0)
        DetailLevel = 0;
    if (DetailLevel > MAX_DETAIL_LEVEL)
        DetailLevel = MAX_DETAIL_LEVEL;

    VideoFullScreen = DefFileBufferReadValueDefault("FullScreen", TRUE);

    if (VideoFullScreen < 0 OR VideoFullScreen > 1)
        VideoFullScreen = TRUE;

    DisplayFullScreen = DefFileBufferReadValueDefault("DisplayFullScreen", FALSE);

    if (DisplayFullScreen < 0 OR DisplayFullScreen > 1)
        DisplayFullScreen = FALSE;

    SetWindowFullscreen(DisplayFullScreen != FALSE);

    DisplayVSync = DefFileBufferReadValueDefault("VSync", TRUE);

    GfxDitherShading = DefFileBufferReadValueDefault("DitherShading", FALSE);
    if (GfxDitherShading < 0 OR GfxDitherShading > 1)
        GfxDitherShading = FALSE;

    PolyTexFilter = DefFileBufferReadValueDefault("TextureFilter", POLY_TEX_FILTER_OFF);
    if (PolyTexFilter < POLY_TEX_FILTER_OFF || PolyTexFilter > POLY_TEX_FILTER_BILINEAR)
        PolyTexFilter = POLY_TEX_FILTER_OFF;
    StoredTexFilter = PolyTexFilter;
    EnvTexFilter = -1;
    {
        /* Same precedence the resolution uses: an explicit request for this
           run beats the stored preference. */
        const char *env = getenv("LBA2_TEXFILTER");
        if (env != NULL) {
            const S32 v = (S32)atoi(env);
            if (v >= POLY_TEX_FILTER_OFF && v <= POLY_TEX_FILTER_BILINEAR) {
                PolyTexFilter = v;
                EnvTexFilter = v;
            }
        }
    }

    if (DisplayVSync < 0 OR DisplayVSync > 1)
        DisplayVSync = TRUE;

    /* Same precedence the language uses: an explicit request for this run beats the stored
       preference, and the preference is kept so WriteConfigFile can tell the two apart. */
    StoredVSync = DisplayVSync;
    if (Control_HasVSync())
        DisplayVSync = (S32)Control_GetVSync();

    /* Renderer exists by now (SetWindowFullscreen above operates on the same
       window/renderer pair), so this applies the saved choice immediately.
       Unlike fullscreen there's no visible boot-flash to avoid, so no early
       Res_LoadBoot* pre-read is needed. */
    Window_SetVSync(DisplayVSync);

    /* #358 fixed-timestep throttle. Default 16 ms (~60 Hz sim) so movement is frame-rate
       independent above 60 fps; 0 restores the historical per-frame simulation. Clamp to a
       sane range (a value near a frame time; 0 = off). See docs/MOVEMENT_FRAMERATE.md. */
    FixedTimestep = DefFileBufferReadValueDefault("FixedTimestep", 16);
    if (FixedTimestep < 0 OR FixedTimestep > 100)
        FixedTimestep = 16;
    /* Before Control_TickHook applies any --fixed-timestep, so this is the
       player's own setting rather than the throttle a harness run forced. */
    StoredFixedTimestep = FixedTimestep;

    /* Console toggle scancode (SDL scancode integer); invalid values fall back to F12 in console module. */
    Console_SetToggleScancode(DefFileBufferReadValueDefault("ConsoleToggleKey", K_F12));

    ptr = DefFileBufferReadStringDefault("FlagDisplayText", "ON");

    if (ptr AND !strcasecmp(ptr, "OFF"))
        FlagDisplayText = 0;
    else
        FlagDisplayText = 1; // default

    // version distributeur
    /* Read so that absent and an explicit 0 stay distinguishable. They mean the
       same to every consumer, and different things to a reader: a store build
       ships no key at all, which is ordinary, while a written 0 is somebody
       choosing no publisher. */
    {
        S32 declaredVersion = UNKNOWN_VERSION;
        const bool declared = (DefFileBufferReadValue2("Version", &declaredVersion) != 0);
        Distrib_SetDeclared(declared);

        /* What the assets say, independently of what anyone wrote down. One
           FileSize, and no read: it goes through OpenRead, so a bank inside a
           mounted disc image is sized from its directory record exactly as a
           loose file is sized with lseek. */
        char ressPath[ADELINE_MAX_PATH];
        GetResPath(ressPath, ADELINE_MAX_PATH, RESS_HQR_NAME);
        Distrib_SetMaster(Distrib_MasterFromRessSize(FileSize(ressPath)));

        bool showSplash = false;
        Distrib_Resolve(declared, declaredVersion, Distrib_GetMaster(), &DistribVersion,
                        &showSplash);

        /* Last say to whoever wrote a config, in either direction: an install
           can suppress the splash its own declaration implies, and a player can
           ask for one their install never declared. Read only, never written,
           so the default keeps deriving from whether a release was declared
           rather than freezing one install's answer into a profile that may be
           pointed at another tomorrow. */
        S32 splashChoice;
        if (DefFileBufferReadValue2("ShowDistribLogo", &splashChoice)) {
            showSplash = (splashChoice != 0);
        }
        Distrib_SetShowSplash(showSplash);
    }

    FlagMenuMouse = DefFileBufferReadValueDefault("MenuMouse", TRUE);
    if (FlagMenuMouse != 0)
        FlagMenuMouse = TRUE;
    else
        FlagMenuMouse = FALSE;

    FlagMouseCamera = DefFileBufferReadValueDefault("MouseCamera", TRUE);
    if (FlagMouseCamera != 0)
        FlagMouseCamera = TRUE;
    else
        FlagMouseCamera = FALSE;

    FlagMouseCameraDrag = DefFileBufferReadValueDefault("MouseCameraDrag", TRUE);
    if (FlagMouseCameraDrag != 0)
        FlagMouseCameraDrag = TRUE;
    else
        FlagMouseCameraDrag = FALSE;

    MouseCameraSensX = DefFileBufferReadValueDefault("MouseSensitivityX", MOUSE_CAM_SENS_DEFAULT);
    if (MouseCameraSensX < 1)
        MouseCameraSensX = 1;
    if (MouseCameraSensX > 10)
        MouseCameraSensX = 10;

    MouseCameraSensY = DefFileBufferReadValueDefault("MouseSensitivityY", MOUSE_CAM_SENS_DEFAULT);
    if (MouseCameraSensY < 1)
        MouseCameraSensY = 1;
    if (MouseCameraSensY > 10)
        MouseCameraSensY = 10;

    MouseCameraInvertY = DefFileBufferReadValueDefault("MouseInvertY", FALSE);
    if (MouseCameraInvertY != 0)
        MouseCameraInvertY = TRUE;
    else
        MouseCameraInvertY = FALSE;

    MouseCameraDivisor = DefFileBufferReadValueDefault("MouseCameraDivisor", MOUSE_CAM_DIVISOR_DEFAULT);
    if (MouseCameraDivisor < 1)
        MouseCameraDivisor = 1;
    if (MouseCameraDivisor > 16)
        MouseCameraDivisor = 16;

    FollowCamOrbitGlide = DefFileBufferReadValueDefault("FollowCamOrbitGlide", FOLLOW_CAM_ORBIT_GLIDE_DEFAULT);
    if (FollowCamOrbitGlide < 0)
        FollowCamOrbitGlide = 0;
    if (FollowCamOrbitGlide > FOLLOW_CAM_ORBIT_GLIDE_MAX)
        FollowCamOrbitGlide = FOLLOW_CAM_ORBIT_GLIDE_MAX;

    ManualCameraSmoothing = DefFileBufferReadValueDefault("ManualCameraSmoothing", MOUSE_CAM_SMOOTHING_DEFAULT);
    if (ManualCameraSmoothing < 1)
        ManualCameraSmoothing = 1;
    if (ManualCameraSmoothing > 16)
        ManualCameraSmoothing = 16;

    FollowCam_ReadConfig();
}

/*──────────────────────────────────────────────────────────────────────────*/
/* The config a run reads from is the file it owns with the one beside the game
   data stacked underneath. Writes refuse while that layer is attached, because
   rewriting a layered buffer would fold the install's keys into the player's
   file, where they would outlive the install they came from.

   So a write is three steps, not one: detach by re-reading the owned file
   alone, write, then stack the game data's config back. Anything persisting a
   setting goes through OpenOwnedConfig / RestackDataConfig or through
   WriteConfigValue below; reaching for DefFileBufferWrite* directly gets a
   refusal, and callers that do not check for one report success while storing
   nothing. */
static void OpenOwnedConfig(void) {
    DefFileBufferInit(PathConfigFile, ScreenAux, (640 * 480 + RECOVER_AREA));
}

static void RestackDataConfig(void) {
    char dataCfg[ADELINE_MAX_PATH];
    GetDefaultCfgPath(dataCfg, ADELINE_MAX_PATH, CFG_NAME);
    DefFileBufferAppendFile(dataCfg);
}

S32 WriteConfigValue(const char *ident, S32 value) {
    OpenOwnedConfig();
    const S32 ok = DefFileBufferWriteValue(ident, value);
    RestackDataConfig();
    return ok;
}

/*──────────────────────────────────────────────────────────────────────────*/
void WriteConfigFile(void) {
    OpenOwnedConfig();

    // Batch the whole save into one fsync + atomic rename. Without this, each of
    // the ~30 writes below would fsync + rename on its own, which is noticeably
    // slow on a settings change. If a batch can't start, the writes fall back to
    // the per-call atomic path and still persist correctly.
    S32 batched = DefFileBufferBeginBatch();

    WriteInputConfig();    // Sauve Config Clavier dans LBA2.CFG
    WriteGamepadConfig();  // Sauve Config Gamepad dans LBA2.CFG
    WriteVolumeSettings(); // Sauve volumes dans LBA2.CFG

    DefFileBufferWriteString(
        "Language", GetLanguageName(ValueToPersist(Language, StoredLanguage, ForcedLanguage())));
#ifdef CDROM
    DefFileBufferWriteString("LanguageCD", GetLanguageName(LanguageCD));
#endif

    DefFileBufferWriteString("LastSave", PlayerName);

    DefFileBufferWriteValue("Shadow", Shadow);
    DefFileBufferWriteValue("AllCameras", AllCameras);
    DefFileBufferWriteValue("FollowCamera", FollowCamera);
    DefFileBufferWriteValue("ReverseStereo", ReverseStereo);
    DefFileBufferWriteValue("DetailLevel", DetailLevel);
    DefFileBufferWriteValue("FullScreen", VideoFullScreen);
    DefFileBufferWriteValue("DisplayFullScreen", DisplayFullScreen);
    DefFileBufferWriteValue("VSync", ValueToPersist(DisplayVSync, StoredVSync, ForcedVSync()));
    DefFileBufferWriteValue("DitherShading", GfxDitherShading);
    /* An environment override is for the run that asked for it: persisting the
       live value would turn `LBA2_TEXFILTER=2 ./lba2cc` into a permanent
       setting nobody chose. */
    DefFileBufferWriteValue("TextureFilter",
                            ValueToPersist(PolyTexFilter, StoredTexFilter, EnvTexFilter));
    DefFileBufferWriteValue(
        "FixedTimestep",
        ValueToPersist(FixedTimestep, StoredFixedTimestep, ForcedFixedTimestep()));
    DefFileBufferWriteValue("ConsoleToggleKey", Console_GetToggleScancode());
    /* Render resolution. Read at boot by Res_LoadBootDimensions (CLI > cfg >
       auto-detect > compile-time default), but only two of those four are a
       preference to keep: the cfg's own value, and a resolution the player
       picked during the run. The other two are left out, so an auto-detected
       size re-derives from the display each launch instead of freezing into the
       cfg, and a --resolution run leaves whatever the config already held. */
    if (Res_ResolutionShouldPersist()) {
        U32 baseW = 0, baseH = 0;
        Res_BaseDimensions(&baseW, &baseH);
        DefFileBufferWriteValue("ResolutionX", (S32)baseW);
        DefFileBufferWriteValue("ResolutionY", (S32)baseH);
    }

    if (FlagDisplayText)
        DefFileBufferWriteString("FlagDisplayText", "ON");
    else
        DefFileBufferWriteString("FlagDisplayText", "OFF");

    DefFileBufferWriteValue("MenuMouse", FlagMenuMouse);
    DefFileBufferWriteValue("MouseCamera", FlagMouseCamera);
    DefFileBufferWriteValue("MouseCameraDrag", FlagMouseCameraDrag);
    DefFileBufferWriteValue("MouseSensitivityX", MouseCameraSensX);
    DefFileBufferWriteValue("MouseSensitivityY", MouseCameraSensY);
    DefFileBufferWriteValue("MouseInvertY", MouseCameraInvertY);
    DefFileBufferWriteValue("MouseCameraDivisor", MouseCameraDivisor);
    DefFileBufferWriteValue("ManualCameraSmoothing", ManualCameraSmoothing);
    DefFileBufferWriteValue("FollowCamOrbitGlide", FollowCamOrbitGlide);
    FollowCam_WriteConfig();

    if (batched) {
        DefFileBufferEndBatch();
    }

    /* Reads after this point see what the boot assembled, which matters because
       this is also called mid-run: the options menu and a resolution switch. */
    RestackDataConfig();
}
