/* Shared fixture for ANIM equivalence tests.
 *
 * Provides helpers to build synthetic animation binary data in memory
 * and set up objects for testing.
 *
 * Animation binary format (reverse-engineered from ANIM.CPP / FRAME.CPP):
 *
 * Global header (8 bytes = 4 U16):
 *   [0] NbFrames, [1] NbGroups, [2] LoopFrame, [3] padding(0)
 *
 * Per frame (NbGroups*8 + 8 bytes):
 *   Frame header (8 bytes = 4 S16):
 *     [0] DeltaTime, [1] reserved, [2] NextBody (-1=no change), [3] reserved
 *   Group 0 (8 bytes, "master group", NOT copied to CurrentFrame):
 *     [0] Master, [1] Alpha0, [2] Beta0, [3] Gamma0
 *   Groups 1..NbGroups-1 (8 bytes each, copied to CurrentFrame[0..NbGroups-2]):
 *     [0] Type, [1] Alpha, [2] Beta, [3] Gamma
 */
#pragma once
#include <OBJECT/AFF_OBJ.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <string.h>

/* Calculate total animation data size in bytes */
static inline U32 anim_data_size(U16 nbFrames, U16 nbGroups) {
    return 8 + (U32)nbFrames * ((U32)nbGroups * 8 + 8);
}

/* Frame offset (bytes from anim start) for frame N */
static inline U32 anim_frame_offset(U16 nbGroups, U16 frame) {
    return ((U32)nbGroups * 8 + 8) * frame + 8;
}

/* Get pointer to frame N's header within anim data */
static inline S16 *anim_frame_ptr(U8 *anim, U16 nbGroups, U16 frame) {
    return (S16 *)(anim + anim_frame_offset(nbGroups, frame));
}

/* Build a synthetic animation in the given buffer.
 * Buffer must be at least anim_data_size(nbFrames, nbGroups) bytes.
 * All values default to 0; caller should fill in group data after. */
static inline void build_anim_header(U8 *buf, U16 nbFrames, U16 nbGroups,
                                     U16 loopFrame, U16 deltaTime) {
    U16 *hdr = (U16 *)buf;
    memset(buf, 0, anim_data_size(nbFrames, nbGroups));
    hdr[0] = nbFrames;
    hdr[1] = nbGroups;
    hdr[2] = loopFrame;
    hdr[3] = 0;
    /* Set DeltaTime and Master for each frame */
    for (U16 f = 0; f < nbFrames; f++) {
        S16 *fp = anim_frame_ptr(buf, nbGroups, f);
        fp[0] = (S16)deltaTime; /* DeltaTime */
        fp[1] = 0;              /* reserved */
        fp[2] = -1;             /* NextBody = no change */
        fp[3] = 0;              /* reserved */
        fp[4] = 0;              /* Master (group 0 Type) */
    }
}

/* Set group data for a specific frame.
 * group=0 is the master group (not copied to CurrentFrame).
 * groups 1..NbGroups-1 are copied to CurrentFrame[0..NbGroups-2]. */
static inline void set_anim_group(U8 *anim, U16 nbGroups, U16 frame,
                                  U16 group, S16 type, S16 alpha,
                                  S16 beta, S16 gamma) {
    S16 *fp = anim_frame_ptr(anim, nbGroups, frame);
    S16 *gp = fp + 4 + group * 4; /* skip frame header (4 S16) + group offset */
    gp[0] = type;
    gp[1] = alpha;
    gp[2] = beta;
    gp[3] = gamma;
}

/* Initialize an object for animation testing.
 * Clears the object and sets minimal fields needed for anim functions.
 * Sets TransFctAnim = NULL so anim data is used directly (no translation). */
static inline void init_test_obj(T_OBJ_3D *obj) {
    ObjectClear(obj);
    obj->Anim.Ptr = NULL;
    obj->Body.Ptr = NULL;
}

/* Animation buffer for ObjectStoreFrame (circular buffer).
 * Call InitObjects() to set up Lib3DBufferAnim globals. */
#define TEST_ANIM_BUFFER_SIZE 8192
static U8 g_anim_buffer[TEST_ANIM_BUFFER_SIZE];

static inline void init_anim_buffer(void) {
    memset(g_anim_buffer, 0, sizeof(g_anim_buffer));
    InitObjects(g_anim_buffer, TEST_ANIM_BUFFER_SIZE, NULL, NULL);
}
