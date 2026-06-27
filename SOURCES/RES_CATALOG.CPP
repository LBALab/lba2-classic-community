/*
 * Resolution catalog. See RES_CATALOG.H. Lifted out of CONSOLE_CMD.CPP when
 * the Display submenu became a second consumer (RES_SWITCH.CPP:39 foretold it).
 */

#include "RES_CATALOG.H"

#include <SYSTEM/WINDOW.H> /* Window_GetDisplayDimensions */

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
