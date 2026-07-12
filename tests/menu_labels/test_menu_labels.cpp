/* Host contract test for the localized menu-string table
 * (SOURCES/MENU_LABELS.{H,CPP}, LocalizedMenuLabels).
 *
 * The table is one row per label, each row holding its own enum id plus the six
 * translations. This links the real shipped table (not a copy) and pins the
 * three ways it can silently go wrong:
 *
 *   1. Misalignment. A row inserted at the wrong position shifts every later
 *      label's text (Resolution renders "Vsync ON", ...). Counts still match and
 *      nothing is NULL, so only the per-row id catches it: id must equal index.
 *
 *   2. Missing translation. C++ brace elision quietly fills a short row with
 *      NULL; GetLocalizedMenuLabel() then hands that NULL to CopyUtf8ToCp850().
 *      The compiler only errors on too MANY initializers, never too few.
 *
 *   3. Format-specifier drift. Several labels are printf formats fed to
 *      FormatUtf8ToCp850(dst, 256, label, args...). A translation that changes,
 *      drops or reorders a specifier -- "%s" where the caller passes an unsigned
 *      -- is undefined behaviour, not a typo. Every language in a row must carry
 *      the same specifier sequence as English.
 */

#include "MENU_LABELS.H"

#include <cstdio>
#include <cstring>

/* Mirrors the language order documented in MENU_LABELS.H (lang[] index). Used
 * only to name the offending column in failure output. */
static const char *kLangName[MENU_LANGUAGE_COUNT] = {
    "EN", "FR", "DE", "ES", "IT", "PT"};

static int failures = 0;

/* Reduce a printf format to its conversion sequence: "Now running at %ux%u." ->
 * "uu", "Revert (%d)" -> "d", "Vsync ON" -> "". A literal "%%" contributes
 * nothing. Flags/width/precision/length are skipped; the conversion char is what
 * determines how the vararg is read, so that is what must match. */
static void specifierSeq(const char *s, char *out, size_t outSize) {
    size_t n = 0;
    for (const char *p = s; *p != '\0'; p++) {
        if (*p != '%') {
            continue;
        }
        p++;
        if (*p == '\0') {
            break;
        }
        if (*p == '%') { /* literal percent, consumes no argument */
            continue;
        }
        while (*p != '\0' && std::strchr("-+ #0123456789.*hlLjzt", *p) != NULL) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (n + 1 < outSize) {
            out[n++] = *p;
        }
    }
    out[n] = '\0';
}

int main() {
    for (int i = 0; i < MENU_LABEL_COUNT; i++) {
        const struct MenuLabelRow *row = &LocalizedMenuLabels[i];

        /* 1. Misalignment guard. */
        if ((int)row->id != i) {
            std::printf("FAIL row %d: id is %d, expected %d "
                        "(rows out of enum order -- every later label renders the wrong string)\n",
                        i, (int)row->id, i);
            failures++;
            continue; /* the row's contents belong to a different label; don't cascade */
        }

        /* 2. Every translation present. */
        int rowComplete = 1;
        for (int lang = 0; lang < MENU_LANGUAGE_COUNT; lang++) {
            const char *s = row->lang[lang];
            if (s == NULL) {
                std::printf("FAIL row %d lang %d (%s): NULL (missing translation)\n",
                            i, lang, kLangName[lang]);
                failures++;
                rowComplete = 0;
            } else if (s[0] == '\0') {
                std::printf("FAIL row %d lang %d (%s): empty string\n",
                            i, lang, kLangName[lang]);
                failures++;
                rowComplete = 0;
            }
        }
        if (!rowComplete) {
            continue;
        }

        /* 3. printf specifiers identical across the row (English is the anchor). */
        char want[16];
        specifierSeq(row->lang[0], want, sizeof want);
        for (int lang = 1; lang < MENU_LANGUAGE_COUNT; lang++) {
            char got[16];
            specifierSeq(row->lang[lang], got, sizeof got);
            if (std::strcmp(want, got) != 0) {
                std::printf("FAIL row %d lang %d (%s): specifiers \"%s\" != EN \"%s\"\n"
                            "     EN: %s\n     %s: %s\n",
                            i, lang, kLangName[lang], got, want,
                            row->lang[0], kLangName[lang], row->lang[lang]);
                failures++;
            }
        }
    }

    if (MENU_LANGUAGE_COUNT != 6) {
        std::printf("FAIL: MENU_LANGUAGE_COUNT is %d, expected 6\n", MENU_LANGUAGE_COUNT);
        failures++;
    }

    if (failures != 0) {
        std::printf("test_menu_labels: %d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("test_menu_labels: ok (%d labels x %d languages, ids aligned, "
                "specifiers consistent)\n",
                (int)MENU_LABEL_COUNT, MENU_LANGUAGE_COUNT);
    return 0;
}
