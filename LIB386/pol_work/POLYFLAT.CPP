#include <POLYGON/POLYFLAT.H>
#include <POLYGON/POLY.H>

#include <stdio.h>

S32 Filler_Flat(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  U32 count = nbLines + 1;
  U8 *curOffLine = (U8 *)Fill_CurOffLine;
  Fill_CurY += count;
  U32 patch = Fill_Patch - 1;

  if (patch == 0)
  {
    Fill_Patch = patch;
  }
  else
  {
    // @@Each_Line:
    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
  }

  while (1)
  {
    // @@EnterLoop:
    U8 *screen = (fillCurXMin >> 16) + curOffLine;
    U32 diffX = (fillCurXMax >> 16) - (fillCurXMin >> 16);
    if ((fillCurXMax >> 16) > (fillCurXMin >> 16))
    {
      // Is the scanline big enough to load the dst lines in cache
      // @@SmallScanLine:
      U8 color = Fill_Color; // Color
      do
      {
        *screen = color;
        screen++;
        --diffX;
      } while (diffX);
    }
    // @@EndScanLine:
    if (!--count)
      break;

    curOffLine += ScreenPitch;
    fillCurXMin += Fill_LeftSlope;
    fillCurXMax += Fill_RightSlope;
  }

  Fill_CurXMin = fillCurXMin;
  Fill_CurXMax = fillCurXMax;
  Fill_CurOffLine = curOffLine;

  return Triangle_ReadNextEdge(NULL, NULL);
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Transparent	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	esi,[Fill_LeftSlope]
			mov	eax,[Fill_RightSlope]

			add	ebx,esi
			add	edx,eax

			add	edi,[ScreenPitch]
@@EnterLoop:
			mov	esi,ebx
			mov	ecx,edx

			shr	esi,16
			push	edi

			shr	ecx,16
			add	edi,esi

			sub	ecx,esi
			jle	@@EndScanLine

			mov	ah,byte ptr [Fill_Color]; Color
@@LoopX:
			mov	al,[edi]

			and	al,0Fh

			or	al,ah

			mov	[edi],al
			inc	edi

			dec	ecx
			jne	@@LoopX

@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			jmp	Triangle_ReadNextEdge
Filler_Transparent	ENDP



; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_Trame		PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_Trame_Parity]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

				; *** Load dst in WB cache ***
			push	edi
			push	edx

			lea	edx,[edi+ecx]
			and	edi,not 31

			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edx
			pop	edi
@@SmallScanLine:
			shr	ecx,1
			je	@@EndScanLine

			xor	esi,1
			mov	eax,edi

			and	eax,1
			inc	ecx

			xor	eax,esi

			mov	eax,[Fill_Color]
			je	@@Pair

			inc	edi
@@Pair:
			shr	ecx,1
			jnc	@@Odd
@@Each_X:
			mov	[edi],al
			add	edi,2
@@Odd:
			mov	[edi],al
			add	edi,2

			dec	ecx
			jne	@@Each_X


@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			xor	esi,1

			mov	[Fill_Trame_Parity],esi
			jmp	Triangle_ReadNextEdge
Filler_Trame		ENDP
*/

// *** CALL: 	ECX = Nb lines to draw
//		EBX = Fill_CurXMin
//		EDX = Fill_CurXMax
S32 Filler_FlatZBuf(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TransparentZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			dec	eax
			mov	esi,[Fill_CurZBufMin]

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog1],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebx
			push	edx

			push	esi
			push	ebp

			mov	eax,[Log]
			mov	edx,esi

			shr	edx,8
			sub	edi,eax

			shl	esi,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	ebp,ebx
			mov	ah,byte ptr [Fill_Color]; Color

			shr	ebx,8

			shl	ebp,32-8
@@LoopX:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push eax
		mov eax,[PtrZBuffer1]
		mov	[edi*2+eax],dx
		pop eax

		push ebx
		mov ebx,[PtrLog]
		mov	al,[edi+ebx]
		pop ebx

			and	al,0Fh

			or	al,ah

		push ebx
		mov ebx,[PtrLog1]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:		inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	esi

			pop	edx
			pop	ebx
