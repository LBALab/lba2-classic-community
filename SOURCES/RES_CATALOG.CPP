/*
 * Resolution catalog. See RES_CATALOG.H. Lifted out of CONSOLE_CMD.CPP when
 * the Display submenu became a second consumer (RES_SWITCH.CPP:39 foretold it).
 */

#include "RES_CATALOG.H"

#include <SYSTEM/WINDOW.H> /* Window_GetDisplayDimensions */

#include <stdio.h> /* snprintf (Res_AspectLabel) */

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

/* Common display aspect ratios, widest-name-first is irrelevant; matched by
   nearest within ~1.5%. The %8 width snap nudges a derived mode a few pixels
   off its exact ratio (e.g. 848x480 is 53:30, not a clean 16:9), so a snapped
   16:9 mode still reads "16:9" instead of an unrecognisable reduced fraction. */
static const struct {
    const char *name;
    U32 a, b;
} kAspects[] = {
    {"5:4", 5, 4},
    {"4:3", 4, 3},
    {"3:2", 3, 2},
    {"16:10", 16, 10},
    {"16:9", 16, 9},
    {"21:10", 21, 10},
    {"21:9", 21, 9},
    {"32:9", 32, 9},
};
#define kAspectsCount ((int)(sizeof(kAspects) / sizeof(kAspects[0])))

void Res_AspectLabel(U32 w, U32 h, char *out, int outSize) {
    if (h == 0) {
        snprintf(out, (size_t)outSize, "?");
        return;
    }
    /* Nearest standard ratio within 1.5%: |w/h - a/b| / (a/b) <= 0.015, kept in
       integers as |w*b - h*a| * 1000 <= 15 * h*a. All terms fit in S32 for the
       supported range (w,h <= 1920; a,b <= 32). */
    for (int i = 0; i < kAspectsCount; i++) {
        S32 d = (S32)(w * kAspects[i].b) - (S32)(h * kAspects[i].a);
        if (d < 0)
            d = -d;
        if (d * 1000 <= 15 * (S32)(h * kAspects[i].a)) {
            snprintf(out, (size_t)outSize, "%s", kAspects[i].name);
            return;
        }
    }
    /* No standard match (an unusual panel, e.g. a 1.547:1 MacBook): a 2-decimal
       ratio. The exact reduced fraction would be unreadable (1728:1117) and, on
       the menu's aspect ladder, drift per rung as each width snaps to %8
       (23:15 / 139:90 / 208:135 for the same panel). The decimal is stable and
       legible; callers that want one label for the whole ladder pass the
       display dimensions, not each snapped mode. */
    snprintf(out, (size_t)outSize, "%.2f:1", (double)w / (double)h);
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

    U32 dispW = 0, dispH = 0;
    if (!Window_GetDisplayDimensions(&dispW, &dispH))
        return n; /* headless: classic only */

    /* A ladder of display-aspect-matched modes at increasing render heights, so
       every entry fills this exact panel (no letterbox/pillarbox) at a quality
       the player picks by height. Computed per monitor, so an odd-aspect panel
       gets its own widths rather than generic ones. Ascending order; the @480
       rung is the same value the boot auto-detect picks, so the first-launch
       resolution is always a listed entry. Heights that don't fit the panel, or
       whose width would exceed 1920 (ultrawide at 1080), are dropped; dups
       against the anchor (a 4:3 panel's @480 is 640x480) are skipped. */
    static const U32 kLadderH[] = {480, 720, 1080};
    static const char *kLadderTag[] = {"widescreen", "hd", "fhd"};
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
    const bool hasDisplay = Window_GetDisplayDimensions(&dispW, &dispH);
    int n = 0;

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
