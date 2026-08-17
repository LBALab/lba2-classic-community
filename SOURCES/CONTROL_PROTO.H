#ifndef CONTROL_PROTO_H
#define CONTROL_PROTO_H

/*
 * The wire mechanism behind the --listen socket, with no socket in it.
 *
 * Three pieces carry the protocol: the buffer that turns a byte stream into whole
 * commands, the buffer that collects a command's output, and the ring that holds
 * pushed log records until a frame can drain them. None of them touch a socket,
 * the console, or engine state, so they build into a host test and run in CI on
 * every platform, while CONTROL_SERVER.CPP keeps the parts that cannot.
 *
 * That split is worth having because this is where the fiddly cases live, and two
 * of them were wrong: a read that overran the input buffer spliced the bytes
 * either side of the gap into one command, and the notice saying a response had
 * been truncated was appended through the buffer that had just refused a line, so
 * it was dropped exactly when it was the only thing left saying so. Both are one
 * assert each here. See tests/control_proto/.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* One command in, one response out. The response holds whatever the command
   printed, so the ceiling is set by the chattiest of them; 64K leaves room. */
#define PROTO_RESP_MAX 65536
/* Matches CONSOLE_LINE_MAX: a longer line could not be executed anyway. */
#define PROTO_LINE_MAX 256
/* Room for several queued commands: a driver may write a burst in one go, and
   only one of them runs per frame. */
#define PROTO_RAW_MAX 8192
/* Sized for roughly a fifth of a second of the chattiest trace at an uncapped
   frame rate, so a driver that stalls briefly loses the oldest lines rather than
   the game losing a frame. */
#define PROTO_EVENT_SLOTS 256
#define PROTO_EVENT_LINE_MAX 192

/* Marks a pushed line, so a driver can tell an asynchronous event from the output
   of the command it just sent. Command output stays bare. */
#define PROTO_EVENT_PREFIX "! "
#define PROTO_TERMINATOR "<<END>>\n"

/* --- Response buffer -------------------------------------------------------- */

typedef struct {
    char buf[PROTO_RESP_MAX];
    int len;
    int over; /* set once a line did not fit */
} T_PROTO_RESP;

void Proto_RespReset(T_PROTO_RESP *r);
/** Append one line plus its newline. Sets `over` and keeps the buffer intact if
    it does not fit; never truncates a line into the buffer. */
void Proto_RespAddLine(T_PROTO_RESP *r, const char *line);
/** Close the response: if anything was dropped, append the notice saying so,
    taking the room back from the tail. A reader must be told the answer is
    short, and the notice cannot be the thing that gets dropped. */
void Proto_RespFinish(T_PROTO_RESP *r);

/* --- Input assembler -------------------------------------------------------- */

typedef struct {
    char buf[PROTO_RAW_MAX];
    int len;
} T_PROTO_IN;

typedef enum {
    PROTO_TAKE_NONE = 0,  /* no complete line yet */
    PROTO_TAKE_LINE,      /* out holds a command */
    PROTO_TAKE_TOO_LONG,  /* a line longer than PROTO_LINE_MAX-1 was discarded */
    PROTO_TAKE_NO_NEWLINE /* buffer full with no newline: the peer is not speaking this */
} T_PROTO_TAKE;

void Proto_InReset(T_PROTO_IN *in);
/** Append `n` bytes. Returns 0, or -1 if they do not all fit. The caller must
    then drop the connection rather than keep part of them, because discarding the
    excess splices the bytes either side of the gap into one command and answers
    something the driver never sent. */
int Proto_InAppend(T_PROTO_IN *in, const char *data, int n);
/** Take the next complete line into `out`, consuming it and any trailing CR. */
T_PROTO_TAKE Proto_InTakeLine(T_PROTO_IN *in, char *out, int out_max);

/* --- Event ring ------------------------------------------------------------- */

typedef struct {
    char slot[PROTO_EVENT_SLOTS][PROTO_EVENT_LINE_MAX];
    int head;
    int count;
    long dropped;
} T_PROTO_RING;

void Proto_RingReset(T_PROTO_RING *ring);
/** Push one record. When the ring is full the OLDEST goes, not the newest: a
    driver that fell behind wants what just happened, not what happened first. */
void Proto_RingPush(T_PROTO_RING *ring, const char *line);
/** Oldest record, or NULL when empty. Valid until the next push. */
const char *Proto_RingTake(T_PROTO_RING *ring);
/** Records lost since the last call, and clears the count. */
long Proto_RingTakeDropped(T_PROTO_RING *ring);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_PROTO_H */
