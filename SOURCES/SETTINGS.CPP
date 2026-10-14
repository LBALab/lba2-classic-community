/* No C_EXTERN.H: nothing here touches engine state but through the rows it is handed,
   and without it tests/settings links this file on its own. */
#include "SETTINGS.H"
#include "CONSOLE/CONSOLE.H"
#include <SYSTEM/DEFFILE.H>

void Settings_ReadConfig(const T_SETTING *table, S32 count) {
    S32 i;

    for (i = 0; i < count; i++) {
        const T_SETTING *s = &table[i];
        S32 def;

        if (s->key == NULL)
            continue;

        /* A renamed key reads its old spelling as the default, so an existing cfg
         * keeps working and the next write moves it to the current name.
         *
         * The old value is then coerced by the current row, not carried through
         * as-is: a rename means the setting kept its meaning and changed its
         * spelling, so what was out of range under the old name is out of range
         * under the new one, and lands on the default rather than on itself. */
        def = (s->legacy != NULL) ? DefFileBufferReadValueDefault(s->legacy, s->def) : s->def;
        *s->value = Settings_Coerce(s, DefFileBufferReadValueDefault(s->key, def));

        /* Which of the stacked configs answered, asked under the current key and
           then the legacy one, since a renamed key is still the player's if their
           file spells it the old way. */
        if (s->source != NULL) {
            T_DEFFILE_ORIGIN from = DefFileBufferKeyOrigin(s->key);
            if (from == DEFFILE_ORIGIN_ABSENT && s->legacy != NULL)
                from = DefFileBufferKeyOrigin(s->legacy);

            switch (from) {
            case DEFFILE_ORIGIN_OWNED:
                *s->source = SETTING_FROM_PROFILE;
                break;
            case DEFFILE_ORIGIN_LAYER:
                *s->source = SETTING_FROM_DATA;
                break;
            default:
                *s->source = SETTING_FROM_DEFAULT;
                break;
            }
        }

        /* Captured before anything applies a per-run override, so this is the
         * player's own setting rather than what a flag forced. */
        if (s->stored != NULL)
            *s->stored = *s->value;
    }
}

/* Rows a replay has borrowed, with what each held before. Sized well past the
 * recorded rows (38 today); a borrow past it is refused and reported by the
 * caller rather than dropped. */
#define SETTINGS_BORROW_MAX 128

typedef struct {
    const T_SETTING *row;
    S32 kept;
} T_SETTING_BORROW;

static T_SETTING_BORROW s_borrowed[SETTINGS_BORROW_MAX];
static S32 s_borrowedCount = 0;

static T_SETTING_BORROW *find_borrow(const T_SETTING *s) {
    S32 i;

    for (i = 0; i < s_borrowedCount; i++) {
        if (s_borrowed[i].row == s)
            return &s_borrowed[i];
    }
    return NULL;
}

S32 Settings_Borrow(const T_SETTING *s, S32 value) {
    T_SETTING_BORROW *b = find_borrow(s);

    if (b == NULL) {
        if (s_borrowedCount >= SETTINGS_BORROW_MAX)
            return 0;
        b = &s_borrowed[s_borrowedCount++];
        b->row = s;
        b->kept = *s->value;
    }
    /* The source is left alone: it says where the player's value came from, and
       the player's value is the one the writer persists while this lasts. */
    *s->value = Settings_Coerce(s, value);
    if (s->on_change != NULL)
        s->on_change(*s->value);
    return 1;
}

void Settings_ReturnBorrowed(void) {
    while (s_borrowedCount > 0) {
        const T_SETTING_BORROW *b = &s_borrowed[--s_borrowedCount];
        *b->row->value = b->kept;
        if (b->row->on_change != NULL)
            b->row->on_change(b->kept);
    }
}

void Settings_WriteConfig(const T_SETTING *table, S32 count) {
    S32 i;

    for (i = 0; i < count; i++) {
        const T_SETTING *s = &table[i];
        const T_SETTING_BORROW *b;
        S32 value;

        if (s->key == NULL)
            continue;

        b = find_borrow(s);
        value = (b != NULL) ? b->kept : *s->value;
        if (s->stored != NULL AND s->forced != NULL) {
            value = (s->source != NULL)
                        ? Settings_ValueToPersistFrom(value, *s->stored, s->forced(), *s->source)
                        : Settings_ValueToPersist(value, *s->stored, s->forced());
        }
        DefFileBufferWriteValue(s->key, value);
    }
}

/* The console's side of a setting: it parses, this decides. Registered as the
 * setter for every row that names a cvar. */
static void Settings_ApplyFromConsole(void *ctx, int value) {
    const T_SETTING *s = (const T_SETTING *)ctx;
    Settings_Apply(s, (S32)value);
}

void Settings_RegisterCvars(const T_SETTING *table, S32 count) {
    S32 i;

    for (i = 0; i < count; i++) {
        const T_SETTING *s = &table[i];
        if (s->cvar == NULL)
            continue;
        /* A setting with two states reads as a bool in the console, so a bare
         * `name` toggles it and `help` says 0|1. That is a question about how
         * many values it has, not about which rule coerces them: a row that
         * treats out of range as unset is still a toggle when its range is 0..1,
         * and typing it as an integer would silently turn its toggle into a
         * print. Everything else takes a number.
         *
         * The console hands the typed value here rather than interpreting it, so
         * a setting answers the console and the config file the same way. */
        const S32 two_valued = (s->type == SETTING_TRUTHY) OR(s->min == 0 AND s->max == 1);
        Console_RegisterCvarCoerced(s->cvar, s->value,
                                    two_valued ? CONSOLE_CVAR_BOOL : CONSOLE_CVAR_INT,
                                    Settings_ApplyFromConsole, (void *)s, s->help);
    }
}
