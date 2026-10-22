#include "../sdl_main_ready.h"
#include "../test_harness.h"
#include "../../LIB386/AIL/SDL/AUDIO_DEVICE.H"
#include <AIL/COMMON.H>
#include <SYSTEM/CRASH.H>
#include <vector>

#if defined(LBA2_USE_SDL2) && defined(LBA2_AUDIO_TESTS)
static const float *prepareOutput = NULL;
static int prepareCalls = 0;
static bool prepareSawUntouchedOutput = false;
#endif

void Crash_PrepareThread(void) {
#if defined(LBA2_USE_SDL2) && defined(LBA2_AUDIO_TESTS)
    if (prepareOutput) {
        ++prepareCalls;
        prepareSawUntouchedOutput = true;
        for (int i = 0; i < 8; ++i)
            if (prepareOutput[i] != 123.0f)
                prepareSawUntouchedOutput = false;
    }
#endif
}

static SDL_AudioSpec specification(int channels = 2, int rate = 44100) {
    SDL_AudioSpec spec = {};
    spec.format = AIL_AUDIO_S16;
    spec.channels = channels;
    spec.freq = rate;
    return spec;
}
static void test_simultaneous_output_streams() {
    SDL_AudioSpec spec = specification();
    AILAudioStream *sample = AILAudio_Open(&spec, NULL, NULL);
    AILAudioStream *music = AILAudio_Open(&spec, NULL, NULL);
    SDL_AudioSpec videoSpec = specification(1, 22050);
    AILAudioStream *video = AILAudio_Open(&videoSpec, NULL, NULL);
    ASSERT_TRUE(sample && music && video);
    AILAudio_Destroy(video);
    AILAudio_Destroy(music);
    AILAudio_Destroy(sample);
}

static void test_queue_gain_pause_and_cleanup() {
    SDL_AudioSpec spec = specification();
    AILAudioStream *stream = AILAudio_Open(&spec, NULL, NULL);
    ASSERT_TRUE(stream != NULL);
    if (!stream)
        return;
    short samples[2048] = {};
    ASSERT_TRUE(AILAudio_Put(stream, samples, sizeof(samples)));
    ASSERT_EQ_INT(sizeof(samples), AILAudio_Queued(stream));
    ASSERT_TRUE(AILAudio_Gain(stream, 0.5f));
    ASSERT_TRUE(AILAudio_Flush(stream));
    ASSERT_TRUE(AILAudio_Pause(stream));
    SDL_Delay(30);
    ASSERT_EQ_INT(sizeof(samples), AILAudio_Queued(stream));
#if !defined(LBA2_USE_SDL2) || !defined(LBA2_AUDIO_TESTS)
    ASSERT_TRUE(AILAudio_Resume(stream));
    const Uint64 start = AIL_Ticks();
    while (AILAudio_Queued(stream) > 0 && AIL_Ticks() - start < 1000)
        SDL_Delay(5);
    ASSERT_EQ_INT(0, AILAudio_Queued(stream));
    ASSERT_TRUE(AILAudio_Pause(stream));
#endif
    AILAudio_Destroy(stream);
}

static void test_wav_memory_loading() {
    const Uint8 wav[] = {
        'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0, 0x22, 0x56, 0, 0,
        0x22, 0x56, 0, 0, 1, 0, 8, 0, 'd', 'a', 't', 'a', 4, 0, 0, 0,
        0, 64, 128, 255};
    SDL_AudioSpec spec = {};
    Uint8 *pcm = NULL;
    Uint32 bytes = 0;
    ASSERT_TRUE(AILAudio_LoadWAVMemory(wav, sizeof(wav), &spec, &pcm, &bytes));
    ASSERT_EQ_INT(22050, spec.freq);
    ASSERT_EQ_INT(1, spec.channels);
    ASSERT_EQ_UINT(4, bytes);
    if (pcm && bytes == 4)
        ASSERT_MEM_EQ(wav + 44, pcm, bytes);
    SDL_free(pcm);
    ASSERT_TRUE(!AILAudio_LoadWAVMemory("bad", 3, &spec, &pcm, &bytes));
}

