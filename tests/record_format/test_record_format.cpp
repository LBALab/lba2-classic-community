/* Host test: the text header of a recording.
 *
 * The header is what makes a recording self-describing, and a reader that gets a field
 * wrong does not fail loudly: it replays against the wrong bindings, or reloads a
 * snapshot it should not, or starts from a clock one step out. Every one of those
 * surfaces hundreds of ticks later as a state mismatch with no obvious cause, which is
 * the expensive way to find a parser bug.
 *
 * What this pins, and why each one is here rather than assumed:
 *
 *   - The binding round trip, which is the property a portable recording rests on: a
 *     table written and read back is the same table. A binding line is one pair per
 *     slot and runs past 380 characters, which is longer than a header-line buffer is
 *     likely to assume, so the truncation cases are checked as carefully as the
 *     success one. A writer that silently emits a short table produces a file that
 *     parses cleanly into different bindings.
 *
 *   - That a short table is refused rather than half-read. A partial binding table is
 *     not a partial answer, it is a different set of bindings, and the slots left
 *     behind would keep whatever the caller's array happened to hold.
 *
 *   - That a field reader stops at its own line. The values are found by key rather
 *     than by position, so nothing structurally stops a short binding line from
 *     reading on into the next field, and a path value has to keep its spaces while
 *     stopping at its newline.
 *
 *   - That a reader leaves its output alone when the key is absent, which is what lets
 *     a caller pre-set a default and is the difference between an old recording
 *     replaying with sane defaults and one replaying with zeroes.
 *
 * The field readers exist at all because the hand-written form they replaced,
 * sscanf(strstr(hdr, "setup.reloaded=") + 15, ...), carries the key's length as a
 * literal that has to be recounted whenever the key is renamed. The offset test below
 * is what makes that impossible to get wrong again.
 */
#include <cstdio>
#include <cstring>

#include <SYSTEM/ADELINE_TYPES.H>

#include "INPUT_BINDINGS.H"
#include "RECORD_FORMAT.H"

static int fails = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            std::fprintf(stderr, "%s:%d: FAIL: ", __FILE__, \
                         __LINE__);                         \
            std::fprintf(stderr, __VA_ARGS__);              \
            std::fprintf(stderr, "\n");                     \
            fails++;                                        \
        }                                                   \
    } while (0)

