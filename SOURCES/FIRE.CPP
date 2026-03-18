/*
;══════════════════════════════════════════════════════════════════════════
;                           █▀▀▀▀  █    █▀▀▀█ █▀▀▀▀
;                           ██▀▀   ██   ██▀█▀ ██▀▀
;                           ▀▀     ▀▀   ▀▀  ▀ ▀▀▀▀▀
;══════════════════════════════════════════════════════════════════════════
*/

#include "FIRE.H"

#include <SYSTEM/UTILS.H>

#include <string.h>

U32 FireSeed = 1876351;

void Do_Fire(U8 *ptrwork1, U8 *ptrwork2, U8 *colortable, U8 *ptrtex)
{
  U8 *random_row = ptrwork1 + (32 * 34);
  U8 *smooth_src = ptrwork1 + 32;
  U8 *smooth_dst = ptrwork2 + 32;
  U8 *tex_src = ptrwork1;
  U8 *tex_dst = ptrtex;

  for (S32 weight = 64; weight > 0; --weight)
  {
    U32 value = ROL32(FireSeed, (FireSeed >> 8) & 0xFFu);
    U16 low_product = (U16)value;

    low_product = (U16)(low_product * (U16)weight);
    FireSeed += 5137827;
    *random_row++ = (U8)(low_product & 0x80u);
  }

  for (S32 y = 34; y > 0; --y)
  {
    S32 column = 0;

    for (S32 x = 32; x > 0; --x)
    {
      U32 sum = 0;

      column -= 32;
      sum += smooth_src[column];

      column += 64;
      sum += smooth_src[column];

      column = (column - 1) & 31;
      sum += smooth_src[column];

      column -= 32;
      sum += smooth_src[column];

      column += 64;
      sum += smooth_src[column];

      column = (column + 2) & 31;
      sum += smooth_src[column];

      column -= 32;
      sum += smooth_src[column];

      column += 64;
      sum += smooth_src[column];

      {
        U16 average = ROR16((U16)sum, 3);

        if ((average & 0x6000u) == 0 && (average & 0x00FFu) != 0)
        {
          --average;
        }

        *smooth_dst++ = (U8)average;
      }

      column &= 31;
    }

    smooth_src += 32;
  }

  memcpy(ptrwork1, ptrwork2 + 32, 32 * 34);

  for (S32 y = 32; y > 0; --y)
  {
    for (S32 x = 32; x > 0; --x)
    {
      U32 color_index = *tex_src >> 1;

      if (color_index > 31)
      {
        color_index = 31;
      }

      *tex_dst++ = colortable[color_index];
      ++tex_src;
    }

    tex_dst += 256 - 32;
  }
}
