#include "RETAILFIX.H"
#include "SETTINGS.H"

/* See RETAILFIX.H. */

S32 RetailFixes = RETAIL_FIXES_YES;
static S32 RetailFixOn[RETAIL_FIX_COUNT] = {1};

static void RetailFix_FixChanged(S32 value);

/* Row order is the cfg's key order. The per-fix rows come first, in enum order,
 * so row i is fix i. The master row is LAST: `settings_before fix_` applies rows
 * in table order, each fix switching the master to Custom on its way to 0, and
 * the master then ends on its own value from before, No.
 *
 * Every row is recorded, so a recording says which rules it ran under, and
 * every row says its value from before; scripts/ci/check-settings-before.py
 * holds a fix_ row to SETTING_BEFORE(v). */
static const T_SETTING RetailFixSettings[] = {
    /* #747. DoFoundObj (INVENT.CPP) leaves GenAnim naming the walk while the
       hero stands in the idle body. */
    {"RetailFixFoundObjWalk", NULL, "fix_found_obj_walk", &RetailFixOn[RETAIL_FIX_FOUND_OBJ_WALK], SETTING_TRUTHY,
     1, 0, 1, NULL, NULL, RetailFix_FixChanged, NULL,
     "Retail fix: a direction held through a found-item popup still moves Twinsen afterwards", SETTING_RECORDED,
     SETTING_BEFORE(0)},
    {"RetailFixes", NULL, "fix_retail", &RetailFixes, SETTING_OR_DEFAULT, RETAIL_FIXES_YES, RETAIL_FIXES_NO,
     RETAIL_FIXES_CUSTOM, NULL, NULL, NULL, NULL,
     "Fix retail bugs: 0 no (the original's rules), 1 yes, 2 custom (each fix_ setting decides)", SETTING_RECORDED,
     SETTING_BEFORE(RETAIL_FIXES_NO)},
};
#define RETAIL_FIX_SETTING_COUNT ((S32)(sizeof(RetailFixSettings) / sizeof(RetailFixSettings[0])))
#define RETAIL_FIX_MASTER_ROW (&RetailFixSettings[RETAIL_FIX_COUNT])

/* One row per fix, then the master: a fix added to the enum without a row, or a
   row without an entry, would make RetailFix_On read the wrong value. */
typedef char RetailFixRowsMatchTheEnum[(RETAIL_FIX_SETTING_COUNT == RETAIL_FIX_COUNT + 1) ? 1 : -1];

/* A fix's row changed at runtime: under Yes or No the change would do nothing,
   so the master moves to Custom, where it does. A replay's borrow stands down
   (SETTINGS.H, Settings_Borrowing): the master is not borrowed, so moving it
   would outlast the replay and be written to the player's lba2.cfg. */
static void RetailFix_FixChanged(S32 value) {
    (void)value;
    if (Settings_Borrowing())
        return;
    if (RetailFixes != RETAIL_FIXES_CUSTOM)
        Settings_Apply(RETAIL_FIX_MASTER_ROW, RETAIL_FIXES_CUSTOM);
}

S32 RetailFix_On(T_RETAIL_FIX fix) {
    if ((S32)fix < 0 || fix >= RETAIL_FIX_COUNT)
        return 0;
    return RetailFix_Effective(RetailFixes, RetailFixOn[fix]);
}

void RetailFix_MenuSelect(void) {
    S32 anyOff = 0;
    S32 i;

    for (i = 0; i < RETAIL_FIX_COUNT; i++) {
        if (*RetailFixSettings[i].value != RetailFixSettings[i].def)
            anyOff = 1;
    }
    Settings_Apply(RETAIL_FIX_MASTER_ROW, RetailFix_NextMenuValue(RetailFixes, anyOff));
}

void RetailFix_ReadConfig(void) {
    Settings_ReadConfig(RetailFixSettings, RETAIL_FIX_SETTING_COUNT);
}

void RetailFix_WriteConfig(void) {
    Settings_WriteConfig(RetailFixSettings, RETAIL_FIX_SETTING_COUNT);
}

void RetailFix_RegisterCvars(void) {
    Settings_RegisterCvars(RetailFixSettings, RETAIL_FIX_SETTING_COUNT);
}

const T_SETTING *RetailFix_Settings(S32 *count) {
    *count = RETAIL_FIX_SETTING_COUNT;
    return RetailFixSettings;
}
