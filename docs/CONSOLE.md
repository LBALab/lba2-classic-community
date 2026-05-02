# Debug Console

Quake-style drop-down debug console for LBA2. The console is always compiled and works everywhere: in-game, menu, inventory, credits, and during video playback. DEBUG_TOOLS remains a separate optional layer for additional legacy hotkeys/features.

## Build

The console is part of normal builds (no dedicated CMake flag required).

```bash
cmake -B build
cmake --build build
```

The console is supported with the SDL backend (default in this project).

## Toggle and input

- **Backtick (`)** or **F12**: open/close the console.
- When the console is open, all keyboard input is consumed by the console (no game/menu input).
- Type a line and press **Enter** to run a command or cheat.
- **Backspace**: delete last character.
- **Up/Down**: scroll console output; the input line stays fixed at the bottom. When you are not at the newest lines, the last visible scrollback line shows `...`.

## Context command

Run **`context`** to print **cube** and **chapter** on one line (nothing is printed automatically when the console opens). More UI/engine context can be added later.

- Output looks like: `[Context] cube=N chapter=M`.
- Values are numeric only when **give**-style scene rules pass (loaded scene, non-phantom cube, not during ACF). Otherwise they are `-` so stale globals are not mistaken for live play state.

## Discovery

- **help** – list all commands with short descriptions. With an argument, **help &lt;name&gt;** prints usage and context for that command or cvar (e.g. `help cube`, `help fps`, `help context`).
- **cmdlist** – list command names only.
- **varlist** – list cvars (variables) with descriptions.
- **buildinfo** – print build timestamp and CMake options (SOUND_BACKEND, MVIDEO_BACKEND, ENABLE_ASM). The same string is written to the log at startup.

Unknown commands print a hint to use **cmdlist**. Commands that need an in-game scene (**give**, **savebug**) print a short reason if used while a video is playing, before a scene exists, or in the phantom-cube state.

## Cheats (by name)

These mirror the classic key-sequence cheats; you can type their name directly at the prompt and press Enter:

- **life** – max life
- **magic** – max magic
- **full** – full points
- **gold** – add gold/kashes
- **speed** – toggle frame rate display
- **clover** – clover
- **box** – clover box
- **pingouin** – MecaPingouin

## Commands

| Command       | Description |
|---------------|-------------|
| **cube** &lt;n&gt; | Request change to cube number &lt;n&gt; (applied next frame in game; cube changes no longer trigger autosave). |
| **load**      | Load save by player name as printed by `listsaves`, or by file name (with optional `.lba`). |
| **loadbug**   | Load bug save by name or file (optional `.lba`), default `bug`. |
| **listsaves** | List save games (player names from .lba files). |
| **listbugs**  | List bug saves in the bugs directory (same path as savebug). |
| **savebug** [name] | Save current game to bugs directory; optional name (default `bug`). |
| **timer** [ms] | Advance in-game timer by N ms (default 200). |
| **status** | Print island, cube, chapter, object/zone counts, FPS, timer, position. |
| **screenshot** | Save the **next** frame as PNG in the shoot directory **without** the console visible (uses SavePNG). |
| **give** &lt;n&gt; | Play “found object” sequence for inventory entry &lt;n&gt;; some items (e.g. weapons, protopack) also update gameplay state. Mostly for visual/debug use. |
| **playvideo** &lt;name&gt; | Play ACF video by name. |
| **listvideos** | List available ACF video names. |
| **playjingle** &lt;1-26&gt; | Play jingle by number. |
| **playmusic** &lt;num&gt; [loop 0\|1] | Play music track by number (optional loop flag, default 1). |
| **playsample** &lt;num&gt; [freq] [decal] [repeat] [volume] [pan] | Play sample by index with optional params: pitchbend (`freq`, default `0x1000`), random pitch range (`decal`), repeat count (`repeat`, 0=loop, default 1), volume (0–127, default 127), pan (0–127, default 64). |
| **audio** ... | Audio commands: `audio sample play/stop_all`, `audio music play/stop`, `audio global pause/resume/stop_all/reset/reverse_stereo/log`. Calls HQ/AIL functions directly (see [AUDIO.md](AUDIO.md)). |
| **video** ... | Video commands: `video log <0|1>`. Toggle PlayAcf diagnostic logging (video name, path, language for INTRO). |
| **exit**      | Exit the game immediately (clean shutdown). |

## CVars

Get/set with `varname` (print value) or `varname value` (set).

| Cvar | Type | Description |
|------|------|-------------|
| **fps** | bool | Frame rate display. |
| **horizon** | bool | Draw horizon / cubes around. |
| **zv** | bool | Draw ZV boxes. |

## Layout

- Console is a panel at the top of the screen.
- Scrollback shows recent lines; the last line is the input with a `]> ... _` prompt. When scrolled up, an ellipsis `...` appears on the last visible scrollback line to indicate older messages above.
- Output from commands and cheats appears in the scrollback.

## Implementation notes

- **Independent of game buffer**: The console uses its own 8-bit overlay buffer. It is drawn via `AffStringToBuffer` (LIB386/SVGA) and composited in the video layer’s **pre-present callback** (`Console_PrePresent`), so it never touches the game’s `Log` or dirty-box pipeline.
- **Event-driven input**: Keys are fed from the event loop via `Console_FeedEvent` (registered with `SetEventFilter`). When the console is open, key events are queued and processed each frame by `Console_Update()` (no arguments). **Backtick and F12 are reserved**: they are cleared from `TabKeys` and `Key` so the game never sees them (no clash with e.g. `I_CAMERA`).
- **Hooks**: `SetEventFilter(Console_FeedEvent)` and `SetPrePresentCallback(Console_PrePresent)` are registered in `main()` after `InitAdeline()`. `MyGetInput()` reserves backtick/F12, and when the console is open calls `Console_Update()` and returns.
- **Module**: `SOURCES/CONSOLE/` – `CONSOLE.H`, `CONSOLE.CPP` (core), `CONSOLE_CMD.CPP` (commands/cvars). Core links only LIB386 (AFFSTR for text) and SDL for events; no dependency on game `Log` or dirty-box.
- **Gameplay integration**: Commands call existing engine functions (`LoadGameNumCube`, `PlayAcf`, `DoFoundObj`, etc.) rather than introducing console-only code paths. Cube changes no longer trigger autosave to keep debug teleports tidy; other save behavior is unchanged.

### Extending commands and cheats

- **Commands**: Add new handlers in `SOURCES/CONSOLE/CONSOLE_CMD.CPP` and register them via `Console_RegisterCommandEx("name", handler, "short description", "usage line", "context line")` (or `Console_RegisterCommand` if usage/context are omitted) inside `Console_RegisterAll()`.
- **CVars**: Register new tunables via `Console_RegisterCvar` in `CONSOLE_CMD.CPP`, wiring them to existing globals rather than duplicating state.
- **Cheats**: To add a cheat that works both from key sequences and the console, extend `CheatNames[]` and `ApplyCheat` in `SOURCES/CHEATCOD.CPP`; the console will automatically route matching command names through `TryExecuteCheatByName`.
- **Safety**: Prefer read-only inspection commands or one-shot state changes that leave the game in a consistent, save/load-safe state. If a command is inherently risky (e.g. heavy state mutation), call it out explicitly in its help text.
