#include "C_EXTERN.H"
#include "CONFIG.H"
#include "DIRECTORIES.H"
#include "GAMEMENU.H"
#include "GAMEMENU_CAPTURE.H"
#include "GAMEMENU_INTERNAL.H"
#include "IMAGE_LOAD.H"
#include "INPUT.H"
#include "MESSAGE.H"
#include "PERSO.H"
#include "RES_MENU.H"
#include "RES_SWITCH.H"

#ifdef USE_GPURENDERER
#include <GL/RENDER_GL.H>
#include <SYSTEM/OPENXR.H>
#endif

#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/WINDOW.H>

/* Display and resolution pages.
 *
 * The Display parent is deliberately shallow: Resolution stays on it as the
 * primary action, while each group that would overflow the 480-tall canvas
 * (Graphics, Image, Effects, VR, Window) is its own page. Builders write rows
 * by index and derive the count from the rows they wrote, so a conditional
 * page cannot leave DoGameMenu reading past its list. */

U16 DisplayMenu[MENU_HEADER_WORDS + 8 * 2];

// Child pages. Each is rebuilt before its DoGameMenu loop, so the count always
// matches the rows written; the arrays are sized for their full fixed lists.
#ifdef USE_GPURENDERER
static U16 DisplayGraphicsMenu[MENU_HEADER_WORDS + 8 * 2];
static U16 DisplayImageMenu[MENU_HEADER_WORDS + 8 * 2];
static U16 DisplayVrMenu[MENU_HEADER_WORDS + 8 * 2];
static U16 DisplayVrInteriorMenu[MENU_HEADER_WORDS + 4 * 2];
static U16 DisplayVrBodyMenu[MENU_HEADER_WORDS + 3 * 2];
#endif
static U16 DisplayWindowMenu[MENU_HEADER_WORDS + 3 * 2];

/* Rebuild the Display submenu rows at runtime. The static initializer above
 * only backs the buffer; the pair list is conditional (the VR page only with
 * an immersive OpenXR session, and the Window page drops Fullscreen where
 * windowed mode is unsupported). The count must match the pairs actually
 * written or DoGameMenu reads past the intended rows. */
/* Rebuild the Display submenu rows at runtime. The pair list is conditional
 * (the VR page only with an immersive OpenXR session, and the Window page
 * drops Fullscreen where windowed mode is unsupported). Writing by index and
 * deriving the count from the rows actually written keeps DoGameMenu from
 * reading past the intended rows. */
static void BuildDisplayMenuEntries(void) {
    S32 n = 0;

    DisplayMenu[0] = 0;
    DisplayMenu[2] = 260;
    DisplayMenu[3] = 0;
    MenuSetRow(DisplayMenu, n++, 0, MENU_ID_BACK);
    MenuSetRow(DisplayMenu, n++, 0, MENU_ID_RESOLUTION);
#ifdef USE_GPURENDERER
    MenuSetRow(DisplayMenu, n++, 0, MENU_ID_DISPLAY_GRAPHICS);
    MenuSetRow(DisplayMenu, n++, 0, MENU_ID_DISPLAY_IMAGE);
    if (OpenXR_IsActive())
        MenuSetRow(DisplayMenu, n++, 0, MENU_ID_DISPLAY_VR);
#endif
    MenuSetRow(DisplayMenu, n++, 0, MENU_ID_DISPLAY_WINDOW);
    MenuSetRow(DisplayMenu, n++, 0, MENU_ID_DISPLAY_FPS);
    MenuSetRow(DisplayMenu, n++, 0, MENU_ID_FPS_LIMIT);
    MenuSetCount(DisplayMenu, n);
}

