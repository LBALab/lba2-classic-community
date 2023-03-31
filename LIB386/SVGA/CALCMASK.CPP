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
  /*
;#pragma aux CalcGraphMsk	"*"	\
;	parm	[eax] [esi] [edi]	\
;	modify	[ebx ecx edx]

CalcGraphMsk			PROC \
      uses ebx ecx edx edi esi ebp\
      numbrick: DWORD, \
      bankbrick: DWORD, \
      ptmask: DWORD
      mov eax, numbrick
      mov esi, bankbrick
      mov edi, ptmask

      push	edi
      add	esi, [esi+eax*4]; ESI = Begin Data

;-----------------------------------------------
      mov	ebx, [esi]	; Get DX, DY, Hot X, Hot Y
      add	esi, 4		; bh = NbLine
      mov	[edi], ebx	; Stock DX, DY, Hot X, Hot Y
      add	edi, 4

      xor	ecx, ecx
;----------------------------------------------- Init NbBlock for this line
NextLine:
      mov	word ptr [edi],0000h
      mov	ebp, edi	; Sauve Pos NbBlockDst
      mov	bl, [esi]	; BL = Nb Block for this line
      inc	esi
      inc	edi		; Jump NbBlockDst
      xor	ah,ah		; AH = Flag :           si AH=0   => Code "Jump"
            ;			si AH=1   => Code "Mask"
            ; On commence toujours pas un "Jump"
SameLine:		mov	al, [esi]
      inc	esi   		; OpCode
      mov	cl, al          ; Sauve AL
      and	cl, 00111111b
      inc	cl		; One More Please...
;-----------------------------------------------
      test	al, 10000000b
      jne	RepeatCol
      test	al, 01000000b
      jne	CopyCol
;----------------------------------------------- 00 Jump CL Zero
      test	ah,ah		; Teste si AH = 1
      je	@@Same_Block1
      inc	edi
      xor	ah,ah
      inc	byte ptr [ebp]	; Incremente Nb de blocks (car chgt de block)
      mov	[edi],ah	; Ecrit zero a cause du ADD
@@Same_Block1:
      add	[edi], cl
      jmp	EndLoop
;----------------------------------------------- 01 Repeat Nb Col
RepeatCol:
      test	ah,ah
      jne	@@Same_Block2
      mov	byte ptr [edi+1],0	; Precaution avec le ADD
      inc	edi
      inc	byte ptr [ebp]
      mov	ah,1
@@Same_Block2:
      add	[edi], cl	; NbData += Nb
      inc	esi		; pts++
      jmp	EndLoop
;----------------------------------------------- 10 Copy Nb Col
CopyCol:
      test	ah,ah
      jne	@@Same_Block3
      mov	byte ptr [edi+1],0
      inc	edi
      inc	byte ptr [ebp]
      mov	ah,1
@@Same_Block3:
      add	[edi], cl	; NbData += Nb
      add	esi, ecx	; pts += nb
;----------------------------------------------- End Loop
EndLoop:		dec	bl		; Nb Block--
      jne	SameLine	; Continue Same Line
;----------------------------------------------- Cloture Eventuelle
      inc	edi
      inc	byte ptr [ebp]	; NbBlockDst++
;-----------------------
      dec	bh		; NbLine--
      jne	NextLine	; Next Line
;-----------------------
      pop	ebx
      mov	eax, edi

      sub	eax, ebx	; Size Mask
      ret
;-----------------------
CalcGraphMsk		endp
*/

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
