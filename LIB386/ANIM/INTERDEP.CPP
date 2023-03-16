/*

		.686p
		.XMM

		.MODEL	SMALL, C

		include	AFF_OBJ.INC

;*──────────────────────────────────────────────────────────────────────────*
		.DATA

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	X0			:DWORD
		EXTRN	C	Y0			:DWORD
		EXTRN	C	Z0			:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	MatrixLib2		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	InitMatrixStd		:DWORD
		EXTRN	C	RotatePoint		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TimerRefHR		:DWORD

		ASSUME  DS:SEG TimerRefHR

;*──────────────────────────────────────────────────────────────────────────*
		EXTRN	C	TransFctAnim		:DWORD

;*──────────────────────────────────────────────────────────────────────────*
		.CODE

;*──────────────────────────────────────────────────────────────────────────*
		PUBLIC	C	ObjectSetInterDep

;*══════════════════════════════════════════════════════════════════════════*
*/
#include <ANIM/INTERDEP.H>

#include <SYSTEM/TIMER.H>
#include <ANIM.H>

S32	ObjectSetInterDep(T_OBJ_3D *obj)
// int __usercall __spoils<ecx> ObjectSetInterDep@<eax>(int a1@<ebp>, int a2@<edi>)
{
	/*
;void	ObjectSetInterDep(T_OBJ *obj)

;#pragma aux ObjectSetInterDep			\
;	parm		[ebx]			\
;	modify exact 	[eax ecx edx esi edi]

ObjectSetInterDep PROC \
			uses ebx ecx edx edi esi ebp\
			obj: DWORD
			mov ebx, obj

		mov	eax, [ebx].OBJ_3D.NextTexture
		mov	ecx, [ebx].OBJ_3D.NextBody

		mov	[ebx].OBJ_3D.Texture, eax
		mov	eax, [TimerRefHR]

		mov	[ebx].OBJ_3D.Body, ecx
		mov	edx, [ebx].OBJ_3D.ObjTime		; same time as before?

		cmp	eax,edx
		je	exit1
		jb	fixerror
forcerecomp:
		mov	[ebx].OBJ_3D.ObjTime, eax		; upgrade time

		mov	esi, [ebx].OBJ_3D.Anim
		mov	eax, [TransFctAnim]

		test	eax, eax
		jz	skiptrans

		push	esi
		call	eax
		mov		esi, eax
		add		esp,4
skiptrans:
		push	esi
		mov	eax, [ebx].OBJ_3D.ObjTime

		mov	edx, [ebx].OBJ_3D.NextOfsFrame
		add	esi, edx

		xor	edx, edx
		mov	ecx, [ebx].OBJ_3D.NextTimer

		mov	ebp, [ebx].OBJ_3D.LastTimer

		cmp	eax, ecx
		jae	timesup

		mov	[ebx].OBJ_3D.Status, STATUS_CHANGE

		sub	ecx, ebp		; ecx = nb steps
		sub	eax, ebp		; eax = delta steps
		mov	ebp, ecx
		inc	ecx
		shl	eax, 16			; delta steps  (1.15.16)
		shr	ecx, 1			; nb steps / 2
		add	eax, ecx		; to get nearest neighbour
		div	ebp			; delta steps / nb steps
		mov	ebp, eax		; factor (1.15.16)

returntime:
		mov	ecx, [ebx].OBJ_3D.Master	; Master
		mov	eax, [ebx].OBJ_3D.ObjAlpha
		mov	edx, [ebx].OBJ_3D.ObjBeta
		test	ecx, 1			; MasterRot ?
		jnz	animmaster
		mov	ecx, [ebx].OBJ_3D.ObjGamma
returnmaster:

		push	ebx
		push	esi

		mov	ebx, edx
		mov	edi, offset MatrixLib2

		call	[InitMatrixStd]

		pop	esi
		pop	ebx

		movsx	edx, word ptr[esi+2]	; X		(17.15.0)
		movsx	edi, word ptr[esi+4]	; Y
		movsx	eax, word ptr[esi+6]	; Z
		imul	edx, ebp		; result	(1.15.16)
		imul	edi, ebp
		sar	edx, 16			; result	(17.15.0)
		imul	eax, ebp
		mov	ecx, edx
		sar	edi, 16
		sub	ecx, [ebx].OBJ_3D.LastAnimStepX
		mov	[ebx].OBJ_3D.LastAnimStepX, edx; X
		push	ecx
		sar	eax, 16
		mov	edx, edi
		mov	ecx, eax
		sub	edx, [ebx].OBJ_3D.LastAnimStepY
		sub	ecx, [ebx].OBJ_3D.LastAnimStepZ
		mov	[ebx].OBJ_3D.LastAnimStepY, edi; Y
		mov	[ebx].OBJ_3D.LastAnimStepZ, eax; Z
		mov	[ebx].OBJ_3D.Interpolator, ebp	; interpolator

		pop	eax
		mov	esi, offset MatrixLib2
		mov	ebp, ebx
		mov	ebx, edx

		call	[RotatePoint]

		mov	ebx, ebp

		mov	eax, [X0]
		mov	edx, [Y0]
		mov	ecx, [Z0]
		add	eax, [ebx].OBJ_3D.ObjX
		add	edx, [ebx].OBJ_3D.ObjY
		add	ecx, [ebx].OBJ_3D.ObjZ
		mov	[ebx].OBJ_3D.ObjX, eax
		mov	eax, [ebx].OBJ_3D.Status
		mov	[ebx].OBJ_3D.ObjY, edx
		cmp	eax, STATUS_CHANGE
		pop	ebp
		mov	[ebx].OBJ_3D.ObjZ, ecx
		jne	frame

		ret
timesup:
		mov	ebp, 10000h
		mov	[ebx].OBJ_3D.Status, STATUS_FRAME+STATUS_CHANGE
		jmp	returntime
animmaster:
		mov	dx, [esi+10]		; Alpha
		shl	edx, 20
		mov	di, [esi+12]		; Beta
		shl	edi, 20
		mov	ax, [esi+14]		; Gamma
		shl	eax, 20
		sar	edx, 20			; data		(17.15.0)
		sar	edi, 20
		sar	eax, 20
		imul	edx, ebp		; result	(1.15.16)
		imul	edi, ebp
		sar	edx, 16			; result	(17.15.0)
		mov	ecx, [ebx].OBJ_3D.ObjAlpha
		imul	eax, ebp
		sar	edi, 16
		add	ecx, edx
		sub	ecx, [ebx].OBJ_3D.LastAnimStepAlpha
		mov	[ebx].OBJ_3D.LastAnimStepAlpha, edx	; Alpha
		and	ecx, 4095
		mov	edx, [ebx].OBJ_3D.ObjBeta
		mov	[ebx].OBJ_3D.ObjAlpha, ecx
		push	ecx
		add	edx, edi
		sar	eax, 16
		mov	ecx, [ebx].OBJ_3D.ObjGamma
		sub	edx, [ebx].OBJ_3D.LastAnimStepBeta
		add	ecx, eax
		and	edx, 4095
		mov	[ebx].OBJ_3D.LastAnimStepBeta, edi	; Beta
		sub	ecx, [ebx].OBJ_3D.LastAnimStepGamma
		mov	[ebx].OBJ_3D.LastAnimStepGamma, eax	; Gamma
		and	ecx, 4095
		mov	[ebx].OBJ_3D.ObjBeta, edx
		mov	[ebx].OBJ_3D.ObjGamma, ecx
		pop	eax
		jmp	returnmaster

		mov	eax, [edi+4]		; next

		and	eax, 4095
		jz      optequal1

		cmp     eax, 2048
		jle     ok1

		sub     eax, 4096
ok1:
		imul    eax, ebx
		sar	eax, 16
		add     edx, eax
		and	edx, 4095
optequal1:

frame:
		xor	eax, eax
		mov	[ebx].OBJ_3D.Interpolator, eax

		mov	[ebx].OBJ_3D.LastAnimStepX, eax; struct -> stepX = 0
		mov	[ebx].OBJ_3D.LastAnimStepY, eax; struct -> stepY = 0
		mov	[ebx].OBJ_3D.LastAnimStepZ, eax; struct -> stepZ = 0

		mov	[ebx].OBJ_3D.LastAnimStepAlpha, eax ; struct -> stepAlpha = 0
		mov	[ebx].OBJ_3D.LastAnimStepBeta, eax  ; struct -> stepBeta  = 0
		mov	[ebx].OBJ_3D.LastAnimStepGamma, eax ; struct -> stepGamma = 0

		mov	edx, [ebx].OBJ_3D.NextFrame
		mov	esi, [ebx].OBJ_3D.NextOfsFrame
		mov	[ebx].OBJ_3D.LastFrame, edx
		mov	[ebx].OBJ_3D.LastOfsFrame, esi
		mov	[ebx].OBJ_3D.LastOfsIsPtr, eax

		inc	edx			; next frame

		mov	eax, [ebx].OBJ_3D.NextNbGroups	; nb groups
		mov	[ebx].OBJ_3D.LastNbGroups, eax	; LastNbGroups
		mov	[ebx].OBJ_3D.NbGroups, eax	; nb groups

		lea	ecx, [eax*2-2]		; 2 dwords per group, no group 0
		cmp	edx, [ebx].OBJ_3D.NbFrames
		jge	lframe

		lea	eax, [esi+ecx*4+8+8]
backframe:
		add	esi, 16			; esi -> group 1
		mov	[ebx].OBJ_3D.NextOfsFrame, eax
		add	eax, ebp
		add	esi, ebp
		mov     edi, [ebx].OBJ_3D.ObjTime
		movzx	ebp, word ptr[eax+8]
		movzx	eax, word ptr[eax]
		mov	[ebx].OBJ_3D.Master, ebp
		mov	[ebx].OBJ_3D.NextFrame, edx
		mov	[ebx].OBJ_3D.LastTimer, edi	; this time
		add	eax, edi
		lea	edi, [ebx].OBJ_3D.CurrentFrame
		mov	[ebx].OBJ_3D.NextTimer, eax	; next time
		mov	eax, [ebx].OBJ_3D.Status

        ; FIXME UASM refuses to compile "rep movsd" without 686 architecture and XMM
        ;   is it confusing it with some other instruction? Is it not supported by 486?
		rep	movsd			; copy frame

		ret				; ok, return
lframe:
		mov	edx, [ebx].OBJ_3D.LoopFrame
		mov	eax, [ebx].OBJ_3D.LoopOfsFrame
		mov	[ebx].OBJ_3D.Status, STATUS_FRAME+STATUS_LAST+STATUS_CHANGE
		jmp	backframe
exit1:
		test	[ebx].OBJ_3D.Status, STATUS_BODY
		jnz	forcerecomp
		jmp	exit0
fixerror:
		mov	edx, [ebx].OBJ_3D.ObjTime
		mov	[ebx].OBJ_3D.ObjTime, eax
		sub	edx, eax
		mov	eax, [ebx].OBJ_3D.NextTimer
		mov	ecx, [ebx].OBJ_3D.LastTimer
		sub	eax, edx
		sub	ecx, edx
		mov	[ebx].OBJ_3D.NextTimer, eax
		mov	[ebx].OBJ_3D.LastTimer, ecx
exit0:
		xor	eax, eax
		mov	[ebx].OBJ_3D.Status, eax	; didn't do anything
		ret

ObjectSetInterDep endp
	*/
  int v2; // ebx@1
  int v3; // ecx@1
  unsigned int v4; // eax@1
  unsigned int v5; // edx@1
  int v6; // esi@3
  unsigned int v7; // eax@5
  int v8; // esi@5
  unsigned int v9; // ecx@5
  int v10; // ebp@5
  unsigned int v11; // ebp@6
  int v12; // eax@7
  int v13; // ecx@8
  signed int v14; // eax@9
  int v15; // edi@9
  int v16; // ecx@9
  int v17; // edx@9
  int v18; // ecx@9
  int v19; // edx@9
  int v20; // ecx@9
  int result; // eax@9
  __int16 v22; // dx@12
  int v23; // edx@12
  int v24; // edi@12
  int v25; // ecx@12
  int v26; // edx@12
  int v27; // eax@12
  int v28; // ecx@12
  int v29; // edx@12
  int v30; // ecx@12
  int v31; // edx@13
  int v32; // esi@13
  int v33; // edx@13
  int v34; // eax@13
  int v35; // ecx@13
  int v36; // eax@14
  int v37; // eax@15
  int v38; // edi@15
  int v39; // ebp@15
  int v40; // eax@15
  int v41; // edx@19
  int v42; // edx@19
  int v43; // ecx@19
  int v44; // [sp-18h] [bp-1Ch]@5

  v2 = obj->NextTexture;
  v3 = obj->NextBody;

	obj->Texture = v2;
  v4 = TimerRefHR;
	obj->Body = v3;
  v5 = obj->Time;
	// Same time as before?
  if ( v4 == v5 )
  {
    if (obj->Status & FLAG_BODY)
		{
      goto LABEL_3;
		}
		obj->Status = 0;
    return obj->Status;
  }
  if ( v4 < v5 )
  {
    v41 = obj->Time;
		obj->Time = v4;
    v42 = v41 - v4;
    v43 = obj->LastTimer - v42;
    obj->NextTimer -= v42;
    obj->LastTimer = v43;
		obj->Status = 0;
    return obj->Status;
  }
LABEL_3:
	obj->Time = v4; // Upgrade time
	T_PTR_NUM anim = obj->Anim;

	if (TransFctAnim !== NULL)
	{
		anim.Ptr = TransFctAnim(anim.Num);
	}
  if ( TransFctAnim )
    v6 = TransFctAnim(*(_DWORD *)(v2 + 32));
  v44 = v6;
  v7 = *(_DWORD *)(v2 + 120);
  v8 = *(_DWORD *)(v2 + 68) + v6;
  v9 = *(_DWORD *)(v2 + 72);
  v10 = *(_DWORD *)(v2 + 56);
  if ( v7 >= v9 )
  {
    v11 = 0x10000;
    *(_DWORD *)(v2 + 124) = 5;
  }
  else
  {
    *(_DWORD *)(v2 + 124) = 4;
    v11 = (((v9 - v10 + 1) >> 1) + ((v7 - v10) << 16)) / (v9 - v10);
  }
  v12 = *(_DWORD *)(v2 + 12);
  if ( *(_DWORD *)(v2 + 128) & 1 )
  {
    v22 = *(_WORD *)(v8 + 10);
    LOWORD(a2) = *(_WORD *)(v8 + 12);
    LOWORD(v12) = *(_WORD *)(v8 + 14);
    v23 = (signed int)(v11 * (*(_DWORD *)(v2 + 16) << 20 >> 20)) >> 16;
    v24 = (signed int)(v11 * (a2 << 20 >> 20)) >> 16;
    v25 = v23 + *(_DWORD *)(v2 + 12) - *(_DWORD *)(v2 + 104);
    *(_DWORD *)(v2 + 104) = v23;
    v26 = *(_DWORD *)(v2 + 16);
    *(_DWORD *)(v2 + 12) = v25 & 0xFFF;
    v27 = (signed int)(v11 * (v12 << 20 >> 20)) >> 16;
    v28 = v27 + *(_DWORD *)(v2 + 20);
    v29 = (v24 + v26 - *(_DWORD *)(v2 + 108)) & 0xFFF;
    *(_DWORD *)(v2 + 108) = v24;
    v30 = v28 - *(_DWORD *)(v2 + 112);
    *(_DWORD *)(v2 + 112) = v27;
    v13 = v30 & 0xFFF;
    *(_DWORD *)(v2 + 16) = v29;
    *(_DWORD *)(v2 + 20) = v13;
  }
  else
  {
    v13 = *(_DWORD *)(v2 + 20);
  }
  InitMatrixStd(v13);
  v14 = v11 * *(_WORD *)(v8 + 6);
  v15 = (signed int)(v11 * *(_WORD *)(v8 + 4)) >> 16;
  v16 = ((signed int)(v11 * *(_WORD *)(v8 + 2)) >> 16) - *(_DWORD *)(v2 + 92);
  *(_DWORD *)(v2 + 92) = (signed int)(v11 * *(_WORD *)(v8 + 2)) >> 16;
  v14 >>= 16;
  v17 = v15 - *(_DWORD *)(v2 + 96);
  v18 = v14 - *(_DWORD *)(v2 + 100);
  *(_DWORD *)(v2 + 96) = v15;
  *(_DWORD *)(v2 + 100) = v14;
  *(_DWORD *)(v2 + 116) = v11;
  RotatePoint(v18);
  v19 = *(_DWORD *)(v2 + 4) + Y0;
  v20 = *(_DWORD *)(v2 + 8) + Z0;
  *(_DWORD *)v2 += X0;
  result = *(_DWORD *)(v2 + 124);
  *(_DWORD *)(v2 + 4) = v19;
  *(_DWORD *)(v2 + 8) = v20;
  if ( result != 4 )
  {
    *(_DWORD *)(v2 + 116) = 0;
    *(_DWORD *)(v2 + 92) = 0;
    *(_DWORD *)(v2 + 96) = 0;
    *(_DWORD *)(v2 + 100) = 0;
    *(_DWORD *)(v2 + 104) = 0;
    *(_DWORD *)(v2 + 108) = 0;
    *(_DWORD *)(v2 + 112) = 0;
    v31 = *(_DWORD *)(v2 + 64);
    v32 = *(_DWORD *)(v2 + 68);
    *(_DWORD *)(v2 + 48) = v31;
    *(_DWORD *)(v2 + 52) = v32;
    *(_DWORD *)(v2 + 44) = 0;
    v33 = v31 + 1;
    v34 = *(_DWORD *)(v2 + 76);
    *(_DWORD *)(v2 + 60) = v34;
    *(_DWORD *)(v2 + 132) = v34;
    v35 = 2 * v34 - 2;
    if ( v33 >= *(_DWORD *)(v2 + 88) )
    {
      v33 = *(_DWORD *)(v2 + 80);
      v36 = *(_DWORD *)(v2 + 84);
      *(_DWORD *)(v2 + 124) = 7;
    }
    else
    {
      v36 = v32 + 4 * v35 + 16;
    }
    *(_DWORD *)(v2 + 68) = v36;
    v37 = v44 + v36;
    v38 = *(_DWORD *)(v2 + 120);
    v39 = *(_WORD *)(v37 + 8);
    v40 = *(_WORD *)v37;
    *(_DWORD *)(v2 + 128) = v39;
    *(_DWORD *)(v2 + 64) = v33;
    *(_DWORD *)(v2 + 56) = v38;
    *(_DWORD *)(v2 + 72) = v38 + v40;
    result = *(_DWORD *)(v2 + 124);
    qmemcpy((void *)(v2 + 136), (const void *)(v44 + v32 + 16), 4 * v35);
  }
  return result;
}