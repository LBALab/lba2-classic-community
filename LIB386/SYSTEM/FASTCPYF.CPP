
#include <SYSTEM/FASTCPY.H>
#include <string.h>
#include <SYSTEM/DEBUG_TRACES.H>

static void copy_qword_via_x87(U8 *dst, const U8 *src)
{
	double in_bits;
	volatile double fp_value;
	double out_bits;

	memcpy(&in_bits, src, 8);
	fp_value = in_bits;
	out_bits = fp_value;
	memcpy(dst, &out_bits, 8);
}

void FastCopy(void *dst, void *src, U32 len)
{
	LIB386_TRACE_CPP("FastCopyF", "1", "dst=%p src=%p len=%u", dst, src, len);
	U8 *dst_bytes = (U8 *)dst;
	const U8 *src_bytes = (const U8 *)src;
	U32 tail_bytes = len & 7u;
	U32 block_count = len >> 4;

	if (len < 8u) {
		memcpy(dst_bytes, src_bytes, len);
		return;
	}

	if ((len & 8u) != 0u) {
		copy_qword_via_x87(dst_bytes, src_bytes);
		dst_bytes += 8;
		src_bytes += 8;
	}

	while (block_count-- != 0u) {
		copy_qword_via_x87(dst_bytes, src_bytes);
		copy_qword_via_x87(dst_bytes + 8, src_bytes + 8);
		dst_bytes += 16;
		src_bytes += 16;
	}

	if (tail_bytes != 0u) {
		memcpy(dst_bytes, src_bytes, tail_bytes);
	}
}
