#include "DEMO_SEED.H"

#ifdef __cplusplus
extern "C" {
#endif

U32 Demo_RngSeed(S32 demo_slide, U32 timer_ref_hr, S32 new_cube) {
    if (demo_slide)
        return (U32)new_cube;
    return timer_ref_hr;
}

#ifdef __cplusplus
}
#endif
