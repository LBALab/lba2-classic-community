#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

U32 ROR32(U32 value, U32 shift);
U32 ROL32(U32 value, U32 shift);

U16 ROR16(U16 value, U32 shift);
U16 ROL16(U16 value, U32 shift);

U8 ROR8(U8 value, U32 shift);
U8 ROL8(U8 value, U32 shift);

enum ROUND_TYPE
{
    ROUND_TYPE_FLOAT, // FPU control word for nearest rounding mode
    ROUND_TYPE_INT // FPU control word for CHOP rounding mode
};

extern ROUND_TYPE RoundType;

double ROUND(double value);
