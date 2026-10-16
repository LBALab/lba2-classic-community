#include "../test_harness.h"
#include "../../LIB386/PLATFORM/SDL2/SWITCH.H"

#include <SDL.h>
#include <switch.h>
#include <string>

static AppletOperationMode mode;
static Result createResult, showResult;
static int creates, shows, errors;
static std::string dialogText, fullscreenText;

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
    RUN_TEST(test_installation_and_output_dimensions);
    RUN_TEST(test_native_message_and_failures);
    TEST_SUMMARY();
    return test_failures ? 1 : 0;
}
