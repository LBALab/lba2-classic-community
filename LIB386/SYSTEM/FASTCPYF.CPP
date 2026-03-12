
#include <SYSTEM/FASTCPY.H>
#include <string.h>

void FastCopy(void *dst, void *src, U32 len)
{
	memcpy(dst, src, len);
}
