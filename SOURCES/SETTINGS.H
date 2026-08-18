#ifndef SETTINGS_H
#define SETTINGS_H

/* Self-contained: the host test includes this on its own, so it cannot rely on
   NULL or S32 having arrived from whatever included it first. */
#include <SYSTEM/ADELINE_TYPES.H>

#include <stddef.h>

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

/* Told what the setting now holds, after it has been stored and coerced.
 *
 * A setting whose value has to reach something else (a driver, a surface that
 * caches it) cannot be served by a bare pointer: whoever writes the pointer has
 * to remember the second half, and every writer has to remember it separately.
 * That is how the console's reverse-stereo verb came to swap the sample driver
 * without recording the setting, so the choice was never saved and the next
 * reapply undid it.
 *
 * NULL for a setting that is only ever read by whoever needs it, which is most
 * of them. */
typedef void (*T_SETTING_APPLY_FN)(S32 value);

/* Where a setting's live value came from.
 *
 * The value alone does not say, and three of these are written back differently.
 * A value the player's own file gave is theirs and is persisted. One the game
 * data's config gave is real for this run and is not a preference: writing it
 * back would make the install's choice outlive the install. One a per-run flag
 * forced is not a preference either, which the stored column already handles.
 *
 * Ordered so a later source beats an earlier one, which is the order they are
 * applied in during boot. */
typedef enum {
    SETTING_FROM_DEFAULT = 0, /* nothing named it; the row's compiled default */
    SETTING_FROM_DATA,        /* the config shipped with the game data */
    SETTING_FROM_PROFILE,     /* the file this run owns: the player's own choice */
    SETTING_FROM_FLAG,        /* a per-run command-line override */
    SETTING_FROM_RUNTIME      /* set during the run: console, socket or menu */
} T_SETTING_SOURCE;

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
     * A row that also tracks its source answers this exactly: the run set it or
     * it did not, whatever the values happen to be. Without one the comparison
     * falls back to the value, which cannot tell setting a value by hand from
     * never touching it when the two agree. */
    S32 *stored;
    T_SETTING_FORCED_FN forced;
    T_SETTING_APPLY_FN on_change; /* told the new value after a runtime write; NULL if nothing to do */
    T_SETTING_SOURCE *source;     /* where the live value came from; NULL to not track it */
    const char *help;             /* console help text */
} T_SETTING;

/* Store a value into a setting and tell its owner, which is what "setting" a
 * setting means at runtime. One place, so every writer coerces and notifies
 * alike rather than each remembering both halves.
 *
 * The hook is handed the stored value, not the one that arrived, so it sees what
 * the setting actually holds after coercion. A hook that was told the raw value
 * would apply something the setting does not hold.
 *
 * Deliberately not used by Settings_ReadConfig. At boot the table is read before
 * the subsystems a hook would talk to are up, and the boot path already applies
 * what it needs once the whole table is in. Firing there would call into a
 * driver that does not exist yet, to no purpose. */
static inline void Settings_Apply(const T_SETTING *s, S32 value);

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
        if (value >= s->min && value <= s->max)
            return value;
        /* Falling back to a default the row itself puts out of range would hand
         * back exactly the kind of value this rule exists to refuse, so the
         * default is held to the same bounds. A row declared that way is a
         * mistake; this contains it rather than propagating it. */
        if (s->def < s->min)
            return s->min;
        if (s->def > s->max)
            return s->max;
        return s->def;

    case SETTING_CLAMP:
    default:
        if (value < s->min)
            value = s->min;
        if (value > s->max)
            value = s->max;
        return value;
    }
}

static inline void Settings_Apply(const T_SETTING *s, S32 value) {
    *s->value = Settings_Coerce(s, value);
    /* Recorded before the hook runs, so a hook that reads the source sees the
       write it is being told about rather than the one before it. */
    if (s->source != NULL)
        *s->source = SETTING_FROM_RUNTIME;
    if (s->on_change != NULL)
        s->on_change(*s->value);
}

/* Which value to persist for a setting a flag may be forcing this run: the
 * stored preference while the live value is still the one the flag forced,
 * otherwise the live value, because then something changed it on purpose.
 * `forced` is -1 for a run that did not use the flag.
 *
 * Header-side and free of engine state for the same reason as the coercion
 * above: this is the rule that decides whether `--fixed-timestep 100` or
 * `LBA2_TEXFILTER=2` leaks permanently into a player's lba2.cfg, and getting it
 * backwards is invisible to any check that reads back a cfg the engine wrote. */
static inline S32 Settings_ValueToPersist(S32 live, S32 stored, S32 forced) {
    return (forced >= 0 && live == forced) ? stored : live;
}

/* Record that this run set a setting, for a writer that does not go through
 * Settings_Apply. The console exposes some settings as commands rather than as
 * cvars, because they take words rather than numbers, and those assign the live
 * value themselves. Without this they would look untouched to the rule below and
 * a forced run would put the player's old value back over the player's own
 * choice. */
static inline void Settings_MarkSetThisRun(T_SETTING_SOURCE *source) {
    if (source != NULL)
        *source = SETTING_FROM_RUNTIME;
}

/* What a row persists, given where its live value came from.
 *
 * The rule the value comparison was reaching for: a run that set the value keeps
 * it, a run that merely inherited one does not. Only the flag case needs the
 * source to decide, which is why a row without one still gets the old answer
 * rather than a worse one.
 *
 * Left alone deliberately: a value from the game data's config is returned as it
 * stands. Not writing it back is the fix for an install's settings becoming the
 * player's, and it is a change to what the file ends up holding rather than to
 * this arithmetic. */
static inline S32 Settings_ValueToPersistFrom(S32 live, S32 stored, S32 forced, T_SETTING_SOURCE source) {
    if (forced < 0)
        return live; /* no flag touched it */
    if (live != forced)
        return live; /* moved off what the flag forced, so the run changed it */
    /* Sitting on the forced value, which is the case the value cannot answer:
       either the run set it there or nothing touched it. Only here does the
       source decide, which keeps this a refinement of the comparison rather than
       a replacement for it. A writer that assigns the global without marking the
       run therefore behaves exactly as it did before, rather than losing its
       change. */
    return (source == SETTING_FROM_RUNTIME) ? live : stored;
}

/* Apply the cfg to every row that names a key, clamped. */
extern void Settings_ReadConfig(const T_SETTING *table, S32 count);

/* Persist every row that names a key, in table order. */
extern void Settings_WriteConfig(const T_SETTING *table, S32 count);

/* Register every row that names a cvar, carrying its range to the console. */
extern void Settings_RegisterCvars(const T_SETTING *table, S32 count);

#endif /* SETTINGS_H */
