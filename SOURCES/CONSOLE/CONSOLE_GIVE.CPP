#include "CONSOLE_GIVE.H"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/*
 * Inventory item name table. `index` is the COMMON.H FLAG_* value; `isCount`
 * marks items that carry a quantity (FLAG_INV_FEWER). Aliases are allowed:
 * several rows may share an index (FLAG_DISQUE_ROUTE / FLAG_VISIONNEUSE both
 * map to 24). Names are lowercase, derived from the FLAG_* identifiers.
 */
typedef struct {
    const char *name;
    S32 index;
    int isCount;
} GiveItem;

static const GiveItem s_items[] = {
    {"holomap", 0, 0},
    {"magicball", 1, 0},
    {"darts", 2, 1},
    {"sendellball", 3, 0},
    {"tunic", 4, 0},
    {"pearl", 5, 0},
    {"pyramidkey", 6, 0},
    {"steeringwheel", 7, 0},
    {"money", 8, 1},
    {"pistol", 9, 0},
    {"sword", 10, 0},
    {"glove", 11, 0},
    {"protopack", 12, 0},
    {"ferryticket", 13, 0},
    {"penguin", 14, 1},
    {"gazogem", 15, 0},
    {"halfmedallion", 16, 0},
    {"gallicacid", 17, 0},
    {"song", 18, 0},
    {"thunderring", 19, 0},
    {"umbrella", 20, 0},
    {"gem", 21, 1},
    {"conch", 22, 0},
    {"blowgun", 23, 0},
    {"routedisc", 24, 0},
    {"acfviewer", 24, 0}, /* alias: FLAG_VISIONNEUSE shares index 24 */
    {"lucitart", 25, 0},
    {"radio", 26, 0},
    {"flower", 27, 0},
    {"slate", 28, 0},
    {"translator", 29, 0},
    {"diploma", 30, 0},
    {"knartakey", 31, 0},
    {"supkey", 32, 0},
    {"mosquikey", 33, 0},
    {"blafardkey", 34, 0},
    {"queenkey", 35, 0},
    {"pickaxe", 36, 0},
    {"bourgmestrekey", 37, 0},
    {"bourgmestrenote", 38, 0},
    {"protection", 39, 0},
    {"clover", CONSOLE_GIVE_IDX_CLOVER, 1},
};

static S32 give_item_count(void) {
    return (S32)(sizeof(s_items) / sizeof(s_items[0]));
}

S32 Console_GiveItemRowCount(void) {
    return give_item_count();
}

const char *Console_GiveItemNameByRow(S32 row) {
    if (row < 0 || row >= give_item_count())
        return NULL;
    return s_items[row].name;
}

S32 Console_GiveResolveItem(const char *token) {
    if (token == NULL || token[0] == '\0')
        return -1;

    for (S32 i = 0; i < give_item_count(); i++) {
        if (strcasecmp(token, s_items[i].name) == 0)
            return s_items[i].index;
    }

    /* Numeric fallback: real inventory slots only, so a stray `give 251`
       cannot reach a name-only item. */
    {
        char *end = NULL;
        long v = strtol(token, &end, 0);
        if (end != token && end != NULL && *end == '\0' && v >= 0 &&
            v < CONSOLE_GIVE_MAX_INVENTORY)
            return (S32)v;
    }
    return -1;
}

int Console_GiveIsCountItem(S32 index) {
    for (S32 i = 0; i < give_item_count(); i++) {
        if (s_items[i].index == index)
            return s_items[i].isCount;
    }
    return 0;
}

const char *Console_GiveItemName(S32 index) {
    for (S32 i = 0; i < give_item_count(); i++) {
        if (s_items[i].index == index)
            return s_items[i].name;
    }
    return NULL;
}

int Console_GiveParse(int argc, const char *argv[], S32 *outIndex, S32 *outCount,
                      const char **outError) {
    if (argc < 2 || argv[1] == NULL || argv[1][0] == '\0') {
        *outError = "Usage: give <item> [count]  (run 'give' with no args to list items)";
        return 0;
    }

    S32 index = Console_GiveResolveItem(argv[1]);
    if (index < 0) {
        *outError = "Unknown item. Run 'give' with no args to list valid item names.";
        return 0;
    }

    S32 count = 1;
    if (argc >= 3 && argv[2] != NULL && argv[2][0] != '\0') {
        if (!Console_GiveIsCountItem(index)) {
            *outError = "That item is not countable; omit the count argument.";
            return 0;
        }
        char *end = NULL;
        long v = strtol(argv[2], &end, 0);
        if (end == argv[2] || end == NULL || *end != '\0') {
            *outError = "Count must be a number.";
            return 0;
        }
        if (v < 1) {
            *outError = "Count must be 1 or more.";
            return 0;
        }
        count = (S32)v;
    }

    *outIndex = index;
    *outCount = count;
    return 1;
}
