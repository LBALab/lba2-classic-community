#ifndef CONSOLE_GIVE_H
#define CONSOLE_GIVE_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure inventory-item name resolution for the `give` console command.
 * Lives in libconsole, which deliberately does not include COMMON.H, so the
 * indices below are literals mirrored from COMMON.H FLAG_* — CONSOLE_CMD.CPP
 * #error-guards them against the real constants so they cannot drift.
 */

/* Mirrored FLAG_* indices (see SOURCES/COMMON.H). */
#define CONSOLE_GIVE_IDX_MONEY 8
#define CONSOLE_GIVE_IDX_CLOVER 251

/* Mirrors COMMON.H MAX_INVENTORY: real inventory slots are 0..this-1. */
#define CONSOLE_GIVE_MAX_INVENTORY 40

/*
 * Resolve a token to a flag index: a case-insensitive item name, or a numeric
 * index. The numeric form is accepted only for real inventory slots
 * (0..CONSOLE_GIVE_MAX_INVENTORY-1); items outside that range (e.g. clover)
 * are reachable by name only. Returns -1 if the token resolves to nothing.
 */
S32 Console_GiveResolveItem(const char *token);

/* Non-zero if the item at `index` is a countable item (darts, money, ...). */
int Console_GiveIsCountItem(S32 index);

/* Canonical name for a flag index, or NULL if it is not a give-able item. */
const char *Console_GiveItemName(S32 index);

/* Iteration over the name table, for the `give` no-args listing. */
S32 Console_GiveItemRowCount(void);
const char *Console_GiveItemNameByRow(S32 row);

/*
 * Full argument parse for `give <item> [count]`. argv[0] is the command name,
 * argv[1] the item, argv[2] the optional count. On success returns 1 with
 * *outIndex set and *outCount set (1 when the count is omitted). On failure
 * returns 0 with *outError set to a static reason string.
 */
int Console_GiveParse(int argc, const char *argv[], S32 *outIndex, S32 *outCount,
                      const char **outError);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_GIVE_H */
