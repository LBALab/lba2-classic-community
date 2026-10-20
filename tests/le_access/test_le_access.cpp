/*
 * The little-endian accessors in SYSTEM/LE.H read game data at any byte offset
 * and give the value the file stores, whatever the CPU's byte order.
 *
 * The expected values are written out from the bytes by hand, not computed
 * through a cast, so the test means the same on a big-endian host (it is meant
 * to pass under qemu-user for s390x or PowerPC too).
 */

#include "test_harness.h"

#include <SYSTEM/LE.H>

#include <string.h>

static const U8 Bytes[] = {0x34, 0x12, 0xFE, 0xFF, 0x78, 0x56, 0x34, 0x12, 0x01, 0x80, 0x00, 0x00};

static void test_reads_little_endian() {
    ASSERT_EQ_UINT(0x1234u, ReadLE_U16(Bytes));
    ASSERT_EQ_INT(0x1234, ReadLE_S16(Bytes));
    ASSERT_EQ_UINT(0xFFFEu, ReadLE_U16(Bytes + 2));
    ASSERT_EQ_INT(-2, ReadLE_S16(Bytes + 2));
    ASSERT_EQ_UINT(0x12345678u, ReadLE_U32(Bytes + 4));
    ASSERT_EQ_INT(0x12345678, ReadLE_S32(Bytes + 4));
    ASSERT_EQ_UINT(0xFFFE1234u, ReadLE_U32(Bytes));
    ASSERT_EQ_INT(-0x1EDCC, ReadLE_S32(Bytes)); /* 0xFFFE1234 */
}

/* Every odd and even offset gives the bytes found there, the case a cast read
 * gets wrong on 32-bit ARM. */
static void test_reads_at_every_offset() {
    U8 buf[16];
    for (int i = 0; i < 16; i++)
        buf[i] = (U8)(0x10 + i);
    for (int off = 0; off + 4 <= 16; off++) {
        const U32 b0 = buf[off], b1 = buf[off + 1], b2 = buf[off + 2], b3 = buf[off + 3];
        ASSERT_EQ_UINT(b0 | (b1 << 8), ReadLE_U16(buf + off));
        ASSERT_EQ_UINT(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24), ReadLE_U32(buf + off));
    }
}

static void test_sign_extends_at_the_top_bit() {
    const U8 min16[] = {0x00, 0x80};
    const U8 max16[] = {0xFF, 0x7F};
    const U8 min32[] = {0x00, 0x00, 0x00, 0x80};
    const U8 neg1[] = {0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ_INT(-32768, ReadLE_S16(min16));
    ASSERT_EQ_INT(32767, ReadLE_S16(max16));
    ASSERT_EQ_INT((S32)0x80000000u, ReadLE_S32(min32));
    ASSERT_EQ_INT(-1, ReadLE_S32(neg1));
    ASSERT_EQ_UINT(0xFFFFFFFFu, ReadLE_U32(neg1));
}

static void test_writes_little_endian_at_any_offset() {
    U8 buf[12];
    memset(buf, 0xAA, sizeof(buf));
    WriteLE_U16(buf + 1, 0x1234);
    WriteLE_S32(buf + 3, -2);
    WriteLE_S16(buf + 7, -32768);
    WriteLE_U32(buf + 8, 0x12345678u); /* overwrites the S16's high byte */
    const U8 expected[12] = {0xAA, 0x34, 0x12, 0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x78, 0x56, 0x34, 0x12};
    ASSERT_MEM_EQ(expected, buf, sizeof(buf));
}

static void test_write_then_read_round_trips() {
    U8 buf[8];
    const S32 values[] = {0, 1, -1, 32767, -32768, 0x7FFFFFFF, (S32)0x80000000u, 123456789};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        for (int off = 0; off < 4; off++) {
            WriteLE_S32(buf + off, values[i]);
            ASSERT_EQ_INT(values[i], ReadLE_S32(buf + off));
            WriteLE_S16(buf + off, (S16)values[i]);
            ASSERT_EQ_INT((S16)values[i], ReadLE_S16(buf + off));
        }
    }
}

int main() {
    RUN_TEST(test_reads_little_endian);
    RUN_TEST(test_reads_at_every_offset);
    RUN_TEST(test_sign_extends_at_the_top_bit);
    RUN_TEST(test_writes_little_endian_at_any_offset);
    RUN_TEST(test_write_then_read_round_trips);
    TEST_SUMMARY();
    return test_failures ? 1 : 0;
}
