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
 * the process default and FE_TOWARDZERO, and every caller restores what it
 * set, so the control-word write is skipped when the mode already matches —
 * it runs per polygon otherwise. */
void ROUND_SetFpuMode(int mode) {
    static int current = -1;
    if (current != mode) {
        fesetround(mode);
        current = mode;
    }
}

