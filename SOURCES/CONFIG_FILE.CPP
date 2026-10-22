#include "C_EXTERN.H"

#include "CONFIG_FILE.H"
#include "SETTINGS.H"
#include "CONSOLE/CONSOLE.H" /* Console_SetToggleScancode */
#include "CONTROL.H"         /* Control_Has* / Control_Get*: the per-run overrides */
#include "DIRECTORIES.H"     /* the cfg's path */
#include "DISTRIB.H"         /* Distrib_* : the release the data says it is */
#include "FOLLOWCAM.H"       /* the Auto camera's settings */
#include "FOLLOWCAM_CFG.H"   /* and their defaults */
#include "GAMEMENU.H"        /* SetDetailLevel */
#include "INPUT.H"           /* Read/WriteInputConfig, Read/WriteGamepadConfig */
#include "JOYSTICK.H"        /* JoystickDeadzone */
#include "MOUSE_CAPTURE.H"   /* MouseConfine */
#include "RES_SWITCH.H"
#include "RETAILFIX.H"         /* the retail fixes' settings */
#include <PLATFORM/PLATFORM.H> /* PLATFORM_FILTER_*, and why a mode was refused */
#include <POLYGON/POLYFILT.H>  /* PolyTexFilter / EnvTexFilter */
#include <SVGA/SCREEN.H>
#include <SYSTEM/KEYBOARD_KEYS.H> /* K_F12, the console toggle default */
#include <SYSTEM/LOG.H>
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>

extern S32 GfxDitherShading; /* defined in POLYFILT's caller; no header declares it */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AMBIANCE.H"

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
/* How the frame is filtered when stretched to the window: one of
   PLATFORM_FILTER_*. Only this file reads it; the backend holds the mode in use. */
static S32 DisplayPresentFilter = PLATFORM_FILTER_NEAREST;
static S32 StoredPresentFilter = PLATFORM_FILTER_NEAREST;
static S32 EnvPresentFilter = -1;
static S32 StoredFixedTimestep = 16;
static S32 StoredLanguage = 0;
static S32 StoredVSync = TRUE;

/* Where the live value came from, for the four rows a per-run override can force.
   Only those need it: the rule a source decides is what a forced run leaves
   behind, and a row no flag touches keeps its value either way. */
static T_SETTING_SOURCE SourceTexFilter = SETTING_FROM_DEFAULT;
static T_SETTING_SOURCE SourcePresentFilter = SETTING_FROM_DEFAULT;
static T_SETTING_SOURCE SourceFixedTimestep = SETTING_FROM_DEFAULT;
static T_SETTING_SOURCE SourceVSync = SETTING_FROM_DEFAULT;

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

/* -1 when the run was not given the flag, which is what Settings_ValueToPersist wants. */
static S32 ForcedFixedTimestep(void) {
    return Control_HasFixedTimestep() ? (S32)Control_GetFixedTimestep() : -1;
}

static S32 ForcedLanguage(void) {
    return Control_HasLanguage() ? (S32)Control_GetLanguage() : -1;
}

static S32 ForcedVSync(void) {
    return Control_HasVSync() ? (S32)Control_GetVSync() : -1;
}

/* The texture filter is forced by an environment variable rather than a flag,
   so its "was it forced" answer is a variable the read path already sets. */
static S32 ForcedTexFilter(void) {
    return EnvTexFilter;
}

static S32 ForcedPresentFilter(void) {
    return EnvPresentFilter;
}

/* The setting keeps what was asked for, so a preference survives a build that
   cannot do it; that build presents on nearest and the log says why. */
static void ApplyPresentFilter(S32 mode) {
    if (Window_SetPresentFilter(mode))
        return;
    Log_Warn("Present filter %d is not available: %s. Using nearest.", (int)mode, g_platform.system.get_error());
    Window_SetPresentFilter(PLATFORM_FILTER_NEAREST);
}

/* MainLoop derives Shadow, RainEnable and the sea and horizon from DetailLevel on
   entry, so a value set after that, by a replay borrowing the recording's, only
   counts once they are derived again. RainEnable matters to a replay: rain draws
   from the shared random stream. */
