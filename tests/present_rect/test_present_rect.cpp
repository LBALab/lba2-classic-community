/* Host-only golden/characterization test for the windowed-present geometry
 * (LIB386/SYSTEM/PRESENT_RECT.CPP).
 *
 * Pins the current behaviour of two pure functions factored out of WINDOW.CPP:
 *
 *   ComputePresentRect    forward: framebuffer -> centered pillarbox/letterbox
 *                         destination rect inside the window.
 *   WindowPointToSurface  inverse: a window pixel -> framebuffer pixel.
 *
 * The two must agree (the inverse mirrors the forward rect), and today that
 * agreement is maintained by hand in two places. This test locks the numbers
 * down so a future swap to SDL_SetRenderLogicalPresentation can be checked as a
 * behaviour-preserving refactor: run it against the current code to capture the
 * golden values, then again after the swap and diff. See docs/WIDESCREEN.md for
 * the logical-presentation evaluation.
 *
 * PRESENT_RECT.CPP is deliberately SDL-free, so this links without any window /
 * renderer plumbing and runs in CI alongside the other host tests.
 */

#include <SYSTEM/PRESENT_RECT.H>

#include <cstdio>

static int failures = 0;

static void checkRect(const char *name, int ow, int oh, U32 sw, U32 sh, float ex,
                      float ey, float ew, float eh) {
    PresentRect r;
    ComputePresentRect(ow, oh, sw, sh, &r);
    if ((r.x != ex) || (r.y != ey) || (r.w != ew) || (r.h != eh)) {
        std::printf("FAIL %s: rect(window %dx%d, surface %ux%u) = "
                    "{%g,%g,%g,%g}, want {%g,%g,%g,%g}\n",
                    name, ow, oh, sw, sh, r.x, r.y, r.w, r.h, ex, ey, ew, eh);
        failures++;
    }
}

static void checkPoint(const char *name, S32 wx, S32 wy, int ow, int oh, U32 sw,
                       U32 sh, S32 esx, S32 esy) {
    S32 sx = -999;
    S32 sy = -999;
    WindowPointToSurface(wx, wy, ow, oh, sw, sh, &sx, &sy);
    if ((sx != esx) || (sy != esy)) {
        std::printf("FAIL %s: point(%d,%d in window %dx%d, surface %ux%u) = "
                    "(%d,%d), want (%d,%d)\n",
                    name, wx, wy, ow, oh, sw, sh, sx, sy, esx, esy);
        failures++;
    }
}

int main() {
    /* --- ComputePresentRect: forward geometry --------------------------- */
    /* Window matches the framebuffer exactly: full-window rect, no bars. */
    checkRect("1:1", 640, 480, 640, 480, 0, 0, 640, 480);
    /* 4:3 framebuffer in a 16:9 window -> pillarbox: bars left/right. */
    checkRect("pillarbox 4:3-in-16:9", 1280, 720, 640, 480, 160, 0, 960, 720);
    /* 16:9 framebuffer in a 4:3 window -> letterbox: bars top/bottom. */
    checkRect("letterbox 16:9-in-4:3", 1024, 768, 1280, 720, 0, 96, 1024, 576);
    /* Same aspect, integer 2x upscale: fills the window. */
    checkRect("integer 2x", 640, 480, 320, 240, 0, 0, 640, 480);
    /* Real-world odd widescreen panel (1366x768 is not exactly 16:9). */
    checkRect("odd panel 1366x768", 1366, 768, 640, 480, 171, 0, 1024, 768);
    /* Degenerate inputs fall back to the full surface rect. */
    checkRect("degenerate surface", 800, 600, 0, 0, 0, 0, 0, 0);
    checkRect("degenerate output", 0, 0, 640, 480, 0, 0, 640, 480);

    /* --- WindowPointToSurface: inverse mapping -------------------------- */
    /* The centre of the window maps to the centre of the framebuffer. */
    checkPoint("pillarbox centre", 640, 360, 1280, 720, 640, 480, 320, 240);
    checkPoint("letterbox centre", 512, 384, 1024, 768, 1280, 720, 640, 360);
    checkPoint("1:1 interior", 100, 50, 640, 480, 640, 480, 100, 50);
    checkPoint("integer 2x interior", 200, 100, 640, 480, 320, 240, 100, 50);

    /* Points in the bars clamp to the nearest framebuffer edge, and the far
       content corner clamps to the last valid framebuffer index. */
    checkPoint("pillarbox left bar", 0, 0, 1280, 720, 640, 480, 0, 0);
    checkPoint("pillarbox content max", 1119, 719, 1280, 720, 640, 480, 639, 479);
    checkPoint("pillarbox past right bar", 1280, 720, 1280, 720, 640, 480, 639, 479);

    /* Degenerate / no-window cases pass the point straight through. */
    checkPoint("degenerate surface", 50, 60, 800, 600, 0, 0, 50, 60);
    checkPoint("no-window passthrough", 50, 60, 0, 0, 640, 480, 50, 60);

    if (failures != 0) {
        std::printf("test_present_rect: %d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("test_present_rect: ok\n");
    return 0;
}
