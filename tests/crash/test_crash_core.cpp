/* Host test for the crash report's platform-independent half (SYSTEM/CRASH_CORE.H):
 * line formatting, the module table, the frame cap and the state fields. The
 * block is captured through the test's own CrashSys_Write, so no signal is
 * raised here; the process-level test launches a child that really crashes. */
#include <SYSTEM/CRASH.H>
#include <SYSTEM/CRASH_CORE.H>
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

static char g_out[16384];
static size_t g_outLength;

void CrashSys_Write(const char *data, U32 size) {
    if (g_outLength + size >= sizeof g_out)
        size = (U32)(sizeof g_out - 1 - g_outLength);
    memcpy(g_out + g_outLength, data, size);
    g_outLength += size;
    g_out[g_outLength] = '\0';
}

static void capture_reset(void) {
    g_outLength = 0;
    g_out[0] = '\0';
}

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static int count_lines_starting(const char *prefix) {
    int count = 0;
    const char *p = g_out;
    size_t n = strlen(prefix);
    while (*p) {
        if (!strncmp(p, prefix, n))
            count++;
        p = strchr(p, '\n');
        if (!p)
            break;
        p++;
    }
    return count;
}

static void write_line_hex(U64 value) {
    CrashLine line;
    CrashLine_Begin(&line, "v ");
    CrashLine_Hex(&line, value);
    CrashLine_Write(&line);
}

static void write_line_dec(S64 value) {
    CrashLine line;
    CrashLine_Begin(&line, "v ");
    CrashLine_Dec(&line, value);
    CrashLine_Write(&line);
}

/* Numbers are formatted without stdio, so the edges are checked by hand. */
static void test_number_formatting(void) {
    capture_reset();
    write_line_hex(0);
    write_line_hex(0x10);
    write_line_hex(0xffffffffffffffffULL);
    write_line_dec(0);
    write_line_dec(-6);
    write_line_dec(2321310);
    write_line_dec((S64)0x8000000000000000ULL);
    ASSERT_TRUE(strcmp(g_out, "CRASH v 0x0\n"
                              "CRASH v 0x10\n"
                              "CRASH v 0xffffffffffffffff\n"
                              "CRASH v 0\n"
                              "CRASH v -6\n"
                              "CRASH v 2321310\n"
                              "CRASH v -9223372036854775808\n") == 0);
}

/* A line longer than the buffer is cut, and still ends in a newline. */
static void test_long_line_is_cut(void) {
    CrashLine line;
    int i;
    capture_reset();
    CrashLine_Begin(&line, "long");
    for (i = 0; i < 200; i++)
        CrashLine_Str(&line, " word");
    CrashLine_Write(&line);
    ASSERT_EQ_INT(CRASH_LINE_SIZE, (int)g_outLength);
    ASSERT_TRUE(g_out[g_outLength - 1] == '\n');
    ASSERT_TRUE(strchr(g_out, '\n') == g_out + g_outLength - 1);
}

static void add_module(const char *path, uintptr_t base, uintptr_t start, uintptr_t end, const U8 *id,
                       int idLength, int isUuid) {
    CrashModule *module = CrashModule_Reserve();
    ASSERT_TRUE(module != NULL);
    if (!module)
        return;
    module->base = base;
    module->start = start;
    module->end = end;
    CrashModule_SetName(module, path);
    memcpy(module->id, id, (size_t)idLength);
    module->idLength = (U8)idLength;
    module->idIsUuid = (U8)isUuid;
    CrashModule_Publish();
}

/* The test binary stands in for the engine: its module holds the core's code. */
static uintptr_t engine_address(void) {
    return (uintptr_t)&CrashBlock_EngineModule;
}

static const U8 k_uuid[16] = {0x26, 0x00, 0x22, 0xe5, 0xff, 0xee, 0x31, 0x87,
                              0x99, 0x16, 0x33, 0x22, 0x3c, 0xbd, 0xa9, 0xdf};
static const U8 k_buildId[4] = {0xde, 0xad, 0xbe, 0xef};

static void setup_modules(void) {
    uintptr_t here = engine_address();
    CrashModule_Reset();
    add_module("/opt/lib/libother.so", 0x1000, 0x1000, 0x2000, k_buildId, 4, 0);
    add_module("/Applications/Game.app/Contents/MacOS/lba2cc", here - 0x100, here - 0x100, here + 0x100, k_uuid,
               16, 1);
    add_module("C:\\Windows\\System32\\unused.dll", 0x9000, 0x9000, 0xa000, k_buildId, 4, 0);
}

