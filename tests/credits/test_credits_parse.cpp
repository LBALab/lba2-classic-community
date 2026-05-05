/*
 * Host-only regression tests for the credits HQR record parser.
 *
 * Pins issue #65: the on-disk record stride must come from
 * sizeof(S_CRED_OBJ_2_DISK) (frozen 32-bit ABI), not sizeof(S_CRED_OBJ_2)
 * (the runtime struct, which grew on 64-bit because of T_OBJ_3D's
 * pointer-sized fields). Walking at the wrong stride mis-reads
 * OffBody/OffAnim and ObjectInitAnim then walks off the heap.
 *
 * See SOURCES/CREDITS_PARSE.{H,CPP}, docs/ABI.md, PR #66, PR #67.
 */
#include "CREDITS_PARSE.H"

#include <cstdio>
#include <cstring>

static int g_failed = 0;
static int g_test_failed = 0;

#define CHECK(cond)                                    \
    do {                                               \
        if (!(cond)) {                                 \
            std::fprintf(stderr, "  FAIL %s:%d: %s\n", \
                         __FILE__, __LINE__, #cond);   \
            g_test_failed = 1;                         \
        }                                              \
    } while (0)

#define CHECK_EQ(actual, expected)                                                     \
    do {                                                                               \
        long long _a = (long long)(actual);                                            \
        long long _e = (long long)(expected);                                          \
        if (_a != _e) {                                                                \
            std::fprintf(stderr, "  FAIL %s:%d: %s == %s (got %lld, expected %lld)\n", \
                         __FILE__, __LINE__, #actual, #expected, _a, _e);              \
            g_test_failed = 1;                                                         \
        }                                                                              \
    } while (0)

#define RUN(test)                                \
    do {                                         \
        g_test_failed = 0;                       \
        std::printf("[ RUN  ] %s\n", #test);     \
        test();                                  \
        if (g_test_failed) {                     \
            std::printf("[ FAIL ] %s\n", #test); \
            g_failed = 1;                        \
        } else {                                 \
            std::printf("[  OK  ] %s\n", #test); \
        }                                        \
    } while (0)

/* Build a synthetic credits blob: header + nb records, each with a unique
 * OffBody/OffAnim pattern so the test can verify the parser indexes records
 * at the right stride. Returns the blob size; the blob lives in `out_buf`. */
static S32 build_blob(S32 nb_objects, U8 *out_buf, S32 out_buf_size) {
    const S32 hdr_size = (S32)sizeof(S_CRED_INFOS_2);
    const S32 rec_size = (S32)sizeof(S_CRED_OBJ_2_DISK);
    const S32 needed = hdr_size + rec_size * nb_objects;
    if (needed > out_buf_size)
        return -1;

    std::memset(out_buf, 0, (size_t)needed);

    S_CRED_INFOS_2 *hdr = (S_CRED_INFOS_2 *)out_buf;
    hdr->NbObjects = nb_objects;
    hdr->OffTxt = hdr_size + rec_size * nb_objects;
    hdr->OffXpl = hdr->OffTxt + 8;

    S_CRED_OBJ_2_DISK *recs = (S_CRED_OBJ_2_DISK *)(out_buf + hdr_size);
    for (S32 i = 0; i < nb_objects; i++) {
        recs[i].OffBody = 0xB00000 + i;
        recs[i].OffAnim[0] = 0xA00000 + i;
        recs[i].OffAnim[1] = 0xA80000 + i;
    }
    return needed;
}

/* The whole point of this test file: confirm the parser reads OffBody/OffAnim
 * at the on-disk stride for every record, including past index 0. The bug
 * (issue #65) showed up at higher indices because (sizeof(S_CRED_OBJ_2) -
 * sizeof(S_CRED_OBJ_2_DISK)) compounds. */
static void test_reads_offsets_at_disk_stride(void) {
    U8 buf[8 * 1024];
    const S32 size = build_blob(5, buf, (S32)sizeof(buf));
    CHECK(size > 0);

    S_CRED_PARSED_OFFSETS out[16];
    const S32 rc = ParseCreditsBlob(buf, size, out, 16);
    CHECK_EQ(rc, 5);
    for (S32 i = 0; i < 5; i++) {
        CHECK_EQ(out[i].OffBody, 0xB00000 + i);
        CHECK_EQ(out[i].OffAnim[0], 0xA00000 + i);
        CHECK_EQ(out[i].OffAnim[1], 0xA80000 + i);
    }
}

/* The on-disk record size is the part of the ABI that absolutely must not
 * drift. If a contributor edits S_CRED_OBJ_2_DISK and changes its size, the
 * compile-time asserts in CREDITS_PARSE.CPP will fire — but this runtime
 * check makes the contract visible in test output too. */
static void test_disk_layout_is_locked(void) {
    CHECK_EQ(sizeof(S_CRED_INFOS_2), 12);
    CHECK_EQ(sizeof(S_CRED_OBJ_2_DISK), 420);
}

static void test_zero_objects_is_valid(void) {
    U8 buf[64];
    const S32 size = build_blob(0, buf, (S32)sizeof(buf));
    S_CRED_PARSED_OFFSETS out[1] = {{0, {0, 0}}};
    CHECK_EQ(ParseCreditsBlob(buf, size, out, 1), 0);
}

/* Tamper OffTxt so disk_block != Nb * sizeof(S_CRED_OBJ_2_DISK). This is the
 * shape the original (pre-#66) bug took: the parser had implicit trust in
 * the in-memory struct size, which differed from the on-disk size on 64-bit. */
static void test_disk_block_mismatch_rejected(void) {
    U8 buf[8 * 1024];
    const S32 size = build_blob(3, buf, (S32)sizeof(buf));
    S_CRED_INFOS_2 *hdr = (S_CRED_INFOS_2 *)buf;
    hdr->OffTxt += 8; /* off by a bogus amount */

    S_CRED_PARSED_OFFSETS out[16];
    CHECK_EQ(ParseCreditsBlob(buf, size, out, 16), CRED_PARSE_ERR_DISK_BLOCK);
}

static void test_nb_objects_overflow_rejected(void) {
    U8 buf[8 * 1024];
    const S32 size = build_blob(3, buf, (S32)sizeof(buf));
    S_CRED_PARSED_OFFSETS out[2];
    CHECK_EQ(ParseCreditsBlob(buf, size, out, 2), CRED_PARSE_ERR_NB_RANGE);
}

static void test_negative_nb_objects_rejected(void) {
    U8 buf[64];
    std::memset(buf, 0, sizeof(buf));
    S_CRED_INFOS_2 *hdr = (S_CRED_INFOS_2 *)buf;
    hdr->NbObjects = -1;
    hdr->OffTxt = (S32)sizeof(*hdr);

    S_CRED_PARSED_OFFSETS out[1];
    CHECK_EQ(ParseCreditsBlob(buf, (S32)sizeof(buf), out, 1), CRED_PARSE_ERR_NB_RANGE);
}

static void test_truncated_header_rejected(void) {
    U8 tiny[4] = {0};
    S_CRED_PARSED_OFFSETS out[1];
    CHECK_EQ(ParseCreditsBlob(tiny, (S32)sizeof(tiny), out, 1),
             CRED_PARSE_ERR_TRUNC_HDR);
}

static void test_truncated_records_rejected(void) {
    U8 buf[8 * 1024];
    const S32 size = build_blob(3, buf, (S32)sizeof(buf));
    /* Lie about blob_size: claim only the header is present. */
    S_CRED_PARSED_OFFSETS out[16];
    (void)size;
    CHECK_EQ(ParseCreditsBlob(buf, (S32)sizeof(S_CRED_INFOS_2), out, 16),
             CRED_PARSE_ERR_TRUNC_RECS);
}

static void test_null_args_rejected(void) {
    U8 buf[64] = {0};
    S_CRED_PARSED_OFFSETS out[1];
    CHECK_EQ(ParseCreditsBlob(NULL, 100, out, 1), CRED_PARSE_ERR_NULL);
    CHECK_EQ(ParseCreditsBlob(buf, 100, NULL, 1), CRED_PARSE_ERR_NULL);
    CHECK_EQ(ParseCreditsBlob(buf, 100, out, 0), CRED_PARSE_ERR_NULL);
}

int main(void) {
    RUN(test_disk_layout_is_locked);
    RUN(test_reads_offsets_at_disk_stride);
    RUN(test_zero_objects_is_valid);
    RUN(test_disk_block_mismatch_rejected);
    RUN(test_nb_objects_overflow_rejected);
    RUN(test_negative_nb_objects_rejected);
    RUN(test_truncated_header_rejected);
    RUN(test_truncated_records_rejected);
    RUN(test_null_args_rejected);
    return g_failed ? 1 : 0;
}
