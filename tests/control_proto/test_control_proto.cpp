/**
 * Host-only tests for the --listen wire mechanism (no socket, no engine, no data).
 *
 * Intent (must stay aligned with production code):
 * - SOURCES/CONTROL_PROTO.CPP holds the three buffers CONTROL_SERVER.CPP drives:
 *   the input assembler that turns a byte stream into whole commands, the
 *   response buffer that collects one command's output, and the event ring that
 *   holds pushed log records between frames.
 * - The socket half (accept, recv, send, the console call, the re-entrancy guard)
 *   is not here and cannot be: it needs a running engine. What is here is the
 *   arithmetic, which is where the cases that are easy to get wrong live.
 *
 * Two of the assertions cover failures that reasoning about the code missed and
 * driving a live engine caught: an over-long read that splices two commands into
 * one, and a truncation notice dropped by the buffer that just refused a line.
 * Both are cheap to hold still here.
 */

#include "CONTROL_PROTO.H"

#include <cassert>
#include <cstdio>
#include <cstring>

/* --- Input assembler -------------------------------------------------------- */

static void feed(T_PROTO_IN *in, const char *s) {
    assert(Proto_InAppend(in, s, (int)strlen(s)) == 0);
}

static void test_takes_one_whole_line_at_a_time(void) {
    T_PROTO_IN in;
    char out[PROTO_LINE_MAX];

    Proto_InReset(&in);
    feed(&in, "status\nzonelist\n");

    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_LINE);
    assert(strcmp(out, "status") == 0);
    /* One per call: a burst must not run inside a single frame, because commands
       sharing a frame share its side effects. */
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_LINE);
    assert(strcmp(out, "zonelist") == 0);
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_NONE);
}

static void test_partial_line_waits_for_its_newline(void) {
    T_PROTO_IN in;
    char out[PROTO_LINE_MAX];

    Proto_InReset(&in);
    feed(&in, "stat");
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_NONE);
    feed(&in, "us\n");
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_LINE);
    assert(strcmp(out, "status") == 0);
}

static void test_carriage_return_is_stripped(void) {
    T_PROTO_IN in;
    char out[PROTO_LINE_MAX];

    Proto_InReset(&in);
    feed(&in, "status\r\n");
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_LINE);
    assert(strcmp(out, "status") == 0);
}

static void test_overlong_line_is_reported_and_consumed(void) {
    T_PROTO_IN in;
    char out[PROTO_LINE_MAX];
    char big[PROTO_LINE_MAX + 64];

    memset(big, 'x', sizeof(big) - 2);
    big[sizeof(big) - 2] = '\n';
    big[sizeof(big) - 1] = '\0';

    Proto_InReset(&in);
    feed(&in, big);
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_TOO_LONG);
    /* Consumed, not left to be offered again every frame for the rest of the run. */
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_NONE);

    /* And the next command still lands. */
    feed(&in, "status\n");
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_LINE);
    assert(strcmp(out, "status") == 0);
}

/* Clamping the excess and keeping the rest splices the bytes either side of the
   gap into one command, so the caller has to be told instead: from there it would
   be answering things nobody sent, and it can only close. */
static void test_append_refuses_rather_than_splicing(void) {
    T_PROTO_IN in;
    char out[PROTO_LINE_MAX];
    static char flood[PROTO_RAW_MAX];

    Proto_InReset(&in);
    memset(flood, 'x', sizeof(flood));
    assert(Proto_InAppend(&in, flood, (int)sizeof(flood)) == 0); /* exactly fills */
    assert(Proto_InAppend(&in, "tail\n", 5) == -1);              /* one byte over */
    /* Refused whole: nothing of the rejected write is in the buffer. */
    assert(in.len == PROTO_RAW_MAX);
    assert(memchr(in.buf, '\n', (size_t)in.len) == NULL);

    /* A full buffer with no newline is a peer not speaking this protocol. */
    assert(Proto_InTakeLine(&in, out, sizeof(out)) == PROTO_TAKE_NO_NEWLINE);
    assert(in.len == 0);
}

/* --- Response buffer -------------------------------------------------------- */

static void test_response_collects_lines_with_newlines(void) {
    T_PROTO_RESP r;

    Proto_RespReset(&r);
    Proto_RespAddLine(&r, "one");
    Proto_RespAddLine(&r, "two");
    Proto_RespFinish(&r);

    assert(r.len == 8);
    assert(memcmp(r.buf, "one\ntwo\n", 8) == 0);
    assert(!r.over);
}

