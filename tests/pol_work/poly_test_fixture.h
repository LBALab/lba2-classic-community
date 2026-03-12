/* poly_test_fixture.h — shared setup for polygon rendering tests.
 *
 * Sets up a framebuffer, TabOffLine[], clip region, and the polygon
 * filler dispatch tables (Switch_Fillers) so Fill_Poly works.
 */
#ifndef POLY_TEST_FIXTURE_H
#define POLY_TEST_FIXTURE_H

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <string.h>

#define TEST_POLY_W  160
#define TEST_POLY_H  120
#define TEST_POLY_SIZE (TEST_POLY_W * TEST_POLY_H)

static U8 g_poly_framebuf[TEST_POLY_SIZE];

/* Set up screen state for polygon rendering tests. */
static void setup_polygon_screen(void)
{
    memset(g_poly_framebuf, 0, sizeof(g_poly_framebuf));
    Log = g_poly_framebuf;
    ModeDesiredX = TEST_POLY_W;
    ModeDesiredY = TEST_POLY_H;
    for (U32 i = 0; i < TEST_POLY_H; i++)
        TabOffLine[i] = i * TEST_POLY_W;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = TEST_POLY_W - 1;
    ClipYMax = TEST_POLY_H - 1;
    ScreenPitch = TEST_POLY_W;
    PTR_TabOffLine = TabOffLine;
    /* Normal fill mode (no fog, no zbuffer) */
    Switch_Fillers(FILL_POLY_NO_TEXTURES);
}

/* Create a Struc_Point with screen coords only. */
static Struc_Point make_point(S16 x, S16 y)
{
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    return p;
}

/* Create a Struc_Point with screen coords + light. */
static Struc_Point make_point_lit(S16 x, S16 y, U16 light)
{
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    p.Pt_Light = light;
    return p;
}

/* Create a Struc_Point with screen coords + UV texture coords. */
static Struc_Point make_point_uv(S16 x, S16 y, U16 u, U16 v)
{
    Struc_Point p;
    memset(&p, 0, sizeof(p));
    p.Pt_XE = x;
    p.Pt_YE = y;
    p.Pt_MapU = u;
    p.Pt_MapV = v;
    return p;
}

/* Count nonzero pixels in the framebuffer region [x0..x1) × [y0..y1). */
static int count_nonzero_pixels(int x0, int y0, int x1, int y1)
{
    int count = 0;
    for (int y = y0; y < y1 && y < TEST_POLY_H; y++)
        for (int x = x0; x < x1 && x < TEST_POLY_W; x++)
            if (g_poly_framebuf[y * TEST_POLY_W + x] != 0) count++;
    return count;
}

/* LCG random for randomized stress tests. */
static U32 poly_rng_state;
static void poly_rng_seed(U32 s) { poly_rng_state = s; }
static U32 poly_rng_next(void) {
    poly_rng_state = poly_rng_state * 1103515245u + 12345u;
    return (poly_rng_state >> 16) & 0x7FFF;
}

#endif /* POLY_TEST_FIXTURE_H */
