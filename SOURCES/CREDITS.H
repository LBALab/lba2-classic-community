#ifndef CREDITS_H
#define CREDITS_H

#include "CREDITS_PARSE.H"

/* Cap for the static parsed-objects array. Retail credits HQR has 29 entries;
   64 leaves headroom and keeps the static buffer size modest. */
#define CRED_MAX_OBJECTS 64

typedef struct {
    // reserve place pour player
    T_OBJ_3D Obj;         // lib struct
    BOUND_MOVE RealAngle; // synchro de la rotation

    S32 Flag;
    S32 DestX;
    S32 DestZ;

    S32 OffBody; // offset
    S32 OffAnim[2];

} S_CRED_OBJ_2;

/* S_CRED_INFOS_2, S_CRED_OBJ_2_DISK, and the on-disk size constants live in
 * CREDITS_PARSE.H so the parser can be tested without dragging in T_OBJ_3D
 * and the rest of the game runtime. See CREDITS_PARSE.H + tests/credits/. */

extern S32 GamePlayCredits(const char *file_name, S32 mode);

#endif // CREDITS_H
