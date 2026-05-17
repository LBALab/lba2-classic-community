/* Host-only test for SOURCES/SAVENAME_VALIDATION.{H,CPP}.
 *
 * IsDatetimeReservedShape gates the keyboard text-entry flow so a typed
 * save name can't collide with a gamepad-generated datetime name. Pure
 * logic, no engine dependencies — table-driven test.
 */

#include <cassert>
#include <cstdio>
#include <cstring>

#include "SAVENAME_VALIDATION.H"

struct Case {
    const char *input;
    S32 expected; /* TRUE = reject (reserved), FALSE = accept */
    const char *why;
};

int main() {
    const Case cases[] = {
        /* Reserved (would collide with gamepad-generated names) */
        {"2026-05-17 14-30-00", TRUE, "bare datetime shape"},
        {"0000-00-00 00-00-00", TRUE, "all-zeros datetime"},
        {"9999-12-31 23-59-59", TRUE, "max-digit datetime"},
        {"Citadel Island - 2026-05-17 14-30-00", TRUE, "scene-prefixed"},
        {"X - 2026-05-17 14-30-00", TRUE, "minimal scene prefix"},
        {"Désert de la Feuille Blanche - 2026-05-17 14-30-00", TRUE,
         "longest known retail island prefix"},

        /* Accepted (real saves a player might want to type) */
        {"My Save", FALSE, "ordinary name"},
        {"", FALSE, "empty"},
        {"2026-05-17", FALSE, "date only, too short"},
        {"14-30-00", FALSE, "time only, too short"},
        {"2026-05-17 14-30-0", FALSE, "one digit short"},
        {"2026-05-17 14-30-00 hero", FALSE, "trailing word after datetime"},
        {"Hero 2026-05-17 14-30-00", FALSE, "space-glued prefix (no ' - ')"},
        {"Foo-2026-05-17 14-30-00", FALSE, "dash-glued prefix (no ' - ')"},
        {"X-2026-05-17 14-30-00", FALSE, "single-dash glue"},
        {"2026/05/17 14-30-00", FALSE, "wrong date separator"},
        {"2026-05-17_14-30-00", FALSE, "legacy underscore datetime form"},
        {"abcd-ef-gh ij-kl-mn", FALSE, "letters where digits expected"},
        {"2026-05-17  14-30-00", FALSE, "double space between date and time"},
    };

    const size_t n = sizeof(cases) / sizeof(cases[0]);
    size_t fails = 0;
    for (size_t i = 0; i < n; i++) {
        S32 got = IsDatetimeReservedShape(cases[i].input);
        if (got != cases[i].expected) {
            std::fprintf(stderr,
                         "FAIL: %-55s expected=%d got=%d (%s)\n",
                         cases[i].input, (int)cases[i].expected, (int)got,
                         cases[i].why);
            fails++;
        }
    }

    /* NULL input is its own bucket (avoiding NULL in the table makes the
     * harness simpler). */
    if (IsDatetimeReservedShape(NULL) != FALSE) {
        std::fprintf(stderr, "FAIL: NULL input must return FALSE\n");
        fails++;
    }

    assert(fails == 0);
    std::printf("test_save_name_validation: %zu cases passed\n", n + 1);
    return 0;
}
