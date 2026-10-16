#include "../test_harness.h"
#include "../../LIB386/PLATFORM/SDL2/SWITCH.H"

#include <SDL.h>
#include <errno.h>
#include <switch.h>
#include <unistd.h>
#include <string>

static AppletOperationMode mode;
static Result createResult, showResult;
static int creates, shows, errors;
static std::string dialogText, fullscreenText;
static Result socketResult;
static int socketInits, socketExits, connections, warnings;
static int connectionSocket = -1;
static bool connectionFails;

extern "C" {
struct in_addr __nxlink_host = {0};
}

extern "C" Result socketInitializeDefault(void) {
    ++socketInits;
    return socketResult;
}
extern "C" int nxlinkStdio(void) {
    ++connections;
    if (connectionFails) {
        errno = ECONNREFUSED;
        return -1;
    }
    connectionSocket = dup(fileno(stderr));
    ASSERT_TRUE(connectionSocket >= 0);
    return connectionSocket;
}
extern "C" void socketExit(void) {
    ++socketExits;
    if (connectionSocket >= 0) {
        const int probe = dup(connectionSocket);
        const int probeError = errno;
        if (probe >= 0)
            close(probe);
        ASSERT_EQ_INT(-1, probe);
        ASSERT_EQ_INT(EBADF, probeError);
        connectionSocket = -1;
    }
}

extern "C" AppletOperationMode appletGetOperationMode(void) {
    return mode;
}
extern "C" Result errorApplicationCreate(ErrorApplicationConfig *config, const char *dialog, const char *fullscreen) {
    ++creates;
    config->initialized = 1;
    dialogText = dialog;
    fullscreenText = fullscreen;
    return createResult;
}
extern "C" Result errorApplicationShow(ErrorApplicationConfig *config) {
    ++shows;
    ASSERT_EQ_INT(1, config->initialized);
    return showResult;
}

static void capture_log(void *, PlatformLogLevel level, const char *) {
    if (level == PLATFORM_LOG_ERROR)
        ++errors;
    if (level == PLATFORM_LOG_WARN)
        ++warnings;
}

static void test_netloader_lifecycle_and_failures() {
    socketResult = 0;
    socketInits = socketExits = connections = warnings = 0;
    connectionFails = false;
    __nxlink_host.s_addr = 0;
    Platform_SetLogOutput(capture_log, NULL);

    ASSERT_TRUE(SDL2_SwitchInitNetloader());
    SDL2_SwitchEndNetloader();
    ASSERT_EQ_INT(0, socketInits);
    ASSERT_EQ_INT(0, connections);
    ASSERT_EQ_INT(0, socketExits);

    __nxlink_host.s_addr = 1;
    socketResult = 0x789;
    ASSERT_TRUE(!SDL2_SwitchInitNetloader());
    ASSERT_TRUE(strstr(SDL_GetError(), "0x00000789") != NULL);
    SDL2_SwitchEndNetloader();
    ASSERT_EQ_INT(1, socketInits);
    ASSERT_EQ_INT(0, connections);
    ASSERT_EQ_INT(0, socketExits);
    ASSERT_EQ_INT(1, warnings);

    socketResult = 0;
    connectionFails = true;
    ASSERT_TRUE(!SDL2_SwitchInitNetloader());
    ASSERT_TRUE(strstr(SDL_GetError(), strerror(ECONNREFUSED)) != NULL);
    SDL2_SwitchEndNetloader();
    ASSERT_EQ_INT(2, socketInits);
    ASSERT_EQ_INT(1, connections);
    ASSERT_EQ_INT(1, socketExits);
    ASSERT_EQ_INT(2, warnings);

    connectionFails = false;
    ASSERT_TRUE(SDL2_SwitchInitNetloader());
    ASSERT_TRUE(SDL2_SwitchInitNetloader());
    ASSERT_EQ_INT(3, socketInits);
    ASSERT_EQ_INT(2, connections);
    ASSERT_EQ_INT(1, socketExits);
    SDL2_SwitchEndNetloader();
    SDL2_SwitchEndNetloader();
    ASSERT_EQ_INT(2, socketExits);
    ASSERT_EQ_INT(2, warnings);

    ASSERT_TRUE(SDL2_SwitchInitNetloader());
    SDL2_SwitchEndNetloader();
    ASSERT_EQ_INT(4, socketInits);
    ASSERT_EQ_INT(3, connections);
    ASSERT_EQ_INT(3, socketExits);
    __nxlink_host.s_addr = 0;
    Platform_SetLogOutput(NULL, NULL);
}

