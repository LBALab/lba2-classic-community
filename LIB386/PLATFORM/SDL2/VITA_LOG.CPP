#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "VITA_LOG.H"

#include <debugnet.h>
#include <psp2/net/net.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace {
FILE *s_trace = NULL;
FILE *s_stream = NULL;
FILE *s_stdout = NULL;
FILE *s_stderr = NULL;
int s_socket = -1;
bool s_network = false;

bool ValidIPv4(const char *ip) {
    for (int part = 0; part < 4; ++part) {
        unsigned int value = 0, digits = 0;
        while (*ip >= '0' && *ip <= '9') {
            if (++digits > 3)
                return false;
            value = value * 10 + (unsigned int)(*ip++ - '0');
        }
        if (!digits || value > 255)
            return false;
        if (part == 3)
            return *ip == '\0';
        if (*ip++ != '.')
            return false;
    }
    return false;
}

ssize_t WriteOutput(void *, const char *bytes, size_t length) {
    const size_t written = fwrite(bytes, 1, length, s_trace);
    if (written != length) {
        fprintf(s_stderr, "Vita boot trace write failed: %s\n", strerror(errno));
        return -1;
    }
    if (s_network) {
        for (size_t offset = 0; offset < length;) {
            const size_t count = length - offset > 1000 ? 1000 : length - offset;
            const int sent = sceNetSend(s_socket, bytes + offset, count, 0);
            if (sent != (int)count) {
                fprintf(s_trace, "[Vita boot] DEBUGNET send failed: 0x%08x; continuing with local trace\n",
                        (unsigned int)sent);
                s_network = false;
                break;
            }
            offset += count;
        }
    }
    return (ssize_t)length;
}

void InitNetwork(const char *path) {
    FILE *config = fopen(path, "rb");
    if (!config) {
        if (errno == ENOENT)
            fprintf(stderr, "[Vita boot] DEBUGNET disabled: put receiver IPv4 address in %s\n", path);
        else
            fprintf(stderr, "[Vita boot] Cannot read DEBUGNET config %s: %s\n", path, strerror(errno));
        return;
    }
    char ip[64];
    const size_t size = fread(ip, 1, sizeof(ip) - 1, config);
    const bool tooLong = fgetc(config) != EOF;
    const bool readError = ferror(config) != 0;
    const int closeResult = fclose(config);
    if (readError || closeResult != 0 || tooLong) {
        fprintf(stderr, "[Vita boot] Cannot read DEBUGNET IPv4 address from %s (maximum 63 bytes)\n", path);
        return;
    }
    ip[size] = '\0';
    size_t end = size;
    while (end && (ip[end - 1] == '\r' || ip[end - 1] == '\n' || ip[end - 1] == ' ' || ip[end - 1] == '\t'))
        ip[--end] = '\0';
    if (strlen(ip) != end || !ValidIPv4(ip)) {
        fprintf(stderr, "[Vita boot] Invalid DEBUGNET IPv4 address in %s\n", path);
        return;
    }
    fprintf(stderr, "[Vita boot] Starting DEBUGNET to %s:18194/UDP\n", ip);
    const int result = debugNetInit(ip, 18194, DEBUG);
    const debugNetConfiguration *configState = debugNetGetConf();
    if (configState)
        s_socket = configState->SocketFD;
    if (result != 1 || !configState || !configState->debugnet_initialized || s_socket < 0) {
        fprintf(stderr, "[Vita boot] DEBUGNET initialization failed: result=%d socket=%d\n", result, s_socket);
        return;
    }
    s_network = true;
    fprintf(stderr, "[Vita boot] DEBUGNET output ready (UDP delivery is best-effort)\n");
}
} // namespace

bool SDL2_VitaInitDiagnostics(const char *tracePath, const char *networkConfigPath) {
    if (s_stream) {
        fprintf(stderr, "Vita diagnostics are already active\n");
        return false;
    }
    s_trace = fopen(tracePath, "wb");
    if (!s_trace) {
        fprintf(stderr, "Cannot create Vita boot trace %s: %s\n", tracePath, strerror(errno));
        return false;
    }
    if (setvbuf(s_trace, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Cannot make Vita boot trace unbuffered\n");
        fclose(s_trace);
        s_trace = NULL;
        return false;
    }
    if (fputs("[Vita boot] Entered main; opening diagnostic output\n", s_trace) == EOF) {
        fprintf(stderr, "Cannot write Vita boot trace: %s\n", strerror(errno));
        fclose(s_trace);
        s_trace = NULL;
        return false;
    }
    cookie_io_functions_t functions = {};
    functions.write = WriteOutput;
    s_stream = fopencookie(NULL, "w", functions);
    if (!s_stream || setvbuf(s_stream, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Cannot create Vita diagnostic output stream: %s\n", strerror(errno));
        if (s_stream)
            fclose(s_stream);
        fclose(s_trace);
        s_stream = NULL;
        s_trace = NULL;
        return false;
    }
    s_stdout = stdout;
    s_stderr = stderr;
    stdout = s_stream;
    stderr = s_stream;
    InitNetwork(networkConfigPath);
    fprintf(stderr, "[Vita boot] Local stdout/stderr capture ready\n");
    return true;
}

void SDL2_VitaEndDiagnostics(void) {
    if (!s_stream)
        return;
    fprintf(stderr, "[Vita boot] Diagnostic shutdown\n");
    fflush(s_stream);
    /* Restore stdio before releasing anything its write callback uses. */
    stdout = s_stdout;
    stderr = s_stderr;
    if (fclose(s_stream) != 0)
        fprintf(stderr, "Cannot close Vita diagnostic stream: %s\n", strerror(errno));
    s_stream = NULL;
    s_network = false;
    if (s_socket >= 0) {
        const int result = sceNetSocketClose(s_socket);
        if (result < 0)
            fprintf(s_trace, "[Vita boot] Cannot close DEBUGNET socket: 0x%08x\n", (unsigned int)result);
        s_socket = -1;
    }
    /* debugNetFinish frees SceNet's heap without terminating SceNet. The
       process owns that heap until exit, after all atexit handlers finish. */
    if (fclose(s_trace) != 0)
        fprintf(stderr, "Cannot close Vita boot trace: %s\n", strerror(errno));
    s_trace = NULL;
}
