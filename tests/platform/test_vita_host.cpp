#include "../test_harness.h"
#include "../../LIB386/PLATFORM/SDL2/PRIVATE.H"
#include "../../LIB386/PLATFORM/SDL2/VITA.H"

#include <string>

static void test_installation_and_data_override() {
    ASSERT_TRUE(strcmp(SDL2_VitaBasePath(), "app0:/") == 0);
    ASSERT_TRUE(strcmp(SDL2_VitaUserPath(), "ux0:/data/lba2cc/user/") == 0);
    const char *original = SDL_getenv("LBA2_GAME_DIR");
    const std::string saved = original ? original : "";
    ASSERT_EQ_INT(0, SDL_setenv("LBA2_GAME_DIR", "", 1));
    ASSERT_TRUE(SDL2_VitaInitGameData());
    ASSERT_TRUE(strcmp(SDL_getenv("LBA2_GAME_DIR"), "ux0:/data/lba2/") == 0);
    ASSERT_TRUE(SDL2_VitaInitGameData());
    ASSERT_TRUE(strcmp(SDL_getenv("LBA2_GAME_DIR"), "ux0:/data/lba2/") == 0);
    ASSERT_EQ_INT(0, SDL_setenv("LBA2_GAME_DIR", "uma0:/my-games/lba2/", 1));
    ASSERT_TRUE(SDL2_VitaInitGameData());
    ASSERT_TRUE(strcmp(SDL_getenv("LBA2_GAME_DIR"), "uma0:/my-games/lba2/") == 0);
    ASSERT_EQ_INT(0, SDL_setenv("LBA2_GAME_DIR", saved.c_str(), 1));
}

static void test_native_window_configuration() {
    SDL_ClearHints();
    int width = 1, height = 2;
    ASSERT_TRUE(SDL2_VitaConfigureWindow(&width, &height));
    ASSERT_EQ_INT(960, width);
    ASSERT_EQ_INT(544, height);
    ASSERT_TRUE(strcmp(SDL_GetHint(SDL_HINT_RENDER_DRIVER), "VITA gxm") == 0);
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "fixture", SDL_HINT_OVERRIDE));
    width = 3;
    height = 4;
    ASSERT_TRUE(!SDL2_VitaConfigureWindow(&width, &height));
    ASSERT_TRUE(strstr(SDL_GetError(), "Vita GXM") != NULL);
    ASSERT_EQ_INT(3, width);
    ASSERT_EQ_INT(4, height);
    SDL_ClearHints();
}

int main() {
    RUN_TEST(test_installation_and_data_override);
    RUN_TEST(test_native_window_configuration);
    TEST_SUMMARY();
    return test_failures != 0;
}
