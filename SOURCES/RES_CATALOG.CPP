/*
 * Resolution catalog. See RES_CATALOG.H. Lifted out of CONSOLE_CMD.CPP when
 * the Display submenu became a second consumer (RES_SWITCH.CPP:39 foretold it).
 */

#include "RES_CATALOG.H"

#include <SYSTEM/WINDOW.H> /* Window_GetDisplayDimensions */
#include <SYSTEM/OPENXR.H> /* OpenXR_GetSwapchainWidth/Height (Quest display) */

/* True if (w, h) already appears in the first `n` entries of `list`. Defined
   below; forward-declared so the menu-list builder can dedup against itself. */
static bool catalog_contains(const ResModeEntry *list, int n, U32 w, U32 h);

/* Always-present recommended modes, ordered. Filtered to <= display at build
   time; 640x480 is pinned at index 0 unconditionally. */
static const ResModeEntry kRecommended[] = {
    {640, 480, "classic"},        /* 4:3, the authentic-mode anchor */
    {768, 480, "recommended"},    /* 16:10 */
    {1024, 480, "recommended"},   /* ~21:10 */
    {1280, 720, "experimental"},  /* 16:9; Y>480, holoplan zoom may glitch */
    {1920, 1080, "experimental"}, /* 16:9; same caveat */
};
#define kRecommendedCount ((int)(sizeof(kRecommended) / sizeof(kRecommended[0])))

/* Curated "advanced" list (console --all only). Reasonable common modes; a
   full multiples-of-8 enumeration would emit hundreds of entries. */
static const ResModeEntry kAdvanced[] = {
    {320, 200, "advanced"},
    {320, 240, "advanced"},
    {400, 300, "advanced"},
    {512, 384, "advanced"},
    {800, 600, "advanced"},
    {856, 480, "advanced"},
    {960, 540, "advanced"},
    {1024, 600, "advanced"},
    {1024, 768, "advanced"},
    {1152, 648, "advanced"},
    {1280, 800, "advanced"},
    {1280, 960, "advanced"},
    {1280, 1024, "advanced"},
    {1366, 768, "advanced"},
    {1440, 900, "advanced"},
    {1600, 900, "advanced"},
    {1600, 1024, "advanced"},
    {1680, 1024, "advanced"},
};
#define kAdvancedCount ((int)(sizeof(kAdvanced) / sizeof(kAdvanced[0])))

void Res_Aspect(U32 w, U32 h, U32 *outA, U32 *outB) {
    U32 a = w, b = h;
    while (b) {
        U32 t = b;
        b = a % b;
        a = t;
    }
    if (a == 0)
        a = 1;
    *outA = w / a;
    *outB = h / a;
}

U32 Res_SnapNativeW(U32 dispW) {
    if (dispW > RES_MAX_W)
        dispW = RES_MAX_W;
    dispW &= ~(U32)7;
    if (dispW < RES_MIN_W)
        return 0;
    return dispW;
}

U32 Res_SnapNativeH(U32 dispH) {
    if (dispH > RES_MAX_H)
        dispH = RES_MAX_H;
    if (dispH < RES_MIN_H)
        return 0;
    return dispH;
}

U32 Res_SnapAspectWidth(U32 dispW, U32 dispH, U32 targetH) {
    if (dispW == 0 || dispH == 0 || targetH == 0)
        return 0;
    /* W = round_down((targetH * dispW / dispH) / 8) * 8: the width that holds
       the display aspect at targetH render height. 64-bit intermediate so wide
       displays don't overflow the multiply. Returns 0 (caller drops the entry)
       when the result falls outside [RES_MIN_W, RES_MAX_W]; e.g. an ultrawide
       at 1080 height would need > 1920 width. */
    U32 w = (U32)(((U64)targetH * (U64)dispW) / (U64)dispH);
    w &= ~(U32)7;
    if (w < (U32)RES_MIN_W || w > (U32)RES_MAX_W)
        return 0;
    return w;
}

U32 Res_SnapWidescreenWidth(U32 dispW, U32 dispH) {
    return Res_SnapAspectWidth(dispW, dispH, 480);
}

/* Effective display for resolution candidates: the SDL window's display when
   available, widened/tallened to the OpenXR swapchain on head-mounted builds —
   the swapchain is what the player actually sees, and it can exceed both the
   Android activity surface and the desktop caps (Quest 3 mono: 1680x1760). */
static bool Res_EffectiveDisplay(U32 *outW, U32 *outH) {
    U32 w = 0, h = 0;
    if (Window_GetDisplayDimensions(&w, &h)) {
        *outW = w;
        *outH = h;
    } else {
        *outW = 0;
        *outH = 0;
    }
    U32 sw = OpenXR_GetSwapchainWidth();
    U32 sh = OpenXR_GetSwapchainHeight();
    if (sw > *outW)
        *outW = sw;
    if (sh > *outH)
        *outH = sh;
    return *outW != 0 && *outH != 0;
}

/* Head-mounted resolution ladder. The in-headset view is the OpenXR swapchain,
 * whose aspect is the eye FOV's (~square on Quest) — not an aspect the game's
 * content is authored for. Offering panel-aspect or panel-native sizes made
 * the world look stretched and let oversized entries crop; so on a headset the
 * menu offers ONLY the game's intended modern aspect (16:9), sized to fit the
 * swapchain width exactly: every entry renders fully visible, centred with
 * cinema bars, and switching between them never changes the framing. The
 * largest entry is the swapchain-width maximum (1680x945 on a Quest 3). */