namespace {

/* A table with a different value in every cell, so a round trip that shifts, drops or
   duplicates a slot cannot come back looking correct. */
void FillDistinct(T_DEF_KEY *tab, S32 slots) {
    for (S32 i = 0; i < slots; i++) {
        tab[i].Key1 = (U32)(1000 + i * 2);
        tab[i].Key2 = (U32)(1001 + i * 2);
    }
}

int Same(const T_DEF_KEY *a, const T_DEF_KEY *b, S32 slots) {
    for (S32 i = 0; i < slots; i++) {
        if (a[i].Key1 != b[i].Key1 || a[i].Key2 != b[i].Key2)
            return 0;
    }
    return 1;
}

void CheckBindingRoundTrip() {
    T_DEF_KEY src[MAX_INPUT_SLOTS], dst[MAX_INPUT_SLOTS];
    char line[1024];

    FillDistinct(src, MAX_INPUT_SLOTS);
    std::memset(dst, 0, sizeof dst);

    CHECK(RecFmt_WriteBindings(line, sizeof line, src, MAX_INPUT_SLOTS,
                               "bindings.keyboard=") == 1,
          "writing a full table must succeed");
    CHECK(RecFmt_ParseBindings(line, "bindings.keyboard=", dst, MAX_INPUT_SLOTS) == 1,
          "reading it back must succeed");
    CHECK(Same(src, dst, MAX_INPUT_SLOTS), "the table must survive the round trip");
}

/* The retail tables specifically, since those are what a real recording carries. */
void CheckRetailTablesRoundTrip() {
    T_DEF_KEY dst[MAX_INPUT_SLOTS];
    char line[1024];

    RestoreInput();

    CHECK(RecFmt_WriteBindings(line, sizeof line, DefKeys, MAX_INPUT_SLOTS,
                               "bindings.keyboard=") == 1,
          "the retail keyboard table must fit");
    std::memset(dst, 0, sizeof dst);
    CHECK(RecFmt_ParseBindings(line, "bindings.keyboard=", dst, MAX_INPUT_SLOTS) == 1,
          "and read back");
    CHECK(Same(DefKeys, dst, MAX_INPUT_SLOTS), "unchanged");

    CHECK(RecFmt_WriteBindings(line, sizeof line, GamepadKeys, MAX_INPUT_SLOTS,
                               "bindings.gamepad=") == 1,
          "the retail pad table must fit");
    std::memset(dst, 0, sizeof dst);
    CHECK(RecFmt_ParseBindings(line, "bindings.gamepad=", dst, MAX_INPUT_SLOTS) == 1,
          "and read back");
    CHECK(Same(GamepadKeys, dst, MAX_INPUT_SLOTS), "unchanged");
}

/* How long the line actually is, stated rather than assumed. A caller sizing a buffer
   from a guess is the bug this number exists to prevent. */
void CheckLineIsLongerThanAHeaderLineLooks() {
    T_DEF_KEY src[MAX_INPUT_SLOTS];
    char line[1024];

    FillDistinct(src, MAX_INPUT_SLOTS);
    CHECK(RecFmt_WriteBindings(line, sizeof line, src, MAX_INPUT_SLOTS,
                               "bindings.keyboard=") == 1,
          "must fit in 1024");
    CHECK(std::strlen(line) > 256,
          "a binding line is longer than 256 bytes (got %d); a reader sized for a "
          "short header line will cut it",
          (int)std::strlen(line));
}

/* Truncation is refused, not emitted. The failure this prevents is a file that parses
   cleanly into a table the player never had. */
void CheckTruncationIsRefused() {
    T_DEF_KEY src[MAX_INPUT_SLOTS];
    char small[64];

    FillDistinct(src, MAX_INPUT_SLOTS);
    CHECK(RecFmt_WriteBindings(small, sizeof small, src, MAX_INPUT_SLOTS,
                               "bindings.keyboard=") == 0,
          "a buffer too small for the table must be refused");

    /* Exactly too small for the trailing newline, which is the boundary a length
       comparison written after the fact tends to get wrong. */
    {
        T_DEF_KEY one[1];
        char buf[32];
        one[0].Key1 = 7;
        one[0].Key2 = 9;
        CHECK(RecFmt_WriteBindings(buf, sizeof buf, one, 1, "k=") == 1,
              "one pair fits in 32 bytes");
        CHECK(!std::strcmp(buf, "k=7:9\n"), "and reads exactly `k=7:9\\n`, got `%s`", buf);

        /* "k=7:9\n" is six characters plus the terminator. */
        CHECK(RecFmt_WriteBindings(buf, 7, one, 1, "k=") == 1, "seven bytes is enough");
        CHECK(RecFmt_WriteBindings(buf, 6, one, 1, "k=") == 0,
              "six is not, and must be refused rather than dropping the newline");
    }
}

/* A short table is a different table, so it is refused rather than half-applied. */
void CheckShortTableIsRefused() {
    T_DEF_KEY dst[MAX_INPUT_SLOTS];
    const char *hdr = "bindings.keyboard=1:2,3:4\n";

    std::memset(dst, 0xEE, sizeof dst);
    CHECK(RecFmt_ParseBindings(hdr, "bindings.keyboard=", dst, MAX_INPUT_SLOTS) == 0,
          "a table with two pairs must not satisfy a request for %d",
          (int)MAX_INPUT_SLOTS);
    CHECK(RecFmt_ParseBindings(hdr, "bindings.keyboard=", dst, 2) == 1,
          "but two pairs are exactly two slots");
    CHECK(dst[0].Key1 == 1 && dst[0].Key2 == 2 && dst[1].Key1 == 3 && dst[1].Key2 == 4,
          "and they are the two that were written");
}

/* The table stops at its own newline. Without the bound, a short line followed by
   another comma-bearing field reads on and returns a full table built partly from the
   next field's numbers, which is worse than failing. */
void CheckParseStopsAtTheLine() {
    T_DEF_KEY dst[4];
    /* Two pairs on the binding line and three on the next field, so a walk that runs
       past the newline finds exactly the four pairs it was asked for and returns
       success. Counts that do not add up to `slots` fail either way and so would not
       test the bound at all. */
    const char *hdr =
        "bindings.keyboard=1:2,3:4\n"
        "something.else=5:6,7:8,9:10\n";

    std::memset(dst, 0, sizeof dst);
    CHECK(RecFmt_ParseBindings(hdr, "bindings.keyboard=", dst, 4) == 0,
          "a two-pair line must not be completed from the field below it");
    CHECK(dst[2].Key1 == 0 && dst[3].Key1 == 0,
          "and nothing from that field may reach the table (got %u, %u)",
          (unsigned)dst[2].Key1, (unsigned)dst[3].Key1);
}

void CheckMissingBindingKey() {
    T_DEF_KEY dst[MAX_INPUT_SLOTS];
    CHECK(RecFmt_ParseBindings("other=1:2\n", "bindings.keyboard=", dst,
                               MAX_INPUT_SLOTS) == 0,
          "an absent key must not report success");
    CHECK(RecFmt_ParseBindings(NULL, "bindings.keyboard=", dst, MAX_INPUT_SLOTS) == 0,
          "and neither must a null header");
}

const char *kHeader =
    "LBA2REC 8\n"
    "engine=0.13.0-dev\n"
    "mode.headless=1\n"
    "mode.fixed_dt=16\n"
    "setup.island=2\n"
    "setup.cube=65\n"
    "setup.snapshot=/home/someone/my recordings/session.rec.lba\n"
    "setup.reloaded=0\n"
    "setup.reload_clock=0\n"
    "clock.timer_ref_hr=5347837\n";

void CheckFieldReaders() {
    U32 u = 0;
    S32 i = 0;
    char s[256];

    CHECK(RecFmt_ReadU32(kHeader, "clock.timer_ref_hr=", &u) == 1 && u == 5347837u,
          "clock baseline reads back (got %u)", (unsigned)u);
    CHECK(RecFmt_ReadInt(kHeader, "setup.reloaded=", &i) == 1 && i == 0,
          "reloaded reads back (got %d)", (int)i);
    CHECK(RecFmt_ReadInt(kHeader, "setup.cube=", &i) == 1 && i == 65,
          "cube reads back (got %d)", (int)i);
    CHECK(RecFmt_ReadStr(kHeader, "setup.snapshot=", s, sizeof s) == 1,
          "snapshot path reads back");
    CHECK(!std::strcmp(s, "/home/someone/my recordings/session.rec.lba"),
          "a path keeps its spaces and stops at the newline, got `%s`", s);
}

/* The value starts at strlen(key), which is the whole reason these readers exist.
   `mode.fixed_dt=` and `mode.fixed_timestep=` share a prefix, so a reader that
   searched for the shorter key and stepped a hand-counted distance would land inside
   the longer one's value. */
void CheckKeyLengthIsNotHandCounted() {
    const char *hdr =
        "mode.fixed_timestep=0\n"
        "mode.fixed_dt=16\n";
    S32 v = -1;

    CHECK(RecFmt_ReadInt(hdr, "mode.fixed_dt=", &v) == 1 && v == 16,
          "the step reads 16 even though a longer key shares its prefix (got %d)",
          (int)v);
    v = -1;
    CHECK(RecFmt_ReadInt(hdr, "mode.fixed_timestep=", &v) == 1 && v == 0,
          "and the longer key reads its own value (got %d)", (int)v);
}

/* An absent key leaves the caller's default in place. Zeroing it instead is how an
   older recording would start replaying from clock zero rather than from its own. */
void CheckAbsentKeyLeavesTheDefault() {
    U32 u = 4242;
    S32 i = -7;
    char s[16];

    std::strcpy(s, "keepme");
    CHECK(RecFmt_ReadU32(kHeader, "not.here=", &u) == 0 && u == 4242u,
          "an absent u32 leaves the default (got %u)", (unsigned)u);
    CHECK(RecFmt_ReadInt(kHeader, "not.here=", &i) == 0 && i == -7,
          "an absent int leaves the default (got %d)", (int)i);
    CHECK(RecFmt_ReadStr(kHeader, "not.here=", s, sizeof s) == 0 && !std::strcmp(s, "keepme"),
          "an absent string leaves the default (got `%s`)", s);
}

/* A value longer than the caller's buffer is refused. A cut path is a wrong path, and
   it would be used to reload a snapshot. */
void CheckOversizedValueIsRefused() {
    char small[8];
    std::strcpy(small, "keepme");
    CHECK(RecFmt_ReadStr(kHeader, "setup.snapshot=", small, sizeof small) == 0,
          "a path longer than the buffer must be refused");
    CHECK(!std::strcmp(small, "keepme"), "and must not have written a partial path");
}

void CheckNonNumericValue() {
    const char *hdr = "setup.cube=banana\n";
    S32 v = 5;
    CHECK(RecFmt_ReadInt(hdr, "setup.cube=", &v) == 0 && v == 5,
          "a value that is not a number must not report success (got %d)", (int)v);
}

/* The last line of a header may arrive without its newline. */
void CheckValueAtEndOfBuffer() {
    const char *hdr = "engine=0.13.0-dev\nsetup.cube=42";
    S32 v = 0;
    char s[64];
    CHECK(RecFmt_ReadInt(hdr, "setup.cube=", &v) == 1 && v == 42,
          "a trailing field with no newline still reads (got %d)", (int)v);
    CHECK(RecFmt_ReadStr(hdr, "setup.cube=", s, sizeof s) == 1 && !std::strcmp(s, "42"),
          "and so does its string form, got `%s`", s);
}

} // namespace

int main() {
    CheckBindingRoundTrip();
    CheckRetailTablesRoundTrip();
    CheckLineIsLongerThanAHeaderLineLooks();
    CheckTruncationIsRefused();
    CheckShortTableIsRefused();
    CheckParseStopsAtTheLine();
    CheckMissingBindingKey();
    CheckFieldReaders();
    CheckKeyLengthIsNotHandCounted();
    CheckAbsentKeyLeavesTheDefault();
    CheckOversizedValueIsRefused();
    CheckNonNumericValue();
    CheckValueAtEndOfBuffer();

    if (fails == 0) {
        std::printf("test_record_format: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_record_format: %d failure(s)\n", fails);
    return 1;
}
