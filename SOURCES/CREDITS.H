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

/* On-disk layout of an S_CRED_OBJ_2 record inside lba2.hqr (CREDITS_HQR_NAME,
   index 0). Frozen at the original 32-bit DOS ABI — the credits HQR is a
   1997 retail asset and never changes. The runtime S_CRED_OBJ_2 above is
   larger on 64-bit because T_OBJ_3D contains pointer-sized fields, so reading
   the file with sizeof(S_CRED_OBJ_2) as the stride mis-aligns every record
   (this was issue #65). Always parse via this struct or via the explicit
   per-record stride derived from S_CRED_INFOS_2.OffTxt. See docs/ABI.md. */
#define S_CRED_OBJ_2_DISK_OBJ_BYTES 376
#define S_CRED_OBJ_2_DISK_REALANGLE_BYTES 20

typedef struct {
    char Obj[S_CRED_OBJ_2_DISK_OBJ_BYTES];               // 32-bit T_OBJ_3D
    char RealAngle[S_CRED_OBJ_2_DISK_REALANGLE_BYTES];   // BOUND_MOVE (fixed-width)
    S32 Flag;
    S32 DestX;
    S32 DestZ;
    S32 OffBody;
    S32 OffAnim[2];
} S_CRED_OBJ_2_DISK;

extern S32 GamePlayCredits(const char *file_name, S32 mode);

#endif // CREDITS_H
