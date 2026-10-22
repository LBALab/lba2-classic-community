#include "../sdl_main_ready.h"
#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/SDK.H"
#include "../../LIB386/AIL/SDL/STREAM_SDL.H"
#include <AIL/COMMON.H>
#include <AIL/STREAM.H>
#include <AIL/VIDEO_AUDIO.H>
#include <SYSTEM/CRASH.H>
#include <string.h>
#include <vector>

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
    char largePath[] = TEST_TMP_DIR "/audio-playback-large.wav";
    std::vector<unsigned char> large(44 + 4 * 1024 * 1024, 128);
    const unsigned char header[] = {
        'R', 'I', 'F', 'F', 36, 0, 64, 0, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0, 0x22, 0x56, 0, 0,
        0x22, 0x56, 0, 0, 1, 0, 8, 0, 'd', 'a', 't', 'a', 0, 0, 64, 0};
    memcpy(&large[0], header, sizeof(header));
    FILE *file = fopen(largePath, "wb");
    ASSERT_TRUE(file != NULL);
    if (file) {
        ASSERT_EQ_INT((int)large.size(), (int)fwrite(&large[0], 1, large.size(), file));
        fclose(file);
        PlayStream(largePath);
        ASSERT_TRUE(IsStreamPlaying());
        ASSERT_TRUE(Stream_TestQueuedBytes() <= 65536);
        PauseStream();
        Uint32 filling = (Uint32)SDL_GetTicks();
        while (Stream_TestQueuedBytes() <= 65536 - 2048 &&
               (Uint32)SDL_GetTicks() - filling < 2000)
            SDL_Delay(2);
        int queued = Stream_TestQueuedBytes();
        ASSERT_TRUE(queued > 65536 - 2048 && queued <= 65536);
        ResumeStreamOutput();
        SDL_Delay(40);
        ASSERT_EQ_INT(queued, Stream_TestQueuedBytes());
        ASSERT_EQ_INT(0, IsStreamPlaying());
        ResumeStream();
        ASSERT_TRUE(IsStreamPlaying());
        PauseStream();
        PlayStream(wavPath);
        ResumeStream();
        ASSERT_TRUE(strcmp(StreamName(), largePath) == 0);
        ASSERT_TRUE(IsStreamPlaying());
        PauseStream();
        Uint32 stopped = (Uint32)SDL_GetTicks();
        StopStream();
        ASSERT_TRUE((Uint32)SDL_GetTicks() - stopped < 500);
        remove(largePath);
    }
    PlayStream(wavPath);
    Uint32 playing = (Uint32)SDL_GetTicks();
    while (IsStreamPlaying() && (Uint32)SDL_GetTicks() - playing < 2000)
        SDL_Delay(2);
    ASSERT_EQ_INT(0, IsStreamPlaying());
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
    TEST_SDL_MAIN_READY();
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
