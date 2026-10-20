#include "CREDITS_PARSE.H"

#include <cstddef>

#include <SYSTEM/LE.H>

/* Lock the on-disk record layout at compile time. If any of these fail, the
 * credits HQR layout has drifted from the 1997 32-bit DOS ABI and the parser
 * below will read garbage. See docs/ABI.md and issue #65. */
typedef char ABI_assert_S_CRED_INFOS_2_size[(sizeof(S_CRED_INFOS_2) == 12) ? 1 : -1];
typedef char ABI_assert_S_CRED_OBJ_2_DISK_size[(sizeof(S_CRED_OBJ_2_DISK) == 420) ? 1 : -1];
typedef char ABI_assert_S_CRED_OBJ_2_DISK_offset_OffBody[(offsetof(S_CRED_OBJ_2_DISK, OffBody) == 408) ? 1 : -1];
typedef char ABI_assert_S_CRED_OBJ_2_DISK_offset_OffAnim[(offsetof(S_CRED_OBJ_2_DISK, OffAnim) == 412) ? 1 : -1];

S32 ParseCreditsBlob(const U8 *blob, S32 blob_size,
                     S_CRED_PARSED_OFFSETS *out, S32 out_max) {
    if (!blob || !out || out_max <= 0)
        return CRED_PARSE_ERR_NULL;

    if (blob_size < (S32)sizeof(S_CRED_INFOS_2))
        return CRED_PARSE_ERR_TRUNC_HDR;

    /* The blob is little-endian: read each field at its offset in the layout
       locked above rather than through the structs. */
    const S32 nb = ReadLE_S32(blob + offsetof(S_CRED_INFOS_2, NbObjects));
    const S32 offTxt = ReadLE_S32(blob + offsetof(S_CRED_INFOS_2, OffTxt));

    if (nb < 0 || nb > out_max)
        return CRED_PARSE_ERR_NB_RANGE;

    const S32 disk_block = offTxt - (S32)sizeof(S_CRED_INFOS_2);
    const S32 expected = (S32)sizeof(S_CRED_OBJ_2_DISK) * nb;
    if (disk_block != expected)
        return CRED_PARSE_ERR_DISK_BLOCK;

    if (blob_size < (S32)sizeof(S_CRED_INFOS_2) + expected)
        return CRED_PARSE_ERR_TRUNC_RECS;

    const U8 *recs = blob + sizeof(S_CRED_INFOS_2);
    for (S32 i = 0; i < nb; i++) {
        const U8 *rec = recs + (size_t)i * sizeof(S_CRED_OBJ_2_DISK);
        out[i].OffBody = ReadLE_S32(rec + offsetof(S_CRED_OBJ_2_DISK, OffBody));
        out[i].OffAnim[0] = ReadLE_S32(rec + offsetof(S_CRED_OBJ_2_DISK, OffAnim));
        out[i].OffAnim[1] = ReadLE_S32(rec + offsetof(S_CRED_OBJ_2_DISK, OffAnim) + 4);
    }

    return nb;
}