/* The engine module comes before the frames; after them come only the modules a
 * frame fell in. An address in no module is printed as it is. */
static void test_block_modules_and_frames(void) {
    uintptr_t here = engine_address();
    setup_modules();
    capture_reset();

    CrashBlock_Begin();
    CrashBlock_EngineModule();
    CrashBlock_Frame(here);
    CrashBlock_Frame(0x1234);
    CrashBlock_Frame(0x4242);
    CrashBlock_EndFrames();
    CrashBlock_Modules();
    CrashBlock_End();

    /* base is an address, so check the parts that do not depend on it. */
    ASSERT_TRUE(strstr(g_out, "CRASH module lba2cc base=0x") != NULL);
    ASSERT_TRUE(strstr(g_out, " id=260022E5-FFEE-3187-9916-33223CBDA9DF\nCRASH frame 00 lba2cc+0x100\n") != NULL);
    ASSERT_TRUE(strstr(g_out, "CRASH frame 01 libother.so+0x234\n"
                              "CRASH frame 02 0x4242\n"
                              "CRASH module libother.so base=0x1000 id=deadbeef\n"
                              "CRASH ==== end ====\n") != NULL);
    ASSERT_EQ_INT(1, count_lines_starting("CRASH module lba2cc "));
    ASSERT_TRUE(strstr(g_out, "unused.dll") == NULL);
    ASSERT_TRUE(starts_with(g_out, "CRASH ==== fatal signal ====\nCRASH module lba2cc "));
}

/* A second block starts with no module marked used. */
static void test_block_resets_used_modules(void) {
    setup_modules();
    capture_reset();
    CrashBlock_Begin();
    CrashBlock_Frame(0x1800);
    CrashBlock_EndFrames();
    CrashBlock_Modules();
    capture_reset();
    CrashBlock_Begin();
    CrashBlock_Frame(0x4242);
    CrashBlock_EndFrames();
    CrashBlock_Modules();
    ASSERT_TRUE(strstr(g_out, "libother.so") == NULL);
}

/* Up to the head, every frame is written as it arrives. Past it, the last frames
 * are kept and a line says how many between were dropped. */
static void test_frame_cap_keeps_both_ends(void) {
    char expect[64];
    U32 total = 100;
    U32 i;
    CrashModule_Reset();
    capture_reset();
    CrashBlock_Begin();
    for (i = 0; i < total; i++)
        CrashBlock_Frame(0x10000 + i);
    CrashBlock_EndFrames();

    ASSERT_EQ_INT(CRASH_FRAMES_HEAD + CRASH_FRAMES_TAIL, count_lines_starting("CRASH frame "));
    ASSERT_TRUE(strstr(g_out, "CRASH frame 00 0x10000\n") != NULL);
    snprintf(expect, sizeof expect, "CRASH frame %u 0x%x\n", CRASH_FRAMES_HEAD - 1,
             0x10000 + CRASH_FRAMES_HEAD - 1);
    ASSERT_TRUE(strstr(g_out, expect) != NULL);
    snprintf(expect, sizeof expect, "CRASH frames skipped %u\n", total - CRASH_FRAMES_HEAD - CRASH_FRAMES_TAIL);
    ASSERT_TRUE(strstr(g_out, expect) != NULL);
    snprintf(expect, sizeof expect, "CRASH frame %u 0x%x\n", total - CRASH_FRAMES_TAIL,
             0x10000 + total - CRASH_FRAMES_TAIL);
    ASSERT_TRUE(strstr(g_out, expect) != NULL);
    ASSERT_TRUE(strstr(g_out, "CRASH frame 99 0x10063\n") != NULL);
    /* The tail is written in order, after the skipped line. */
    ASSERT_TRUE(strstr(g_out, "skipped") < strstr(g_out, "CRASH frame 84 "));
    ASSERT_TRUE(strstr(g_out, "CRASH frame 84 ") < strstr(g_out, "CRASH frame 99 "));
}

