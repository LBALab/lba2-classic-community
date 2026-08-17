#include "CONTROL_PROTO.H"

#include <stdio.h>
#include <string.h>

/* --- Response buffer -------------------------------------------------------- */

void Proto_RespReset(T_PROTO_RESP *r) {
    r->len = 0;
    r->over = 0;
}

void Proto_RespAddLine(T_PROTO_RESP *r, const char *line) {
    int n;

    if (!line)
        return;
    n = (int)strlen(line);
    if (r->len + n + 1 > PROTO_RESP_MAX - 1) {
        r->over = 1;
        return;
    }
    memcpy(r->buf + r->len, line, (size_t)n);
    r->len += n;
    r->buf[r->len++] = '\n';
}

void Proto_RespFinish(T_PROTO_RESP *r) {
    char mark[64];
    int mn;

    if (!r->over)
        return;

    /* Not through Proto_RespAddLine: the buffer it would append to is the one
       that just refused a line, so the notice would be dropped exactly when it is
       the only thing telling the reader the response is short. */
    mn = snprintf(mark, sizeof(mark), "[ctlsrv] output truncated at %d bytes\n", PROTO_RESP_MAX);
    if (mn <= 0 || mn >= (int)sizeof(mark))
        return;
    if (r->len + mn > PROTO_RESP_MAX)
        r->len = PROTO_RESP_MAX - mn;
    memcpy(r->buf + r->len, mark, (size_t)mn);
    r->len += mn;
    r->over = 0;
}

/* --- Input assembler -------------------------------------------------------- */

void Proto_InReset(T_PROTO_IN *in) {
    in->len = 0;
}

int Proto_InAppend(T_PROTO_IN *in, const char *data, int n) {
    if (n <= 0)
        return 0;
    if (n > PROTO_RAW_MAX - in->len)
        return -1;
    memcpy(in->buf + in->len, data, (size_t)n);
    in->len += n;
    return 0;
}

T_PROTO_TAKE Proto_InTakeLine(T_PROTO_IN *in, char *out, int out_max) {
    int i, n, consumed;

    for (i = 0; i < in->len; i++) {
        if (in->buf[i] != '\n')
            continue;

        n = i;
        if (n > 0 && in->buf[n - 1] == '\r')
            n--;
        consumed = i + 1;

        /* Consume before reporting either way: the caller may be unable to run
           the line, and leaving it queued would offer it again next frame. */
        if (n < out_max) {
            memcpy(out, in->buf, (size_t)n);
            out[n] = '\0';
        }
        memmove(in->buf, in->buf + consumed, (size_t)(in->len - consumed));
        in->len -= consumed;

        return (n < out_max) ? PROTO_TAKE_LINE : PROTO_TAKE_TOO_LONG;
    }

    if (in->len >= PROTO_RAW_MAX) {
        in->len = 0;
        return PROTO_TAKE_NO_NEWLINE;
    }
    return PROTO_TAKE_NONE;
}

/* --- Event ring ------------------------------------------------------------- */

void Proto_RingReset(T_PROTO_RING *ring) {
    ring->head = 0;
    ring->count = 0;
    ring->dropped = 0;
}

void Proto_RingPush(T_PROTO_RING *ring, const char *line) {
    int slot;

    if (!line)
        return;
    if (ring->count == PROTO_EVENT_SLOTS) {
        ring->head = (ring->head + 1) % PROTO_EVENT_SLOTS;
        ring->count--;
        ring->dropped++;
    }
    slot = (ring->head + ring->count) % PROTO_EVENT_SLOTS;
    strncpy(ring->slot[slot], line, PROTO_EVENT_LINE_MAX - 1);
    ring->slot[slot][PROTO_EVENT_LINE_MAX - 1] = '\0';
    ring->count++;
}

const char *Proto_RingTake(T_PROTO_RING *ring) {
    const char *line;

    if (ring->count == 0)
        return NULL;
    line = ring->slot[ring->head];
    ring->head = (ring->head + 1) % PROTO_EVENT_SLOTS;
    ring->count--;
    return line;
}

long Proto_RingTakeDropped(T_PROTO_RING *ring) {
    long n = ring->dropped;
    ring->dropped = 0;
    return n;
}
