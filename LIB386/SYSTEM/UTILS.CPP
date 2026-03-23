#include <SYSTEM/UTILS.H>

#include <math.h>

ROUND_TYPE RoundType = ROUND_TYPE_FLOAT;

double ROUND(double value) {
    if (RoundType == ROUND_TYPE_FLOAT) {
        return roundf(value);
    } else {
        return (double)(S64)value;
    }
}
