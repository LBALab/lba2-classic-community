#include "C_EXTERN.H"

#include "SETTINGS.H"
#include "CONSOLE/CONSOLE.H"
#include <SYSTEM/DEFFILE.H>

S32 Settings_Coerce(const T_SETTING *s, S32 value) {
    switch (s->type) {
    case SETTING_TRUTHY:
        return value ? TRUE : FALSE;

    case SETTING_RAW:
        return value;

    case SETTING_OR_DEFAULT:
        return (value<s->min OR value> s->max) ? s->def : value;

    case SETTING_CLAMP:
    default:
        if (value < s->min)
            value = s->min;
        if (value > s->max)
            value = s->max;
        return value;
    }
}

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
        if (s->stored != NULL AND s->forced != NULL) {
            const S32 forced = s->forced();
            if (forced >= 0 AND value == forced)
                value = *s->stored;
        }
        DefFileBufferWriteValue(s->key, value);
    }
}

void Settings_RegisterCvars(const T_SETTING *table, S32 count) {
    S32 i;

    for (i = 0; i < count; i++) {
        const T_SETTING *s = &table[i];
        if (s->cvar == NULL)
            continue;
        /* A toggle reads as a bool in the console (no arg toggles it); everything
         * else takes a number, whatever its coercion does with an odd one. */
        Console_RegisterCvarRanged(s->cvar, s->value,
                                   (s->type == SETTING_TRUTHY) ? CONSOLE_CVAR_BOOL : CONSOLE_CVAR_INT,
                                   s->min, s->max, s->help);
    }
}
