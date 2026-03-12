/*
    DWORD	TabOffset[]
      Brick:
          - BYTE Delta X
          - BYTE Delta Y
        Line(Delta Y):
        - BYTE NbBlock
           Block:
            - 00xxxxxxb  xxxxxx zeros to jump
            - 01xxxxxxb  xxxxxx Copy Pix
            - 10xxxxxxb  xxxxxx Repeat Pix
            - BYTE datas[]
*/

#include <SVGA/CALCMASK.H>
#include <stdio.h>

S32 CalcGraphMsk(S32 numbrick, U8 *bankbrick, U8 *ptmask)
{
  U8 *srcData = bankbrick + *((U32 *)bankbrick + numbrick);
  U8 *dstData = ptmask;
  dstData[0] = srcData[0]; // Delta X
  dstData[1] = srcData[1]; // Delta Y
  dstData[2] = srcData[2]; // Hot X
  dstData[3] = srcData[3]; // Hot Y

  U8 nbLine = srcData[1]; // NbLine (Delta Y)

  srcData += 4;
  dstData += 4;

  do
  {
    // NextLine:
    // Init NbBlock for this line
    dstData[0] = 0;
    dstData[1] = 0;
    U8 nbBlock = srcData[0];
    U8 *nbBlockDst = dstData;
    srcData++;
    dstData++; // Jump NbBlockDst

    // Flag:
    //   if flag=0   => Code "Jump"
    //   if flag=1   => Code "Mask"
    // We still don't start a "Jump"
    U8 flag = 0;
    do
    {
      // SameLine:
      U8 opCode = *srcData;
      srcData++;
      U8 count = (opCode & 0x3F) + 1;
      if (opCode & 0x80)
      {
        // RepeatCol:
        if (!flag)
        {
          dstData[1] = 0;
          dstData++;
          nbBlockDst[0]++; // Increment Nb of blocks (because block change)
          flag = 1;
        }
        // @@Same_Block2:
        *dstData += count;
        ++srcData;
      }
      else if (opCode & 0x40)
      {
        if (!flag)
        {
          dstData[1] = 0;
          dstData++;
          nbBlockDst[0]++;
          flag = 1;
        }
        *dstData += count;
        srcData += count;
      }
      else
      {
        if (flag)
        {
          ++dstData;
          flag = 0;
          nbBlockDst[0]++; // Increment Nb of blocks (because block change)
          *dstData = 0;
        }
        *dstData += count;
      }
      --nbBlock;
    } while (nbBlock);
    dstData++;
    nbBlockDst[0]++;
    nbLine--;
  } while (nbLine);

  return dstData - ptmask;
}