static void ApplyDetailLevel(S32 level) {
    (void)level;
    SetDetailLevel();
}

/* LBA2_PRESENT_FILTER takes the names, or the numbers the cfg and console use. */
static S32 ParsePresentFilter(const char *text) {
    static const char *const names[] = {"nearest", "linear", "pixelart"};
    S32 i;

    for (i = PLATFORM_FILTER_NEAREST; i <= PLATFORM_FILTER_PIXELART; i++) {
        const bool isNumber = (text[0] == '0' + i AND text[1] == '\0');
        if (isNumber OR !strcasecmp(text, names[i]))
            return i;
    }
    return -1;
}

/* The boot and display settings, declared once. Row order is the cfg's key
 * order, which is user-visible: lba2.cfg is rewritten in this order.
 *
 * Three of these can be forced for one run by a flag, and must not be left
 * behind for the next: the stored column keeps what the cfg held and the writer
 * persists that instead. Language is not here because it is written as a name
 * rather than a number; its override is still handled by hand below.
 *
 * The display rows are left out of recordings: they change how a frame is
 * shown, not what the simulation does. VSync and FixedTimestep are left out
 * because the header already carries them as `mode.vsync` and
 * `mode.fixed_timestep`, and Shadow because SetDetailLevel overwrites it at
 * MainLoop entry, so the recorded DetailLevel decides it. */
static const T_SETTING BootSettings[] = {
    {"Shadow", NULL, NULL, &Shadow, SETTING_RAW, 3, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL, NULL, NULL, NULL,
     SETTING_NOT_RECORDED},
    {"AllCameras", NULL, NULL, &AllCameras, SETTING_RAW, TRUE, SETTING_MIN_NONE, SETTING_MAX_NONE, NULL, NULL, NULL},
    /* User-facing name: "Auto camera" (menu/docs); the legacy spelling is read
       so an existing cfg keeps working. The camera module owns its cvar. */
    {"FollowCamera", "AutoCameraCenter", NULL, &FollowCamera, SETTING_OR_DEFAULT, FALSE, 0, 1, NULL, NULL, NULL},
    {"DetailLevel", NULL, NULL, &DetailLevel, SETTING_CLAMP, MAX_DETAIL_LEVEL, 0, MAX_DETAIL_LEVEL, NULL, NULL,
     ApplyDetailLevel},
    {"FullScreen", NULL, NULL, &VideoFullScreen, SETTING_OR_DEFAULT, TRUE, 0, 1, NULL, NULL, NULL, NULL, NULL,
     SETTING_NOT_RECORDED},
    {"DisplayFullScreen", NULL, NULL, &DisplayFullScreen, SETTING_OR_DEFAULT, TRUE, 0, 1, NULL, NULL, NULL, NULL, NULL,
     SETTING_NOT_RECORDED},
    {"VSync", NULL, NULL, &DisplayVSync, SETTING_OR_DEFAULT, TRUE, 0, 1, &StoredVSync, ForcedVSync, NULL, &SourceVSync,
     NULL, SETTING_NOT_RECORDED},
    {"DitherShading", NULL, "gfx_dither", &GfxDitherShading, SETTING_OR_DEFAULT, FALSE, 0, 1, NULL, NULL, NULL, NULL,
     "Ordered dither on Gouraud shade rows (softens 16-step banding)", SETTING_NOT_RECORDED},
    {"TextureFilter", NULL, "gfx_texfilter", &PolyTexFilter, SETTING_OR_DEFAULT, POLY_TEX_FILTER_OFF,
     POLY_TEX_FILTER_OFF, POLY_TEX_FILTER_BILINEAR, &StoredTexFilter, ForcedTexFilter, NULL, &SourceTexFilter,
     "Texture filtering: 0 off, 1 horizontal, 2 bilinear", SETTING_NOT_RECORDED},
    {"PresentFilter", NULL, "gfx_presentfilter", &DisplayPresentFilter, SETTING_OR_DEFAULT, PLATFORM_FILTER_NEAREST,
     PLATFORM_FILTER_NEAREST, PLATFORM_FILTER_PIXELART, &StoredPresentFilter, ForcedPresentFilter, ApplyPresentFilter,
     &SourcePresentFilter, "Filter stretching the frame to the window: 0 nearest, 1 linear, 2 pixelart",
     SETTING_NOT_RECORDED},
    {"FixedTimestep", NULL, NULL, &FixedTimestep, SETTING_OR_DEFAULT, 16, 0, 100,
     &StoredFixedTimestep, ForcedFixedTimestep, NULL, &SourceFixedTimestep, NULL, SETTING_NOT_RECORDED},
};
#define BOOT_SETTING_COUNT ((S32)(sizeof(BootSettings) / sizeof(BootSettings[0])))