#ifdef USE_GPURENDERER
static void BuildDisplayGraphicsMenu(void) {
    S32 n = 0;

    DisplayGraphicsMenu[0] = 0;
    DisplayGraphicsMenu[2] = 260;
    DisplayGraphicsMenu[3] = 0;
    MenuSetRow(DisplayGraphicsMenu, n++, 0, MENU_ID_BACK);
#ifndef LBA2_OPENXR
    /* Immersive Quest builds render through OpenXR's GL context and force the
       GL renderer on, so offering a switch to the software renderer would
       only produce a frame the headset cannot present. */
    MenuSetRow(DisplayGraphicsMenu, n++, 0, MENU_ID_DISPLAY_RENDERER);
#endif
    MenuSetRow(DisplayGraphicsMenu, n++, 0, MENU_ID_DISPLAY_TEXFILTER);
    MenuSetRow(DisplayGraphicsMenu, n++, 0, MENU_ID_DISPLAY_ANTIALIAS);
#ifndef __ANDROID__
    /* SSAA multiplies the framebuffer per axis. The Android build clamps it
       to Native in PERSO/GL_SetRenderQuality because 2x/4x at headset
       resolution exceeds the mobile fill-rate and memory budget, so the row
       would only offer values that cannot take effect. */
    MenuSetRow(DisplayGraphicsMenu, n++, 0, MENU_ID_DISPLAY_QUALITY);
#endif
    MenuSetRow(DisplayGraphicsMenu, n++, 7, MENU_ID_DETAIL_LEVEL);
    MenuSetRow(DisplayGraphicsMenu, n++, 0, MENU_ID_DISPLAY_SOFTSHADOWS);
    MenuSetCount(DisplayGraphicsMenu, n);
}

static void BuildDisplayImageMenu(void) {
    S32 n = 0;

    DisplayImageMenu[0] = 0;
    DisplayImageMenu[2] = 260;
    DisplayImageMenu[3] = 0;
    MenuSetRow(DisplayImageMenu, n++, 0, MENU_ID_BACK);
    MenuSetRow(DisplayImageMenu, n++, MENU_TYPE_IMAGE_BRIGHTNESS, MENU_ID_DISPLAY_BRIGHTNESS);
    MenuSetRow(DisplayImageMenu, n++, MENU_TYPE_IMAGE_CONTRAST, MENU_ID_DISPLAY_CONTRAST);
    MenuSetRow(DisplayImageMenu, n++, MENU_TYPE_IMAGE_SATURATION, MENU_ID_DISPLAY_SATURATION);
    MenuSetRow(DisplayImageMenu, n++, 0, MENU_ID_DISPLAY_COLORGRADE);
    MenuSetRow(DisplayImageMenu, n++, 0, MENU_ID_DISPLAY_DITHER);
    MenuSetRow(DisplayImageMenu, n++, 0, MENU_ID_DISPLAY_SHARPEN);
    MenuSetCount(DisplayImageMenu, n);
}

static void BuildDisplayVrMenu(void) {
    S32 n = 0;

    DisplayVrMenu[0] = 0;
    DisplayVrMenu[2] = 260;
    DisplayVrMenu[3] = 0;
    MenuSetRow(DisplayVrMenu, n++, 0, MENU_ID_BACK);
    MenuSetRow(DisplayVrMenu, n++, 0, MENU_ID_DISPLAY_STEREO3D);
    MenuSetRow(DisplayVrMenu, n++, 0, MENU_ID_DISPLAY_VRZOOM);
    MenuSetRow(DisplayVrMenu, n++, 0, MENU_ID_DISPLAY_VRSUPERSAMPLE);
    MenuSetRow(DisplayVrMenu, n++, 0, MENU_ID_DISPLAY_VRINTERIOR);
    MenuSetRow(DisplayVrMenu, n++, 0, MENU_ID_DISPLAY_VRBODY);
    MenuSetCount(DisplayVrMenu, n);
}

