#include <3D/GETANG2D.H>

#include <3D/TANTAB.H>

S32 GetAngleVector2D(S32 x, S32 z)
{
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
