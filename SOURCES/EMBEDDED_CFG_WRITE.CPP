#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/FILES.H>

extern "C" const unsigned char g_embeddedLba2CfgBytes[];
extern "C" const U32 g_embeddedLba2CfgBytesSize;

extern "C" int WriteEmbeddedDefaultLba2Cfg(const char *destPath) {
    // Atomic whole-buffer write: a partial default cfg (process killed
    // mid-write) would otherwise be the seed the engine reads on the very next
    // launch. Either the full default lands or destPath is left untouched.
    return WriteFileAtomic(destPath, g_embeddedLba2CfgBytes,
                           g_embeddedLba2CfgBytesSize);
}
