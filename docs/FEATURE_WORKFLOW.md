# Feature workflow — reasoning and docs

When tackling a big new feature or tool, follow this workflow: read existing docs first, map the code, then document as you go. This doc shows the reasoning process for several example features.

Truth hierarchy: code > this document > external sources.

---

## Example 1: add console commands

**Goal:** Add a new command to the Quake-style debug console.

**Reasoning:**

1. **AGENTS.md:** Console is always available; new commands are additive and should avoid gameplay changes by default.

2. **Read first:** [docs/CONSOLE.md](CONSOLE.md) — lists existing commands, cvars, layout. Commands are registered somewhere; cvars have get/set pattern.

3. **Map the code:** Search for where commands are registered (e.g. `CONSOLE_CMD`, `cmdlist`, command table). Add new entry following the same pattern.

4. **Docs to update:** CONSOLE.md — add the new command to the Commands table with description and usage.

5. **Preservation:** Console lives in SOURCES/CONSOLE/; no French comments to remove. Add new comments alongside any existing ones.

---

## Example 2: headless mode

**Goal:** Run the engine without a window (e.g. for CI, automation, or server-side).

**Reasoning:**

1. **AGENTS.md:** Optional features behind flags → must be opt-in (build option or runtime flag). Preserve the nature of the game → headless changes *how* the game runs, not gameplay logic.

2. **Read first:**
   - [docs/LIFECYCLES.md](LIFECYCLES.md) — MainLoop frame order: `MyGetInput`, `ManageTime`, `DoDir`, `DoTrack`, `CheckZoneSce`, `DoLife`, `AffScene`. Headless would skip or stub `AffScene` and input.
   - [docs/GLOSSARY.md](GLOSSARY.md) — Entry point: `MainGameMenu` → `MainLoop`. Need to find where SDL/window is initialized.
   - [SOURCES/PERSO.CPP](../SOURCES/PERSO.CPP) — MainLoop, main entry.

3. **Map the code:**
   - Where does SDL create the window? (SYSTEM/, or early in main)
   - What does `AffScene` depend on? (Log, Screen, ModeDesiredX/Y)
   - Can we run `DoLife`/`DoTrack` without rendering? Likely yes; they update object state.

4. **Docs to create/update** (in the same commit as the code change):
   - New `docs/HEADLESS.md` — how to build with `-DHEADLESS=ON`, what runs (main loop steps), what is skipped (render, input), use cases (automation, tests).
   - Update README build options table.
   - Update LIFECYCLES if we add a "headless branch" to the main loop.

5. **Cross-platform:** Headless should work on Linux, macOS, and Windows. Avoid platform-specific headless tricks (e.g. Xvfb on Linux) in core code; document as optional for CI. Contributors and agents work on all three platforms—do not assume Linux.

---

## Example 3: change the game menu

**Goal:** Add a new menu option, reorder entries, or change Options structure.

**Reasoning:**

1. **AGENTS.md:** Preserve the nature of the game → menu changes affect UX. If adding (e.g. new Options toggle), make it opt-in or behind a config key. If reordering, ensure we don't break existing behavior.

2. **Read first:** [docs/MENU.md](MENU.md) — Menu tree, terms (CURRENTSAVE, SavingEnable, FlagSpeak), entry points (`MainGameMenu`, `BuildGameMainMenu`, `DoGameMenu`), template → build → drive flow, Menu layout, and Languages and localization (`BuildCustomMenuText`, language submenu, `InitLanguage` / config).

3. **Map the code:**
   - `RealGameMainMenu` — static template (text IDs 70–75)
   - `BuildGameMainMenu` — filters by runtime state
   - `GameOptionMenu` — Options submenu (text IDs 11–47)
   - `DoGameMenu` — generic driver; handles sliders (type 2–7)
   - Text IDs from `TEXT.HQR`; need to add new string if new label

4. **Docs to update:**
   - MENU.md — update menu tree, add new entry to Options if applicable
   - CONFIG.md — if new option is persisted (e.g. new slider)

5. **Preservation:** GAMEMENU.CPP may have French comments; preserve them. Menu structure is part of the game feel; document the change and rationale.

---

## Example 4: how does the camera work?

**Goal:** Understand camera behavior for a feature (e.g. camera control, replay, or debug view).

**Reasoning:**

1. **AGENTS.md:** Documentation first → use GLOSSARY, LIFECYCLES before inferring from code.

2. **Read first:**
   - [docs/CAMERA.md](CAMERA.md) — Interior vs exterior paths, `CameraCenter`, `SearchCameraPos`, Auto camera / `FollowCamera` (community).
   - [docs/GLOSSARY.md](GLOSSARY.md) — Zone type 1 = camera. `AllCameras` in CONFIG.
   - [docs/LIFECYCLES.md](LIFECYCLES.md) — Scene load phase 6: "Initialize camera position". Main loop step 7: `AffScene` (render).
   - [docs/MENU.md](MENU.md) — Options → Cameras (46/47), toggles `AllCameras`; Advanced options for Auto camera (`FollowCamera`).
   - [docs/CONFIG.md](CONFIG.md) — `AllCameras`, `FollowCamera` (Auto camera), legacy `AutoCameraCenter`.