/* The mouse's camera and menu preferences. Read by the manual camera path in
 * EXTFUNC.CPP, which has no module of its own to own them yet.
 *
 * The console clamps to the same ranges the cfg does. Two of them are
 * load-bearing: cam_mouse_div divides the pointer's motion, and at 100 or
 * above cam_glide's tail never decays. */
static const T_SETTING MouseSettings[] = {
    {"MenuMouse", NULL, NULL, &FlagMenuMouse, SETTING_TRUTHY, TRUE, 0, 1, NULL, NULL, NULL, NULL},
    /* Not recorded: it changes where the cursor may go, not what the game does with it. */
    {"MouseConfine", NULL, "mouse_confine", &MouseConfine, SETTING_CLAMP, MOUSE_CONFINE_DEFAULT, MOUSE_CONFINE_NEVER,
     MOUSE_CONFINE_ALWAYS, NULL, NULL, NULL, NULL, "Keep the cursor in the window: 0 never, 1 fullscreen, 2 always",
     SETTING_NOT_RECORDED, SETTING_BEFORE(MOUSE_CONFINE_NEVER)},
    {"MouseCamera", NULL, "cam_mouse", &FlagMouseCamera, SETTING_TRUTHY, TRUE, 0, 1, NULL, NULL, NULL, NULL,
     "Mouse camera enable", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"MouseCameraDrag", NULL, "cam_mouse_drag", &FlagMouseCameraDrag, SETTING_TRUTHY, TRUE, 0, 1,
     NULL, NULL, NULL, NULL, "Mouse camera: 1=right-drag, 0=free orbit", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"MouseSensitivityX", NULL, "cam_mouse_x", &MouseCameraSensX, SETTING_CLAMP, MOUSE_CAM_SENS_DEFAULT, 1, 10,
     NULL, NULL, NULL, NULL, "Mouse orbit sensitivity (1-10)", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"MouseSensitivityY", NULL, "cam_mouse_y", &MouseCameraSensY, SETTING_CLAMP, MOUSE_CAM_SENS_DEFAULT, 1, 10,
     NULL, NULL, NULL, NULL, "Mouse elevation sensitivity (1-10)", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"MouseInvertY", NULL, "cam_mouse_invert", &MouseCameraInvertY, SETTING_TRUTHY, FALSE, 0, 1,
     NULL, NULL, NULL, NULL, "Invert mouse elevation", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"MouseCameraDivisor", NULL, "cam_mouse_div", &MouseCameraDivisor, SETTING_CLAMP, MOUSE_CAM_DIVISOR_DEFAULT, 1, 16,
     NULL, NULL, NULL, NULL, "Mouse pixel-to-angle divisor (1-16, lower = faster)", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"ManualCameraSmoothing", NULL, "cam_smooth", &ManualCameraSmoothing, SETTING_CLAMP, MOUSE_CAM_SMOOTHING_DEFAULT,
     1, 16, NULL, NULL, NULL, NULL, "Manual orbit lerp divisor (1 = snap, higher = laggier)", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"FollowCamOrbitGlide", NULL, "cam_glide", &FollowCamOrbitGlide, SETTING_CLAMP, FOLLOW_CAM_ORBIT_GLIDE_DEFAULT, 0,
     FOLLOW_CAM_ORBIT_GLIDE_MAX, NULL, NULL, NULL, NULL,
     "Orbit follow-through: % of orbit speed kept per frame after release (0 = stop dead)", SETTING_RECORDED, SETTING_BEFORE_NONE},
};
#define MOUSE_SETTING_COUNT ((S32)(sizeof(MouseSettings) / sizeof(MouseSettings[0])))

