
#include <SYSTEM/FASTCPY.H>
#include <string.h>
#include <SYSTEM/DEBUG_TRACES.H>

void FastCopy(void *dst, void *src, U32 len)
{
	LIB386_TRACE_CPP("FastCopyF", "1", "dst=%p src=%p len=%u", dst, src, len);
	memcpy(dst, src, len);
}
