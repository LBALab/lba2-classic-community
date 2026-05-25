/* Stereo balance / distance attenuation for the ambient-sound mixer.
 *
 * Extracted from AMBIANCE.CPP so it can be unit-tested in isolation —
 * AMBIANCE.CPP itself pulls in HQR, SDL audio, timer, and palette code
 * that the math here doesn't need. Public declaration stays in
 * AMBIANCE.H so existing consumers don't see the split. */

#include "C_EXTERN.H"
#include "AMBIANCE.H"

#include <3D/REGLE3.H>
#include <SVGA/SCREEN.H>

/* Interior cube uses Distance2D from the listener centre + a tight max
   radius; exterior uses 3D camera distance with a wider falloff window. */
//#define       DISTANCE_SAMPLE_MAX     963
#define DISTANCE_SAMPLE_MAX 700
#define DISTANCE_SAMPLE_MIN_EXT 5000L
#define DISTANCE_SAMPLE_MAX_EXT 30000L

S32 GiveBalance(S32 xp, S32 yp, S32 distance, U32 *pan, S32 *vol) {
    /* Stereo balance keys off the projected screen X.  The original 4:3
       constants (-320, 160, 480, 640+320, +480/-480 for Y) are the literal
       framebuffer geometry: pan ramps from 0 to 32 across the left "quarter"
       of the screen, from 32 to 96 across the middle "half", and from 96 to
       127 across the right "quarter", with off-screen tolerance of half a
       screen X / one screen Y on each side.  Routing those through
       ModeDesiredX/Y keeps the audio image lined up with what the player
       actually sees at any width.  At ModeDesiredX==640 / Y==480 the math
       is bit-identical (the historical "319" off-by-one terms reduce to
       3*MDX/4 - 1). */
    const S32 mdx = (S32)ModeDesiredX;
    const S32 mdy = (S32)ModeDesiredY;
    const S32 panLeftEnd = mdx / 4;           // 160 at 640: end of left ramp
    const S32 panRightStart = (mdx * 3) / 4;  // 480 at 640: start of right ramp
    const S32 outerSteps = panRightStart - 1; // 479 at 640: left/right ramp distance
    const S32 innerSteps = mdx / 2;           // 320 at 640: middle ramp distance

    if ((yp > mdy + mdy)
            OR(yp < -mdy))
        return FALSE;

    if ((xp < -mdx / 2)
            OR(xp >= mdx + mdx / 2))
        return FALSE;

    if (xp < panLeftEnd)
        *pan = RegleTrois(0, 32, outerSteps, xp + mdx / 2);
    else if (xp > panRightStart)
        *pan = RegleTrois(96, 127, outerSteps, xp - panRightStart);
    else
        *pan = RegleTrois(32, 96, innerSteps, xp - panLeftEnd);

    if (CubeMode == CUBE_INTERIEUR)
        if (distance > DISTANCE_SAMPLE_MAX)
            *vol = 0;
        else
            *vol = BoundRegleTrois(127, 0, DISTANCE_SAMPLE_MAX, distance);
    else {
        if (distance <= DISTANCE_SAMPLE_MIN_EXT)
            *vol = 127;
        else if (distance > DISTANCE_SAMPLE_MAX_EXT)
            return FALSE;
        else
            *vol = BoundRegleTrois(127, 0,
                                   DISTANCE_SAMPLE_MAX_EXT - DISTANCE_SAMPLE_MIN_EXT,
                                   distance - DISTANCE_SAMPLE_MIN_EXT);
    }

    return TRUE;
}
