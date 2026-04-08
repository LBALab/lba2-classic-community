#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/FILES.H>

extern "C" const unsigned char g_embeddedLba2CfgBytes[];
extern "C" const U32 g_embeddedLba2CfgBytesSize;

extern "C" int WriteEmbeddedDefaultLba2Cfg(const char *destPath) {
    const S32 handle = OpenWrite(destPath);
    if (!handle) {
        return 0;
    }

    const U32 n = g_embeddedLba2CfgBytesSize;
    U32 offset = 0;
    while (offset < n) {
        const U32 chunk =
            (n - offset > 4096U) ? 4096U : (n - offset);
        const S32 w = Write(handle, g_embeddedLba2CfgBytes + offset, chunk);
        if ((U32)w != chunk) {
            Close(handle);
            return 0;
        }
        offset += chunk;
    }

    Close(handle);
    return 1;
}
