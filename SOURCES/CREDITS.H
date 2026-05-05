#ifndef CREDITS_H
#define CREDITS_H

/* Cap for the static parsed-objects array. Retail credits HQR has 29 entries;
   64 leaves headroom and keeps the static buffer size modest. */
#define CRED_MAX_OBJECTS 64

typedef struct {
    S32 NbObjects;
    S32 OffTxt;
    S32 OffXpl;

} S_CRED_INFOS_2;

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

extern S32 GamePlayCredits(const char *file_name, S32 mode);

#endif // CREDITS_H
