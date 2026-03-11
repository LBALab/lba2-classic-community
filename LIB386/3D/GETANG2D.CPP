#include <3D/GETANG2D.H>

#include <3D/TANTAB.H>

/*
 * Binary search for tangent in TanTab[0..511].
 *
 * The ASM tracks positions as byte offsets (4 bytes per entry) and halves
 * them with SHR, terminating when bit 1 of the byte offset is set.  We
 * replicate this with an integer "step" that starts at 1024 and is halved
 * each iteration; the search ends when step < 4.
 */
static S32 TanTabSearch(U32 tangent)
{
	S32 lo = 0;          /* lower bound  (byte offset) */
	S32 hi = 1024;       /* upper bound  (byte offset) */
	S32 mid = 1024;      /* probe point  (byte offset) */
	S32 step;

	for (;;)
	{
		if (tangent >= (U32)TanTab[mid / 4])
		{
			/* tangent is above — move lower bound up */
			step = mid >> 1;
			lo = step;
			mid = step + hi;
		}
		else
		{
			/* tangent is below — move upper bound down */
			step = mid >> 1;
			hi = step;
			mid = step + lo;
		}

		if (mid & 2)
			break;
	}

	/* lo holds the byte-offset of the winning bracket; convert to index */
	S32 idx = lo >> 1;
	U32 cur  = TanTab[idx];
	U32 next = TanTab[idx + 1];
	if (tangent - cur > next - tangent)
		idx++;

	return idx;
}

S32 GetAngleVector2D(S32 x, S32 z)
{
	U32 flags = 0;   /* bit 0 = negated x, bit 1 = negated z, bit 2 = swapped */

	if (x == 0)
		return z < 0 ? 2048 : 0;

	if (x < 0) { x = -x; flags |= 1; }

	if (z == 0)
		return (flags & 1) ? 3072 : 1024;

	if (z < 0) { z = -z; flags |= 2; }

	if ((U32)x > (U32)z)
	{
		S32 tmp = x; x = z; z = tmp;
		flags |= 4;
	}

	/* 64-bit tangent: edx:eax = (x>>2):(x<<30 + z>>1), then DIV z */
	U32 lo = ((U32)x << 30) + ((U32)z >> 1);
	U32 hi = (U32)x >> 2;
	if (lo < (U32)z >> 1)   /* carry from the addition */
		hi++;
	U32 tangent = (U32)(((U64)hi << 32 | lo) / (U32)z);

	S32 angle = TanTabSearch(tangent);

	if (flags & 4) angle = 1024 - angle;
	if (flags & 2) angle = 2048 - angle;
	if (flags & 1) angle = -angle;

	return angle & 4095;
}