/* Head-mount ladder: resolutions matching the per-eye swapchain dimensions
 * (1680×1760 on Quest 3).  The old 16:9 ladder caused a 1.86× vertical
 * upscale when blitted to the swapchain — every source pixel became ~1.86
 * destination pixels vertically, which the user perceives as blur.  These
 * entries are 1:1 with the swapchain at various quality steps. */
static const ResModeEntry kHeadMountLadder[] = {
    {416, 440, "0.25x"},
    {560, 587, "0.33x"},
    {840, 880, "0.5x"},
    {1120, 1173, "0.67x"},
    {1256, 1320, "0.75x"},
    {RES_QUEST_NATIVE_W, RES_QUEST_NATIVE_H, "native"},
};
#define kHeadMountLadderCount ((int)(sizeof(kHeadMountLadder) / sizeof(kHeadMountLadder[0])))

static bool Res_HeadMounted(void) {
    return OpenXR_GetSwapchainWidth() != 0;
}

int Res_BuildMenuList(ResModeEntry *out, int cap) {
    int n = 0;

    /* The 640x480 classic anchor: authentic 4:3, always offered (it pillarboxes
       on a widescreen panel, by choice). */
    if (n < cap) {
        out[n].w = 640;
        out[n].h = 480;
        out[n].tag = "classic";
        n++;
    }

    /* Head-mounted builds: 16:9 ladder only (see kHeadMountLadder). The
       classic 4:3 anchor is deliberately absent — an aspect change is exactly
       what these entries must not do. */
    if (Res_HeadMounted()) {
        /* Entries above the swapchain size are supersampled: the present
         * blit downscales them to fit, so every option stays fully visible
         * at the same 16:9 framing. */
        for (int i = 0; i < kHeadMountLadderCount && n < cap; i++) {
            out[n++] = kHeadMountLadder[i];
        }
        return n;
    }

    U32 dispW = 0, dispH = 0;
    if (!Res_EffectiveDisplay(&dispW, &dispH))
        return n; /* headless: classic only */

    /* A ladder of display-aspect-matched modes at increasing render heights, so
       every entry fills this exact panel (no letterbox/pillarbox) at a quality
       the player picks by height. Computed per monitor, so an odd-aspect panel
       gets its own widths rather than generic ones. Ascending order; the @480
       rung is the same value the boot auto-detect picks, so the first-launch
       resolution is always a listed entry. Heights that don't fit the panel, or
       whose width would exceed 1920 (ultrawide at 1080), are dropped; dups
       against the anchor (a 4:3 panel's @480 is 640x480) are skipped. */
    static const U32 kLadderH[] = {480, 720, 1080, 1440};
    static const char *kLadderTag[] = {"widescreen", "hd", "fhd", "qhd"};
    for (int i = 0; i < (int)(sizeof(kLadderH) / sizeof(kLadderH[0])) && n < cap; i++) {
        const U32 h = kLadderH[i];
        if (h > dispH)
            continue;
        const U32 w = Res_SnapAspectWidth(dispW, dispH, h);
        if (w == 0 || catalog_contains(out, n, w, h))
            continue;
        out[n].w = w;
        out[n].h = h;
        out[n].tag = kLadderTag[i];
        n++;
    }

    /* Panel-native resolution: fills the panel exactly (on Quest the OpenXR
       swapchain size, e.g. 1680x1760 — the sharpest the headset can show). */
    {
        const U32 nw = Res_SnapNativeW(dispW), nh = Res_SnapNativeH(dispH);
        if (nw && nh && !catalog_contains(out, n, nw, nh) && n < cap) {
            out[n].w = nw;
            out[n].h = nh;
            out[n].tag = "native";
            n++;
        }
    }

    return n;
}

/* True if (w, h) already appears in the first `n` entries of `list`. */
static bool catalog_contains(const ResModeEntry *list, int n, U32 w, U32 h) {
    for (int i = 0; i < n; i++)
        if (list[i].w == w && list[i].h == h)
            return true;
    return false;
}

int Res_BuildCatalog(ResModeEntry *out, int cap, bool includeAdvanced) {
    U32 dispW = 0, dispH = 0;
    const bool hasDisplay = Res_EffectiveDisplay(&dispW, &dispH);
    int n = 0;

    /* Head-mounted builds: the 16:9 ladder only — no curated tables (their
       mixed aspects are monitor-oriented). */
    if (Res_HeadMounted()) {
        for (int i = 0; i < kHeadMountLadderCount && n < cap; i++) {
            out[n++] = kHeadMountLadder[i];
        }
        return n;
    }

    /* Recommended: 640x480 always present, the rest gated by display fit. */
    for (int i = 0; i < kRecommendedCount && n < cap; i++) {
        if (i == 0) {
            out[n++] = kRecommended[i];
            continue;
        }
        if (hasDisplay && (kRecommended[i].w > dispW || kRecommended[i].h > dispH))
            continue;
        out[n++] = kRecommended[i];
    }

    /* Native (synthesised from the current display), deduped. */
    if (hasDisplay && n < cap) {
        U32 nw = Res_SnapNativeW(dispW), nh = Res_SnapNativeH(dispH);
        if (nw && nh && !catalog_contains(out, n, nw, nh)) {
            out[n].w = nw;
            out[n].h = nh;
            out[n].tag = "native";
            n++;
        }
    }

    if (!includeAdvanced)
        return n;

    /* Advanced: display-fit and deduped against everything above. */
    for (int i = 0; i < kAdvancedCount && n < cap; i++) {
        if (hasDisplay && (kAdvanced[i].w > dispW || kAdvanced[i].h > dispH))
            continue;
        if (catalog_contains(out, n, kAdvanced[i].w, kAdvanced[i].h))
            continue;
        out[n++] = kAdvanced[i];
    }

    return n;
}
