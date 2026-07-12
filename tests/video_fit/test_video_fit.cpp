/* Host conformance test for the FMV cinematic fit-rect math
 * (SOURCES/VIDEO_FIT.H, Video_ComputeCineRect).
 *
 * PlayAcf sizes the 320x200 Smacker upscale rectangle once per playback from
 * this function. Two policies, selected by the VideoFullScreen cfg flag:
 *
 *   0 = centred letterbox: fixed 2x -> 640x400, centred. Byte-identical to the
 *       pre-fit player at every resolution.
 *   1 = fit to screen: largest UNIFORM scale that fits modeX x modeY, centred,
 *       remainder pillar/letter-boxed.
 *
 * The pins below lock the rect for a spread of resolutions, and assert the
 * two invariants that matter: at 640x480 both policies collapse to the exact
 * legacy layout (fit factor == 2.0, so the ui_video golden is unchanged
 * whichever branch runs), and the fit branch never distorts (dstW:dstH stays
 * on the 320:200 aspect) nor spills outside the framebuffer.
 */

#include "VIDEO_FIT.H"

#include <cstdio>
#include <cstdlib>

static int failures = 0;

static void checkRect(const char *name, int modeX, int modeY, int fit,
                      int ew, int eh, int ex, int ey) {
    int w = -1, h = -1, x = -1, y = -1;
    Video_ComputeCineRect(modeX, modeY, fit, &w, &h, &x, &y);
    if (w != ew || h != eh || x != ex || y != ey) {
        std::printf("FAIL %s (%dx%d, fit=%d): got {w=%d h=%d x0=%d y0=%d}, "
                    "want {w=%d h=%d x0=%d y0=%d}\n",
                    name, modeX, modeY, fit, w, h, x, y, ew, eh, ex, ey);
        failures++;
    }
}

/* Aspect preserved (no distortion): dstW/dstH must stay on the 320:200 = 8:5
 * ratio to within the independent rounding of each axis (< 1 source pixel).
 * Also assert the rect is centred and fully inside the framebuffer. */
static void checkInvariants(const char *name, int modeX, int modeY, int fit) {
    int w = 0, h = 0, x = 0, y = 0;
    Video_ComputeCineRect(modeX, modeY, fit, &w, &h, &x, &y);

    if (w <= 0 || h <= 0 || w > modeX || h > modeY) {
        std::printf("FAIL %s bounds (%dx%d, fit=%d): w=%d h=%d out of range\n",
                    name, modeX, modeY, fit, w, h);
        failures++;
    }
    if (x != (modeX - w) / 2 || y != (modeY - h) / 2 || x < 0 || y < 0) {
        std::printf("FAIL %s centring (%dx%d, fit=%d): x0=%d y0=%d\n",
                    name, modeX, modeY, fit, x, y);
        failures++;
    }
    /* |w*200 - h*320| < max(320, 200): allows +/-0.5px rounding per axis. */
    long skew = (long)w * 200 - (long)h * 320;
    if (skew < 0)
        skew = -skew;
    if (skew >= 320) {
        std::printf("FAIL %s aspect (%dx%d, fit=%d): w=%d h=%d skew=%ld\n",
                    name, modeX, modeY, fit, w, h, skew);
        failures++;
    }
}

int main() {
    /* --- legacy centred letterbox (fit=0): fixed 2x at every resolution --- */
    checkRect("orig 640x480", 640, 480, 0, 640, 400, 0, 40);
    checkRect("orig 848x480", 848, 480, 0, 640, 400, 104, 40);
    checkRect("orig 1920x480", 1920, 480, 0, 640, 400, 640, 40);
    checkRect("orig 1280x720", 1280, 720, 0, 640, 400, 320, 160);

    /* --- fit to screen (fit=1) ------------------------------------------- */
    checkRect("fit 640x480", 640, 480, 1, 640, 400, 0, 40);    /* == 2.0x, height-limited */
    checkRect("fit 848x480", 848, 480, 1, 768, 480, 40, 0);    /* fills height, pillarbox */
    checkRect("fit 1920x480", 1920, 480, 1, 768, 480, 576, 0); /* height-limited, wide pillars */
    checkRect("fit 1280x720", 1280, 720, 1, 1152, 720, 64, 0); /* fills height */
    checkRect("fit 1024x768", 1024, 768, 1, 1024, 640, 0, 64); /* width-limited, letterbox */

    /* --- key invariant: 640x480 is identical in BOTH branches (factor 2.0),
       so the golden FMV frame does not depend on VideoFullScreen ---------- */
    {
        int w0, h0, x0, y0, w1, h1, x1, y1;
        Video_ComputeCineRect(640, 480, 0, &w0, &h0, &x0, &y0);
        Video_ComputeCineRect(640, 480, 1, &w1, &h1, &x1, &y1);
        if (w0 != w1 || h0 != h1 || x0 != x1 || y0 != y1) {
            std::printf("FAIL 640x480 branch-parity: orig {%d,%d,%d,%d} != "
                        "fit {%d,%d,%d,%d}\n",
                        w0, h0, x0, y0, w1, h1, x1, y1);
            failures++;
        }
        if (w0 != 640 || h0 != 400) {
            std::printf("FAIL 640x480 factor != 2.0: w=%d h=%d\n", w0, h0);
            failures++;
        }
    }

    /* --- no distortion / no overflow across the fit ladder --------------- */
    const int ladder[][2] = {{640, 480}, {848, 480}, {1024, 480}, {1280, 480}, {1920, 480}, {1280, 720}, {1024, 768}, {1920, 1080}};
    for (const auto &r : ladder)
        checkInvariants("fit-ladder", r[0], r[1], 1);

    if (failures != 0) {
        std::printf("test_video_fit: %d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("test_video_fit: ok\n");
    return 0;
}
