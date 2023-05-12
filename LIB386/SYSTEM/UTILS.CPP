#include <SYSTEM/UTILS.H>

#include <math.h>

U32 ROR32(U32 value, U32 shift)
{
    return (value >> shift) | (value << (32 - shift));
}

U32 ROL32(U32 value, U32 shift)
{
    return (value << shift) | (value >> (32 - shift));
}

U16 ROR16(U16 value, U32 shift)
{
    return (value >> shift) | (value << (16 - shift));
}

U16 ROL16(U16 value, U32 shift)
{
    return (value << shift) | (value >> (16 - shift));
}

U8 ROR8(U8 value, U32 shift)
{
    return (value >> shift) | (value << (8 - shift));
}

U8 ROL8(U8 value, U32 shift)
{
    return (value << shift) | (value >> (8 - shift));
}

ROUND_TYPE RoundType = ROUND_TYPE_FLOAT;

double ROUND(double value)
{
  if (RoundType == ROUND_TYPE_FLOAT)
  {
    return roundf(value);
  }
  else
  {
    return (double)(S64)value;
  }
}
