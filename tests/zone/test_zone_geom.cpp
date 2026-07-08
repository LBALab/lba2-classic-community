/*
 * Host test for ZonePointInBox (SOURCES/ZONE_GEOM.H) -- the point-in-zone-box
 * predicate shared by CheckZoneSce / CheckValidObjPos / CheckBuggyZoneGiver and
 * the `zonelist` console command.
 *
 * The whole point of the test is the ASYMMETRIC bound: X/Z are half-open
 * [X0,X1)/[Z0,Z1) while Y is closed [Y0,Y1]. That asymmetry is not cosmetic --
 * the '<' on X1/Z1 is what stops a point on the shared edge of two abutting
 * zones from matching both (the original "dans 2 zones en meme temps" bug the
 * engine comments call out). A regression that "tidied" it to symmetric bounds
 * would reintroduce the double-match; this test fails loudly if it does.
 */
#include "ZONE_GEOM.H"
#include "test_harness.h"

/* A reference box: [0,100) x [0,100] x [0,100).  Note Y is closed. */
static int in(S32 x, S32 y, S32 z) {
    return ZonePointInBox(x, y, z, /*x0*/ 0, /*y0*/ 0, /*z0*/ 0, /*x1*/ 100, /*y1*/ 100, /*z1*/ 100);
}

static void test_interior_point_is_inside(void) {
    ASSERT_EQ_INT(1, in(50, 50, 50));
}

static void test_lower_corner_is_inside(void) {
    /* X0/Y0/Z0 are all inclusive. */
    ASSERT_EQ_INT(1, in(0, 0, 0));
}

static void test_x_upper_bound_is_exclusive(void) {
    ASSERT_EQ_INT(0, in(100, 50, 50)); /* x == X1 -> out */
    ASSERT_EQ_INT(1, in(99, 50, 50));  /* x == X1-1 -> in  */
}

static void test_z_upper_bound_is_exclusive(void) {
    ASSERT_EQ_INT(0, in(50, 50, 100)); /* z == Z1 -> out */
    ASSERT_EQ_INT(1, in(50, 50, 99));  /* z == Z1-1 -> in  */
}

static void test_y_upper_bound_is_inclusive(void) {
    /* The odd one out: Y1 is IN the box. */
    ASSERT_EQ_INT(1, in(50, 100, 50)); /* y == Y1 -> in  */
    ASSERT_EQ_INT(0, in(50, 101, 50)); /* y == Y1+1 -> out */
}

static void test_below_lower_bounds_is_outside(void) {
    ASSERT_EQ_INT(0, in(-1, 50, 50));
    ASSERT_EQ_INT(0, in(50, -1, 50));
    ASSERT_EQ_INT(0, in(50, 50, -1));
}

/* The bug the '<' fix addresses: two zones sharing the plane x == 100 must not
 * both claim a point on that plane.  Zone A = [0,100), zone B = [100,200). */
static void test_abutting_zones_no_double_match_in_x(void) {
    S32 px = 100, py = 50, pz = 50;
    int inA = ZonePointInBox(px, py, pz, 0, 0, 0, 100, 100, 100);
    int inB = ZonePointInBox(px, py, pz, 100, 0, 0, 200, 100, 100);
    ASSERT_EQ_INT(0, inA);       /* left zone releases the shared edge */
    ASSERT_EQ_INT(1, inB);       /* right zone owns it */
    ASSERT_EQ_INT(1, inA + inB); /* exactly one owner, never two */
}

static void test_abutting_zones_no_double_match_in_z(void) {
    S32 px = 50, py = 50, pz = 100;
    int inA = ZonePointInBox(px, py, pz, 0, 0, 0, 100, 100, 100);
    int inB = ZonePointInBox(px, py, pz, 0, 0, 100, 100, 100, 200);
    ASSERT_EQ_INT(1, inA + inB); /* exactly one owner on the shared Z edge */
}

int main(void) {
    RUN_TEST(test_interior_point_is_inside);
    RUN_TEST(test_lower_corner_is_inside);
    RUN_TEST(test_x_upper_bound_is_exclusive);
    RUN_TEST(test_z_upper_bound_is_exclusive);
    RUN_TEST(test_y_upper_bound_is_inclusive);
    RUN_TEST(test_below_lower_bounds_is_outside);
    RUN_TEST(test_abutting_zones_no_double_match_in_x);
    RUN_TEST(test_abutting_zones_no_double_match_in_z);
    TEST_SUMMARY();
    return test_failures != 0;
}
