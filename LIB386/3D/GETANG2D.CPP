#include <3D/GETANG2D.H>
#include <3D/TANTAB.H>
#include <stdio.h>

S32 GetAngleVector2D(S32 x, S32 z)
{
	/*
;S32 	GetAngleVector2D( S32 x, S32 z)			;

;#pragma aux GetAngleVector2D	"*"			\
;	parm		[eax] [ebx]	 		\
;	modify 		[ecx edx esi]

GetAngleVector2D 	PROC \
			uses ebx ecx edx edi esi ebp\
			x: DWORD, z: DWORD
			mov eax, x
			mov ebx, z

		xor	esi, esi		; ESI = Bit field

		test	eax, eax
		jz	exit0

		jns	ok1

		neg	eax
		or	esi, 1			; ESI : bit 0 = 1 if x coord is negative
ok1:
		test	ebx, ebx
		jz	exit1

		jns	ok2

		neg	ebx
		or	esi, 2			; ESI : bit 1 = 1 if z coord is negative
ok2:
		cmp	eax, ebx
		jbe	noswap

		mov	edx, eax
		mov	eax, ebx

		mov	ebx, edx
		or	esi, 4			; ESI : bit 2 = 1 if |z|>|x|
noswap:
		mov	ecx, ebx 		; ECX = major
		mov	edx, eax		; EDX = minor

		shl	eax, 30

		shr	ecx, 1

		shr	edx, 2
		add	eax, ecx

		adc	edx, 0

		div	ebx			; eax = 1.1.30 = tan(ß)

		mov	ebx, ((0+512)/2)*4	; = 0*2+512*2
		xor	edx, edx

		mov     ecx, 512*2
		cmp	eax, ds:TanTab[ebx]
		jge	ga3
ga0:
		shr	ebx, 1

		mov	ecx, ebx
		add	ebx, edx
ga2:
		test	ebx, 2
		jnz	angle

		cmp	eax, ds:TanTab[ebx]
		jl	ga0
ga3:
		shr	ebx, 1

		mov	edx, ebx
		add	ebx, ecx

		jmp	ga2
angle:
		mov	ebx, ds:TanTab[edx*2]
		mov	ecx, ds:TanTab[edx*2+4]

		shr	edx, 1
		sub	ecx, eax ; ECX = next - tangent

		sub	eax, ebx ; EAX = tangent - prev

		cmp	eax, ecx
		jbe	ok

		inc	edx
ok:
		mov	eax, edx
		test	esi, 4

		jz	no2

		mov	eax, 1024
		sub	eax, edx
no2:
		mov	edx, eax
		test	esi, 2

		jz	no1

		mov	eax, 2048
		sub	eax, edx
no1:
		test	esi, 1
		jz	no0

		neg	eax
no0:
		and	eax, 4095
		ret
exit0:
		xor	eax, eax
		test	ebx, ebx

		jns	no4

		mov	eax, 2048
no4:
		ret
exit1:
		mov	eax, 1024
		test	esi, 1

		jz	no3

		mov	eax, 4096-1024
no3:
		ret

GetAngleVector2D endp
	*/
	U8 isXNegative = FALSE;
	U8 isZNegative = FALSE;
	U8 isXZSwapped = FALSE;

	if (x == 0)
	{
		return z < 0 ? 2048 : 0;
	}

	if (x < 0)
	{
		x = -x;
		isXNegative = TRUE;
	}

	if (z == 0)
	{
		return isXNegative ? 4096 - 1024 : 1024;
	}

	if (z < 0)
	{
		z = -z;
		isZNegative = TRUE;
	}

	// This is wrong in the ASM comment, which says |z|>|x|, but the code
	// actually swaps if |x|>|z|
	if (x > z)
	{
		S32 tmp = x;
		x = z;
		z = tmp;
		isXZSwapped = TRUE;
	}

	S32 major = z;
	S32 minor = x;

	x <<= 30;
	major >>= 1;
	minor >>= 2;
	x += major;

	U64 tangent = (((U64)x + major) + ((U64)minor << 32)) / z;


	// Binary search tangent in TanTab. The algorithm is a bit weird but
	// it's what the original implementation in ASM did.
	int i = 256;
	int lastIDecreasing = i;
	int lastIIncreasing = 0;
	U8 found = FALSE;
	while (!found)
	{
		while(tangent < TanTab[i])
		{
			i >>= 1;
			lastIDecreasing = i;
			i += lastIIncreasing;

			if ( i % 2 != 0)
			{
				found = TRUE;
				break;
			}
		}

		if (found)
		{
			break;
		}

		while(tangent >= TanTab[i])
		{
			i >>= 1;
			lastIIncreasing = i;
			i += lastIDecreasing;

			if ( i % 2 != 0)
			{
				found = TRUE;
				break;
			}
		}
	}

	S32 currentTangent = TanTab[i];
	S32 nextTangent = TanTab[i + 1];

	S32 currentDiff = tangent - currentTangent;
	S32 nextDiff = nextTangent - tangent;

	if (currentDiff > nextDiff)
	{
		++i;
	}

	if (isXZSwapped)
	{
		i = 1024 - i;
	}

	if (isZNegative)
	{
		i = 2048 - i;
	}

	if (isXNegative)
	{
		i = -i;
	}

	return i & 4095;
}