@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurZBufMin],esi
			jmp	Triangle_ReadNextEdge
Filler_TransparentZBuf	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TrameZBuf	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_Trame_Parity]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer1],eax

			mov	[PtrZBuffer2],eax
			mov	[PtrZBuffer3],eax

			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog2],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			push	ebx

			mov	ebx,[Fill_CurZBufMin]
			mov	eax,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

				; *** Load dst in WB cache ***
			push	edi
			push	edx

			lea	edx,[edi+ecx]
			and	edi,not 31

			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edx
			pop	edi
@@SmallScanLine:
			shr	ecx,1
			je	@@EndScanLine

			xor	esi,1
			mov	eax,edi

			and	eax,1
			inc	ecx

			xor	eax,esi
			je	@@Pair

			inc	edi
@@Pair:
			push	esi
			push	ebx

			push	edx
			push	ebp

			mov	esi,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_XSlope]

			add	ebx,ebx
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	edx,8
			sub	edi,eax

			shr	ebx,8
			mov	eax,[Fill_Color]

			shr	ecx,1
			jnc	@@Odd
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp0

	push eax
	mov eax,[PtrZBuffer1]
	mov	[edi*2+eax],dx
	pop eax

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp0:		add	edi,2
			add	esi,ebp

			adc	edx,ebx
@@Odd:
	push eax
	mov eax,[PtrZBuffer2]
	cmp	[edi*2+eax],dx
	pop eax

			jb	@@NoDisp2

	push eax
	mov eax,[PtrZBuffer3]
	mov	[edi*2+eax],dx
	pop eax

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp2:		add	edi,2

			dec	ecx
			jne	@@Each_X

			pop	ebp
			pop	edx

			pop	ebx
			pop	esi

@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			xor	esi,1

			mov	[Fill_Trame_Parity],esi
			jmp	Triangle_ReadNextEdge
Filler_TrameZBuf	ENDP
*/

// *** CALL: 	ECX = Nb lines to draw
//		EBX = Fill_CurXMin
//		EDX = Fill_CurXMax
S32 Filler_FlatNZW(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax)
{
  // TODO: Implement ASM
  printf("ASM FUNCTION %s (%s:%d)\n", __func__, __FILE__, __LINE__);
  return 0;
}