static void BuildDisplayVrInteriorMenu(void) {
    S32 n = 0;

    DisplayVrInteriorMenu[0] = 0;
    DisplayVrInteriorMenu[2] = 260;
    DisplayVrInteriorMenu[3] = 0;
    MenuSetRow(DisplayVrInteriorMenu, n++, 0, MENU_ID_BACK);
    MenuSetRow(DisplayVrInteriorMenu, n++, 0, MENU_ID_DISPLAY_VRHOVER);
    MenuSetRow(DisplayVrInteriorMenu, n++, 0, MENU_ID_DISPLAY_SUBPIXEL);
    if (OpenXR_PassthroughSupported())
        MenuSetRow(DisplayVrInteriorMenu, n++, 0, MENU_ID_DISPLAY_VRPASSTHROUGH);
    MenuSetCount(DisplayVrInteriorMenu, n);
}

static void BuildDisplayVrBodyMenu(void) {
    S32 n = 0;

    DisplayVrBodyMenu[0] = 0;
    DisplayVrBodyMenu[2] = 260;
    DisplayVrBodyMenu[3] = 0;
    MenuSetRow(DisplayVrBodyMenu, n++, 0, MENU_ID_BACK);
    MenuSetRow(DisplayVrBodyMenu, n++, 0, MENU_ID_DISPLAY_VRFULLBODY);
    MenuSetRow(DisplayVrBodyMenu, n++, 0, MENU_ID_DISPLAY_VRWORLDSCALE);
    MenuSetCount(DisplayVrBodyMenu, n);
}
#endif

static void BuildDisplayWindowMenu(void) {
    S32 n = 0;

    DisplayWindowMenu[0] = 0;
    DisplayWindowMenu[2] = 260;
    DisplayWindowMenu[3] = 0;
    MenuSetRow(DisplayWindowMenu, n++, 0, MENU_ID_BACK);
    MenuSetRow(DisplayWindowMenu, n++, 0, MENU_ID_DISPLAY_VSYNC);
    if (Window_SupportsWindowedMode())
        MenuSetRow(DisplayWindowMenu, n++, 0, MENU_ID_DISPLAY_FULLSCREEN);
    MenuSetCount(DisplayWindowMenu, n);
}

// Resolution sub-page, opened from the Display submenu's Resolution row.
// Built at runtime from the shared catalog (like GameAskChoice builds
// GameChoiceMenu): a "Back" row plus one plain-button row per resolution.
// s_resMenuEntries / s_resMenuCount back the range-id rows; the U16 buffer
// holds 4 header words + (Back + up to DISPLAY_MENU_MAX_ENTRIES) (type, id)
// pairs. See GereResolutionMenu + BuildCustomMenuText.
DisplayEntry s_resMenuEntries[DISPLAY_MENU_MAX_ENTRIES];
S32 s_resMenuCount;
static U16 ResolutionMenu[MENU_HEADER_WORDS + (1 + DISPLAY_MENU_MAX_ENTRIES) * 2];