static void test_installation_and_output_dimensions() {
    ASSERT_TRUE(strcmp(SDL2_SwitchBasePath(), "sdmc:/switch/lba2cc/") == 0);
    ASSERT_TRUE(strcmp(SDL2_SwitchUserPath(), "sdmc:/switch/lba2cc/user/") == 0);
    int width = 0, height = 0;
    mode = AppletOperationMode_Handheld;
    SDL2_SwitchWindowSize(&width, &height);
    ASSERT_EQ_INT(1280, width);
    ASSERT_EQ_INT(720, height);
    mode = AppletOperationMode_Console;
    SDL2_SwitchWindowSize(&width, &height);
    ASSERT_EQ_INT(1920, width);
    ASSERT_EQ_INT(1080, height);
    mode = AppletOperationMode_Handheld;
    SDL2_SwitchWindowSize(&width, &height);
    ASSERT_EQ_INT(1280, width);
    ASSERT_EQ_INT(720, height);
}

static void test_native_message_and_failures() {
    createResult = showResult = 0;
    creates = shows = errors = 0;
    Platform_SetLogOutput(capture_log, NULL);
    ASSERT_TRUE(SDL2_SwitchMessageBox("Game data not found", "Missing LBA2.HQR"));
    ASSERT_EQ_INT(1, creates);
    ASSERT_EQ_INT(1, shows);
    ASSERT_TRUE(dialogText == "Game data not found");
    ASSERT_TRUE(fullscreenText ==
                "Game data not found\n\nMissing LBA2.HQR\n\n"
                "Game files: sdmc:/switch/lba2cc/data/\n"
                "Settings, saves and logs: sdmc:/switch/lba2cc/user/\n"
                "See README.txt for setup help.");
    ASSERT_EQ_INT(0, errors);

    createResult = 0x123;
    ASSERT_TRUE(!SDL2_SwitchMessageBox("Failure", "Create failed"));
    ASSERT_EQ_INT(2, creates);
    ASSERT_EQ_INT(1, shows);
    ASSERT_TRUE(strstr(SDL_GetError(), "0x00000123") != NULL);
    ASSERT_EQ_INT(1, errors);

    createResult = 0;
    showResult = 0x456;
    ASSERT_TRUE(!SDL2_SwitchMessageBox("Failure", "Show failed"));
    ASSERT_EQ_INT(3, creates);
    ASSERT_EQ_INT(2, shows);
    ASSERT_TRUE(strstr(SDL_GetError(), "0x00000456") != NULL);
    ASSERT_EQ_INT(2, errors);

    const std::string longMessage(2048, 'x');
    ASSERT_TRUE(!SDL2_SwitchMessageBox("Failure", longMessage.c_str()));
    ASSERT_EQ_INT(3, creates);
    ASSERT_EQ_INT(2, shows);
    ASSERT_TRUE(strcmp(SDL_GetError(), "Switch message is too long") == 0);
    ASSERT_EQ_INT(3, errors);
    Platform_SetLogOutput(NULL, NULL);
}

int main() {
    RUN_TEST(test_netloader_lifecycle_and_failures);
    RUN_TEST(test_installation_and_output_dimensions);
    RUN_TEST(test_native_message_and_failures);
    TEST_SUMMARY();
    return test_failures ? 1 : 0;
}
