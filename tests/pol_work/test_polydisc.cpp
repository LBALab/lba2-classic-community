/* Test: Fill_Sphere — disc/sphere rendering (POLYDISC.CPP).
 *
 * Fill_Sphere draws a filled circle using a Bresenham-like algorithm.
 * It reads: Log, TabOffLine, ClipXMin/XMax/YMin/YMax,
 *           Fill_Flag_ZBuffer, Fill_Flag_NZW, Fill_Flag_Fog,
 *           Fill_Logical_Palette, PtrZBuffer
 */
#include "test_harness.h"
#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include "poly_test_fixture.h"
#include <string.h>

/* ── Basic sphere rendering ────────────────────────────────────── */

static void test_sphere_centre(void) {
    setup_polygon_screen();
    Fill_Sphere(0, 0xAA, 80, 60, 20, 0);
    /* Centre pixel should be filled */
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[60 * TEST_POLY_W + 80]);
}

static void test_sphere_radius_coverage(void) {
    setup_polygon_screen();
    Fill_Sphere(0, 0x55, 80, 60, 15, 0);
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    /* A circle of radius 15 should have ~pi*15^2=~707 pixels */
    ASSERT_TRUE(n > 400);
    ASSERT_TRUE(n < 1000);
}

static void test_sphere_small_radius(void) {
    setup_polygon_screen();
    Fill_Sphere(0, 0xCC, 80, 60, 1, 0);
    /* At least the centre pixel should be set */
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n >= 1);
}

static void test_sphere_zero_radius(void) {
    setup_polygon_screen();
    /* Radius 0 should not crash */
    Fill_Sphere(0, 0xDD, 80, 60, 0, 0);
    ASSERT_TRUE(1); /* no crash */
}

/* ── Clipping ──────────────────────────────────────────────────── */

static void test_sphere_partially_clipped(void) {
    setup_polygon_screen();
    /* Sphere at edge — partially clipped */
    Fill_Sphere(0, 0x77, 10, 10, 20, 0);
    /* Some pixels should be drawn in the visible area */
    int n = count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H);
    ASSERT_TRUE(n > 50);
}

static void test_sphere_fully_offscreen(void) {
    setup_polygon_screen();
    /* Sphere entirely outside clip region */
    Fill_Sphere(0, 0xEE, -50, -50, 10, 0);
    ASSERT_EQ_INT(0, count_nonzero_pixels(0, 0, TEST_POLY_W, TEST_POLY_H));
}

/* ── Transparency mode (type 2) ────────────────────────────────── */

static void test_sphere_transparent(void) {
    setup_polygon_screen();
    memset(g_poly_framebuf, 0x11, TEST_POLY_SIZE);
    Fill_Sphere(2, 0xA0, 80, 60, 15, 0);
    /* Transparent blend: (pixel & 0x0F) + (color & 0xF0) */
    U8 centre = g_poly_framebuf[60 * TEST_POLY_W + 80];
    ASSERT_EQ_UINT(0xA1, centre);
}

/* ── Fog mode ──────────────────────────────────────────────────── */

static void test_sphere_fog(void) {
    setup_polygon_screen();
    /* Set up fog: logical palette maps colors */
    Switch_Fillers(FILL_POLY_FOG);
    /* Fill_Logical_Palette: map 0x55 -> 0xAA for testing */
    memset(Fill_Logical_Palette, 0, sizeof(Fill_Logical_Palette));
    Fill_Logical_Palette[0x55] = 0xAA;
    Fill_Sphere(0, 0x55, 80, 60, 15, 0);
    /* With fog, color goes through Fill_Logical_Palette */
    ASSERT_EQ_UINT(0xAA, g_poly_framebuf[60 * TEST_POLY_W + 80]);
    /* Restore normal mode */
    Switch_Fillers(FILL_POLY_NO_TEXTURES);
}

/* ── Randomized stress test ────────────────────────────────────── */