static void test_empty_response_is_empty(void) {
    T_PROTO_RESP r;

    Proto_RespReset(&r);
    Proto_RespFinish(&r);
    assert(r.len == 0);
}

/* Appending the notice through the same bounded path that just dropped a line
   drops the notice too, leaving the reader nothing saying the answer is short. It
   takes its room from the tail instead. */
static void test_truncation_says_so_even_when_full(void) {
    T_PROTO_RESP r;
    char line[512];
    int i;

    memset(line, 'y', sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    /* Filled until even a one-byte line will not fit. Stopping at "a long line was
       refused" is not the same state: there is still room for the notice then, so
       appending it through the bounded path happens to work and the assertion
       below passes against the bug it is here to catch. */
    Proto_RespReset(&r);
    for (i = 0; i < 4096 && !r.over; i++)
        Proto_RespAddLine(&r, line);
    assert(r.over);
    r.over = 0;
    for (i = 0; i < PROTO_RESP_MAX && !r.over; i++)
        Proto_RespAddLine(&r, "z");
    assert(r.over);
    assert(r.len > PROTO_RESP_MAX - 8);

    Proto_RespFinish(&r);
    assert(r.len <= PROTO_RESP_MAX);

    /* Compared against the exact bytes at the exact place, because the buffer is
       a length and not a C string: searching it with strstr runs off the end into
       whatever follows, which reports success on stack litter and makes this
       assertion pass against the bug it exists to catch. */
    {
        char expect[64];
        int en = snprintf(expect, sizeof(expect), "[ctlsrv] output truncated at %d bytes\n",
                          PROTO_RESP_MAX);
        assert(en > 0 && en < (int)sizeof(expect));
        assert(r.len >= en);
        assert(memcmp(r.buf + r.len - en, expect, (size_t)en) == 0);
    }
}

/* --- Event ring ------------------------------------------------------------- */

static void test_ring_is_first_in_first_out(void) {
    T_PROTO_RING ring;

    Proto_RingReset(&ring);
    Proto_RingPush(&ring, "a");
    Proto_RingPush(&ring, "b");

    assert(strcmp(Proto_RingTake(&ring), "a") == 0);
    assert(strcmp(Proto_RingTake(&ring), "b") == 0);
    assert(Proto_RingTake(&ring) == NULL);
    assert(Proto_RingTakeDropped(&ring) == 0);
}

static void test_ring_drops_oldest_and_counts_them(void) {
    T_PROTO_RING ring;
    char buf[32];
    int i;
    const char *first;

    Proto_RingReset(&ring);
    for (i = 0; i < PROTO_EVENT_SLOTS + 10; i++) {
        snprintf(buf, sizeof(buf), "line%d", i);
        Proto_RingPush(&ring, buf);
    }

    /* A driver that fell behind wants what just happened, so the ten lost are the
       ten oldest and the newest is still there. */
    assert(Proto_RingTakeDropped(&ring) == 10);
    assert(Proto_RingTakeDropped(&ring) == 0); /* reading clears the count */

    first = Proto_RingTake(&ring);
    assert(strcmp(first, "line10") == 0);
    for (i = 11; i < PROTO_EVENT_SLOTS + 9; i++)
        (void)Proto_RingTake(&ring);
    assert(strcmp(Proto_RingTake(&ring), "line265") == 0);
    assert(Proto_RingTake(&ring) == NULL);
}

static void test_ring_truncates_a_long_record_and_stays_terminated(void) {
    T_PROTO_RING ring;
    char big[PROTO_EVENT_LINE_MAX * 2];
    const char *got;

    memset(big, 'z', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    Proto_RingReset(&ring);
    Proto_RingPush(&ring, big);
    got = Proto_RingTake(&ring);
    assert(got != NULL);
    assert((int)strlen(got) == PROTO_EVENT_LINE_MAX - 1);
}

int main(void) {
    test_takes_one_whole_line_at_a_time();
    test_partial_line_waits_for_its_newline();
    test_carriage_return_is_stripped();
    test_overlong_line_is_reported_and_consumed();
    test_append_refuses_rather_than_splicing();

    test_response_collects_lines_with_newlines();
    test_empty_response_is_empty();
    test_truncation_says_so_even_when_full();

    test_ring_is_first_in_first_out();
    test_ring_drops_oldest_and_counts_them();
    test_ring_truncates_a_long_record_and_stays_terminated();

    printf("control_proto: all tests passed\n");
    return 0;
}
