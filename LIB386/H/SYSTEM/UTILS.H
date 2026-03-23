#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

static inline U32 ROR32(U32 value, U32 shift) {
    shift &= 31;
    if (shift == 0) {
        return value;
    }
    return (value >> shift) | (value << (32 - shift));
}

static inline U32 ROL32(U32 value, U32 shift) {
    shift &= 31;
    if (shift == 0) {
        return value;
    }
    return (value << shift) | (value >> (32 - shift));
}

static inline U16 ROR16(U16 value, U32 shift) {
    shift &= 15;
    if (shift == 0) {
        return value;
    }
    return (value >> shift) | (value << (16 - shift));
}

static inline U16 ROL16(U16 value, U32 shift) {
    shift &= 15;
    if (shift == 0) {
        return value;
    }
    return (value << shift) | (value >> (16 - shift));
}

static inline U8 ROR8(U8 value, U32 shift) {
    shift &= 7;
    if (shift == 0) {
        return value;
    }
    return (value >> shift) | (value << (8 - shift));
}

static inline U8 ROL8(U8 value, U32 shift) {
    shift &= 7;
    if (shift == 0) {
        return value;
    }
    return (value << shift) | (value >> (8 - shift));
}

enum ROUND_TYPE {
    ROUND_TYPE_FLOAT, // FPU control word for nearest rounding mode
    ROUND_TYPE_INT    // FPU control word for CHOP rounding mode
};

extern ROUND_TYPE RoundType;

double ROUND(double value);
