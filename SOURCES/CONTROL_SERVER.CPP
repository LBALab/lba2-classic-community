#include "CONTROL_SERVER.H"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(LBA2_CONTROL_SERVER)

#include "CONSOLE/CONSOLE.H"
#include "CONTROL_PROTO.H" /* the buffers, and the host tests that hold them still */

#include <SYSTEM/LOG.H>

/* --- Socket seam -------------------------------------------------------------
 *
 * Berkeley sockets and Winsock differ in spelling far more than in behaviour, so
 * everything below this block is one implementation. The differences that matter:
 *
 *   - a Windows socket is a SOCKET (a pointer-sized unsigned handle), not a small
 *     int, and its invalid value is INVALID_SOCKET rather than -1. Storing one in
 *     an int truncates, and `fd < 0` is never true for it. Both mistakes survive
 *     testing, because early handles are small and positive.
 *   - Winsock needs WSAStartup before any call and reports through
 *     WSAGetLastError, not errno.
 *   - MSG_NOSIGNAL has no Windows equivalent and needs none: there is no SIGPIPE
 *     to suppress.
 */

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET sock_t;
typedef int socklen_arg_t;
#define SOCK_INVALID INVALID_SOCKET
#define SOCK_SEND_FLAGS 0
#define sock_valid(s) ((s) != INVALID_SOCKET)
#define sock_close(s) closesocket(s)
#define sock_last_error() WSAGetLastError()
#define sock_would_block(e) ((e) == WSAEWOULDBLOCK)
#define sock_interrupted(e) ((e) == WSAEINTR)
#define sock_poll WSAPoll
#define SOCK_OPTVAL(p) ((const char *)(p))

static int sock_set_nonblocking(sock_t s) {
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0 ? 0 : -1;
}

/* Winsock has no strerror. The numeric code is unambiguous and greps cleanly
   against MSDN, which is what a log line is for. */
static const char *sock_error_text(int err) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "WSA error %d", err);
    return buf;
}

static int s_wsa_up = 0;

static int sock_platform_init(void) {
    WSADATA wsa;
    if (s_wsa_up)
        return 0;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return -1;
    s_wsa_up = 1;
    return 0;
}

static void sock_platform_shutdown(void) {
    if (s_wsa_up) {
        WSACleanup();
        s_wsa_up = 0;
    }
}

#else /* POSIX */

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int sock_t;
typedef socklen_t socklen_arg_t;
#define SOCK_INVALID (-1)
#define SOCK_SEND_FLAGS MSG_NOSIGNAL
#define sock_valid(s) ((s) >= 0)
#define sock_close(s) close(s)
#define sock_last_error() errno
#define sock_would_block(e) ((e) == EAGAIN || (e) == EWOULDBLOCK)
#define sock_interrupted(e) ((e) == EINTR)
#define sock_poll poll
#define SOCK_OPTVAL(p) (p)
#define sock_error_text(e) strerror(e)

