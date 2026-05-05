#ifndef CREDITS_PARSE_H
#define CREDITS_PARSE_H

#include <SYSTEM/ADELINE_TYPES.H>

/* On-disk format and parser for the credits HQR record block.
 *
 * The credits HQR is a 1997 retail asset frozen at the original 32-bit DOS
 * ABI. The runtime S_CRED_OBJ_2 (defined in CREDITS.H) embeds a T_OBJ_3D
 * which grew on 64-bit hosts because of pointer-sized fields, so parsing the
 * file at sizeof(S_CRED_OBJ_2) stride mis-reads OffBody/OffAnim and the
 * subsequent ObjectInitAnim walks off the heap. This was issue #65.
 *
 * S_CRED_OBJ_2_DISK below is the authoritative on-disk record layout. Walk
 * the file at sizeof(S_CRED_OBJ_2_DISK), never sizeof(S_CRED_OBJ_2). The
 * compile-time size assertions in CREDITS_PARSE.CPP ensure the disk struct
 * never silently drifts. See docs/ABI.md.
 */

#define S_CRED_OBJ_2_DISK_OBJ_BYTES 376
#define S_CRED_OBJ_2_DISK_REALANGLE_BYTES 20

typedef struct {
    S32 NbObjects;
    S32 OffTxt;
    S32 OffXpl;
} S_CRED_INFOS_2;

typedef struct {
    char Obj[S_CRED_OBJ_2_DISK_OBJ_BYTES];             /* 32-bit T_OBJ_3D */
    char RealAngle[S_CRED_OBJ_2_DISK_REALANGLE_BYTES]; /* BOUND_MOVE (fixed-width) */
    S32 Flag;
    S32 DestX;
    S32 DestZ;
    S32 OffBody;
    S32 OffAnim[2];
} S_CRED_OBJ_2_DISK;

/* Subset of fields the parser extracts. The runtime caller fills the rest of
 * S_CRED_OBJ_2 (X, Y, Z, Beta, Flag, the embedded T_OBJ_3D) from gameplay
 * state, not from disk. */
typedef struct {
    S32 OffBody;
    S32 OffAnim[2];
} S_CRED_PARSED_OFFSETS;

/* Return codes from ParseCreditsBlob. Success returns NbObjects (>= 0).
 * Any negative value is an error — distinct codes so tests can pin which
 * validation step fired. */
#define CRED_PARSE_ERR_NULL (-1)       /* null pointer arg or out_max <= 0 */
#define CRED_PARSE_ERR_TRUNC_HDR (-2)  /* blob shorter than S_CRED_INFOS_2 */
#define CRED_PARSE_ERR_NB_RANGE (-3)   /* NbObjects negative or > out_max */
#define CRED_PARSE_ERR_DISK_BLOCK (-4) /* OffTxt - hdr != Nb * sizeof(disk record) */
#define CRED_PARSE_ERR_TRUNC_RECS (-5) /* blob shorter than declared records area */

#ifdef __cplusplus
extern "C" {
#endif

/* Parse the header + S_CRED_OBJ_2_DISK records out of `blob`. Writes per-record
 * OffBody/OffAnim into `out[0..NbObjects)`. Returns NbObjects on success, or a
 * negative CRED_PARSE_ERR_* code. Pure function: no globals, no I/O. */
S32 ParseCreditsBlob(const U8 *blob, S32 blob_size,
                     S_CRED_PARSED_OFFSETS *out, S32 out_max);

#ifdef __cplusplus
}
#endif

#endif /* CREDITS_PARSE_H */