/* The right stick's camera preferences and the stick deadzone. The pad's
 * bindings stay with INPUT.CPP, which writes them straight after these. The
 * deadzone's ceiling is load-bearing too: JOYSTICK.CPP narrows it to S16. */
static const T_SETTING GamepadSettings[] = {
    {"GamepadDeadzone", NULL, "pad_deadzone", &JoystickDeadzone, SETTING_CLAMP, LBA2_GAMEPAD_DEADZONE_DEFAULT, 0, 32767,
     NULL, NULL, NULL, NULL, "Gamepad stick deadzone (SDL units 0-32767)"},
    {"GamepadCameraAnalog", NULL, "cam_stick_analog", &GamepadCameraAnalog, SETTING_TRUTHY, TRUE, 0, 1,
     NULL, NULL, NULL, NULL, "Analog right-stick camera in Auto cam", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"GamepadCameraSensX", NULL, "cam_stick_x", &GamepadCameraSensX, SETTING_CLAMP, GAMEPAD_CAM_SENS_DEFAULT, 1, 10,
     NULL, NULL, NULL, NULL, "Right-stick orbit sensitivity (1-10)", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"GamepadCameraSensY", NULL, "cam_stick_y", &GamepadCameraSensY, SETTING_CLAMP, GAMEPAD_CAM_SENS_DEFAULT, 1, 10,
     NULL, NULL, NULL, NULL, "Right-stick elevation sensitivity (1-10)", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"GamepadCameraInvertY", NULL, "cam_stick_invert", &GamepadCameraInvertY, SETTING_TRUTHY, FALSE, 0, 1,
     NULL, NULL, NULL, NULL, "Invert right-stick elevation", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"GamepadCamMaxBeta", NULL, "cam_stick_max_x", &GamepadCamMaxBeta, SETTING_CLAMP, GAMEPAD_CAM_BETA_MAX_DEFAULT,
     1, 200, NULL, NULL, NULL, NULL, "Right-stick orbit speed ceiling (units/frame at sens 10)", SETTING_RECORDED, SETTING_BEFORE_NONE},
    {"GamepadCamMaxAlpha", NULL, "cam_stick_max_y", &GamepadCamMaxAlpha, SETTING_CLAMP, GAMEPAD_CAM_ALPHA_MAX_DEFAULT,
     1, 100, NULL, NULL, NULL, NULL, "Right-stick elevation speed ceiling (units/frame at sens 10)", SETTING_RECORDED, SETTING_BEFORE_NONE},
};
#define GAMEPAD_SETTING_COUNT ((S32)(sizeof(GamepadSettings) / sizeof(GamepadSettings[0])))

/* Every settings table, for whatever has to walk all of them: the recording
 * header today. This file already reads and writes each one, so it is the place
 * that knows the whole set. Returns NULL past the last table. */
const T_SETTING *ConfigFile_SettingTable(S32 i, S32 *count) {
    switch (i) {
    case 0:
        *count = BOOT_SETTING_COUNT;
        return BootSettings;
    case 1:
        *count = MOUSE_SETTING_COUNT;
        return MouseSettings;
    case 2:
        *count = GAMEPAD_SETTING_COUNT;
        return GamepadSettings;
    case 3:
        return FollowCam_Settings(count);
    case 4:
        return Audio_Settings(count);
    case 5:
        return RetailFix_Settings(count);
    default:
        *count = 0;
        return NULL;
    }
}

