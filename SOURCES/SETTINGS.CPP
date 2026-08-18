#include "C_EXTERN.H"

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

        /* Captured before anything applies a per-run override, so this is the
         * player's own setting rather than what a flag forced. */
        if (s->stored != NULL)
            *s->stored = *s->value;
    }
}

void Settings_WriteConfig(const T_SETTING *table, S32 count) {
    S32 i;

    for (i = 0; i < count; i++) {
        const T_SETTING *s = &table[i];
        S32 value;

        if (s->key == NULL)
            continue;

        value = *s->value;
        if (s->stored != NULL AND s->forced != NULL)
            value = Settings_ValueToPersist(value, *s->stored, s->forced());
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