/*
; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TransparentNZW	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			dec	eax
			mov	esi,[Fill_CurZBufMin]

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog1],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	ebx
			push	edx

			push	esi
			push	ebp

			mov	eax,[Log]
			mov	edx,esi

			shr	edx,8
			sub	edi,eax

			shl	esi,32-8
			mov	ebx,[Fill_ZBuf_XSlope]

			mov	ebp,ebx
			mov	ah,byte ptr [Fill_Color]; Color

			shr	ebx,8

			shl	ebp,32-8
@@LoopX:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp

		push ebx
		mov ebx,[PtrLog]
		mov	al,[edi+ebx]
		pop ebx

			and	al,0Fh

			or	al,ah


		push ebx
		mov ebx,[PtrLog1]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp:		inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	esi

			pop	edx
			pop	ebx
@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			mov	[Fill_CurZBufMin],esi
			jmp	Triangle_ReadNextEdge
Filler_TransparentNZW	ENDP





; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_TrameNZW		PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_Trame_Parity]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			mov	[PtrZBuffer2],eax

			mov	eax,[Log]

			mov	[PtrLog],eax
			mov	[PtrLog2],eax

			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			push	ebx

			mov	ebx,[Fill_CurZBufMin]
			mov	eax,[Fill_ZBuf_LeftSlope]

			add	eax,ebx
			pop	ebx

			mov	[Fill_CurZBufMin],eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine


			mov	al,[edi]	; Load 1st dst cache line
			cmp	ecx,32		; Is the scanline big enough to load the dst lines in cache

			jb	@@SmallScanLine

				; *** Load dst in WB cache ***
			push	edi
			push	edx

			lea	edx,[edi+ecx]
			and	edi,not 31

			and	edx,not 31	; AGI on EDI on first loop
@@Preload:
			mov	ah,[edi]
			add	edi,32

			cmp	edi,edx
			jbe	@@Preload

			pop	edx
			pop	edi
@@SmallScanLine:
			shr	ecx,1
			je	@@EndScanLine

			xor	esi,1
			mov	eax,edi

			and	eax,1
			inc	ecx

			xor	eax,esi
			je	@@Pair

			inc	edi
@@Pair:
			push	esi
			push	ebx

			push	edx
			push	ebp

			mov	esi,[Fill_CurZBufMin]
			mov	ebx,[Fill_ZBuf_XSlope]

			add	ebx,ebx
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	edx,8
			sub	edi,eax

			shr	ebx,8
			mov	eax,[Fill_Color]

			shr	ecx,1
			jnc	@@Odd
@@Each_X:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp0

		push ebx
		mov ebx,[PtrLog]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp0:		add	edi,2
			add	esi,ebp

			adc	edx,ebx
@@Odd:
	push eax
	mov eax,[PtrZBuffer2]
	cmp	[edi*2+eax],dx
	pop eax

			jb	@@NoDisp2

		push ebx
		mov ebx,[PtrLog2]
		mov	[edi+ebx],al
		pop ebx

@@NoDisp2:		add	edi,2

			dec	ecx
			jne	@@Each_X

			pop	ebp
			pop	edx

			pop	ebx
			pop	esi

@@EndScanLine:
			pop	edi
			dec	ebp

			jne	@@Each_Line

			mov	[Fill_CurXMin],ebx
			mov	[Fill_CurXMax],edx

			mov	[Fill_CurOffLine],edi
			xor	esi,1

			mov	[Fill_Trame_Parity],esi
			jmp	Triangle_ReadNextEdge
Filler_TrameNZW		ENDP






; *** CALL: 	ECX = Nb lines to draw
;		EBX = Fill_CurXMin
;		EDX = Fill_CurXMax
			ALIGN	4
Filler_FlagZBuffer	PROC
			lea	ebp,[ecx+1]
			mov	eax,[Fill_CurY]

			mov	edi,[Fill_CurOffLine]
			add	eax,ebp

			mov	[Fill_CurY],eax
			mov	eax,[Fill_Patch]

			mov	esi,[Fill_CurZBufMin]
			dec	eax

			jne	@@Each_Line

			mov	[Fill_Patch],eax
			mov	eax,[PtrZBuffer]

			mov	[PtrZBufferPatch],eax
			jmp	@@EnterLoop
			ALIGN	4
@@Each_Line:
			mov	eax,[Fill_LeftSlope]
			add	edi,[ScreenPitch]

			add	ebx,eax
			mov	eax,[Fill_RightSlope]

			add	edx,eax
			mov	eax,[Fill_ZBuf_LeftSlope]

			add	esi,eax
@@EnterLoop:
			mov	eax,ebx
			mov	ecx,edx

			shr	eax,16
			push	edi

			shr	ecx,16
			add	edi,eax

			sub	ecx,eax
			jle	@@EndScanLine

			push	edx


				; *** Flat loop ***
			push	esi
			push	ebx

			mov	ebx,[Fill_ZBuf_XSlope]

			push	ebp
			mov	edx,esi

			shl	esi,32-8
			mov	ebp,ebx

			shl	ebp,32-8
			mov	eax,[Log]

			shr	ebx,8
			sub	edi,eax

			shr	edx,8
			xor	eax,eax
@@LoopX:
		push eax
		mov eax,[PtrZBufferPatch]
		cmp	[edi*2+eax],dx
		pop eax

			jb	@@NoDisp
			mov	[IsPolygonHidden],eax
@@NoDisp:
			inc	edi
			add	esi,ebp

			adc	edx,ebx
			dec	ecx

			jne	@@LoopX

			pop	ebp
			pop	ebx

			pop	esi
			pop	edx
@@EndScanLine:
			pop	edi

			dec	ebp
			jne	@@Each_Line

			mov	[Fill_CurZBufMin],esi
			mov	[Fill_CurXMin],ebx

			mov	[Fill_CurXMax],edx
			mov	[Fill_CurOffLine],edi

			jmp	Triangle_ReadNextEdge
Filler_FlagZBuffer	ENDP

_TEXT			ENDS

;			The
			End
*/