#if defined(LBA2_USE_SDL2) && defined(LBA2_AUDIO_TESTS)
static void test_mix_prepares_thread_before_output() {
    SDL_AudioSpec spec = specification();
    AILAudioStream *stream = AILAudio_Open(&spec, NULL, NULL);
    ASSERT_TRUE(stream != NULL);
    if (!stream)
        return;
    const short samples[8] = {16384, -16384, 16384, -16384,
                              16384, -16384, 16384, -16384};
    const float silence[8] = {};
    const float mixed[8] = {0.5f, -0.5f, 0.5f, -0.5f,
                            0.5f, -0.5f, 0.5f, -0.5f};
    ASSERT_TRUE(AILAudio_Put(stream, samples, sizeof(samples)));
    for (int active = 0; active < 2; ++active) {
        if (active)
            ASSERT_TRUE(AILAudio_Resume(stream));
        float output[8];
        for (int i = 0; i < 8; ++i)
            output[i] = 123.0f;
        prepareCalls = 0;
        prepareSawUntouchedOutput = false;
        prepareOutput = output;
        AILAudio_TestMix(output, 8);
        prepareOutput = NULL;
        ASSERT_EQ_INT(1, prepareCalls);
        ASSERT_TRUE(prepareSawUntouchedOutput);
        ASSERT_MEM_EQ(active ? mixed : silence, output, sizeof(output));
    }
    AILAudio_Destroy(stream);
}

static void test_converted_output_bytes_and_eof() {
    Uint32 rng = 0xDEADBEEF;
    for (int round = 0; round < 40; ++round) {
        SDL_AudioSpec spec = specification(round % 2 ? 1 : 2, round % 3 ? 44100 : 22050);
        if (round % 4 == 0)
            spec.format = AIL_AUDIO_U8;
        const int frames = 4096;
        std::vector<Uint8> input(frames * spec.channels * (AIL_AUDIO_BITSIZE(spec.format) / 8));
        for (size_t i = 0; i < input.size(); ++i) {
            rng = rng * 1103515245u + 12345u;
            input[i] = (Uint8)(rng >> 16);
        }
        AILAudioStream *stream = AILAudio_Open(&spec, NULL, NULL);
        SDL_AudioStream *reference = SDL_NewAudioStream(spec.format, spec.channels, spec.freq,
                                                        AUDIO_F32SYS, 2, 44100);
        ASSERT_TRUE(stream && reference);
        if (!stream || !reference) {
            AILAudio_Destroy(stream);
            SDL_FreeAudioStream(reference);
            continue;
        }
        ASSERT_TRUE(AILAudio_Put(stream, &input[0], (int)input.size()));
        ASSERT_EQ_INT(input.size(), AILAudio_Queued(stream));
        ASSERT_TRUE(AILAudio_Flush(stream));
        ASSERT_EQ_INT(0, SDL_AudioStreamPut(reference, &input[0], (int)input.size()));
        ASSERT_EQ_INT(0, SDL_AudioStreamFlush(reference));
        const int bytes = SDL_AudioStreamAvailable(reference);
        ASSERT_TRUE(bytes > 0);
        std::vector<float> expected(bytes / sizeof(float) + 16, 0.0f);
        std::vector<float> actual(expected.size(), 123.0f);
        ASSERT_EQ_INT(bytes, SDL_AudioStreamGet(reference, &expected[0], bytes));
        ASSERT_TRUE(AILAudio_Gain(stream, 0.5f));
        for (size_t i = 0; i < expected.size(); ++i)
            expected[i] *= 0.5f;
        AILAudio_TestRead(stream, &actual[0], (int)actual.size());
        ASSERT_MEM_EQ(&expected[0], &actual[0], expected.size() * sizeof(float));
        ASSERT_EQ_INT(0, AILAudio_Queued(stream));
        AILAudio_Destroy(stream);
        SDL_FreeAudioStream(reference);
    }
}

