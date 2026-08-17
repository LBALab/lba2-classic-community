#ifndef FOLLOWCAM_MATH_H
#define FOLLOWCAM_MATH_H

#include <SYSTEM/ADELINE_TYPES.H>

/* The Auto camera's angle arithmetic, in one place and free of engine state.
 *
 * Angles are 4096 units to the turn. Two of the three functions here express a
 * single relationship, which the camera code has repeated inline at every site
 * that aims the camera at the hero:
 *
 *   BetaCam = (2048 - (AddBetaCam + heroBeta)) & 4095
 *
 * `BetaCam` is a world angle and `AddBetaCam` is an offset from the hero's
 * facing, so the pair only agree while something maintains that rule. Every
 * writer of `BetaCam` that leaves `AddBetaCam` alone creates a difference the
 * follow update reads as catch-up owed and spends, which is where the camera's
 * snapping bugs have come from. Solving the rule for the current angle is what
 * hands the camera to the follow update without anything owed.
 *
 * The host test in tests/camera/test_followcam_math.cpp checks the round trip
 * over every angle pair, and that the lerp always converges.
 */

/* Shortest way round: the signed difference between two angles, in [-2048, 2048].
 * Written out at several sites before this, each doing its own pair of wraps.
 *
 * Both inputs are expected already inside the turn, which is what every caller
 * holds. The half turn keeps the sign of the subtraction rather than resolving
 * one way, so a target exactly opposite is approached in the direction the
 * arithmetic fell out at, and the two callers agree with each other. Masking the
 * subtraction first would look tidier and would flip that direction at the
 * antipode, on exactly the 2048 pairs where the two are furthest apart. */
static inline S32 FollowCamAngleDiff(S32 to, S32 from) {
    S32 d = to - from;
    if (d > 2048)
        d -= 4096;
    if (d < -2048)
        d += 4096;
    return d;
}

/* Where the follow update aims: behind a hero facing `heroBeta`, offset by `pan`. */
static inline S32 FollowCamTargetBetaFor(S32 pan, S32 heroBeta) {
    return (2048 - (pan + heroBeta)) & 4095;
}

/* The inverse: the pan that makes the above come out at `betaCam`. Used when the
 * camera has been aimed by someone other than the follow update (the player
 * orbiting, an authored shot) and the follow update has to be told about it. */
static inline S32 FollowCamPanForAngle(S32 betaCam, S32 heroBeta) {
    return (2048 - betaCam - heroBeta) & 4095;
}

/* One frame of the rotation lerp: how far to move toward a target `diff` away.
 *
 * `divisor` sets the laziness. The minimum step matters more than it looks:
 * integer division truncates to zero whenever |diff| < divisor, and `diff` here
 * is already past the snap threshold, so without a floor the camera would
 * neither step nor snap. It would sit a few units off target for ever, with the
 * dirty check firing and the exterior re-rendering every idle frame.
 *
 * Callers snap outright when |diff| is within their threshold; this handles the
 * case past it, and never overshoots.
 */
static inline S32 FollowCamRotStep(S32 diff, S32 divisor, S32 minStep) {
    S32 step;

    if (divisor < 1)
        divisor = 1;

    if (diff == 0)
        return 0; /* already there: promoting this to a minimum step would walk the
                   * camera off the target and back, for ever, which is the failure
                   * the minimum step exists to prevent rather than to cause */

    step = diff / divisor;
    if (step >= 0 && step < minStep) /* >= 0: a truncated-to-zero step is the case this is for */
        step = (diff > 0) ? minStep : -minStep;
    else if (step < 0 && step > -minStep)
        step = -minStep;

    /* Not in the code this was taken from, and inert where it is called from:
     * callers snap within a threshold of 10 while the minimum step is 3, so the
     * step can never exceed what is left. It is here so the function is correct
     * on its own terms rather than only under its callers' guarantees, since a
     * later change to either constant would otherwise make the camera step past
     * the target and approach it from the other side. */
    if (diff > 0 && step > diff)
        step = diff;
    if (diff < 0 && step < diff)
        step = diff;

    return step;
}

#endif /* FOLLOWCAM_MATH_H */