3. **Also in code:** Zone type 1 (camera zones), `AffScene` / projection, `ChangeCube`/`OBJECT.CPP` for camera init, projection globals (`LIB386/3D/PROJ` — `XCentre`, `YCentre`, `NearClip`, etc.).

4. **Code locations:** Search for `Camera`, `Alpha`, `Beta`, `Gamma` (view angles), `XCentre`, `YCentre`, projection globals.

---

## Example 5: extract a subsystem from an original file

**Goal:** Move something that grew inside a 1997 file into its own translation unit, or pull a rule
that is spelled inline at many call sites into one place, without changing behaviour.

**Reasoning:**

1. **Count what CI can see of the code you are about to move, before anything else.** A
   refactor promises the behaviour is unchanged, and that promise is only worth what the tests
   backing it are worth. Find out which they are first:

   ```bash
   # is any of it linked into a host test (the only thing CI runs on every push)?
   grep -rn "SOURCES/YOUR_FILE.CPP" tests/*/CMakeLists.txt
   # is there a fixture that drives it? (local only: needs retail data and a display)
   grep -rln "your-verb\|YourFunc" tests/automation/
   ```

   Expect the answer to be no. Roughly **5% of `SOURCES` is linked into a host test**, and every
   file that is has no 1997 ancestor; the fixtures in [`tests/automation`](../tests/automation/README.md) are referenced by no
   workflow, and `LBA2_BUILD_ASM_EQUIV_TESTS` is `OFF` in all of them. So for most of the tree
   the honest starting position is that nothing would catch you.

   **A test you found is not yet an oracle: check it can fail.** A binary that calls `TEST_SUMMARY()` and
   falls off the end of `main` exits 0 whatever failed, and ctest reads that as a pass; two in this tree
   did until `test(camera): put the HD recompose rule under CI` fixed them, and the way that was
   established was to break the formula on purpose and watch the run stay green. Do that once, to the
   test you are about to lean on, before you lean on it. Coverage that cannot fail is worse than none,
   because it is quoted in the PR.

   **If nothing covers it, building that cover is the first commit of the refactor, not a later
   one.** Not a full suite: one host test over the part that can be reached without engine
   state, or one fixture pinning the surface as it behaves today. Everything below assumes it
   exists, and none of it is safe without it. [docs/TESTING.md](TESTING.md) has what runs where.

