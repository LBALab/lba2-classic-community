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
   - Text IDs from `text.hqr`; need to add new string if new label

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

## General workflow for big features

1. **Read AGENTS.md** — principles, Never, When Modifying X Do Y.
2. **Read relevant docs** — GLOSSARY, LIFECYCLES, and the doc for the subsystem (CONSOLE, MENU, AUDIO, etc.).
3. **Map the code** — find entry points, data flow, where your change plugs in.
4. **Document as you go** — if no doc exists, create one (e.g. HEADLESS.md, CAMERA.md). If behavior changes, update the doc in the same commit (environment trumps willpower).
5. **Preserve** — French comments, ASCII art; add new comments alongside.
6. **Verify** — run tests, format check. For LIB386 changes, run equivalence tests.
7. **When ambiguous** — ask the user before proceeding; do not guess.
