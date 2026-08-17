#ifndef SETTINGS_H
#define SETTINGS_H

#include <SYSTEM/ADELINE_TYPES.H>

/* One declaration per setting, read by every surface that exposes it.
 *
 * A tunable reaches the player through three generic files: the cfg reader, the
 * cfg writer, and the console's cvar table. Naming the setting in each of them
 * costs three edits per setting and puts its bounds on only one of the three
 * paths, so a value arriving by another route is unchecked. That is not a
 * hypothetical: the console assigns straight through a bare pointer, which is
 * why several call sites carry a hand-written "clamp where it is used" comment.
 *
 * A module declares its settings once, as a table, and hands that table to each
 * surface. The bounds travel with the setting, so every writer is bounded and no
 * surface needs to know a setting's name.
 *
 * Plain data on purpose: the table is a static array a reader can take in at a
 * glance, and it is read at a known point in boot rather than assembled by
 * anything that runs before main. */

/* What to do with a cfg value outside the declared range. The engine does not do
 * one thing here, it does four, and flattening them silently changes what a
 * hand-edited or older cfg boots into. Each row says which it means.
 *
 * A future pass may well decide the engine should only do one of these. That is
 * a deliberate change to make on its own, not a side effect of tabulating. */
typedef enum {
    SETTING_CLAMP,      /* bring it to the nearer bound (DetailLevel) */
    SETTING_OR_DEFAULT, /* out of range is treated as unset (FullScreen, TextureFilter) */
    SETTING_TRUTHY,     /* any non-zero is on (the Auto camera's toggles) */
    SETTING_RAW         /* no range at all; whatever the cfg said stands (AllCameras) */
} T_SETTING_TYPE;

/* For a setting the engine deliberately leaves unbounded. Clamping against these
 * is a no-op, so a row says what it means rather than omitting a field. */
#define SETTING_MIN_NONE (-2147483647 - 1)
#define SETTING_MAX_NONE 2147483647

/* What a per-run flag forced this setting to, or -1 for a run that did not use
 * it. Wraps whichever Control_HasX / Control_GetX pair belongs to the flag. */
typedef S32 (*T_SETTING_FORCED_FN)(void);

typedef struct {
    const char *key;    /* lba2.cfg key; NULL if this surface does not persist it */
    const char *legacy; /* older key to fall back to when `key` is absent; NULL if none */
    const char *cvar;   /* console name; NULL if it is not exposed there */
    S32 *value;         /* the live global the module owns */
    T_SETTING_TYPE type;
    S32 def; /* default when the cfg has no entry */
    S32 min;
    S32 max;
    /* Per-run overrides. A flag that says "this run only" must not leave its
     * value behind for the next one, so `stored` keeps what the cfg held and the
     * writer persists that instead whenever the live value is still the one the
     * flag forced. Both NULL for a setting no flag can override.
     *
     * The comparison is on the value, not on who last wrote it: settings reached
     * through cvars have no on-change hook, so setting the same value by hand
     * during an overridden run cannot be told from not touching it. Setting a
     * different one persists, which is the case that matters. */
    S32 *stored;
    T_SETTING_FORCED_FN forced;
    const char *help; /* console help text */
} T_SETTING;

/* What the cfg said, turned into what the setting may hold. The one place a
 * range is enforced, and the only part of this file free of engine state, so the
 * host test in tests/settings covers it without booting anything.
 *
 * Worth having covered: reducing these four rules to two silently moved six
 * settings to a different value on an out-of-range cfg, and no oracle that reads
 * back a cfg the engine itself wrote can see that, because those values are all
 * in range by construction. */
static inline S32 Settings_Coerce(const T_SETTING *s, S32 value) {
    switch (s->type) {
    case SETTING_TRUTHY:
        return value ? 1 : 0;

    case SETTING_RAW:
        return value;

    case SETTING_OR_DEFAULT:
        return (value < s->min || value > s->max) ? s->def : value;

    case SETTING_CLAMP:
    default:
        if (value < s->min)
            value = s->min;
        if (value > s->max)
            value = s->max;
        return value;
    }
}

/* Apply the cfg to every row that names a key, clamped. */
extern void Settings_ReadConfig(const T_SETTING *table, S32 count);

/* Persist every row that names a key, in table order. */
extern void Settings_WriteConfig(const T_SETTING *table, S32 count);

/* Register every row that names a cvar, carrying its range to the console. */
extern void Settings_RegisterCvars(const T_SETTING *table, S32 count);

#endif /* SETTINGS_H */
