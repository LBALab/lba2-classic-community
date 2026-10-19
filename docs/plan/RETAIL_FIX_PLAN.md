# Retail fixes: plan

How a fix to a bug the original game has (label `retail`) stays reversible. Decided on
[#760](https://github.com/LBALab/lba2-classic-community/issues/760); the first user is #747
(PR #759). This plan is what that PR builds.

**Retail means what shipped in the 1990s**: the 1997 CD releases (DOS and Windows 95). The 2022
Classic release is a later port, not a reference. Where the two differ, the 90s build is the
original, and lba2cc matching Classic instead is a port difference to put back, not a retail
bug. Evidence for "the original does this" comes from a 90s build or the 1997 source (commit
333929a), and Classic is at most a second witness.

## The decision, in short

- One options-menu row, **Fix retail bugs**: Yes / No. Default Yes. No means retail rules,
  including any fix added in a later version.
- Each fix has its own setting, in `lba2.cfg` and the console. Changing one from the console
  switches the row to **Custom**. The row shows Custom only once a fix's setting differs from
  its default, or when the cfg says Custom.
- Yes and No never overwrite the per-fix settings, so going back to Custom brings them back.
- Recordings carry the row and every fix's setting.
- No setting for: #511 (it restored retail behaviour), #745 (goes back to the CD release's
  behaviour), fixes that only change what is seen or heard, and fixes where the original had no
  defined result to go back to (memory corruption). See "What the backlog needs" for where that
  line falls.
- The "Retail rules" watermark and presets are later work (presets with #733).

## Where it fits

The settings table in [SETTINGS.H](../../SOURCES/SETTINGS.H) already gives everything a fix
needs from one row: the cfg key, the console cvar with its range, the recording header line, the
replay borrow, and a value from before (`SETTING_BEFORE`) that `settings_before <prefix>` applies.
[FOLLOWCAM.CPP](../../SOURCES/FOLLOWCAM.CPP) is the worked example of a module that owns a table
and hands it to the surfaces through `_ReadConfig` / `_WriteConfig` / `_RegisterCvars` /
`_Settings`, and [the camera's decision](../knowledge/decisions/one-auto-camera-each-behaviour-a-switch.md)
is the same shape: one feature, each behaviour a switch.

What is new is one row that overrides the others. Nothing in the table does that today.

## Design

### Module

`SOURCES/RETAILFIX.CPP` and `RETAILFIX.H`, a new module that owns its globals (AGENTS.md, "Adding
a new subsystem"). It exposes:

```c
typedef enum {
    RETAIL_FIX_FOUND_OBJ_WALK, /* #747 */
    RETAIL_FIX_COUNT
} T_RETAIL_FIX;

#define RETAIL_FIXES_NO 0
#define RETAIL_FIXES_YES 1
#define RETAIL_FIXES_CUSTOM 2

S32 RetailFix_On(T_RETAIL_FIX fix);   /* what a fix site asks */
S32 RetailFix_AnyCustom(void);        /* any per-fix setting off its default */
S32 RetailFix_NextMenuValue(S32 cur); /* the menu row's cycle */
void RetailFix_ReadConfig(void);
void RetailFix_WriteConfig(void);
void RetailFix_RegisterCvars(void);
const T_SETTING *RetailFix_Settings(S32 *count);
```

`RetailFix_On` is the only thing a fix site calls:

```c
master == RETAIL_FIXES_CUSTOM ? per_fix[fix] : master
```

The arithmetic for `RetailFix_On`, `RetailFix_AnyCustom` and the cycle goes in the header as
`static inline` over plain values, the way `Settings_Coerce` is, so a host test covers it without
booting anything.

### Rows

| cfg key | cvar | type | default | min..max | before | recorded |
|---|---|---|---|---|---|---|
| `RetailFixFoundObjWalk` | `fix_found_obj_walk` | TRUTHY | 1 | 0..1 | 0 | yes |
| `RetailFixes` | `fix_retail` | OR_DEFAULT | 1 | 0..2 | 0 | yes |

- cfg keys follow the cfg's CamelCase and cvars the console's prefix_snake, as every other row
  does. All cvars start with `fix_`, so `settings_before fix_` is retail rules.
- **The master row goes last in the table.** `settings_before fix_` applies rows in table order,
  and each per-fix row switches the master to Custom as it changes (below). With the master last,
  it ends on its own value from before, No.
- Help strings name the issue the fix closes, so `help fix_found_obj_walk` says what it is.

### Switching to Custom

A per-fix row's `on_change` hook sets the master to Custom. That is what makes a console edit
take effect instead of being silently overridden.

**Trap: the replay borrow calls the same hook.** `Settings_Borrow` and `Settings_ReturnBorrowed`
(SETTINGS.CPP) both call `on_change`. A replay that borrows a per-fix value would switch the
player's master to Custom, which is not borrowed, so it is never given back and gets written
to `lba2.cfg`. Two ways out:

1. **Recommended:** SETTINGS.CPP keeps a flag while borrowing or returning and exposes
   `Settings_Borrowing()`. The hook does nothing while it is set. One flag, owned by the code
   that causes the problem, plus a case in `tests/settings/test_settings_borrow.cpp`.
2. No hook. The console prints a note when a per-fix cvar is set while the master is not
   Custom. Simpler, but the issue comment promised the switch to Custom.

The cfg reader does not fire hooks at boot (by design, see `Settings_Apply`'s comment). So a
hand edit of a per-fix key in `lba2.cfg` needs `RetailFixes: 2` as well. At boot, a non-default
per-fix key cannot be told apart from one kept on purpose under Yes or No. docs/CONFIG.md says
so next to the keys.

### Menu row

- `MENU_ID_RETAIL_FIXES` added to `AdvancedOptionMenu` (GAMEMENU.CPP), which takes it from 7
  rows to 8. **Check that 8 rows fit at 640x480**: no menu has more than 7 today. Capture it with
  the `ui` command at `--resolution 640x480 --fixed-dt 16`.
- Labels in MENU_LABELS.CPP, six languages: "Fix retail bugs: Yes / No / Custom". Have
  the translations checked by someone who speaks each language.
- Selecting the row sets `RetailFix_NextMenuValue(master)` **through `Settings_Apply`**, so the
  setting is marked as set this run and persists. The Auto/Classic camera row assigns its global
  directly (`FollowCamera ^= 1`); don't copy that.
- The cycle: Yes → No → Custom → Yes, with Custom skipped unless the master is Custom or
  `RetailFix_AnyCustom()`.

### Fix site

#759's change in `DoFoundObj` (INVENT.CPP) gains `RetailFix_On(RETAIL_FIX_FOUND_OBJ_WALK) AND` in
its condition. Each later fix is one enum entry, one row, one label-free help string and one `if`.

### Wiring

- `ConfigFile_SettingTable` (CONFIG_FILE.CPP) gets a case for `RetailFix_Settings`, which is
  what puts the rows in the recording header and the replay borrow.
- `RetailFix_ReadConfig` is called beside `FollowCam_ReadConfig` (CONFIG_FILE.CPP), and
  `RetailFix_WriteConfig` beside `FollowCam_WriteConfig`.
- `RetailFix_RegisterCvars` is called beside `FollowCam_RegisterCvars` (CONSOLE_CMD.CPP).
- CMake source lists. Watch the build-graph ratchet: a new module adds include edges, so measure
  with `LBA2_BUILD_TESTS=ON`, as CI does.

### Recordings

- Both rows are recorded, so a recording says which rules it ran under. A replay borrows them
  like any other setting.
- A recording made before a fix existed carries no line for it, and the replay runs on the
  player's value (Yes, by default). This follows
  [a missing field is not a value](../knowledge/decisions/a-missing-field-is-not-a-value.md):
  don't fill in "off". So **every fix PR re-runs the committed recordings**. #759 already did,
  with no change.

## Tests

| What | Where | Shows |
|---|---|---|
| `RetailFix_On`, `AnyCustom`, the cycle, for every master value | new host test in `tests/settings/` | the override and the menu logic, without the engine |
| a borrowed per-fix row leaves the master alone; returning it does too | `tests/settings/test_settings_borrow.cpp` | the trap above stays closed |
| #747 with the fix off, two ways: `fix_retail 0`, and `fix_retail 2; fix_found_obj_walk 0` | `tests/automation/test_found_obj_held_walk.sh`, new arms | off reproduces main (0 walked on both saves); on still passes |
| `settings_before fix_` ends on master 0 and each fix 0 | same fixture, or a small `tests/automation` one | retail rules from the console |
| every `fix_` row declares its value from before | `scripts/ci/check-settings-before.py`, extended from `CAM_PLAIN` alone to a second prefix | a new fix cannot forget `SETTING_BEFORE(0)` |
| 8 menu rows fit at 640x480 | `ui` capture, by eye | the row is reachable |
| committed recordings replay unchanged | the automation suite | Yes is the old behaviour for every recording without the fix |

Run the automation suite alone; other sessions share the machine.

## Docs in the same PR

- `docs/CONFIG.md`: the two keys, and that a hand edit needs `RetailFixes: 2`.
- `docs/CONSOLE.md`: the cvars.
- `docs/knowledge/decisions/`: a Decision, "a retail fix is a switch, and one row turns them all
  off", owned by the settings concept, citing #760. Run `make knowledge-check`.
- `CHANGELOG.md` [Unreleased]: the menu row, under the #747 entry #759 already has.
- **AGENTS.md Principle 3** says "Default builds should match original behavior". With fixes on
  by default, that is no longer true as written. Needs the maintainer's wording (see below).
- `INPUT_PLAN.md` still lists #508 as open; fix it while touching docs, or leave it to its own
  one-line PR.

## What the backlog needs

Checked against every open issue labelled `retail` or `changes-play` (2026-10-19). #759 builds
only what #747 needs, but the shape has to hold for the rest.

### Which bucket an issue goes in

- **A retail fix** is a bug the designers did not intend. It gets a `fix_` setting, is on by
  default, and falls under the master row.
- **An enhancement** changes the game's design. It is off by default (AGENTS.md Principle 3),
  has its own setting, and is **not** under the master row.
- **A display option** only changes what is seen or heard and never a replay. It goes with the
  display settings, not with `fix_`.

Each issue that changes play says which bucket it is in before work starts. The `retail` label
alone does not decide it.

| Issue | What | Bucket | Shape |
|---|---|---|---|
| #747 | held direction stuck after the item popup | fix | one check (this PR) |
| #746 | running jump lost after a scene change or load | fix | one check, once the cause is known |
| #748 | Super Jetpack cannot turn while descending | fix | one check, once the cause is known |
| #423 | Discreet mode: rotation snaps on release | fix | one check; either suggested fix changes heading, so changes play |
| #749 | some penguin pickups set the count to 1 | fix | game-data fix, below |
| #750 | Wannie mines clover box respawns | fix | game-data fix, below |
| #751 | two clover boxes share one quest flag | fix | game-data fix, below |
| #600 | Distance2D/3D overflow | port bug | not retail: restore the original, no setting (below) |
| #707 | one-brick skip at an outdoor scene boundary | to decide | two code paths, below |
| #538 | NPCs respawn on every outdoor crossing | likely enhancement | two code paths, below |
| #745 | catching on wall corners | port difference | the 1997 CD release does not catch; lba2cc follows Classic's regression, so the fix restores the original, no setting |
| #735, #144 | texture swim, terrain seams | display option | not `fix_` |
| #498, #59 | the bat's path, lava rock collision | unknown | no confirmed cause or retail check yet |

EPmager's checks of #746-#751 compared the DOS CD release and Classic, so the CD side is the
one that counts. The rows marked "once the cause is known" still need the cause found in the
1997 source before calling them retail.

### Game-data fixes (#749, #750, #751)

The bug is in the original life scripts, not in engine code, so the fix overrides game data.

- **A save keeps what a fix wrote.** Once a fix has set a quest flag or a count in a save,
  switching it off does not undo that. After #750's fix, the box stays gone even with the fix
  off. A setting changes the rules from then on, and "off is the original" holds for state
  written afterwards. docs/CONFIG.md and each fix's help string say so.
- **The data differs between releases.** An override names the scene, the object and the exact
  script bytes it expects, and does nothing when they don't match. A test runs it against every
  data set the Mac has (US CD, GOG Classic, GOG DOSBox, Steam).
- **When it applies.** Scripts are read when a scene loads. Either the override is applied at
  load, and a change takes effect at the next scene, or it intercepts at the opcode and takes
  effect at once. The first fix decides which, and the help string says it.

### Engine-library fixes

- **LIB386 cannot ask the game.** `scripts/ci/check-arch.py` enforces "LIB386 must not include
  SOURCES", so a retail fix there cannot call `RetailFix_On`. If one comes up, the library owns
  a plain flag for that fix, and RETAILFIX.CPP sets it whenever the effective value changes: on
  boot, on any change to the master or that fix's row, and on a replay borrow and return. No
  open issue needs this yet.
- **Check the original before calling a bug retail.** #600 looked like one and is not. The
  1997 `DISTANCE.ASM` (commit 333929a) squares with one-operand `imul`, so each square is the
  full 64-bit `edx:eax`, sums with `add`/`adc`, and takes `QSqr`, an integer square root of the
  64-bit value rounded down. The 1997 DOS executable (`LBA2.EXE` as GOG ships it) has the same
  instructions at file offsets 0x546e8 (2D) and 0x54703 (3D), each jumping to the `QSqr` at
  0x69ed8. (The 2022 Classic `TLBA2C.exe` has them too, at 0xa77d2 and 0xa780a, but it is not
  the reference.) The 32-bit overflow came in with the C port (d20cd465). So the fix restores the
  original and gets no setting: a 64-bit sum and an integer square root that rounds down like
  `QSqr`, not `sqrtl`, since `long double` is only a `double` on arm64 and can round up near a
  perfect square. Checked against the assembly with the equivalence harness in `tests/3D` on an
  x86 lane.

### Two code paths (#707, #538)

A setting works mechanically, but the original path has to stay alive and tested next to a
large new one. Before either starts:

- Decide its bucket. #538 keeps actors resident across a crossing, which is a design change;
  #707 is closer to a bug, but the maintainer wants it fixed in the engine.
- If it gets a setting, the original path keeps its own fixture. #707 notes that moving the
  arrival point makes every later position differ in a recording that crosses a boundary, so
  the setting off has to replay the committed recordings unchanged.

### What "retail rules" covers

The master row turns off retail fixes. It does not turn off enhancements that change play,
because those have their own settings outside it. So "Fix retail bugs: No" is not the whole of
retail rules once a play-changing enhancement exists. A watermark or an Original preset has to
check every setting that changes play, not just this row. Nothing to build now; just don't
describe the row as more than it is.

## Out of scope

- #745 (no setting; the CD release's behaviour), #511 (restored retail).
- The watermark (EPmager's point 4), and presets (#733, which will reuse the same "show the
  match, or Custom" idea). A later Original preset is `settings_before` over everything, with
  this master at No.
- Retrofitting past fixes: last session's audit found no merged fix that changes play against
  retail apart from #511. The rest are port bugs, undefined behaviour or presentation.

## Open for the maintainer

1. **Principle 3's new wording.** A suggestion: "New behaviour that changes the experience is
   opt-in. A fix to a bug the original has is on by default and can be switched off; the
   original stays reachable."
2. **Names.** `RetailFixes` / `fix_retail` for the row, `RetailFix<Name>` / `fix_<name>` for the
   fixes. The fix's own name is `found_obj_walk` here; something shorter is fine.
3. **Switching to Custom:** the flag in SETTINGS.CPP (recommended), or a console note with no
   switch.
4. **The bucket rule** in "What the backlog needs" (fix, enhancement, display option), and which
   bucket #707 and #538 go in.

## Order of work

1. Rebase `fix/found-obj-held-walk` onto main (it is two commits on an older main).
2. Settings flag and its borrow test. Pure, so it lands first.
3. RETAILFIX module, rows, wiring, host test.
4. Fix site in INVENT.CPP behind `RetailFix_On`.
5. Menu row and labels; capture at 640x480.
6. Fixture arms, settings-before check, full suite.
7. Docs, knowledge Decision, `make knowledge-check`, then `graphify update .` and
   `make knowledge-graph`.
8. Format with the CI's clang-format (Docker), then push to #759 and mark it ready.
