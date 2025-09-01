#include <SVGA/FONT.H>

#include <unistd.h>

void *PtrFont = NULL;
S32 InterLeave = 1;
S32 InterSpace = 10;

S32 SizeFont(char *str)
{
	U32 size = 0;
	while (*str != '\0')
	{
		U8 c = *str;
		if (c == 32)
		{
			size += InterSpace;
		}
		else
		{
			U32 charSize = *(U32 *)((U8 *)PtrFont + c * 4);
			size += InterLeave;
			size += *((U8 *)PtrFont + charSize);
		}
		str++;
	}

	return size;
}

S32 CarFont(S32 x, S32 y, U8 c)
{
	if (c == 32)
	{
		return InterSpace;
	}

	S32 charSize = *(S32 *)((U8 *)PtrFont + c * 4);
	S32 size = InterLeave + *((U8 *)PtrFont + charSize);
	AffMask(c, x, y, PtrFont);
	return size;
}

S32 Font(S32 x, S32 y, const char *str)
{
	if (PtrFont == NULL)
	{
		return 0;
	}

	while (*str != '\0')
	{
		U8 c = *str;
		if (c == 32)
		{
			x += InterSpace;
		}
		else
		{
			AffMask(c, x, y, PtrFont);
			S32 charSize = *(S32 *)((U8 *)PtrFont + c * 4);
			x += InterLeave;
			x += *((U8 *)PtrFont + charSize);
		}
		str++;
	}

	return 0; // TODO: make this void
}