/* Just past the head, nothing is skipped and nothing is repeated. */
static void test_frame_cap_short_tail(void) {
    U32 i;
    CrashModule_Reset();
    capture_reset();
    CrashBlock_Begin();
    for (i = 0; i < CRASH_FRAMES_HEAD + 3; i++)
        CrashBlock_Frame(0x20000 + i);
    CrashBlock_EndFrames();
    ASSERT_EQ_INT(CRASH_FRAMES_HEAD + 3, count_lines_starting("CRASH frame "));
    ASSERT_TRUE(strstr(g_out, "skipped") == NULL);
    ASSERT_TRUE(strstr(g_out, "CRASH frame 34 0x20022\n") != NULL);
}

static S32 g_island = 4;
static S16 g_cube = -90;
static U8 g_flag = 200;
static S32 g_hero[3] = {15360, -4096, 24576};
static S8 g_signedByte = -3;
static U64 g_timer = 0x1122334455667788ULL;

/* Each size and signedness reads back as registered, a repeated name joins with
 * commas, and the values are read when the block is written. */
static void test_state_fields(void) {
    Crash_AddState("island", &g_island, 4, 1);
    Crash_AddState("cube", &g_cube, 2, 1);
    Crash_AddState("flag", &g_flag, 1, 0);
    Crash_AddState("hero", &g_hero[0], 4, 1);
    Crash_AddState("hero", &g_hero[1], 4, 1);
    Crash_AddState("hero", &g_hero[2], 4, 1);
    Crash_AddState("sb", &g_signedByte, 1, 1);
    Crash_AddState("timer", &g_timer, 8, 0);
    Crash_AddState("bad", &g_island, 3, 1); /* not a field size: ignored */
    g_island = 5;

    capture_reset();
    CrashBlock_State();
    ASSERT_TRUE(strcmp(g_out,
                       "CRASH state island=5 cube=-90 flag=200 hero=15360,-4096,24576 sb=-3 timer=0x1122334455667788\n") ==
                0);
}

/* A table too long for one line continues on another state line, and no field is
 * split between the two. Runs after test_state_fields, so it fills the rest. */
static void test_state_wraps(void) {
    static const char *const names[] = {"alpha_field", "bravo_field", "charlie_field", "delta_field",
                                        "echo_field", "foxtrot_field", "golf_field", "hotel_field",
                                        "india_field", "juliett_field", "kilo_field", "lima_field",
                                        "mike_field", "november_field", "oscar_field", "papa_field",
                                        "quebec_field", "romeo_field", "sierra_field", "tango_field",
                                        "uniform_field", "victor_field", "whiskey_field", "xray_field"};
    static S32 value = -1234567890;
    const char *p;
    int i;
    for (i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        Crash_AddState(names[i], &value, 4, 1);
    Crash_AddState("over_the_limit", &value, 4, 1); /* the table is full */

    capture_reset();
    CrashBlock_State();
    ASSERT_TRUE(count_lines_starting("CRASH state ") >= 2);
    ASSERT_TRUE(strstr(g_out, " xray_field=-1234567890\n") != NULL);
    ASSERT_TRUE(strstr(g_out, "over_the_limit") == NULL);
    for (p = g_out; *p;) {
        const char *nl = strchr(p, '\n');
        ASSERT_TRUE(nl != NULL);
        if (!nl)
            break;
        ASSERT_TRUE(nl - p < CRASH_LINE_SIZE);
        ASSERT_TRUE(nl[-1] >= '0' && nl[-1] <= '9'); /* ends on a value, not mid-field */
        p = nl + 1;
    }
}

static void test_build_line(void) {
    capture_reset();
    CrashCore_SetBuild("0.13.0-dev", "c045edbc024a");
    CrashBlock_Build();
    ASSERT_TRUE(starts_with(g_out, "CRASH build 0.13.0-dev c045edbc024a "));
    ASSERT_EQ_INT(1, count_lines_starting("CRASH build "));
}

int main(void) {
    RUN_TEST(test_number_formatting);
    RUN_TEST(test_long_line_is_cut);
    RUN_TEST(test_block_modules_and_frames);
    RUN_TEST(test_block_resets_used_modules);
    RUN_TEST(test_frame_cap_keeps_both_ends);
    RUN_TEST(test_frame_cap_short_tail);
    RUN_TEST(test_state_fields);
    RUN_TEST(test_state_wraps);
    RUN_TEST(test_build_line);
    TEST_SUMMARY();
    return test_failures != 0;
}
