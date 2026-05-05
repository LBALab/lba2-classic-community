#include "CREDITS_PARSE.H"

#include <cstddef>

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

    const S_CRED_INFOS_2 *infos = (const S_CRED_INFOS_2 *)blob;
    const S32 nb = infos->NbObjects;

    if (nb < 0 || nb > out_max)
        return CRED_PARSE_ERR_NB_RANGE;

    const S32 disk_block = infos->OffTxt - (S32)sizeof(S_CRED_INFOS_2);
    const S32 expected = (S32)sizeof(S_CRED_OBJ_2_DISK) * nb;
    if (disk_block != expected)
        return CRED_PARSE_ERR_DISK_BLOCK;

    if (blob_size < (S32)sizeof(S_CRED_INFOS_2) + expected)
        return CRED_PARSE_ERR_TRUNC_RECS;

    const S_CRED_OBJ_2_DISK *recs =
        (const S_CRED_OBJ_2_DISK *)(blob + sizeof(S_CRED_INFOS_2));
    for (S32 i = 0; i < nb; i++) {
        out[i].OffBody = recs[i].OffBody;
        out[i].OffAnim[0] = recs[i].OffAnim[0];
        out[i].OffAnim[1] = recs[i].OffAnim[1];
    }

    return nb;
}
