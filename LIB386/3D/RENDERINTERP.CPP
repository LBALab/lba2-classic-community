#include <3D/RENDERINTERP.H>

// Render-interpolation core (issue #412, docs/RENDER_INTERP_PLAN.md). Pure: no globals, no clock read.

S32 Render_LerpCoord(S32 prev, S32 cur, U32 num, U32 den) {
    // Byte-exact endpoints so a frame on a step boundary matches the un-interpolated position (den==0
    // is interpolation off / no throttle -> current). num>=den also returns cur, so a clamped
    // Render_InterpNum boundary renders the settled position exactly.
    if (den == 0 || num >= den) {
        return cur;
    }
    if (num == 0) {
        return prev;
    }
    // 64-bit product so a full-cube span (delta up to ~65535) times num cannot overflow S32. Truncates
    // toward zero; the sub-unit error never accumulates because each frame recomputes from prev/cur.
    return prev + (S32)((long long)(cur - prev) * (long long)num / (long long)den);
}

U32 Render_InterpNum(U32 now, U32 lastSim, S32 dt) {
    if (dt <= 0) {
        return 0; // interpolation off
    }
    // Signed so a backwards clock (a modal restore, a cube change; never expected at render time)
    // reads <=0 rather than a huge unsigned banked value.
    S32 banked = (S32)(now - lastSim);
    if (banked <= 0) {
        return 0;
    }
    if (banked >= dt) {
        return (U32)dt; // at or past the next step -> render the current position
    }
    return (U32)banked;
}