static void test_mix_pause_and_independent_lifetimes() {
    SDL_AudioSpec spec = specification();
    AILAudioStream *first = AILAudio_Open(&spec, NULL, NULL);
    AILAudioStream *second = AILAudio_Open(&spec, NULL, NULL);
    ASSERT_TRUE(first && second);
    if (!first || !second) {
        AILAudio_Destroy(first);
        AILAudio_Destroy(second);
        return;
    }
    short samples[8] = {16384, -16384, 16384, -16384, 16384, -16384, 16384, -16384};
    ASSERT_TRUE(AILAudio_Put(first, samples, sizeof(samples)));
    ASSERT_TRUE(AILAudio_Put(second, samples, sizeof(samples)));
    ASSERT_TRUE(AILAudio_Resume(first));
    ASSERT_TRUE(AILAudio_Gain(first, 0.5f));
    float output[4] = {};
    AILAudio_TestMix(output, 4);
    const float expectedFirst[4] = {0.25f, -0.25f, 0.25f, -0.25f};
    ASSERT_MEM_EQ(expectedFirst, output, sizeof(output));
    ASSERT_EQ_INT(sizeof(samples), AILAudio_Queued(second));
    ASSERT_TRUE(AILAudio_Resume(second));
    AILAudio_TestMix(output, 4);
    const float expectedBoth[4] = {0.75f, -0.75f, 0.75f, -0.75f};
    ASSERT_MEM_EQ(expectedBoth, output, sizeof(output));
    AILAudio_Destroy(first);
    AILAudio_TestMix(output, 4);
    const float expectedSecond[4] = {0.5f, -0.5f, 0.5f, -0.5f};
    ASSERT_MEM_EQ(expectedSecond, output, sizeof(output));
    AILAudio_Destroy(second);
}

static void SDLCALL produce(void *userdata, AILAudioStream *stream, int requested, int) {
    ++*(int *)userdata;
    std::vector<short> samples(requested / sizeof(short), 16384);
    ASSERT_TRUE(AILAudio_Put(stream, &samples[0], requested));
}
static void test_pull_callback_and_recursive_lock() {
    SDL_AudioSpec spec = specification();
    int calls = 0;
    AILAudioStream *stream = AILAudio_Open(&spec, produce, &calls);
    ASSERT_TRUE(stream != NULL);
    if (!stream)
        return;
    AILAudio_Lock(stream);
    AILAudio_Lock(stream);
    AILAudio_Unlock(stream);
    AILAudio_Unlock(stream);
    float actual[32], expected[32];
    for (int i = 0; i < 32; ++i)
        expected[i] = 0.5f;
    AILAudio_TestRead(stream, actual, 32);
    ASSERT_MEM_EQ(expected, actual, sizeof(actual));
    ASSERT_EQ_INT(1, calls);
    AILAudio_Destroy(stream);
}
#endif

int main() {
    TEST_SDL_MAIN_READY();
#ifdef LBA2_USE_SDL2
    SDL_SetMainReady();
#endif
    ASSERT_TRUE(AIL_SetHeadless());
    InitAIL();
    ASSERT_TRUE(SDL_WasInit(SDL_INIT_AUDIO) != 0);
    RUN_TEST(test_simultaneous_output_streams);
    RUN_TEST(test_queue_gain_pause_and_cleanup);
    RUN_TEST(test_wav_memory_loading);
#if defined(LBA2_USE_SDL2) && defined(LBA2_AUDIO_TESTS)
    RUN_TEST(test_mix_prepares_thread_before_output);
    RUN_TEST(test_converted_output_bytes_and_eof);
    RUN_TEST(test_mix_pause_and_independent_lifetimes);
    RUN_TEST(test_pull_callback_and_recursive_lock);
#endif
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    TEST_SUMMARY();
    return test_failures != 0;
}