void ConfigFile_RegisterCvars(void) {
    Settings_RegisterCvars(BootSettings, BOOT_SETTING_COUNT);
    Settings_RegisterCvars(MouseSettings, MOUSE_SETTING_COUNT);
    Settings_RegisterCvars(GamepadSettings, GAMEPAD_SETTING_COUNT);
}

const T_SETTING *ConfigFile_Setting(const char *key) {
    S32 i;

    for (i = 0; i < BOOT_SETTING_COUNT; i++) {
        if (BootSettings[i].key != NULL AND !strcmp(BootSettings[i].key, key))
            return &BootSettings[i];
    }
    return NULL;
}

void ConfigFile_MarkVSyncSetThisRun(void) {
    Settings_MarkSetThisRun(&SourceVSync);
}

void ConfigFile_MarkFixedTimestepSetThisRun(void) {
    Settings_MarkSetThisRun(&SourceFixedTimestep);
}

void ReadConfigFile(void) {
    const char *ptr;

    ReadInputConfig(); // Lit Config Clavier dans LBA2.CFG
    Settings_ReadConfig(GamepadSettings, GAMEPAD_SETTING_COUNT);
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

    Settings_ReadConfig(BootSettings, BOOT_SETTING_COUNT);

    /* The settings above are values; these are the side effects taking them. */
    SetWindowFullscreen(DisplayFullScreen != FALSE);

    /* An explicit request for this run beats the stored preference, the same
       precedence the resolution and the language use. */
    EnvTexFilter = -1;
    {
        const char *env = getenv("LBA2_TEXFILTER");
        if (env != NULL) {
            const S32 v = (S32)atoi(env);
            if (v >= POLY_TEX_FILTER_OFF && v <= POLY_TEX_FILTER_BILINEAR) {
                PolyTexFilter = v;
                EnvTexFilter = v;
            }
        }
    }

    EnvPresentFilter = -1;
    {
        const char *env = getenv("LBA2_PRESENT_FILTER");
        if (env != NULL) {
            const S32 mode = ParsePresentFilter(env);
            if (mode >= 0) {
                DisplayPresentFilter = mode;
                EnvPresentFilter = mode;
            } else {
                Log_Warn("LBA2_PRESENT_FILTER=%s is not a present filter; expected nearest, linear or pixelart.", env);
            }
        }
    }

    if (Control_HasVSync())
        DisplayVSync = (S32)Control_GetVSync();

    /* Renderer exists by now (SetWindowFullscreen above operates on the same
       window/renderer pair), so this applies the saved choice immediately.
       Unlike fullscreen there's no visible boot-flash to avoid, so no early
       Res_LoadBoot* pre-read is needed. */
    Window_SetVSync(DisplayVSync);
    ApplyPresentFilter(DisplayPresentFilter);

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

    Settings_ReadConfig(MouseSettings, MOUSE_SETTING_COUNT);
    FollowCam_ReadConfig();
    RetailFix_ReadConfig();
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

    WriteInputConfig(); // Sauve Config Clavier dans LBA2.CFG
    Settings_WriteConfig(GamepadSettings, GAMEPAD_SETTING_COUNT);
    WriteGamepadConfig();  // Sauve Config Gamepad dans LBA2.CFG
    WriteVolumeSettings(); // Sauve volumes dans LBA2.CFG

    DefFileBufferWriteString(
        "Language", GetLanguageName(Settings_ValueToPersist(Language, StoredLanguage, ForcedLanguage())));
#ifdef CDROM
    DefFileBufferWriteString("LanguageCD", GetLanguageName(LanguageCD));
#endif

    DefFileBufferWriteString("LastSave", PlayerName);

    Settings_WriteConfig(BootSettings, BOOT_SETTING_COUNT);
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

    Settings_WriteConfig(MouseSettings, MOUSE_SETTING_COUNT);
    FollowCam_WriteConfig();
    RetailFix_WriteConfig();

    if (batched) {
        DefFileBufferEndBatch();
    }

    /* Reads after this point see what the boot assembled, which matters because
       this is also called mid-run: the options menu and a resolution switch. */
    RestackDataConfig();
}