static void test_sphere_random(void) {
    poly_rng_seed(0xDEADBEEF);
    for (int i = 0; i < 30; i++) {
        setup_polygon_screen();
        S32 cx = (S32)(poly_rng_next() % (TEST_POLY_W + 60)) - 30;
        S32 cy = (S32)(poly_rng_next() % (TEST_POLY_H + 60)) - 30;
        S32 r = (S32)(poly_rng_next() % 40);
        S32 color = (S32)(poly_rng_next() & 0xFF) | 1;
        S32 type = (S32)(poly_rng_next() % 3); /* 0=flat, 1=flat, 2=transp */
        Fill_Sphere(type, color, cx, cy, r, 0);
        ASSERT_TRUE(1);
    }
}

/* ── ASM-vs-CPP equivalence ─────────────────────────────────────── */

extern "C" void asm_Fill_Sphere(void);

/* Watcom register calling convention:
 * ESI = Type_Sphere, EDX = Color_Sphere, EAX = Centre_X,
 * EBX = Centre_Y, ECX = Rayon, EDI = zBufferValue */
static void call_asm_Fill_Sphere(S32 type, S32 color,
                                 S32 cx, S32 cy, S32 r, S32 zbuf) {
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Fill_Sphere\n\t"
        "pop %%ebp"
        :
        : "S"(type), "d"(color), "a"(cx), "b"(cy), "c"(r), "D"(zbuf)
        : "memory", "cc");
}

static U8 disc_cpp_buf[TEST_POLY_SIZE];
static U8 disc_asm_buf[TEST_POLY_SIZE];

static void test_asm_equiv_sphere_centre(void) {
    setup_polygon_screen();
    Fill_Sphere(0, 42, 80, 60, 20, 0);
    memcpy(disc_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    call_asm_Fill_Sphere(0, 42, 80, 60, 20, 0);
    memcpy(disc_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(disc_asm_buf, disc_cpp_buf, TEST_POLY_SIZE,
                          "Fill_Sphere solid centre");
}

static void test_asm_equiv_sphere_clipped(void) {
    setup_polygon_screen();
    Fill_Sphere(0, 33, 5, 5, 25, 0);
    memcpy(disc_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

    setup_polygon_screen();
    call_asm_Fill_Sphere(0, 33, 5, 5, 25, 0);
    memcpy(disc_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

    ASSERT_ASM_CPP_MEM_EQ(disc_asm_buf, disc_cpp_buf, TEST_POLY_SIZE,
                          "Fill_Sphere clipped top-left");
}

static void test_asm_random_spheres(void) {
    poly_rng_seed(0xCAFEBABE);
    for (int i = 0; i < 300; i++) {
        S32 cx = (S32)(poly_rng_next() % (TEST_POLY_W + 60)) - 30;
        S32 cy = (S32)(poly_rng_next() % (TEST_POLY_H + 60)) - 30;
        S32 r = (S32)(poly_rng_next() % 40);
        S32 color = (S32)(poly_rng_next() & 0xFE) | 1;

        setup_polygon_screen();
        Fill_Sphere(0, color, cx, cy, r, 0);
        memcpy(disc_cpp_buf, g_poly_framebuf, TEST_POLY_SIZE);

        setup_polygon_screen();
        call_asm_Fill_Sphere(0, color, cx, cy, r, 0);
        memcpy(disc_asm_buf, g_poly_framebuf, TEST_POLY_SIZE);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "random sphere #%d cx=%d cy=%d r=%d col=%d",
                 i, cx, cy, r, color);
        ASSERT_ASM_CPP_MEM_EQ(disc_asm_buf, disc_cpp_buf, TEST_POLY_SIZE, msg);
    }
}

int main(void) {
    RUN_TEST(test_sphere_centre);
    RUN_TEST(test_sphere_radius_coverage);
    RUN_TEST(test_sphere_small_radius);
    RUN_TEST(test_sphere_zero_radius);
    RUN_TEST(test_sphere_partially_clipped);
    RUN_TEST(test_sphere_fully_offscreen);
    RUN_TEST(test_sphere_transparent);
    RUN_TEST(test_sphere_fog);
    RUN_TEST(test_sphere_random);
    RUN_TEST(test_asm_equiv_sphere_centre);
    RUN_TEST(test_asm_equiv_sphere_clipped);
    RUN_TEST(test_asm_random_spheres);
    TEST_SUMMARY();
    return test_failures != 0;
}