// Menu row index DrawGameMenu paints with the green "active value" plasma
// marker (distinct from the blue cursor). -1 = no marker.
//
// A file-static rather than a per-row flag in the menu array: the U16 (type,id)
// menu format has no spare per-row slot, and growing it for a single marker
// isn't worth touching every menu. The contract instead: only the resolution
// sub-page sets this (BuildResolutionMenu), and it is cleared back to -1 on
// every exit path (GereResolutionMenu, Resolution_MainCapture), paired with
// s_resMenuCount = 0, so no other menu can render a marker. See DrawOneChoice's
// select bitmask.
S32 s_menuMarkRow = -1;
// Apply a resolution chosen from the sub-page. Switches live via Res_Switch,
// persists to lba2.cfg, and repaints the menu backdrop. No keep/revert dialog:
// the catalog only offers modes that fit the current display, "resolution" here
// is the internal render-framebuffer size (always scaled to the existing
// window, never an OS display-mode change that could black out the screen), and
// the open list is itself the escape hatch: the player can immediately pick
// another mode or 640x480. The revert dialog also needs a live scene to repaint
// over, which the boot main menu doesn't have. See GereResolutionMenu.
static void ApplyResolutionChoice(U32 w, U32 h) {
    if (w == ModeDesiredX && h == ModeDesiredY)
        return;

    if (Res_Switch(w, h)) {
        // Persist immediately so the next launch boots at the chosen
        // resolution. Mirrors the Phase 0 keep/revert "Keep" path.
        WriteConfigFile();

        // Res_Switch tears down Log/Screen and the cube reload inside
        // it may have clobbered SkySeaTexture (where the plasma fire
        // bar lives), so the next DoGameMenu redraw would show the
        // row on bare black with a stale plasma highlight.
        //
        // Two repaint shapes depending on context:
        //   FlagShadeMenu TRUE  → in-game (player opened Options via
        //     ESC). Repaint the live 3D scene the same way the
        //     keep/revert dialog does (RES_SWITCH.CPP:472-482), then
        //     shade so the menu items pop over the dimmed scene.
        //   FlagShadeMenu FALSE → boot main menu (player is at the
        //     Resume / New / Load screen). The right background is
        //     the PCR_MENU stretched PCX image, not the default cube
        //     AffScene would paint (which would be Twinsen's house —
        //     wrong context for the boot menu). Reload the BG using
        //     the same Image_LoadStretched MainGameMenu uses, no
        //     shade — the boot menu doesn't darken its own backdrop.
        BoxReset();
        BoxStaticAdd(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1);
        if (FlagShadeMenu) {
            AffScene(AFF_ALL_FLIP);
        } else {
            char tmpFilePathScreen[ADELINE_MAX_PATH];
            GetResPath(tmpFilePathScreen, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
            Image_LoadStretched(tmpFilePathScreen, Screen, PCR_MENU);
            CopyScreen(Screen, Log);
        }
        BackupScreen(TRUE);
        InitPlasmaMenu();
        if (FlagShadeMenu) {
            ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1,
                        (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
        }
        BoxStaticFullflip();
    }
}

static void BuildResolutionMenu(void); /* defined below, near GereResolutionMenu */

// Harness one-shot capture of the Display submenu. Mirrors
// Menu_MainCapture's pattern: do the minimal setup the regular
// OptionsMenu → GereDisplayMenu path does (plasma init, palette, dial
// bank), arm the shared Menu_RequestCapture countdown, drive DoGameMenu
// directly on DisplayMenu so the capture site inside DoGameMenu fires
// and returns 1000. Background pixels come from whatever the caller
// (cmd_ui) put in Log/Screen — for `ui --black-bg display` that's a
// solid clear; otherwise the live scene.
void Display_MainCapture(const char *path) {
    HQ_StopSample();
    InitPlasmaMenu();

    BuildDisplayMenuEntries();

    CopyScreen(Screen, Log);
    BoxStaticFullflip();
    PtrPal = PtrPalNormal;
    PaletteSync(PtrPalNormal);
    InitDial(0);

    Menu_RequestCapture(path);
    DoGameMenu(DisplayMenu);
    Menu_RequestCapture(NULL); /* disarm if DoGameMenu somehow exited otherwise */
}

// Harness one-shot capture of the resolution sub-page. Same shape as
// Display_MainCapture but builds the runtime ResolutionMenu first (the page is
// generated, not a static array), then drives DoGameMenu(ResolutionMenu) with
// the capture armed. BuildResolutionMenu also sets s_resMenuCount so the
// range-id rows resolve in BuildCustomMenuText during the capture draw.
void Resolution_MainCapture(const char *path) {
    HQ_StopSample();
    InitPlasmaMenu();

    CopyScreen(Screen, Log);
    BoxStaticFullflip();
    PtrPal = PtrPalNormal;
    PaletteSync(PtrPalNormal);
    InitDial(0);

    BuildResolutionMenu();
    /* Park the cursor on Back so the green "current" marker on the active
       resolution row is visible in the capture (the live menu opens with the
       cursor on the current row, where the blue cursor bar takes precedence). */
    ResolutionMenu[0] = 0;

    Menu_RequestCapture(path);
    DoGameMenu(ResolutionMenu);
    Menu_RequestCapture(NULL); /* disarm if DoGameMenu somehow exited otherwise */
    /* Clear the sub-page-local statics so neither the green marker nor the
       BuildCustomMenuText range guard can affect any other menu. */
    s_menuMarkRow = -1;
    s_resMenuCount = 0;
}

// Rebuild the runtime ResolutionMenu array from the shared catalog: a "Back"
// row plus one plain-button row per resolution (id = MENU_ID_RESOLUTION_BASE +
// index), cursor landing on the current resolution if present. Refreshes
// s_resMenuEntries / s_resMenuCount, which back the range-id rows in
// BuildCustomMenuText. Shared by GereResolutionMenu and Resolution_MainCapture.
static void BuildResolutionMenu(void) {
    s_resMenuCount = Display_BuildEntries(s_resMenuEntries);

    // Header: selected (set below), entry count (Back + rows), y center, dia.
    MenuSetCount(ResolutionMenu, 1 + s_resMenuCount);
    ResolutionMenu[2] = 260;
    ResolutionMenu[3] = 0;
    // Back row, then one plain-button row per catalog entry (id = base + i).
    MenuSetRow(ResolutionMenu, 0, 0, MENU_ID_BACK);
    for (S32 i = 0; i < s_resMenuCount; i++)
        MenuSetRow(ResolutionMenu, 1 + i, 0, (U16)(MENU_ID_RESOLUTION_BASE + i));
    // Land the cursor on the current resolution if it's in the list, else Back;
    // mark that same row green so it stays flagged as active once the cursor
    // moves off it (cursor blue wins while they coincide). -1 = none.
    S32 cur = Display_FindCurrentEntry(s_resMenuEntries, s_resMenuCount);
    ResolutionMenu[0] = (cur >= 0) ? (U16)(1 + cur) : 0;
    s_menuMarkRow = (cur >= 0) ? (1 + cur) : -1;
}

// Resolution sub-page, opened from the Display submenu's Resolution row.
// Builds a select-and-confirm list at runtime from the shared catalog (the
// GameAskChoice pattern): a "Back" row plus one row per resolution, HD
// included. The recommended tier fits the on-screen budget without scrolling.
// Picking a row applies it immediately (ApplyResolutionChoice). Mirrors
// GereDisplayMenu's backdrop setup so the list draws over the shaded scene
// (in-game) or the boot-menu image.
static S32 GereResolutionMenu(void) {
    S32 select;
    S32 flag = 0;

    BuildResolutionMenu();

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    }

    while (!flag) {
        select = DoGameMenu(ResolutionMenu);

        if (select == 26 || select == 1000) {
            flag = 1;
        } else if (select >= MENU_ID_RESOLUTION_BASE &&
                   select < MENU_ID_RESOLUTION_BASE + s_resMenuCount) {
            const DisplayEntry *e = &s_resMenuEntries[select - MENU_ID_RESOLUTION_BASE];
            ApplyResolutionChoice(e->w, e->h);
            // Keep the page open so the player can pick another mode (or back
            // out). The display-derived list is stable across the switch; move
            // the green marker to whatever row is current now.
            S32 c = Display_FindCurrentEntry(s_resMenuEntries, s_resMenuCount);
            s_menuMarkRow = (c >= 0) ? (1 + c) : -1;
        }
    }

    // Clear the sub-page-local statics: the green marker row, and the count the
    // BuildCustomMenuText range guard keys on. With s_resMenuCount back to 0 the
    // guard (num >= BASE && num < BASE + count) can't match any other menu's id.
    s_menuMarkRow = -1;
    s_resMenuCount = 0;

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    WaitNoInput();

    return FALSE;
}

// Restore the menu backdrop after a child page has drawn over it. Screen holds
// the clean scene/menu image (DrawOneChoice only modifies Log), so copying it
// back and re-applying the shade leaves the parent page clean.
static void DisplayMenuResetBackdrop(void) {
    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1);

    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    }
}

