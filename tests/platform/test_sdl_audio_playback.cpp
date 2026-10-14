#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/SDK.H"
#include <AIL/COMMON.H>
#include <AIL/STREAM.H>
#include <AIL/VIDEO_AUDIO.H>
#include <SYSTEM/CRASH.H>
#include <string.h>

/* Playback is tested without installing the process-wide crash handler. */
void Crash_PrepareThread(void) {}

static int logCalls = 0;
static char wavPath[] = TEST_TMP_DIR "/audio-playback-fixture.wav";
static void SDLCALL count_log(void *, int, SDL_LogPriority, const char *) {
    ++logCalls;
}

static void test_disabled_audio_skips_playback() {
    ASSERT_EQ_INT(0, SDL_WasInit(SDL_INIT_AUDIO));
    logCalls = 0;
    SDL_ClearError();
    OpenStream();
    PlayStream(wavPath);
    char missingPath[] = TEST_TMP_DIR "/missing-audio-fixture.ogg";
    PlayStream(missingPath);
    ASSERT_EQ_INT(0, IsStreamPlaying());
    ASSERT_TRUE(StreamName()[0] == '\0');
    PauseStream();
    ResumeStream();
    ASSERT_EQ_INT(0, StartVideoAudio(22050, 1, 0));
    const unsigned char pcm[] = {0, 64, 128, 255};
    PushVideoAudio(pcm, sizeof(pcm));
    ResumeVideoAudio();
    PauseVideoAudio();
    StopVideoAudio();
    StopStream();
    ASSERT_EQ_INT(0, logCalls);
    ASSERT_TRUE(SDL_GetError()[0] == '\0');
    ASSERT_EQ_INT(0, SDL_WasInit(SDL_INIT_AUDIO));
}

static void test_enabled_audio_and_real_errors() {
    InitAIL();
    ASSERT_TRUE(SDL_WasInit(SDL_INIT_AUDIO) != 0);
    logCalls = 0;
    PlayStream(wavPath);
    ASSERT_TRUE(strcmp(StreamName(), wavPath) == 0);
    StopStream();
    ASSERT_EQ_INT(1, StartVideoAudio(22050, 1, 0));
    StopVideoAudio();
    ASSERT_EQ_INT(0, logCalls);
    SDL_ClearError();
    ASSERT_EQ_INT(0, StartVideoAudio(-1, 1, 0));
    ASSERT_TRUE(logCalls > 0);
    ASSERT_TRUE(SDL_GetError()[0] != '\0');
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

int main() {
#ifdef LBA2_USE_SDL2
    SDL_SetMainReady();
#endif
    const unsigned char wav[] = {
        'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0, 0x22, 0x56, 0, 0,
        0x22, 0x56, 0, 0, 1, 0, 8, 0, 'd', 'a', 't', 'a', 4, 0, 0, 0,
        0, 64, 128, 255};
    FILE *file = fopen(wavPath, "wb");
    ASSERT_TRUE(file != NULL);
    if (!file)
        return 1;
    ASSERT_EQ_INT(sizeof(wav), fwrite(wav, 1, sizeof(wav), file));
    ASSERT_EQ_INT(0, fclose(file));
    ASSERT_TRUE(AIL_SetHeadless());
    SDL_LogOutputFunction previous;
    void *previousData;
#ifdef LBA2_USE_SDL2
    SDL_LogGetOutputFunction(&previous, &previousData);
    SDL_LogSetOutputFunction(count_log, NULL);
#else
    SDL_GetLogOutputFunction(&previous, &previousData);
    SDL_SetLogOutputFunction(count_log, NULL);
#endif
    RUN_TEST(test_disabled_audio_skips_playback);
    RUN_TEST(test_enabled_audio_and_real_errors);
    RUN_TEST(test_disabled_audio_skips_playback);
#ifdef LBA2_USE_SDL2
    SDL_LogSetOutputFunction(previous, previousData);
#else
    SDL_SetLogOutputFunction(previous, previousData);
#endif
    ASSERT_EQ_INT(0, remove(wavPath));
    TEST_SUMMARY();
    return test_failures != 0;
}
