//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_HQR
#define LIB_SYSTEM_HQR

#include <SYSTEM/LE.H>

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
extern "C" {
#endif

//──────────────────────────────────────────────────────────────────────────
#pragma pack(push, 1)
typedef struct
{
    U32 SizeFile;
    U32 CompressedSizeFile;
    S16 CompressMethod; // 0 stored, 1 LZSS, 2 LZMIT
} COMPRESSED_HEADER;
#pragma pack(pop)

// The entry header is little-endian on disk. Call this on the struct right
// after reading its 10 bytes into it; it is a no-op on a little-endian CPU.
static inline void HQR_HeaderFromLE(COMPRESSED_HEADER *header) {
    const U8 *raw = (const U8 *)header;
    const U32 size = ReadLE_U32(raw);
    const U32 compressed = ReadLE_U32(raw + 4);
    const S16 method = ReadLE_S16(raw + 8);
    header->SizeFile = size;
    header->CompressedSizeFile = compressed;
    header->CompressMethod = method;
}

//──────────────────────────────────────────────────────────────────────────
#ifdef __cplusplus
}
#endif

//──────────────────────────────────────────────────────────────────────────
#endif //LIB_SYSTEM_HQR

//──────────────────────────────────────────────────────────────────────────