2. **CODESTYLE.md next:** ["Where new code goes"](../CODESTYLE.md#where-new-code-goes) and
   ["Features and surfaces"](../CODESTYLE.md#features-and-surfaces) say what the result has to look
   like: a new subsystem gets its own TU, a module owns its own state, and a surface must not name a
   feature's variables. What follows is the order to get there in, distilled from the two extractions
   that have been done this way (the Auto camera, PRs #533/#540/#542/#544, and the cfg reader, #541).

3. **Cut along the testable line first.** The part that reads no engine globals comes out before
   anything moves, as a header with a host test. From then on the refactor has an oracle that runs on
   every platform with no retail data. [SOURCES/FOLLOWCAM_MATH.H](../SOURCES/FOLLOWCAM_MATH.H) is the
   worked example, tested by `tests/camera/test_followcam_math.cpp`, and it landed a full PR before
   the module itself moved. If you cannot name that part, you are not ready to start.

4. **Make the extracted part correct on its own terms.** A helper lifted out of one call site leans
   on guarantees that caller happened to provide. Restore them inside the helper even where they are
   inert today, and say in the comment that they are inert and why:
   [`FollowCamRotStep`](../SOURCES/FOLLOWCAM_MATH.H) carries an overshoot clamp its two callers can
   never trigger, because a later change to either constant would otherwise walk the camera past its
   target.

5. **Move before you change.** The commit that creates the file is a move: same lines, new home, no
   edits. `refactor(camera): give the Auto camera its own file` is 472 insertions against 420
   deletions across four files, and reviewing it is checking that nothing changed. What you want to
   fix on the way gets its own commit afterwards.

6. **Then ownership.** The header declares exactly what the `.CPP` defines, and the module's globals
   come out of [SOURCES/C_EXTERN.H](../SOURCES/C_EXTERN.H) and
   [SOURCES/GLOBAL.CPP](../SOURCES/GLOBAL.CPP). The mechanical test is the include list: after the
   move, the files that genuinely use the module are the ones that had to add the include. For the
   camera that was seven, against the ninety-odd that could previously reach it by accident.

   **The include list is directional, so check both ends.** It proves who can reach the module,
   which is the win the camera got: seven includers against ninety. It says nothing about what the
   new file itself drags in, and `FOLLOWCAM.CPP` opens with `C_EXTERN.H` and pulls 184 headers,
   against a median translation unit's 5. Ask the build:

   ```bash
   scripts/ci/check-build-graph.py --report   # near the median decoupled; near the top did not
   ```

   Landing heavy is not a blocker, and no extraction so far has avoided it. It is the difference
   between having moved the code and having moved the coupling, and it is worth knowing which one
   the PR did.

   **A guard whose caller already decides it reads as intentional and moves with the code.** The include
   list proves what compiles, not what is reachable. `FollowCamHDExcess` was called as
   `CameraZone ? 0 : ...` inside a function whose only caller sits in `if (!CameraZone)`, so the test could
   never be true; it survived a whole extraction and had its non-existent behaviour written up in a fresh
   header on the way. When a conditional moves, check who calls it now, and delete the ones the caller
   already answers rather than carrying them into the new file.

7. **Then surfaces, one entry point each.** The cfg reader, the console, the CLI table and the
   options menu call into the module rather than naming its variables. Expect exactly one leak of
   private state and give it a named entry point instead of widening the header; both extractions so
   far found exactly one.

8. **Bugs found on the way are not part of the refactor.** They get their own commit, test first and
   allowed to fail, as in `test(camera): put the HD recompose rule under CI, and let two tests fail`
   followed by the fix. A behaviour change buried in a move commit is invisible to review.

9. **Write the rule down as you find it.** Three `docs(style)` commits landed inside those PRs, each
   recording something the extraction had just taught. Later means never.

10. **Re-read every doc that describes what you moved, then have the change reviewed.** A refactor
    produces false documentation in bulk, and CI cannot see any of it:
    `scripts/ci/check-docs-symbols.py` catches a doc naming a symbol in a file it has left, not a
    statement about behaviour that has become untrue. One audio change here left six false claims
    across three docs with every check green.

    Grep the docs for each identifier you moved and each surface you changed. Treat any list that
    claims to be complete as a defect until you have re-counted it: "all current call sites are
    covered", "two tables exist today", and a type named as the example for a rule it no longer
    follows were the three that went stale, and each reads as authoritative while being wrong.

    The review pass is not optional on a change like this, and not because the build might break.
    Its two catches are the doc that still describes the old shape, and the site of the same class
    you did not convert: the negative-volume clamp fixed for one key and missed for its neighbour
    two lines below, the stereo setting routed through two surfaces of three. Tests found none of
    those. Review found all of them.

**Docs to update:** the subsystem's own doc if the layout it describes moved, and
[CODESTYLE.md](../CODESTYLE.md) if the extraction taught a rule that generalises. Run
`scripts/ci/check-docs-symbols.py` afterwards: a doc naming a symbol in the file it has just left is
exactly what that check exists to catch.

---

## General workflow for big features

1. **Read AGENTS.md** — principles, Never, When Modifying X Do Y.
2. **Read relevant docs** — GLOSSARY, LIFECYCLES, and the doc for the subsystem (CONSOLE, MENU, AUDIO, etc.).
3. **Map the code** — find entry points, data flow, where your change plugs in.
4. **Document as you go** — if no doc exists, create one (e.g. HEADLESS.md, CAMERA.md). If behavior changes, update the doc in the same commit (environment trumps willpower).
5. **Preserve** — French comments, ASCII art; add new comments alongside.
6. **Verify** — run tests, format check. For LIB386 changes, run equivalence tests.
7. **When ambiguous** — ask the user before proceeding; do not guess.

## General workflow for refactors

The list above is feature-shaped: it assumes new behaviour and asks what to document. A refactor
promises the opposite, so its risks are different.

1. **Get an oracle before touching anything**, per step 1 of Example 5. A refactor with no way to
   say "same as before" is a rewrite, and in this tree the default is that no such way exists.
   Host tests are the cheapest (no retail data, every platform); the UI goldens in
   `tests/automation` and the projection corpus cover what needs a booted engine, at the price
   of not running in CI.
2. **No behaviour change inside a refactor commit.** If a commit both moves code and fixes
   something, split it. This repo reviews per commit rather than splitting PRs, and that only works
   when each commit answers one question.
3. **Convert one surface per commit, not one pattern per commit.** A sweep across every caller of a
   thing is neither reviewable nor bisectable. One file or one screen at a time, each verifiable
   against its own golden.
4. **Scope by what the rule owns, not by what shares a variable.** `ModeDesiredX` is a UI anchor at
   one site, a row stride at another and a projection origin at a third. Pulling all three into one
   helper because they spell the same global would invent a relationship the code does not have.
5. **Landing early is fine.** The rule having one home is the win; every caller reaching it is not a
   precondition for merging. An unconverted site is a known cost, not a regression.
6. **Assume you converted all of a class and did not.** Every miss on the changes this file
   describes has been the same shape: a sibling two lines away, a third surface where two were
   checked. After the last commit, grep for the pattern you were fixing rather than for the sites
   you remember fixing.
7. **Before pushing:** `scripts/ci/check-format.sh`, `scripts/ci/check-docs-links.sh` and
   `scripts/ci/check-docs-symbols.py` are what CI will run, and all three run locally. None of them
   can see a doc that is merely wrong.
