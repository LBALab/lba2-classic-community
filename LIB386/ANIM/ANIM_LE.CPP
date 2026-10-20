#include <ANIM/ANIM.H>
#include <SYSTEM/LE.H>

/* Animations are little-endian on disk and every field is 16 bits: an 8-byte
   header (NbFrames, NbGroups, LoopFrame, padding), then per frame a time, a
   step X, Y, Z and per group a type and three angles. The animation code reads
   them through typed pointers, so a loader calls AnimFromLE once on each freshly
   loaded animation. On a little-endian CPU it does nothing. */
void AnimFromLE(void *anim, S32 size) {
    if (!LBA_BIG_ENDIAN || !anim || size < 2)
        return;
    FromLE16InPlace(anim, (U32)size / 2);
}
