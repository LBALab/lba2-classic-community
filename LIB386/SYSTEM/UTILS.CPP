#include <SYSTEM/UTILS.H>

#include <fenv.h>
#include <math.h>

ROUND_TYPE RoundType = ROUND_TYPE_FLOAT;

double ROUND(double value) {
    if (RoundType == ROUND_TYPE_FLOAT) {
        return roundf(value);
    } else {
        return (double)(S64)value;
    }
}

/* The polygon pipeline toggles the x87-compatible rounding mode around its
 * truncation windows (fldcw in the ASM).  The mode only ever flips between
 * the process default and FE_TOWARDZERO, so the control-word write is skipped
 * when the mode already matches — it runs per polygon otherwise.  Read the
 * live mode rather than caching it: fesetenv()/fesetround() elsewhere (the
 * tests do this between cases) would otherwise leave a stale cache. */
void ROUND_SetFpuMode(int mode) {
    if (fegetround() != mode) {
        fesetround(mode);
    }
}
