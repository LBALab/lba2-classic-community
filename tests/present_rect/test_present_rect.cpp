/* Host conformance test for the windowed-present path (LIB386/SYSTEM/WINDOW.CPP).
 *
 * The engine hands SDL the framebuffer size as the logical presentation size
 * (LETTERBOX) and lets SDL own both the on-screen present rect and the
 * window->framebuffer coordinate mapping (SDL_RenderCoordinatesFromWindow).
 * WindowToSurfaceCoords then clamps SDL's result to a valid [0, res-1] index.
 *
 * This test pins that behaviour against a headless software renderer so an SDL
 * upgrade that changes the letterbox math, or a regression in our clamp, is
 * caught in CI. The expected values were captured from SDL's actual output and
 * cross-checked against the previous hand-rolled geometry: interior points map
 * identically; the documented differences are SDL's half-pixel centring on
 * odd-gap windows and unclamped extrapolation in the bars (which the clamp
 * below absorbs).
 */

#include <SDL3/SDL.h>

#include <cstdio>

static int failures = 0;

/* Mirror of WINDOW.CPP WindowToSurfaceCoords' clamp, layered on SDL's mapping. */
static void mapToSurface(SDL_Renderer *r, int surfW, int surfH, int wx, int wy, int *sx, int *sy) {
    float lx = 0.0f;
    float ly = 0.0f;
    SDL_RenderCoordinatesFromWindow(r, (float)wx, (float)wy, &lx, &ly);
    int x = (int)lx;
    int y = (int)ly;
    if (x < 0) {
        x = 0;
    } else if (x >= surfW) {
        x = surfW - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= surfH) {
        y = surfH - 1;
    }
    *sx = x;
    *sy = y;
}

/* Build a software renderer whose output is winW x winH and whose logical
 * (framebuffer) size is surfW x surfH, matching the engine's setup. */
static SDL_Renderer *makeRenderer(int winW, int winH, int surfW, int surfH, SDL_Surface **outSurf) {
    SDL_Surface *surf = SDL_CreateSurface(winW, winH, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r = SDL_CreateSoftwareRenderer(surf);
    SDL_SetRenderLogicalPresentation(r, surfW, surfH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    *outSurf = surf;
    return r;
}

/* Present rect: centered, aspect-fit. Tolerance 1.0 absorbs SDL's half-pixel
 * centring on odd letterbox gaps. */
static void checkRect(const char *name, int winW, int winH, int surfW, int surfH, float ex, float ey,
                      float ew, float eh) {
    SDL_Surface *surf = NULL;
    SDL_Renderer *r = makeRenderer(winW, winH, surfW, surfH, &surf);
    SDL_FRect got;
    SDL_GetRenderLogicalPresentationRect(r, &got);
    if ((SDL_fabsf(got.x - ex) > 1.0f) || (SDL_fabsf(got.y - ey) > 1.0f) ||
        (SDL_fabsf(got.w - ew) > 1.0f) || (SDL_fabsf(got.h - eh) > 1.0f)) {
        std::printf("FAIL %s: rect {%.2f,%.2f,%.2f,%.2f}, want ~{%.0f,%.0f,%.0f,%.0f}\n", name, got.x,
                    got.y, got.w, got.h, ex, ey, ew, eh);
        failures++;
    }
    SDL_DestroyRenderer(r);
    SDL_DestroySurface(surf);
}

static void checkPoint(const char *name, int winW, int winH, int surfW, int surfH, int wx, int wy,
                       int esx, int esy) {
    SDL_Surface *surf = NULL;
    SDL_Renderer *r = makeRenderer(winW, winH, surfW, surfH, &surf);
    int sx = -999;
    int sy = -999;
    mapToSurface(r, surfW, surfH, wx, wy, &sx, &sy);
    if ((sx != esx) || (sy != esy)) {
        std::printf("FAIL %s: point (%d,%d) -> (%d,%d), want (%d,%d)\n", name, wx, wy, sx, sy, esx,
                    esy);
        failures++;
    }
    SDL_DestroyRenderer(r);
    SDL_DestroySurface(surf);
}

int main() {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("test_present_rect: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* --- present rect: centered, aspect-fit ----------------------------- */
    checkRect("1:1", 640, 480, 640, 480, 0, 0, 640, 480);
    checkRect("pillarbox 4:3-in-16:9", 1280, 720, 640, 480, 160, 0, 960, 720);
    checkRect("letterbox 16:9-in-4:3", 1024, 768, 1280, 720, 0, 96, 1024, 576);
    checkRect("integer 2x", 640, 480, 320, 240, 0, 0, 640, 480);
    checkRect("odd panel 1366x768", 1366, 768, 640, 480, 171, 0, 1024, 768);

    /* --- coordinate mapping: interior is exact -------------------------- */
    checkPoint("1:1 centre", 640, 480, 640, 480, 320, 240, 320, 240);
    checkPoint("pillarbox centre", 1280, 720, 640, 480, 640, 360, 320, 240);
    checkPoint("letterbox centre", 1024, 768, 1280, 720, 512, 384, 640, 360);
    checkPoint("integer 2x centre", 640, 480, 320, 240, 320, 240, 160, 120);

    /* --- bars and corners clamp to a valid index ------------------------ */
    checkPoint("pillarbox left bar", 1280, 720, 640, 480, 0, 0, 0, 0);
    checkPoint("pillarbox far corner", 1280, 720, 640, 480, 1279, 719, 639, 479);
    checkPoint("letterbox far corner", 1024, 768, 1280, 720, 1023, 767, 1278, 719);
    checkPoint("negative coords clamp", 1280, 720, 640, 480, -50, -50, 0, 0);

    SDL_Quit();
    if (failures != 0) {
        std::printf("test_present_rect: %d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("test_present_rect: ok\n");
    return 0;
}
