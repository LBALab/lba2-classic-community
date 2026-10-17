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

    /* SaveFileStemFromName: names are CP850, as the game holds them. */
    struct StemCase {
        const char *name;
        const char *stem;
        const char *why;
    };
    const StemCase stems[] = {
        {"D\x82sert de la Feuille Blanche - 2026-10-17 23-38-06",
         "Desert de la Feuille Blanche - 2026-10-17 23-38-06", "French island, the #719 repro"},
        {"Sendell-H\x94hle", "Sendell-Hohle", "German o umlaut"},
        {"W\x81ste des Leeren Blattes", "Wuste des Leeren Blattes", "German u umlaut"},
        {"Celebraci\xA2n", "Celebracion", "Spanish o acute"},
        {"Celebra\x87\xC6o", "Celebracao", "Portuguese c cedilla, a tilde"},
        {"Po\x87os de Sendell", "Pocos de Sendell", "Portuguese c cedilla"},
        {"\x90t\x82", "Ete", "capital E acute"},
        {"Stra\xE1"
         "e",
         "Strasse", "sharp s folds to two letters"},
        {"\xC9\xCD", "__", "box drawing has no letter"},
        {"Citadel Island - 2026-10-17 23-38-06", "Citadel Island - 2026-10-17 23-38-06",
         "ASCII passes through"},
        {"a/b\\c:d*e?f\"g<h>i|j", "a_b_c_d_e_f_g_h_i_j", "characters Windows forbids"},
        {"tab\there", "tab_here", "control character"},
        {"name. . ", "name", "trailing dots and spaces"},
        {"...", "save", "nothing left"},
        {"", "save", "empty"},
        {"CON", "_CON", "device name"},
        {"nul", "_nul", "device name, any case"},
        {"Com7", "_Com7", "COM port"},
        {"lpt1.x", "_lpt1.x", "device name before a dot"},
        {"CONSOLE", "CONSOLE", "longer than a device name"},
        {"COM0", "COM0", "COM0 is not a device"},
    };
    const size_t ns = sizeof(stems) / sizeof(stems[0]);
    for (size_t i = 0; i < ns; i++) {
        char out[128];
        SaveFileStemFromName(stems[i].name, out, sizeof out);
        if (std::strcmp(out, stems[i].stem) != 0) {
            std::fprintf(stderr, "FAIL: stem of \"%s\" = \"%s\", expected \"%s\" (%s)\n",
                         stems[i].name, out, stems[i].stem, stems[i].why);
            fails++;
        }
    }

    /* A small buffer truncates and stays terminated, with room for the
     * device-name prefix. */
    {
        char out[8];
        SaveFileStemFromName("\x90t\x82 2026", out, sizeof out);
        if (std::strcmp(out, "Ete 20") != 0) {
            std::fprintf(stderr, "FAIL: truncated stem = \"%s\"\n", out);
            fails++;
        }
        SaveFileStemFromName("aux", out, sizeof out);
        if (std::strcmp(out, "_aux") != 0) {
            std::fprintf(stderr, "FAIL: short-buffer device stem = \"%s\"\n", out);
            fails++;
        }
        SaveFileStemFromName(NULL, out, sizeof out);
        if (std::strcmp(out, "save") != 0) {
            std::fprintf(stderr, "FAIL: NULL name stem = \"%s\"\n", out);
            fails++;
        }
    }

    /* Every stem is printable ASCII with no forbidden character, for every
     * byte a name can hold. */
    for (int c = 1; c < 256; c++) {
        char name[4] = {'x', (char)c, 'x', '\0'};
        char out[16];
        SaveFileStemFromName(name, out, sizeof out);
        for (const char *p = out; *p; p++) {
            const unsigned char u = (unsigned char)*p;
            if (u < 32 || u > 126 || std::strchr("<>:\"/\\|?*", u) != NULL) {
                std::fprintf(stderr, "FAIL: byte 0x%02X gave stem byte 0x%02X\n", c, u);
                fails++;
                break;
            }
        }
    }

    if (fails != 0) {
        std::fprintf(stderr, "test_save_name_validation: %zu failures\n", fails);
        return 1;
    }
    std::printf("test_save_name_validation: %zu cases passed\n", n + 1 + ns + 3 + 255);
    return 0;
}
