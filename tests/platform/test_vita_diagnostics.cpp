#include "../test_harness.h"
#include "../../LIB386/PLATFORM/SDL2/VITA_LOG.H"
#include <debugnet.h>
#include <psp2/net/net.h>

#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

static std::string directory;
static std::string packets;
static std::string receiver;
static int initCalls, sendCalls, closeCalls;
static bool rejectInit, rejectSend;
static debugNetConfiguration configuration;

extern "C" int debugNetInit(const char *ip, int port, int level) {
    ++initCalls;
    receiver = ip;
    if (port != 18194 || level != DEBUG)
        return 0;
    configuration = {rejectInit ? 0 : 1, rejectInit ? -1 : 42, level};
    return configuration.debugnet_initialized;
}
extern "C" debugNetConfiguration *debugNetGetConf(void) {
    return &configuration;
}
extern "C" int sceNetSend(int socket, const void *data, unsigned int length, int flags) {
    ++sendCalls;
    if (rejectSend || socket != 42 || flags != 0 || length > 1000)
        return -42;
    packets.append((const char *)data, length);
    return (int)length;
}
extern "C" int sceNetSocketClose(int socket) {
    ++closeCalls;
    return socket == 42 ? 0 : -1;
}

static std::string path(const char *name) {
    return directory + "/" + name;
}
static std::string trace() {
    std::ifstream file(path("boot.log"));
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
static void reset(const char *ip) {
    unlink(path("boot.log").c_str());
    unlink(path("receiver.txt").c_str());
    packets.clear();
    receiver.clear();
    initCalls = sendCalls = closeCalls = 0;
    rejectInit = rejectSend = false;
    if (ip) {
        std::ofstream file(path("receiver.txt"));
        file << ip;
    }
}
static bool start() {
    return SDL2_VitaInitDiagnostics(path("boot.log").c_str(), path("receiver.txt").c_str());
}

static void test_local_capture_before_user_directory() {
    reset(NULL);
    FILE *originalOut = stdout, *originalError = stderr;
    const bool ok = start();
    if (ok) {
        fprintf(stdout, "stdout before engine init\n");
        fprintf(stderr, "stderr before engine init\n");
        SDL2_VitaEndDiagnostics();
    }
    ASSERT_TRUE(ok);
    ASSERT_TRUE(stdout == originalOut && stderr == originalError);
    const std::string text = trace();
    ASSERT_TRUE(text.find("Entered main") != std::string::npos);
    ASSERT_TRUE(text.find("stdout before engine init\n") != std::string::npos);
    ASSERT_TRUE(text.find("stderr before engine init\n") != std::string::npos);
    ASSERT_TRUE(text.find("Diagnostic shutdown") != std::string::npos);
    ASSERT_EQ_INT(0, initCalls);
}

static void test_network_and_long_output() {
    reset("192.168.1.42\r\n");
    const bool ok = start();
    const std::string message(4096, 'x');
    if (ok) {
        fwrite(message.data(), 1, message.size(), stdout);
        SDL2_VitaEndDiagnostics();
    }
    ASSERT_TRUE(ok);
    ASSERT_EQ_INT(1, initCalls);
    ASSERT_TRUE(receiver == "192.168.1.42");
    ASSERT_TRUE(trace().find(message) != std::string::npos);
    ASSERT_TRUE(packets.find(message) != std::string::npos);
    ASSERT_TRUE(packets.find("Diagnostic shutdown") != std::string::npos);
    ASSERT_EQ_INT(1, closeCalls);
}

static void test_network_failures_keep_local_evidence() {
    for (int failure = 0; failure < 2; ++failure) {
        reset("192.168.1.42\n");
        rejectInit = failure == 0;
        rejectSend = failure == 1;
        const bool ok = start();
        if (ok) {
            fprintf(stderr, "local output survives network failure\n");
            SDL2_VitaEndDiagnostics();
        }
        ASSERT_TRUE(ok);
        ASSERT_TRUE(trace().find(failure == 0 ? "initialization failed" : "send failed") != std::string::npos);
        ASSERT_TRUE(trace().find("local output survives network failure") != std::string::npos);
        ASSERT_EQ_INT(failure == 0 ? 0 : 1, closeCalls);
    }
}

static void test_invalid_network_config() {
    const char *invalid[] = {"", "999.1.1.1", "1.2.3", "1.2.3.4 extra", "1.2.3.4\n5.6.7.8",
                             "4294967297.0.0.1", "1.2.3.-1", "1.2.3.+1"};
    for (unsigned int i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        reset(invalid[i]);
        const bool ok = start();
        if (ok)
            SDL2_VitaEndDiagnostics();
        ASSERT_TRUE(ok);
        ASSERT_EQ_INT(0, initCalls);
        ASSERT_TRUE(trace().find("Invalid DEBUGNET IPv4") != std::string::npos);
    }
}

static void test_trace_open_failure_keeps_stdio() {
    reset(NULL);
    FILE *originalOut = stdout, *originalError = stderr;
    ASSERT_TRUE(!SDL2_VitaInitDiagnostics(path("missing/boot.log").c_str(), path("receiver.txt").c_str()));
    ASSERT_TRUE(stdout == originalOut && stderr == originalError);
    ASSERT_EQ_INT(0, initCalls);
    SDL2_VitaEndDiagnostics();
}

int main() {
    char temporary[] = "vita-diagnostics-XXXXXX";
    char *created = mkdtemp(temporary);
    if (!created)
        return 1;
    directory = created;
    RUN_TEST(test_local_capture_before_user_directory);
    RUN_TEST(test_network_and_long_output);
    RUN_TEST(test_network_failures_keep_local_evidence);
    RUN_TEST(test_invalid_network_config);
    RUN_TEST(test_trace_open_failure_keeps_stdio);
    reset(NULL);
    rmdir(directory.c_str());
    TEST_SUMMARY();
    return test_failures != 0;
}