static int sock_set_nonblocking(sock_t s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

static int sock_platform_init(void) {
    return 0;
}

static void sock_platform_shutdown(void) {
}

#endif

#define READ_CHUNK 4096
/* Cap the work one frame will do flushing, so a large backlog cannot turn into a
   visible hitch. Comfortably above the per-frame production rate of any trace. */
#define EVENT_FLUSH_PER_POLL 64

/* A client that has stopped reading must not stall the frame. Give a full send
   this many 1ms poll waits, then drop the connection rather than wait longer:
   an unresponsive driver is its own problem, a stuttering game is ours. */
#define SEND_WAIT_MS 1
#define SEND_WAIT_MAX 20

static sock_t s_listen_fd = SOCK_INVALID;
static sock_t s_client_fd = SOCK_INVALID;
static T_PROTO_IN s_in;
static T_PROTO_RESP s_resp;
static long s_commands = 0;
/* Whatever sink was installed when a command started (the harness echoes console
   output to stdout through one). Held only for the duration of that command. */
static Console_LineSinkFn s_prev_sink = NULL;

/* Non-zero while a command is running. Servicing the socket from the pre-present
   chain is what keeps it answering inside a modal, and it also means a command
   that presents frames (anything opening a cinematic, a dialogue or a UI capture)
   re-enters this file from inside itself. A second command must not start there:
   it would reset the response buffer the outer one is still filling, and hand the
   outer sink back as its own predecessor, so the first line printed would recurse
   until the stack ran out. */
static int s_in_command = 0;

/* `stream on`: the log fan-out feeds this ring, and Poll drains it to the client.
   The log callback never touches the socket, for two reasons: a trace firing
   mid-frame must not be able to block the game on a slow reader, and a write from
   inside a command would interleave with that command's own response.

   The sink is made once and kept for the process, with s_streaming deciding
   whether records are kept, rather than registered and unregistered per `stream
   on`. Log_RemoveSink takes a sink off the dispatch list without releasing its
   pool slot, and the pool is eight deep with three gone at boot, so a handful of
   on/off cycles would have quietly used up the last one and taken the next
   subsystem's sink with it. */
static LogSink *s_log_sink = NULL;
static int s_streaming = 0;
static T_PROTO_RING s_ring;

/* --- Response buffer -------------------------------------------------------- */

/* Console_Print fans out to this while a command runs, one call per line. Passes
   the line on to the sink we displaced, so a driver taking over does not go dark
   in the terminal. With someone watching the game while an agent drives it, the
   agent's commands should still be visible where the run is being watched. */
static void resp_sink(const char *line) {
    /* s_in_command already stops this sink displacing itself. Checked again here
       because the failure that would follow is unbounded recursion, which takes
       the process out with no diagnostic worth reading. */
    if (s_prev_sink && s_prev_sink != resp_sink)
        s_prev_sink(line);
    Proto_RespAddLine(&s_resp, line);
}

static void resp_addf(const char *fmt, ...) {
    char buf[PROTO_LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    resp_sink(buf);
}

/* --- Event stream ----------------------------------------------------------- */

/* Log fan-out callback. Appends one record; never writes to the socket. */
static void event_push(const char *line, LogSeverity sev, LogLineKind kind) {
    (void)sev;
    (void)kind;
    if (!s_streaming)
        return;
    Proto_RingPush(&s_ring, line);
}

static void event_reset(void) {
    Proto_RingReset(&s_ring);
}

static void stream_stop(void) {
    s_streaming = 0;
    event_reset();
}

static void stream_start(void) {
    if (s_streaming)
        return;
    event_reset();
    if (!s_log_sink) {
        /* LOG_DEBUG as the sink's own floor: the global level (`loglevel`) is the
           knob a driver actually turns, and this must not sit above it. */
        s_log_sink = Log_MakeCallbackSink(event_push, LOG_DEBUG);
        if (s_log_sink)
            Log_AddSink(s_log_sink);
    }
    s_streaming = (s_log_sink != NULL);
}

/* --- Client I/O ------------------------------------------------------------- */

static void client_close(const char *why) {
    if (!sock_valid(s_client_fd))
        return;
    stream_stop(); /* before the log line below, or it queues for a dead client */
    sock_close(s_client_fd);
    s_client_fd = SOCK_INVALID;
    Proto_InReset(&s_in);
    Log_Info("[ctlsrv] client gone (%s)", why);
}

/* Returns 0 when the whole buffer went out, -1 when the client is unreachable or
   too slow (caller drops it). Never waits longer than SEND_WAIT_MAX ms. */
static int send_all(const char *buf, int len) {
    int off = 0;
    int waits = 0;

    while (off < len) {
        int n = (int)send(s_client_fd, buf + off, len - off, SOCK_SEND_FLAGS);
        if (n > 0) {
            off += n;
            waits = 0;
            continue;
        }
        {
            int err = sock_last_error();
            if (n < 0 && sock_interrupted(err))
                continue;
            if (n < 0 && sock_would_block(err)) {
                struct pollfd pfd;
                if (++waits > SEND_WAIT_MAX)
                    return -1;
                pfd.fd = s_client_fd;
                pfd.events = POLLOUT;
                pfd.revents = 0;
                sock_poll(&pfd, 1, SEND_WAIT_MS);
                continue;
            }
        }
        return -1;
    }
    return 0;
}

/* Drain queued events to the client. Called from Poll only, never from inside a
   command, so a pushed line can never land in the middle of a response. */
static void flush_events(void) {
    char out[PROTO_EVENT_LINE_MAX + 32];
    long dropped;
    int sent;

    if (!s_streaming || !sock_valid(s_client_fd))
        return;

    dropped = Proto_RingTakeDropped(&s_ring);
    if (dropped > 0) {
        int n = snprintf(out, sizeof(out), PROTO_EVENT_PREFIX "[ctlsrv] dropped %ld line(s)\n",
                         dropped);
        if (send_all(out, n) != 0) {
            client_close("event send failed");
            return;
        }
    }

    for (sent = 0; sent < EVENT_FLUSH_PER_POLL; sent++) {
        const char *line = Proto_RingTake(&s_ring);
        int n;

        if (!line)
            break;
        n = snprintf(out, sizeof(out), PROTO_EVENT_PREFIX "%s\n", line);
        if (send_all(out, n) != 0) {
            client_close("event send failed");
            return;
        }
    }
}

/* --- Command execution ------------------------------------------------------ */

/* Run one line and ship what it printed. Splits on ';' the way --exec does, so a
   command string that works there works here unchanged. */
/* Verbs the socket answers itself. They steer this channel rather than the engine,
   so they are not console commands: an F12 user has no stream to turn on, and
   putting them in `cmdlist` would only offer them something inert. */
static int run_server_verb(const char *line) {
    if (strcmp(line, "stream on") == 0) {
        stream_start();
        resp_addf(s_streaming ? "stream: on (log records follow, prefixed \"%s\")"
                              : "stream: could not register a log sink",
                  PROTO_EVENT_PREFIX);
        return 1;
    }
    if (strcmp(line, "stream off") == 0) {
        stream_stop();
        resp_addf("stream: off");
        return 1;
    }
    if (strcmp(line, "stream") == 0) {
        resp_addf("stream: %s. Use `stream on` / `stream off`.", s_streaming ? "on" : "off");
        resp_addf("  Streams the engine log, which is where the trace verbs write:");
        resp_addf("  objtrace / lifetrace / cubetrace / camtrace / input trace.");
        resp_addf("  Set `loglevel debug` for the traces that log at debug severity.");
        return 1;
    }
    return 0;
}

static void run_line(char *line) {
    char *p = line;
    char *seg;

    Proto_RespReset(&s_resp);

    if (run_server_verb(line)) {
        if (send_all(s_resp.buf, s_resp.len) != 0 ||
            send_all(PROTO_TERMINATOR, (int)strlen(PROTO_TERMINATOR)) != 0)
            client_close("send failed");
        return;
    }

    Console_EnsureRegistered();

    s_in_command = 1;
    s_prev_sink = Console_SetLineSinkForTests(resp_sink);
    while (p && *p) {
        seg = p;
        p = strchr(p, ';');
        if (p)
            *p++ = '\0';
        while (*seg == ' ' || *seg == '\t')
            seg++;
        {
            size_t len = strlen(seg);
            while (len > 0 && (seg[len - 1] == ' ' || seg[len - 1] == '\t'))
                seg[--len] = '\0';
        }
        if (*seg) {
            Console_Execute(seg);
            s_commands++;
        }
    }
    Console_SetLineSinkForTests(s_prev_sink);
    s_prev_sink = NULL;
    s_in_command = 0;

    Proto_RespFinish(&s_resp);

    if (send_all(s_resp.buf, s_resp.len) != 0 ||
        send_all(PROTO_TERMINATOR, (int)strlen(PROTO_TERMINATOR)) != 0)
        client_close("send failed");
}

/* Reply to a command that never reached the console. */
static void reply_error(const char *fmt, ...) {
    va_list ap;
    char buf[PROTO_LINE_MAX];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    Proto_RespReset(&s_resp);
    resp_sink(buf);
    if (send_all(s_resp.buf, s_resp.len) != 0 ||
        send_all(PROTO_TERMINATOR, (int)strlen(PROTO_TERMINATOR)) != 0)
        client_close("send failed");
}

/* Run at most ONE queued command, and only a whole one.
 *
 * One line per presented frame, deliberately. Several commands executed inside a
 * single frame share that frame's side effects, and the engine has slots that hold
 * exactly one request until a present consumes it. The pending screenshot is one,
 * so `screenshot`, act, `screenshot` in one burst used to produce a single image
 * with nothing to say the other was dropped. Spending a frame per line costs well
 * under a millisecond and makes each command's effect its own.
 *
 * Returns 1 if a line ran, 0 if none was complete.
 */
static int run_one_pending(void) {
    char line[PROTO_LINE_MAX];

    switch (Proto_InTakeLine(&s_in, line, (int)sizeof(line))) {
    case PROTO_TAKE_LINE:
        run_line(line);
        return 1;
    case PROTO_TAKE_TOO_LONG:
        reply_error("[ctlsrv] command longer than %d bytes, ignored", PROTO_LINE_MAX - 1);
        return 1;
    case PROTO_TAKE_NO_NEWLINE:
        reply_error("[ctlsrv] %d bytes with no newline, input dropped", PROTO_RAW_MAX);
        return 1;
    case PROTO_TAKE_NONE:
    default:
        return 0;
    }
}

/* --- Public API ------------------------------------------------------------- */

int ControlServer_Start(int port) {
    struct sockaddr_in addr;
    int yes = 1;
    sock_t fd;

    if (sock_valid(s_listen_fd))
        return 0;

    if (sock_platform_init() != 0) {
        Log_Warn("[ctlsrv] socket layer would not start");
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!sock_valid(fd)) {
        Log_Warn("[ctlsrv] socket: %s", sock_error_text(sock_last_error()));
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, SOCK_OPTVAL(&yes), sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    /* Loopback only, never INADDR_ANY: this hands out arbitrary command
       execution, and nothing off this machine has any business reaching it. */
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        Log_Warn("[ctlsrv] bind 127.0.0.1:%d: %s", port, sock_error_text(sock_last_error()));
        sock_close(fd);
        return -1;
    }
    if (listen(fd, 1) != 0) {
        Log_Warn("[ctlsrv] listen: %s", sock_error_text(sock_last_error()));
        sock_close(fd);
        return -1;
    }
    if (sock_set_nonblocking(fd) != 0) {
        Log_Warn("[ctlsrv] non-blocking: %s", sock_error_text(sock_last_error()));
        sock_close(fd);
        return -1;
    }

    s_listen_fd = fd;
    Log_Info("[ctlsrv] listening on 127.0.0.1:%d", port);
    return 0;
}

void ControlServer_Poll(void) {
    char buf[READ_CHUNK];
    int n;

    if (!sock_valid(s_listen_fd))
        return;

    /* Re-entered from inside a command that is presenting frames. Do nothing at
       all here: not the accept, not the read, not the event flush. The command
       owns the response buffer and the displaced line sink until it returns, and
       everything below would take one or both from it.
       Console_IsExecuting covers the case this module cannot see for itself: the
       command presenting frames may have come from F12 or --exec-at rather than
       the socket, and Console_Execute parses into static storage, so starting a
       queued line here would overwrite the argv it is still reading. */
    if (s_in_command || Console_IsExecuting())
        return;

    if (!sock_valid(s_client_fd)) {
        sock_t fd = accept(s_listen_fd, NULL, NULL);
        if (!sock_valid(fd))
            return;
        if (sock_set_nonblocking(fd) != 0) {
            sock_close(fd);
            return;
        }
        {
            int yes = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, SOCK_OPTVAL(&yes), sizeof(yes));
        }
        s_client_fd = fd;
        Proto_InReset(&s_in);
        Log_Info("[ctlsrv] client connected");
        /* Greet, so a driver can block on the first <<END>> and know the engine
           is past boot and polling rather than guess with a sleep. */
        Proto_RespReset(&s_resp);
        resp_addf("lba2cc control server ready");
        if (send_all(s_resp.buf, s_resp.len) != 0 ||
            send_all(PROTO_TERMINATOR, (int)strlen(PROTO_TERMINATOR)) != 0) {
            client_close("greeting failed");
            return;
        }
    }

    n = (int)recv(s_client_fd, buf, sizeof(buf), 0);
    if (n == 0) {
        client_close("disconnected");
        return;
    }
    if (n < 0) {
        int err = sock_last_error();
        if (!sock_would_block(err) && !sock_interrupted(err)) {
            client_close(sock_error_text(err));
            return;
        }
    }
    if (n > 0) {
        /* Refused rather than clamped: dropping the excess would splice the bytes
           either side of the gap into one command, so a driver that overran once
           would go on getting answers to things it never sent. */
        if (Proto_InAppend(&s_in, buf, (int)n) != 0) {
            client_close("input overran the buffer; stream out of step");
            return;
        }
    }
    /* Events first: anything the last command set in motion reaches the driver
       before the next command's response, and never inside one. */
    flush_events();
    /* Always attempt a line, including on a poll that read nothing: a burst read
       earlier may still have commands queued behind the one this frame ran. */
    run_one_pending();
}

void ControlServer_Stop(void) {
    stream_stop();
    client_close("server stopping");
    if (sock_valid(s_listen_fd)) {
        sock_close(s_listen_fd);
        s_listen_fd = SOCK_INVALID;
        Log_Info("[ctlsrv] stopped after %ld command(s)", s_commands);
    }
    sock_platform_shutdown();
}

int ControlServer_IsRunning(void) {
    return sock_valid(s_listen_fd);
}

#else /* built without LBA2_CONTROL_SERVER */

/* Stubs so the call sites stay free of ifdefs. --listen already refuses to arm
   on a build without the feature, so Start is only reached if that check is
   ever bypassed; say so rather than fail silently. */

int ControlServer_Start(int port) {
    (void)port;
    fprintf(stderr, "[ctlsrv] not available: built without LBA2_CONTROL_SERVER\n");
    return -1;
}

void ControlServer_Poll(void) {
}

void ControlServer_Stop(void) {
}

int ControlServer_IsRunning(void) {
    return 0;
}

#endif