// Apply a leaf display setting. The parent page and the Graphics/Image/VR/
// Window child pages share this; Back, Resolution, and the child-page entry
// rows are handled by their callers.
static void DisplayMenuApplyChoice(S32 select) {
    switch (select) {
    case MENU_ID_DISPLAY_FULLSCREEN:
        DisplayFullScreen ^= 1;
        SetWindowFullscreen(DisplayFullScreen != FALSE);
        break;

    case MENU_ID_DISPLAY_VSYNC:
        DisplayVSync ^= 1;
        Window_SetVSync(DisplayVSync);
        break;

#ifdef USE_GPURENDERER
    case MENU_ID_DISPLAY_RENDERER: {
        /* Tear down current renderer, recreate window for the other
           backend, initialize new renderer, rebuild the scene backdrop.
           Same pattern as ApplyResolutionChoice but dimensions stay
           the same — only the rendering backend changes. */
        const bool wasGL = GPURendererActive != FALSE;
        const bool wantGL = !wasGL;

        if (wantGL) {
            /* SW → GL: destroy SDL_Renderer window, create GL window */
            GL_Renderer_Shutdown();
            if (!Window_RecreateWindowForGL(ModeDesiredX, ModeDesiredY,
                                            DisplayFullScreen != FALSE)) {
                /* Window recreation failed — leave everything as-is. */
                break;
            }
            if (!GL_Renderer_Init(ModeDesiredX, ModeDesiredY)) {
                /* GL init failed — fall back to SW. */
                GL_Renderer_Shutdown();
                Window_RecreateWindowForSW(ModeDesiredX, ModeDesiredY,
                                           DisplayFullScreen != FALSE);
                break;
            }
            GPURendererActive = TRUE;
        } else {
            /* GL → SW: shut down GL, recreate SDL_Renderer window */
            GL_Renderer_Shutdown();
            if (!Window_RecreateWindowForSW(ModeDesiredX, ModeDesiredY,
                                            DisplayFullScreen != FALSE)) {
                /* Window recreation failed — we have no renderer. Try
                   to re-init GL as a last resort. */
                if (Window_RecreateWindowForGL(ModeDesiredX, ModeDesiredY,
                                               DisplayFullScreen != FALSE)) {
                    GL_Renderer_Init(ModeDesiredX, ModeDesiredY);
                }
                break;
            }
            GPURendererActive = FALSE;
        }

        /* Rebuild the scene backdrop the same way ApplyResolutionChoice
           does — the scene needs a full redraw after the window swap. */
        BoxReset();
        BoxStaticAdd(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1);
        if (FlagShadeMenu) {
            AffScene(AFF_ALL_FLIP);
        } else {
            char tmpFilePathScreen[ADELINE_MAX_PATH];
            GetResPath(tmpFilePathScreen, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
            Image_LoadStretched(tmpFilePathScreen, Screen, PCR_MENU);
            CopyScreen(Screen, Log);
        }
        BackupScreen(TRUE);
        InitPlasmaMenu();
        if (FlagShadeMenu) {
            ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1,
                        (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
        }
        BoxStaticFullflip();
        PresentCurrentFrame();
        WriteConfigFile();
        break;
    }

    case MENU_ID_DISPLAY_TEXFILTER:
        /* 0 = Nearest, 1 = Bilinear, 2 = Scale2x, 3 = xBR Textures,
         * 4 = xBR Lite — every edge-aware mode applies its rules per texture
         * inside the 3D sampler; the 2D overlay stays native. */
        DisplayTexFilter = (DisplayTexFilter + 1) % 5;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_DITHER:
        DisplayDither ^= 1;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_SOFTSHADOWS:
        DisplaySoftShadows ^= 1;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_COLORGRADE:
        DisplayColorGrade = (DisplayColorGrade + 1) % 3;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_SUBPIXEL:
        DisplaySubPixel ^= 1;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_SHARPEN:
        DisplaySharpen = (DisplaySharpen + 1) % 3;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_ANTIALIAS: {
        /* 0 = Off, 1 = FXAA, 2 = MSAA×2, 3 = MSAA×4 (desktop). */
        static const S32 aaModes[] = {
#ifdef __ANDROID__
            0, 1, 2
#else
            0, 1, 2, 3
#endif
        };
        const S32 aaCount = (S32)(sizeof(aaModes) / sizeof(aaModes[0]));
        S32 idx = 0;
        for (S32 i = 0; i < aaCount; i++) {
            if (DisplayAntiAlias == aaModes[i])
                idx = i;
        }
        DisplayAntiAlias = aaModes[(idx + 1) % aaCount];
        GL_SetAntiAlias(DisplayAntiAlias);
        WriteConfigFile();
        break;
    }

    case MENU_ID_DISPLAY_STEREO3D:
        DisplayStereo3D ^= 1;
        OpenXR_SetStereoEnabled(DisplayStereo3D);
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_VRZOOM:
        /* Percent of the eye's natural FOV the world spans; 120 is the
         * widest the authored terrain grid and sky backdrop cover. */
        VrFovScale += 10;
        if (VrFovScale > 120)
            VrFovScale = 50;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_VRSUPERSAMPLE: {
        /* Render the scene above the eye buffer and downsample on present.
           Off is the shipping default; 1.25x is the practical step, 1.5x the
           quality-at-any-cost step.  A target that cannot be allocated keeps
           the previous scale and is not persisted. */
        const S32 next = (VrSuperSample + 1) % 3;
        if (GL_SetSuperSample(next)) {
            VrSuperSample = next;
            WriteConfigFile();
        }
        break;
    }

    case MENU_ID_DISPLAY_VRHOVER:
        VrInteriorHover ^= 1;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_VRPASSTHROUGH:
        VrPassthrough ^= 1;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_VRFULLBODY:
        VrFullBody ^= 1;
        WriteConfigFile();
        break;

    case MENU_ID_DISPLAY_VRWORLDSCALE:
        VrWorldScale += 10;
        if (VrWorldScale > 200)
            VrWorldScale = 50;
        WriteConfigFile();
        break;

#ifndef __ANDROID__
    case MENU_ID_DISPLAY_QUALITY:
        if (RenderQuality < 4)
            RenderQuality *= 2;
        else
            RenderQuality = 1;
        GL_SetRenderQuality(RenderQuality);
        WriteConfigFile();
        break;
#endif
#endif

    case MENU_ID_DISPLAY_FPS:
        DebugFps ^= 1;
        break;

    case MENU_ID_FPS_LIMIT: {
        static const S32 kFpsLimits[] = {0, 30, 60, 72, 90, 120};
        const S32 count = (S32)(sizeof(kFpsLimits) / sizeof(kFpsLimits[0]));
        S32 i;
        for (i = 0; i < count; i++) {
            if (kFpsLimits[i] == FpsLimit) {
                break;
            }
        }
        if (i >= count) {
            i = 0; /* unknown cfg value: cycle starts from Off */
        }
        FpsLimit = kFpsLimits[(i + 1) % count];
        Window_SetFrameLimit(FpsLimit);
        break;
    }

    default:
        break;
    }
}

// Drive one of the Display child pages. The caller has already rebuilt the
// child's rows. Reset the backdrop before the first draw (erasing the parent
// page) and again after the last (erasing the child rows for the parent's
// redraw).
static void GereDisplaySubMenu(U16 *menu) {
    DisplayMenuResetBackdrop();

    for (;;) {
        S32 select = DoGameMenu(menu);

        if (select == MENU_ID_BACK || select == MENU_RET_ESC)
            break;

        DisplayMenuApplyChoice(select);
    }

    DisplayMenuResetBackdrop();
    WaitNoInput();
}

#ifdef USE_GPURENDERER
static void GereDisplayGraphicsMenu(void) {
    BuildDisplayGraphicsMenu();
    GereDisplaySubMenu(DisplayGraphicsMenu);
}

static void GereDisplayImageMenu(void) {
    BuildDisplayImageMenu();
    GereDisplaySubMenu(DisplayImageMenu);
}

static void GereDisplayVrInteriorMenu(void) {
    BuildDisplayVrInteriorMenu();
    GereDisplaySubMenu(DisplayVrInteriorMenu);
}

static void GereDisplayVrBodyMenu(void) {
    BuildDisplayVrBodyMenu();
    GereDisplaySubMenu(DisplayVrBodyMenu);
}

static void GereDisplayVrMenu(void) {
    BuildDisplayVrMenu();
    DisplayMenuResetBackdrop();
    for (;;) {
        S32 select = DoGameMenu(DisplayVrMenu);
        if (select == MENU_ID_BACK || select == MENU_RET_ESC)
            break;
        if (select == MENU_ID_DISPLAY_VRINTERIOR) {
            GereDisplayVrInteriorMenu();
            DisplayMenuResetBackdrop();
        } else if (select == MENU_ID_DISPLAY_VRBODY) {
            GereDisplayVrBodyMenu();
            DisplayMenuResetBackdrop();
        } else {
            DisplayMenuApplyChoice(select);
        }
    }
    DisplayMenuResetBackdrop();
    WaitNoInput();
}
#endif

static void GereDisplayWindowMenu(void) {
    BuildDisplayWindowMenu();
    GereDisplaySubMenu(DisplayWindowMenu);
}

// Display submenu: the small top-level page of display settings. Resolution
// opens its generated list; Graphics, Image, VR (OpenXR only), and Window open
// fixed child pages. The FPS rows are leaves on the parent. Keeping the groups
// separate stops the list outgrowing the 480px framebuffer.
S32 GereDisplayMenu(void) {
    S32 select;
    S32 flag = 0;
    /* Image-adjust slider values at menu entry: the sliders move inside
       DoGameMenu's type handling (keys and wheel), so the exit-time compare
       is what decides the cfg flush. */
    const S32 brightness0 = DisplayBrightness;
    const S32 contrast0 = DisplayContrast;
    const S32 saturation0 = DisplaySaturation;

    /* Rows are rebuilt by BuildDisplayMenuEntries so the count always
       matches the (type, id) pairs actually written: Back + Resolution
       + [Graphics + Image if GPURENDERER] + [VR if an immersive OpenXR
       session is active] + Window + FPS counter + FPS limit. */
    BuildDisplayMenuEntries();

    DisplayMenuResetBackdrop();

    while (!flag) {
        select = DoGameMenu(DisplayMenu);

        switch (select) {
        case MENU_ID_BACK:
        case MENU_RET_ESC:
            flag = 1;
            break;

        case MENU_ID_RESOLUTION:
            // Open the resolution list sub-page.
            GereResolutionMenu();
            DisplayMenuResetBackdrop();
            break;

#ifdef USE_GPURENDERER
        case MENU_ID_DISPLAY_GRAPHICS:
            GereDisplayGraphicsMenu();
            break;

        case MENU_ID_DISPLAY_IMAGE:
            GereDisplayImageMenu();
            break;

        case MENU_ID_DISPLAY_VR:
            GereDisplayVrMenu();
            break;
#endif

        case MENU_ID_DISPLAY_WINDOW:
            GereDisplayWindowMenu();
            break;

        default:
            DisplayMenuApplyChoice(select);
            break;
        }
    }

    /* Image sliders: one cfg write per menu visit instead of one per adjust
       tick (the sliders repeat while held, like the volume sliders). */
    if (DisplayBrightness != brightness0 || DisplayContrast != contrast0 ||
        DisplaySaturation != saturation0) {
        WriteConfigFile();
    }

    DisplayMenuResetBackdrop();

    WaitNoInput();

    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
