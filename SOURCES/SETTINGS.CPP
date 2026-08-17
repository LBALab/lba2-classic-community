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

        /* A renamed key reads its old spelling as the default, so an existing
         * cfg keeps working and the next write moves it to the current name. */
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
    *s->value = Settings_Coerce(s, (S32)value);
}

void Settings_RegisterCvars(const T_SETTING *table, S32 count) {
    S32 i;

    for (i = 0; i < count; i++) {
        const T_SETTING *s = &table[i];
        if (s->cvar == NULL)
            continue;
        /* A toggle reads as a bool in the console (no arg toggles it); everything
         * else takes a number. The console hands the typed value here rather than
         * interpreting it, so a setting answers the console and the config file
         * the same way. */
        Console_RegisterCvarCoerced(s->cvar, s->value,
                                    (s->type == SETTING_TRUTHY) ? CONSOLE_CVAR_BOOL : CONSOLE_CVAR_INT,
                                    Settings_ApplyFromConsole, (void *)s, s->help);
    }
}
