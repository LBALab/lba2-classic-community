# CLI control harness

Drive the engine from the command line for automation and regression: boot, restore a
save, run console commands, advance the simulation by N ticks, dump engine state to JSON,
take a screenshot, and exit — in one invocation. The foundation for an agentic dev loop
and for record/replay regression nets.

This is an outside-in harness. It reuses existing engine seams — the
[console command bus](CONSOLE.md) for `--exec`, the normal save-load sequence for
`--load`, the existing `SavePNG` for `--screenshot` — rather than changing game logic. It
is not a scripting runtime. See
[AUTOMATION_RESEARCH.md](plan/AUTOMATION_RESEARCH.md) and
[AUTOMATION_PLAN.md](plan/AUTOMATION_PLAN.md) for the design.

Everything above is one invocation: the whole plan is fixed before boot, and the run
either does it or does not. That covers automation and regression, which is most of what
the harness is for. It does not cover a probe loop whose next step depends on what the
last one showed. There is an opt-in socket for that; see
[Driving a running engine](#driving-a-running-engine---listen). It is a debug build only,
and off by default there too.

> **Pass `--headless` for any automated run.** Without it the engine opens a window, and
> **the game pauses when that window loses focus** (by design, for players). A run that
> loses focus gets a stopped clock, and several runs at once fight over it, so results stop
> being reproducible. Six identical `--fixed-dt` runs in parallel produced five different
> sim states; the same six with `--headless` were byte-identical. `--headless` also implies
> `--no-audio`, since a live audio thread branches the simulation too.
>
> Pair it with `--no-autosave` unless you want the run rewriting your `autosave.lba`.

## Build

Always compiled into desktop builds; no CMake flag. Run a normal build:

```bash
make build      # -> build/SOURCES/lba2cc
```

## Usage

```
lba2cc --help                 print the player-facing flag list and exit (an unknown flag
                              is now an error, not silently ignored)
       --help-all             every flag, grouped, including the automation and engine
                              self-test surface below; generated from the parser's own
                              table, so it can't drift from what the engine accepts
       --headless             no window, no audio device: the supported mode for
                              automation. See the note above.
       --no-autosave          don't write autosaves (autosave.lba / current.lba), which an
                              in-game scene transition would otherwise rewrite on your own
                              install. Explicit saves still work: the menu's Save, and the
                              console's `savebug`, which writes to the bugs directory
       --load <slot>          restore a save before the loop starts
       --exec "<cmd>;<cmd>"   run console commands (';'-separated) on the first tick
       --exec-at <T> "<cmds>" run console commands at tick T; repeatable. Use this when a
                              command depends on a scene change having settled: a `cube`
                              change only applies on the NEXT frame, so
                              --exec "cube 154; teleport actor 3" teleports in the OLD
                              cube and the pending change then resets the hero. Instead:
                              --exec "cube 154" --exec-at 10 "teleport actor 3"
       --tick <N>             advance N simulation ticks
       --fixed-dt <ms>        advance the clock by a constant <ms> per tick (deterministic,
                              but only in a --headless run)
       --fixed-timestep <ms>  set the sim throttle (FixedTimestep) for this run only, no cfg
                              persist (0 = off); combine with a small --fixed-dt to force skips
       --vsync <on|off>       set the vsync setting (DisplayVSync) for this run only, no cfg
                              persist. The setting, not the frame pacing: a --exit run drops
                              the presentation cap either way. Pin it for any capture of a
                              surface that prints the setting, such as the Display submenu
       --language <name>      override Language at boot (English | Français | Deutsch |
                              Español | Italiano | Portugues; or EN | FR | DE | SP | IT | PO)
       --no-audio             skip InitAIL() and InitSampleDriver() at boot — bypasses
                              the SDL dummy driver's nanosleep pacing on WSL2 setups
                              (~58% of sys time in projection_demo without this)
       --resolution WxH       override the boot render resolution; overrides both the
                              compile-time default and lba2.cfg's ResolutionX/Y. Width
                              must be a multiple of 8, range 320x200 - 1920x1080.
                              Also the recovery escape hatch ("I can't see anything,
                              get me back": run with --resolution 640x480).
       --log-level <level>    master log verbosity for every sink (adeline.log, the
                              terminal, and the F12 console): debug | info | warn | error.
                              Default info, so debug output is off unless asked for. Also
                              settable with the LBA2_LOG_LEVEL env var (the flag wins) or
                              live via the `loglevel` console command.
       --res-switch-test WxH@TICK
                              harness validation only — fires Res_Switch(W, H) once
                              on tick TICK from inside Control_TickHook. Exercises
                              the runtime switch path without going through the
                              console verb. See docs/RUNTIME_RESOLUTION.md.
       --demo                 attract/demo mode: scripts drive the scene, modals auto-advance
       --dump-state <path>    write a JSON snapshot of engine state
       --screenshot <path>    write a PNG of the rendered frame
       --polyrec <path>       record polygon draw calls of the frame (ENABLE_POLY_RECORDING builds)
       --capture-projection <path>   record every projection-pipeline event to a text file
       --projection-hash <path>      record only an FNV-1a 64-bit digest of the above (CI-friendly)
       --exit                 exit cleanly after the above
```

All flags are optional and composable. With none, the game launches normally. Passing a
flag that *asks the engine to do something* (`--load`, `--exec`, `--exec-at`, `--tick`,
`--dump-state`, `--screenshot`, `--exit`, …) takes the automated boot path and skips the
distributor/Adeline logos. Flags that only describe the environment (`--headless`,
`--no-autosave`, `--resolution`, `--language`, `--no-audio`, `--vsync`) do not, so
`lba2cc --headless` on its own is still just the game, with no window.

Order of operations: `--load` (or a fresh start) → first tick runs `--exec` →
advance to N ticks → `--dump-state` + `--screenshot` → `--exit`.

### Examples

```bash
export LBA2_GAME_DIR=/path/to/data

# Restore a save, advance 30 ticks, snapshot state, exit.
lba2cc --headless --no-autosave --load "021 Palace" --tick 30 --dump-state state.json --exit

# Jump to a cube from a fresh start and screenshot it.
lba2cc --headless --exec "cube 100" --tick 5 --screenshot shot.png --exit

# Batch several commands.
lba2cc --headless --no-autosave --load mysave --exec "cube 100; give clover 3" \
       --tick 5 --dump-state s.json --exit

# Run a command only after the scene change has settled (--exec would race it).
lba2cc --headless --exec "cube 154" --exec-at 10 "teleport actor 3" --tick 60 --exit

# Force a language for the run (bypasses the cfg's Language key — useful for
# regression captures that should be reproducible regardless of the developer's
# local lba2.cfg).
lba2cc --language Français --load Anon1 --exec "ui dialog 1 /tmp/fr.png" --fixed-dt 16 --tick 200 --exit

# Construct a scenario without hunting for the exact save: jump the hero onto an NPC,
# arm the quest flag its script waits on, and watch what the script does.
lba2cc --load mysave --exec "teleport actor 2; varcube 0 1; lifetrace 2" --tick 20 --exit
```

**Driving game state.** The harness has no movement or menu input, so state- and quest-gated
interactions were previously unreachable headlessly. Four console commands close that gap
(see [CONSOLE.md](CONSOLE.md)): `teleport` (position the hero / jump onto an actor), `varcube`
/ `vargame` (read or set the scene/game Life variables that gate quest progression), and
`lifetrace` (log an NPC's Life-script state and every condition it evaluates). Together they
let a single `--exec` line reproduce an interaction (position, quest flags, and script
observability) that would otherwise need a specific playthrough save.

**Driving the camera.** The Auto camera's analog orbit answers only to a mouse or a gamepad in
front of the screen, which puts the whole free-camera path beyond the harness's reach.
`camnudge` feeds the same per-frame nudge those devices feed, at the same point in the frame,
and `camtrace` logs the camera's angle state on every follow-cam update. A camera bug
that reads as "the view tore from one angle to another" becomes a scripted run with numbers:

```bash
# Orbit away, turn the hero in place while the camera holds, then touch the stick.
lba2cc --headless --load mysave --fixed-dt 16 \
    --exec "cam_follow 1; cam_hold_angle 1; camtrace 1" \
    --exec-at 30 "camnudge 40 0 10" --exec-at 100 "input left 60" \
    --exec-at 170 "camnudge 1 0 1" --tick 200 --exit
```

`--load` resolves its argument as a direct file path first, then as a save name in the
save directory, then with a `.lba` suffix — so both `--load "021 Palace"` and
`--load /full/path/021 Palace.LBA` work. Note the *name* form looks in the user save
directory, not the retail `SAVE/` folder, and that the console's own `load` command
resolves by player name via a separate path (`cmd_load`), so a name that works in one
won't necessarily work in the other. Pass a path if in doubt.

### Finding out what you can drive

The harness tells you itself; you shouldn't need to read source or this document to start.

```bash
lba2cc --headless --exec "cmdlist"   --tick 2 --exit   # every console command
lba2cc --headless --exec "help cube" --tick 2 --exit   # usage for one of them
lba2cc --headless --exec "listsaves" --tick 2 --exit   # the names --load accepts
```

Console output is mirrored to stdout, so any command's output is readable from a batch run.
`lba2cc --help` lists the flags and repeats these three lines.

### Notes and limits

- **A rendered artifact needs a tick.** `--screenshot` forces a minimum of one tick (you
  can't screenshot a frame that was never drawn). A `--dump-state` with `--tick 0` reflects
  the loaded state before any simulation step.
- **Vsync is off under the harness.** A `--exit` run disables the renderer's vsync cap so
  the loop runs flat-out instead of at ~60 fps — a long `--tick N` run is ~10-14x faster
  (e.g. 2000 ticks: ~34 s to ~2.5 s here). Presentation only: the dumped state and rendered
  pixels are identical (verified 0 drift over the save corpus). The `DisplayVSync` *setting*
  is a separate thing, still read from the cfg and still printed by the Display submenu;
  `--vsync <on|off>` pins it for the run without touching the cfg.
- **A tick is one main-loop iteration, normally one rendered frame.** In a settled scene
  the two are identical. The exception is a cube transition: when a script (or a `cube`
  command) changes the scene, the loop restarts its body to load the new cube, which counts
  as another tick without a separate rendered frame. So `--tick N` is exact for "advance N
  frames in this scene", and an over-count by the number of cube changes when the run
  crosses scene boundaries.
- **`--exec` is limited to the [console commands](CONSOLE.md)** — `cube`, `give`, `timer`,
  `status`, `list*`, cvars, etc. Run `cmdlist` in the console to see them.
- **Scene-mutating commands need an active scene.** Commands like `give` operate on the
  loaded game and no-op (with a reason printed) outside a normal play state. From a fresh
  start (no `--load`) the intro cube may not yet be a normal play scene, so pair such
  commands with `--load`, or a `cube` jump plus a tick, before relying on them.
- **Modal commands block a headless run.** Commands that open a cinematic, video, or
  dialogue wait for input to dismiss them, so a `--exit` run hangs on them. This includes
  `give <item>` (the found-object cinematic), `playvideo`, `credits`, and `slide`. Use
  non-modal commands: `cube`, `give clover`, `timer`, cvars, `status`. (`give clover`
  takes a different code path with no cinematic.)
- **A fresh start walks into a modal on its own, at about 4s of sim time.** The point above
  is about modals *you* open. The opening scene opens one by itself: object 4's Life script
  runs a `MESSAGE` opcode roughly four seconds in, `Dial` spins in `SpeakAnimation` waiting
  for a dismiss that headless never sends, and because that loop retires no ticks, `--tick N`
  never reaches N. The run does not fail, it spins on one core until the timeout.

  It is the clock that decides, not the tick count, so raising `--fixed-dt` does not buy
  headroom: the wall is ~3.9s of simulated time either way (244 ticks at `--fixed-dt 16`
  survive, 248 do not; 480 at dt 8 survive, 520 do not).

  Three ways past it:

  ```bash
  # Start somewhere else: --load lands in another cube and never meets the script.
  lba2cc --headless --load "021 Palace" --fixed-dt 16 --tick 600 --exit

  # Let the demo reel auto-advance its modals.
  lba2cc --headless --demo --fixed-dt 16 --tick 300 --exit

  # Or dismiss it: arm esc just before the dialogue opens.
  lba2cc --headless --fixed-dt 16 --exec-at 240 "key esc 40" --tick 300 --exit
  ```

  Arming the key early does **not** work. `--exec "key esc 400"` from tick 1 still hangs:
  the modal latches on entry through `InitWaitNoInput`, and a key already held when it opens
  never registers as a press. The press has to land *inside* the modal, which is what the
  `polls` argument is for. `key return` does not dismiss a dialogue; `esc` does.

  Run with `--verbose` and the last line before the hang names the modal
  (`[control] modal: Dial(...)`), which is faster than reaching for a debugger. Those markers
  go to **stderr**, not `adeline.log`.
- **`--exec` fires on the first tick, which races a scene change.** A `cube` change applies
  on the *next* frame, so `--exec "cube 154; teleport actor 3"` runs the teleport in the
  **old** cube and the pending change then resets the hero to the new cube's spawn. Nothing
  reports this; the run just quietly doesn't do what you asked. Schedule the dependent
  command instead: `--exec "cube 154" --exec-at 10 "teleport actor 3"`.
- **The first frame after `--load` is a full redraw.** Anything triggered on tick 1 takes
  the full-redraw path, which hides exactly the partial-frame rendering bugs (stale
  background, dirty-box) the harness is good at catching. To exercise the normal path, drive
  the hero into the event (`--exec-at`, or hold `input up N`) so it lands a few frames in.
  This is why the first repro of the #424 ghost door came back clean.
- **`--demo` only drives an authored demo scene** (the reel at cubes 193-221). On any other
  scene it silently does nothing, so a long run looks like evidence of absence when it's
  just an idle hero. The harness warns when it lands on a non-demo scene.
- **`--polyrec` needs an `ENABLE_POLY_RECORDING` build.** It drives the existing polygon
  draw-call recorder (`tests/SNAPSHOT/`) at the final tick, writing a `.lba2polyrec` file at
  the chosen path — the scripted equivalent of the manual Alt+F9 capture. On a build without
  the option it prints a warning and is a no-op.
- **`--fixed-dt <ms>` makes the run deterministic, *if* the run is `--headless`.** It pins
  the per-tick clock step to a constant instead of wall-clock, so `--dump-state` is
  reproducible run-to-run (see Determinism). This holds only headless: a windowed run
  pauses on focus loss and its audio thread branches the sim, so its clock depends on what
  the rest of the desktop is doing. The step is a fixed *choice*, not a recovered constant. The canonical value
  for golden baselines is **16 ms** (≈ the 60 fps the dump reports). A different step yields a
  different but still-reproducible trajectory, so pick one and keep it. The flag is harness-only
  and has no effect on default gameplay timing. An invalid or non-positive value is ignored with
  a diagnostic.
- **`--demo` runs attract/demo mode** (sets `DemoSlide`). It only does something on an authored
  *demo scene* (the copies at cubes 193-221, see [SCENES.md](SCENES.md)): such a scene drives
  itself from its Life/Track scripts with no player input, and dialogues/choices auto-advance on
  the game clock instead of waiting — so it plays through headless instead of hanging on a prompt.
  (On a regular scene there is no demo script, so the hero just idles.) Combined with `--fixed-dt`
  this is a deterministic scripted playthrough; cube 193 (the retail menu attract,
  `GAMEMENU.CPP:2435`) is exercised this way by `tests/automation/test_demo.sh`. Opt-in; default
  gameplay is unaffected. (Note: a demo scene ends on a cutscene/`PLAY_ACF` that `--demo` does not
  auto-advance, so it still blocks a `--exit` run — cap the tick budget before it; see the modal
  caveat above.)
- **Natural end captures too.** If the simulation ends on its own before the `--tick`
  budget — e.g. the `--demo` attract reel reaches `LM_THE_END` at cube 218, returning
  from `MainLoop` — the harness still writes the configured `--dump-state` / `--screenshot`
  before exit. So you can run `lba2cc --exec "cube 205" --demo --tick 30000 --dump-state
  end.json --exit` and get a snapshot of the Dark Monk finale at tick ≈ 25 443 without
  having to pre-compute the right tick.
- **A `--game-dir` that doesn't hold the game data fails the run.** Discovery falls back to
  the env var, then a persisted path, then auto-discovery, so an unusable `--game-dir` would
  otherwise boot a *different install* and mention it only in the banner: an A/B against
  assets you never asked for, exiting 0. Point it at the folder holding the HQR files (often
  `Common/`), which is what `LBA2_GAME_DIR` expects too.
- **A batch or headless run never opens the game-data folder picker.** `--headless` brings SDL
  up on the dummy video driver, so `SDL_INIT_VIDEO` succeeds and the modal picker really does
  appear, with nobody to answer it: the run would hang forever. It exits with the fix in the
  message instead.
- **A bad `--load` fails the run.** It prints `save not found`, exits non-zero, and writes
  no `--dump-state` / `--screenshot`. It used to carry on with a fresh game and exit 0, so a
  typo'd save name handed back plausible artifacts of the wrong scene (cube 0) and a green
  exit code. Note that a *name* resolves against the user save directory, not the retail
  `SAVE/` folder; pass a path if the save lives elsewhere.

## `--dump-state` JSON

Schema-versioned, hand-written, all-integer fields:

```json
{
  "schema": 1,
  "tick": 30,
  "timer_ref_hr": 41234,
  "fps": 60,
  "scene": { "island": 4, "cube": 154, "cube_mode": "interior",
             "num_objects": 13, "num_zones": 4, "num_tracks": 5, "num_patches": 16 },
  "hero": { "x": 5926, "y": 256, "z": 4935, "alpha": 0, "beta": 2337, "gamma": 0,
            "life": 250, "behaviour": 1, "weapon": 10,
            "body": 2, "anim": 66, "gen_anim": 0, "last_frame": 5, "nb_frames": 15,
            "move": 1, "zone_sce": -1, "flags": 2119, "work_flags": 16 },
  "camera": { "beta": 1964, "alpha": 315, "gamma": 0, "add_beta": 0,
              "distance": 13750, "offset_x": 9298, "offset_y": 2701, "offset_z": 25412,
              "follow": 1, "hold_angle": 1, "base_dist": 13750, "view": 0,
              "zone": 0, "forced": 0, "cine": 0 },
  "inventory": { "magic_level": 3, "magic_point": 60,
                 "gold": 0, "zlitos": 142, "keys": 1, "clover_boxes": 6 },
  "actors": [ { "index": 0, "x": 5926, "y": 256, "z": 4935, "beta": 2337,
                "life": 250, "body": 2, "anim": 66, "move": 1, "flags": 2119 } ],
  "vars": { "count": 256, "nonzero": { "0": 1, "14": 5, "88": 25 } },
  "vars_cube": { "count": 80, "nonzero": { "0": 1 } },
  "log": [ "Cube 100 (change on next frame)" ]
}
```

- `tick` — completed ticks (the hook is inside the loop body, so this counts real
  iterations).
- `timer_ref_hr` — the engine's master clock in ms (accumulated play time). It is
  persisted in saves and restored on `--load` (`SAVEGAME.CPP:845`), so a loaded game's
  clock starts at the save's play-time, not zero — e.g. an early save reads a few hours,
  a late one ~12 h. Expect a large value; only its monotonic advance across ticks is
  meaningful for the harness.
- `scene` — island/cube and live object/zone/track/patch counts.
- `hero` — `ListObjet[0]`: world position, local angles, life, current behaviour and
  weapon, body/anim ids, animation frame, movement and zone state, flag words.
- `inventory` — magic, money (gold/zlitos), keys, clover boxes.
- `actors` — every live object (`0..num_objects-1`); the hero is also `actors[0]`.
- `vars` — `ListVarGame[]` (the saved script-variable array), emitted sparsely as
  `{count, nonzero}` to keep the file small.
- `vars_cube`: `ListVarCube[]`, the scene/cube script-variable array, same sparse shape.
  Together `vars` + `vars_cube` are the full quest state, so dumping a save exposes which
  quest flags it has set (e.g. a gate an NPC's script is waiting on).
- `log` — recent console scrollback lines.

### Seeing the log in a harness run

The stderr sink always emits, so a piped run shows the boot log and any `Log_*`
output **inline on stderr** (plain, severity-tagged, no ANSI when redirected);
you don't have to open `adeline.log`:

```bash
lba2cc --load X --tick 30 --exit 2>&1 1>/dev/null   # log only (stdout is data)
```

`stdout` stays a pure data channel (`--dump-state`, the `--exec` result mirror),
so redirect the two separately. Merging them with a bare `2>&1 | grep ...` is
worse than untidy: a line emitted while the console is driving the run reaches
both the stderr sink and the console's stdout mirror, so the merged capture holds
every line twice, interleaved from two differently-buffered streams. On a
per-frame trace that shows up as repeated timestamps and lines out of order,
which reads exactly like the game clock jumping backwards. Capture one sink. Debug lines are off by default; add
`--log-level debug` to surface them for the run (inline on stderr and in
`adeline.log`). The `--dump-state` `log` field still captures only the last
handful of *console* scrollback lines; stderr is the full stream.

## Driving a running engine (`--listen`)

`--exec` and `--exec-at` need the whole plan before boot. A probe loop whose next step
depends on what the last one showed cannot be written that way, so every observation costs
a boot, a load, and a walk back to the scene. Searching the camera cvars for a value that
fixes a behaviour is that shape.

`--listen <port>` puts a line server in front of the same console command bus, so the verbs
and cvars already there answer a driver outside the process while the run proceeds.
Measured against the alternative on one fixture: **0.6–1.2 ms** per command, against
**282 ms** for a boot-per-probe, and **1–2 ms** for a mid-session `load` to reset the scene.

### Build and run

Not in a normal build. It runs arbitrary console commands for whoever connects, so it is
kept out of any binary a player might run:

```bash
cmake -S . -B build-ctl -DLBA2_CONTROL_SERVER=ON
./build-ctl/SOURCES/lba2cc --headless --no-audio --user-dir /tmp/probe --no-autosave \
    --load "$SAVE" --listen 4444
```

Off again at runtime unless `--listen` names a port, and bound to `127.0.0.1` only. Both
gates are deliberate; neither is a substitute for the other.

```bash
scripts/dev/lba2ctl.py status          # one command
scripts/dev/lba2ctl.py                 # a REPL
```

### Protocol

One command per line in; that command's console output back verbatim, terminated by a line
reading `<<END>>`. Anything the engine logs while the command runs lands in the response
too, since the log fans out to the console. `stream on` additionally pushes every log
record as it happens, each prefixed `! `, which is how the trace verbs (`objtrace`,
`lifetrace`, `cubetrace`, `camtrace`, `input trace`) reach a driver live rather than only
`adeline.log`. One client at a time; `exit` shuts the engine down and is the last command a
connection gets an answer to.

### Two traps worth knowing before reading any number off a session

- **A command runs once per presented frame, not once per simulation tick.** Presents
  outnumber ticks by one to two orders of magnitude here, so a command that changes
  simulation state and a command that reads the consequences can both land inside one tick,
  and the read returns what was true *before* the write. It does not look like a race: it
  looks like a stable, repeatable measurement of the wrong thing. Let a tick elapse
  whenever the read depends on anything the object loop computes: zones, collision,
  animation, position.
- **An uncapped session is a regime no player sees.** Headless with nothing capping the
  loop the engine renders on the order of 1500 frames a second, while harness input is
  metered in sim ticks, so `input up 120` spends itself far faster than wall-clock suggests
  and anything rate-coupled behaves accordingly. Send `fixedtimestep 40` first.

### Notes and limits

- **Not deterministic, by construction.** Commands arrive at whatever tick the driver sent
  them, so a session does not replay. That is the trade for reacting to what you see; when
  a run needs to be reproducible, `--exec-at` is still the tool, and CI should stay on it.
- **`--listen` alone does not end the run.** Without `--tick`, the run is driven by whoever
  connects rather than by a budget. An explicit `--tick` still wins and closes the socket
  when the budget is spent.
- **The console tokenizer does not understand quotes.** Save names with spaces go
  unquoted: `load Desert Island`, never `load "Desert Island"`.
- **An isolated `--user-dir` hides your saves.** `listsaves` returns nothing and a
  mid-session `load` finds nothing until a save is copied into `<user-dir>/save/`. That
  matters because the mid-session reload is what makes a sweep cheap.
- **A driver that stops reading loses lines, and never stalls the game.** Sends are
  bounded; a client that will not drain is dropped rather than allowed to hold a frame.
  Pushed events queue in a ring that discards oldest-first and says how many it dropped.

## Determinism

**Run `--headless`, or none of this holds.** A visible window pauses the game on focus loss
(`HandleEventsTimer` locks the clock), so a run that loses focus, or several runs competing
for it, silently diverge. Measured: six identical `--load --fixed-dt 16 --tick 360` windowed
processes in parallel produced **five distinct sim states** (`timer_ref_hr` spread over 5.7 s,
one run's hero ending up in a different room). The same six with `--headless` were
byte-identical. Everything below assumes headless.

The same `--load X --tick N` is **more reproducible than expected**. Measured by running it
3× and diffing dumps (null audio):

- Interior scene: 275/277 fields bit-identical; only `fps` and `timer_ref_hr` (both
  wall-clock derived) vary.
- Busy exterior scene (35 objects): 490/498 identical; the only state drift is the
  position of the actors that were actively walking, and only ~0.1% — from the ±1-2 ms
  per-tick wall-clock variance feeding movement integration. The `srand(TimerRefHR)` reseed
  produced no observable divergence.

So the dominant nondeterminism is **variable per-tick dt**, not RNG. Two ways to run:

- **Without `--fixed-dt` (variable dt):** build/run with `-DSOUND_BACKEND=null` (or
  `SDL_AUDIODRIVER=dummy`). Assert **exact** on discrete and static fields (scene, vars, hero
  behaviour/body/anim, non-moving actors); allow a small **tolerance** on moving-actor
  positions; ignore `fps` and `timer_ref_hr`.
- **With `--fixed-dt <ms>` (constant dt):** the clock advances by exactly `<ms>` per tick, so
  `timer_ref_hr` becomes `base + N*ms` and moving-actor positions stop jittering. Measured: a
  busy exterior save dumped 10× was byte-identical including `timer_ref_hr` and
  `x/y/z/beta/anim/last_frame`; only `fps` and `log` stay run-specific. So under fixed-dt those
  kinematic fields can be asserted exact, not just within tolerance. This is the prerequisite
  for faithful input replay.

  **Full exterior determinism requires the null sound backend** (`-DSOUND_BACKEND=null`), not
  just `SDL_AUDIODRIVER=dummy`. With the SDL audio backend, voices are serviced on a wall-clock
  callback thread and exterior ambient-sample logic (`GereAmbiance`, `IsSamplePlaying`) branches
  the simulation nondeterministically even under the dummy *driver* — a residual ±1-unit
  actor-position wobble remains. The null *backend* removes that path. Interior scenes are
  deterministic either way. fixed-dt pins the clock; it does not determinise audio-thread timing.

The single RNG seed (`srand(TimerRefHR)` in `ChangeCube`) is already pinned by the restored
save clock — confirmed: under fixed-dt the dumps are identical with no explicit reseed.

Caveat — exactness is **per platform/build**. `--fixed-dt` removes *temporal* nondeterminism,
not the cross-platform coordinate differences from `long double` precision (80-bit on Linux
x86_64, 64-bit on Windows/macOS-ARM; see [PLATFORM.md](PLATFORM.md)). Two platforms running the
identical fixed-dt sequence can still land on slightly different positions. Keep golden
baselines per-platform, or compare across platforms with a tolerance.

Modal and fade loops also advance the fixed-dt clock — necessary because they run between
main-loop tick hooks and would otherwise spin forever on a clock deadline. The main loop's
single render per tick stays free of double-counting, so non-modal ticks (including the
whole savegame baseline corpus) are byte-identical to the merged behaviour. See
[FIXED_DT_RESEARCH.md §7](plan/FIXED_DT_RESEARCH.md) for the loop classes and the design.

## UI capture

Six console verbs drive each modal UI surface from the harness, render a settled frame,
write a PNG via `SavePNG`, then exit cleanly. The world-space `--dump-state` is the
guardrail for *simulation* state; these are the guardrail for *UI* rendering — what
widescreen, font, palette, or layout changes are most likely to disturb.

| Verb | Captures |
|---|---|
| `ui inventory <path>` | The inventory wheel + items + scene background |
| `ui holomap <path>` | The rotating planet globe + island name strip |
| `ui dialog <text-id> <path>` | The dialogue bubble + portrait + typewriter text for that text-id |
| `ui menu-options <path>` | The in-game ESC menu (Volume / Language / Advanced / Controls) over the shaded scene |
| `ui menu-main <path>` | The boot-time main menu (Resume / New Game / Load / Options / Quit) |
| `ui found-object <numvar> <path>` | The found-object cinematic — 3D rotation of `TabInv[numvar]`'s item + dialogue |

Each verb takes the destination path as its last argument. Compose with `--load` and
`--exec` like any other console command; the verb opens the modal, captures, and exits
without waiting for input:

```bash
lba2cc --load mysave.lba --exec "ui inventory inv.png" --fixed-dt 16 --tick 200 --exit
lba2cc --load mysave.lba --exec "ui menu-main main.png" --fixed-dt 16 --tick 120 --exit
```

`text-id` for `ui dialog` indexes into whichever dialogue bank the current save's island
has loaded (`START_FILE_ISLAND + Island`); ids invalid for that bank cause `Dial` to
return early with no capture. `numvar` for `ui found-object` is the `TabInv` slot index
(0 is Holomap, 1 is the magic ball, etc.).

### Driving a modal instead of photographing it

`ui askchoice [--press <key>] <question-id> <choice-id>...` is the odd one out: it opens the
dialogue choice list the way `LM_ASK_CHOICE` does and reports state rather than writing a PNG.

```bash
lba2cc --load mysave.lba --exec-at 40 "ui askchoice --press menus 1 2 3" --tick 120 --exit
# -> ui askchoice done: chose=2 polls=37099 MyKey=0 Input=0x0
```

The option row is picked automatically once `DoGameMenu` is up, the way the capture verbs
auto-exit their modal, because a selection scheduled from outside cannot know when the
question line ends. `--press` names a key held from just after the *chosen* line starts
playing: the one moment a Menus or Esc press is a fresh press mid-playback, which is the
state that mattered for issue #451.

`polls` is how long the modal ran, so a press that lands shows up as a run far shorter than
the same one without it. `MyKey` and `Input` are what the modal left behind: both must be
clear, or the main loop reads the same press again and opens the menu.

This verb needs a voice line to be playing, so it is the one place that cannot use the
`--headless` helpers, which imply `--no-audio`. `IsSamplePlaying` is then always false and
every wait-for-the-line loop falls straight through. `ctl_voice` in `tests/automation/lib.sh`
is the windowless-but-audible pairing; see `test_askchoice_menus_consume.sh`.

### Test fixtures and goldens

Ten `tests/automation/test_ui_*.sh` fixtures byte-compare each verb's output against a
committed PNG golden under `tests/savegame/corpus/baselines/ui/`. They run in
`tests/automation/run.sh` alongside the other harness tests. The goldens were rendered
under the dummy video driver, so the comparison must be too; `ctl_headless` in `lib.sh`
passes `--headless`, which handles that (you no longer need `SDL_VIDEODRIVER=dummy` in the
environment).

A golden is only reproducible if the fixture owns every input the surface reads, and the
boot main menu reads one that is easy to miss: `BuildGameMainMenu` assembles its rows from
the save directory rather than from `--load`, so `save/current.lba` adds the Resume row and
moves the menu's vertical centre from 275 to 335, and any named save adds the Load row.
`ui_menu_main` pinned `current.lba` and inherited the named saves from whoever ran it, which
made it pass for developers with saves on disk and fail for everyone else. Both menu
fixtures now seed a save directory of their own (`seed_menu_save_dir` in `lib.sh`):
`ui_menu_main` captures the returning-player menu, `ui_menu_main_fresh` the three-row one a
new install shows, which is the only `ui_*` golden taken with no save loaded at all.

A second tier of `test_ui_*_wide.sh` fixtures (inventory, menu-options) renders the
same verb at a wider resolution and asserts that the centred 640×480 crop of the
capture is byte-identical to the existing 640 golden — no per-resolution goldens,
the 640 golden stays the single source of truth. See `ui_compare_wide` in `lib.sh`
and `WIDESCREEN.md` for the surface-by-surface picture (only cleanroom `--black-bg`
surfaces with width-independent UI layout qualify; the dialog strip, for example,
intentionally scales with framebuffer width and doesn't).

```bash
LBA2_GAME_DIR=/path/to/data tests/automation/run.sh
```

To regenerate a golden after an intentional rendering change:

```bash
LBA2_UI_REGEN=1 bash tests/automation/test_ui_inventory.sh
```

All fixtures use `Anon1.LBA` as their save (early-game Citadel Island state, items in
inventory, on an island whose dialogue bank has known text-ids).

#### Environmental hygiene

A fixture reads more than `--load` gives it. The engine takes its settings from the
player's `lba2.cfg` with the install's stacked underneath, so every key a surface
displays, and every key a camera or the sim consults, is an input the golden depends on
without declaring. On someone else's machine those keys hold something else and the
capture diverges, which is indistinguishable from a rendering regression by the time you
see it as a hash.

**`lib.sh` gives each test a `LBA2_USER_DIR` of its own** unless the test named one
first, so the profile a fixture reads is one it just created rather than the one the
developer plays with. That is what makes the goldens mean anything off this machine: they
were captured against a fresh profile, so a fresh profile is what reproduces them. It is
done in `lib.sh` rather than per test because the rule only holds if it cannot be
forgotten: a new fixture is isolated by existing.

Measured after the language and resolution pins were in place, four keys still moved a
capture, and each is the developer's own preference rather than anything a test set:

| key | what it moved |
|-----|---------------|
| `DisplayFullScreen` | the Display submenu's `Fullscreen` / `Windowed` row |
| `VSync` | the Display submenu's `Vsync ON` / `Vsync OFF` row |
| `FollowCamera` | the projection corpus, every save |
| `DetailLevel` | the projection corpus, every save |

Chasing that list with one flag per key does not converge, since it grows with every
setting the game gains. A folder of its own covers the keys nobody has thought of yet.

`ctl_headless` still pins `--language English`, `--resolution 640x480` and `--vsync on`
on top. Isolation makes the rest of the profile irrelevant; the pins state the values the
goldens actually assume, so a capture that needs a different one says so on the command
line instead of in whoever's settings happened to be there. `--language` in particular
still does real work: a fresh profile inherits the install's layer, and a French install
ships `Language: Français`.

A fresh profile is not an empty one. It is engine defaults plus the install's layered
`lba2.cfg`, which is where `DisplayFullScreen: 1` comes from, since the engine's own
default for that key is `FALSE`. That layer is a separate problem: values in it become the
player's own settings on first exit, which is #495.

### Adding a new UI surface — the family pattern

Six surfaces in, the pattern is templated. Each surface adds ~50-100 lines of additive
code touching one modal source file plus the dispatcher. The four invariants worth
following:

- **Mirror the normal-caller setup, not just the modal call.** When the menu-options
  verb was first added, the captured menu had garbled plasma, a blank first item, and
  dialogue text bleeding through. Root cause: the in-game ESC handler at
  `PERSO.CPP:846` calls `StopSpeak()` / `InitDial(0)` / `InitPlasmaMenu()` *before*
  `OptionsMenu(FALSE)`, and the harness was skipping all three. Always trace the
  normal call site and replicate everything it does, gated on capture-armed so
  non-harness callers see no behaviour change.

- **Capture from `Log`, not `Screen`, for most modals.** `Log` is the back buffer the
  modal composites into; `BoxUpdate` only flushes dirty regions to `Screen`, so
  capturing `Screen` on the harness path (no prior frame populating it) leaves
  un-flushed regions black. Five of six surfaces need `Log`. Inventory is the lone
  exception — its render path happens to leave `Screen` valid.

- **Pre-arm `PtrMap`/`ObjPtrMap`** for any modal that renders 3D bodies via
  `BodyDisplay`. Without it the texture filler dereferences a null/stale pointer and
  segfaults. The original code re-sets these inside the modal, but typically *after*
  the first `BodyDisplay` call. Hoist the init for the harness path.

- **Exit via the modal's own exit sentinel**, not by simulating input. Each modal
  has its own "I'm done" signal — `flag=2` for `DoFoundObj` / `MenuInventory`,
  `FlagHoloEnd=TRUE` for `HoloGlobe`, returning `1000` from `DoGameMenu`. Set the
  sentinel inside the capture hook then `break` out; the surrounding wrapper exits
  cleanly.

The pieces of a new surface are: a public `Modal_RequestCapture(path)` setter that
arms a static path + iteration countdown; a capture hook in the modal's loop that
SavePNGs and triggers the exit sentinel; a `cmd_ui` dispatcher branch; one
`test_ui_modal.sh` + one committed golden PNG under
`tests/savegame/corpus/baselines/ui/`.

## Session recording

`--record <path>` captures a session and `--replay <path>` plays it back, checking a per-tick
digest of simulation state and naming the first tick that stops matching. Where `--exec` and
`--input` drive the engine from a script, this drives it from what a person actually did, and it
records the console commands alongside the input so a recording can stand in for a fixture.

| Flag | What it does |
|---|---|
| `--record <name>` | Record from the first input poll, menus included. One file, holding the savegames at each end of the session. A name with no directory in it lands in `<userDir>/recordings/` |
| `--replay <name>` | Replay a recording and report where it stops matching. Resolved the same way |

Both need `--fixed-dt`: on the host-sampled clock a replay is not exact, which was measured rather
than assumed. Give the replay more ticks than the recording holds, or it ends on `--tick` before
the stream runs out and the summary never prints, which reads exactly like a pass.

The `rec` console verb does the same mid-session, and unlike the flags it snapshots and reloads
first so the recording starts where a replay can arrive the same way. Full reference, including the
limits worth knowing before trusting a result, in [RECORDING.md](RECORDING.md).

## Projection capture

Two flags record what the projection pipeline produced during a harness run, so the
output can be byte-compared across builds. This is the regression net for Phase 1 of
the [widescreen plan](WIDESCREEN.md) — the safety check Phase 2's projection-origin
changes must not disturb at 640×480 before any wider frame ships.

| Flag | Output | When to use |
|---|---|---|
| `--capture-projection <path>` | Full text — one event per line, hundreds of MB for a `--demo` replay | Local diagnosis when a hash diverges |
| `--projection-hash <path>` | A single FNV-1a 64-bit digest line over the same content | CI baselines, the committed fixture |

The two flags are composable; both can be active and they agree exactly on what was
captured (the hash is computed over the same line text the full sink would write).
Both are no-ops on default builds — one branch per projection call site when the
flags aren't set.

### Event opcodes

Each captured line is one event. Sequence numbers are 1-based and global across the
run, so ordering can be diffed without wall-clock state.

| Opcode | Fired by | Format |
|---|---|---|
| `SETPROJ` | `SetProjection` (exterior 3D perspective setup) | `seq= xc= yc= clip= fx= fy=` |
| `SETISO` | `SetIsoProjection` (interior orthographic setup) | `seq= xc= yc=` |
| `PROJ` | `LongProjectPoint3D` (per-vertex perspective projection) | `seq= x= y= z= -> ret= xp= yp=` |
| `PROJISO` | `LongProjectPointIso` (per-vertex orthographic projection) | `seq= x= y= z= -> xp= yp=` |
| `PRLI` | `ProjectList3DF` (batched perspective) | `seq= nbpt= orgx= orgy= orgz=` |
| `PRLIISO` | `ProjectListIso` (batched orthographic) | `seq= nbpt= orgx= orgy= orgz=` |
| `M2S` | `Map2Screen` (isometric brick world-to-screen) | `seq= x= y= z= -> xs= ys=` |

A 5-tick exterior replay produces ~10K events (~660 KB full text, 60 bytes as a hash).
An interior replay is smaller (~2.5K events, dominated by `M2S` calls). A full
`--demo` replay produces hundreds of MB of full text — use `--projection-hash` for
that volume.

### Examples

```bash
# Full text capture for local diagnosis.
lba2cc --load "002 Downtown.LBA" --fixed-dt 16 --tick 5 \
       --capture-projection /tmp/out.projrec --exit

# Hash only — CI baseline shape.
lba2cc --load "002 Downtown.LBA" --fixed-dt 16 --tick 5 \
       --projection-hash /tmp/out.hash --exit

# Both — full text plus its hash, with matching content.
lba2cc --load "002 Downtown.LBA" --fixed-dt 16 --tick 5 \
       --capture-projection /tmp/full.projrec --projection-hash /tmp/full.hash --exit
```

### Test fixtures and baselines

Two committed fixtures under [`tests/projection/baselines/`](../tests/projection/baselines/),
driven by sibling tests under [`tests/automation/`](../tests/automation/). Coverage
matrix per save lives in [`tests/projection/README.md`](../tests/projection/README.md).

| Test | Fixture | Scope | Cost |
|---|---|---|---|
| [`test_projection_corpus.sh`](../tests/automation/test_projection_corpus.sh) | `corpus_640x480.projrec.hash` | One line per save (50 entries) across the committed save corpus | ~7 s |
| [`test_projection_demo.sh`](../tests/automation/test_projection_demo.sh) | `demo_640x480.projrec.hash` | 30000-tick attract-mode snapshot; cross-scene transitions saves can't reach | ~35 s, opt-in via `PROJREC_RUN_DEMO=1` |

```bash
# Validate the corpus (covers the projection-pipeline change sites for all 50 saves).
LBA2_GAME_DIR=/path/to/data bash tests/automation/test_projection_corpus.sh

# Opt in to the demo snapshot too.
LBA2_GAME_DIR=/path/to/data PROJREC_RUN_DEMO=1 \
    bash tests/automation/test_projection_demo.sh

# Regenerate everything after an intentional engine change.
LBA2_BIN=build/SOURCES/lba2cc LBA2_GAME_DIR=/path/to/data \
    bash scripts/dev/regen_projrec_baselines.sh

# Or regenerate per-test in place.
LBA2_GAME_DIR=/path/to/data LBA2_PROJECTION_REGEN=1 \
    bash tests/automation/test_projection_corpus.sh
```

### Diagnosing a hash divergence

The committed baseline is just a hash. When it differs from the build's output, the
text isn't there to diff — you have to regenerate it:

```bash
# Current build:
lba2cc --load "002 Downtown.LBA" --fixed-dt 16 --tick 5 \
       --capture-projection /tmp/current.projrec --exit

# Switch to the prior build (last known good), then repeat:
lba2cc --load "002 Downtown.LBA" --fixed-dt 16 --tick 5 \
       --capture-projection /tmp/prior.projrec --exit

# First event that differs:
diff /tmp/prior.projrec /tmp/current.projrec | head
```

The sequence number on the first divergent event tells you which projection call
changed — `SETPROJ` for the setup, `PROJ`/`PROJISO` for individual vertices, `M2S`
for the isometric brick origin. Cross-reference with `tests/projection/README.md`
for the workflow notes.

If the divergence is non-deterministic (same build produces different hashes), the
problem is upstream of projection — `--fixed-dt`, RNG seed, or new global state
leaking timing information.

## Tests

`tests/automation/` drives the real binary and asserts on the dumped state. Local-only —
they need retail data and a display, and skip cleanly otherwise (not part of host-only
`make test`). They double as usage documentation.

```bash
LBA2_GAME_DIR=/path/to/data tests/automation/run.sh
# LBA2_TEST_SAVE=/path/to/save.lba  to pin the save used by --load tests
```

The keystone is `test_tick.sh`: it loads, advances 1 vs 60 ticks, and asserts the game
clock and the hero's idle-animation frame both advanced — proving the loop steps the
simulation, not just a counter.

### Running them on Windows

The suite runs under MSYS2 (UCRT64) against a native build. One rule accounts for most
of what goes wrong there: **MSYS2 rewrites a standalone path argument on its way to a
native binary, but not a path inside a longer string.** So `--dump-state "$out"` arrives
as `D:/...` and works, while `--exec "ui menu-main $out"` arrives verbatim and the engine
cannot create the file. The same applies to `python3`, which is also a native binary:
pass paths as `sys.argv` entries, never spliced into the text of `python3 -c`. Use
`engine_path` from `lib.sh` where a path has to travel inside a longer argument; it is
`cygpath -m` on Windows and the identity everywhere else.

Two more shapes, both handled in `lib.sh` rather than per test. A path the engine wrote
comes back in the engine's own spelling, ending with the platform separator and with CRLF
line endings, so compare folders with `norm_path` rather than as strings. And a glob
expands in the locale's collating order, which is byte order under a typical Linux locale
and case-insensitive under MSYS2, so `LC_COLLATE=C` is pinned there to keep a corpus walk
in one order on both platforms.

`pip install Pillow` (or `pacman -S mingw-w64-ucrt-x86_64-python-pillow`) for the
`test_ui_*_wide.sh` pair, which skip without it.

The menu goldens carry a per-surface `--exclude` rectangle covering the plasma strip at
the top of the panel. Those rectangles are vestigial. The strip seeds its vertices and
speeds from `Rnd()`, and while `Rnd()` was `rand() % n` that made the strip start
somewhere else on Windows and stay there, because libc's `rand()` is a different
algorithm with a different `RAND_MAX` per platform (#530). `Rnd()` draws from the
engine's own generator (`LIB386/SYSTEM/RANDOM.CPP`), and a `ui menu-main` capture is
byte-identical on the two platforms with the strip included, measured. The rectangles
can come out whenever somebody wants to re-verify the five goldens on both hosts;
leaving them in only costs that band its coverage.

Nothing else about the strip ever differed: it is stepped the same number of times on
both (30 steps over a capture, measured), and `Do_Plasma`'s evolution is pure integer and
pinned bit-for-bit by `tests/plasma_steps/`, which passes with the same digest on both
platforms. Only the starting state was ever unportable.

The rectangle's `y` follows the panel, whose height follows the entry count, so it lives
in each test beside its golden and needs re-measuring if a menu gains or loses a row. A
diff of a failing capture against the golden gives it directly. The excluded band is not
left unwatched: `ui_compare` asserts it still holds between 8 and 64 distinct colours,
which a drawn strip does (16 or 17), a strip that failed to draw does not (1 to 3), and
the uninitialised texture `InitPlasmaMenu` exists to prevent does not either (hundreds).

Two divergences are the platform rather than the harness, and are expected to fail there:

- `test_ui_found_object.sh` differs in the item model on the right of the frame, not the
  strip: a handful of pixels across an identical 88-colour palette, which is the same
  projection rounding as below rather than anything about the surface. Masking it would
  remove what the test is for.
- `test_projection_corpus.sh` differs on 25 of its 50 saves: 19 by hash alone, which is
  the rounding the `long double` + `lrintl` x87 emulation exists to pin, and 6 by event
  count, which is a different path through the replay and not explained by rounding.

`test_res_catalog_sweep.sh` is a third case, and a different kind: the engine segfaults
on Windows at the sub-640 widths, intermittently, so the sweep passes or fails depending
on the run. Ten captures per mode, same build, same verb:

| mode | Windows | Linux |
| --- | --- | --- |
| 320x200 | 3/10 crashed | 0/10 |
| 320x240 | 2/10 crashed | 0/10 |
| 640x480 | 0/10 | 0/10 |

Not the fixture, and not a golden that needs deciding: an intermittent crash in a real
mode the resolution catalogue offers. Reproduce with a bare capture at that size rather
than through the sweep, which only reports which modes failed:

```bash
lba2cc --headless --no-autosave --resolution 320x200 \
       --exec "ui resolution D:/cap.png" --tick 6 --exit
```

`test_cli_flag_contract.sh` is the one that keeps the harness honest about the machine it
runs on. A flag names a mode for one run, so a run told to render at 640x480 or throttle the
sim must leave the player's settings exactly as it found them; the exceptions are the flags
whose job is to act on stored state, declared in `CLI_ARGS.CPP`'s `writes` column and printed
under `--help-all`. The test holds every flag to that column in both directions, and checks
its own case list against `--help-all` so a flag added to the table cannot go untested. It
exists because three flags had already broken the rule while their help text claimed
otherwise, and a leaked setting only ever shows up as the next run behaving differently for
no visible reason.

### Driving the opening (`test_walkthrough_opening.sh`)

The walkthrough e2e: cold boot into Twinsen's house, out through the front door, then a
walk under injected input, all of it from a new game with no save in play. Four properties
of the opening shape it, and each one is a way for a test written in there to look like it
proves something it does not (all four measured, not assumed; see issue #447).

| | |
|---|---|
| The new game is stamped on frame one | The hero's own Life script sets game vars 94 (`FLAG_DINO_VOYAGE`) and 253 (`FLAG_CHAPTER`) during the first simulated tick, before any state a test can dump. No cold-boot test can watch those go 0 to 1, so they serve as a boot assertion and never as a probe. The first flags that do flip during the run are 164 and 165, set by cube 49's script on arrival |
| A tick-0 `teleport` is undone | `--exec` fires on the first tick, ahead of the opening script placing the hero, which then puts him back. The console reports the move and the run behaves as though nothing happened. Use `--exec-at 1` for anything positional |
| The house opening owns the hero | He walks his opening track with no input at all, so a displacement test in the house passes on the script alone, and injected input moves him not at all. The first place input actually walks him is outside, in cube 49 |
| The house talks after ~200 ticks | The opening dialogue opens a modal and the loop stops ticking. A house run either stays short of it or passes `skipmodals on` |

Nothing inside the house advances quest state on its own, which is why the walkthrough's
first quest event is the door: a teleport into any of the cube's scenaric boxes latches the
hero's `ZoneSce` and stops there, and the giver boxes still want the grounded action edge no
headless run has driven (see `zonelist` above). Every number the fixture asserts is identical
on each retail master (Activision and EA, disc rips and the re-releases alike), so a failure
is the engine rather than the install.

## Roadmap

The harness is built so these are additive, not rewrites. Ordered by dependency — each step
makes the next more valuable, and faithful input replay must not be built on a
non-deterministic base.

1. **Baseline gallery** *(done)*. Golden `--dump-state` (+ screenshots) over the
   committed save corpus, living alongside it at `tests/savegame/corpus/baselines/`. A
   regression net for the widescreen / projection work: the world-space dump is the
   guardrail that rendering changes don't perturb the simulation; screenshots are the
   human-reviewed visual. See `docs/plan/AUTOMATION_PLAN.md`.
2. **`--fixed-dt` deterministic mode** *(done)*. Pin the per-tick timer step so the simulation
   is independent of wall-clock — the determinism measurements pointed at variable dt as the
   lever. The only RNG seed (`srand(TimerRefHR)` at `OBJECT.CPP:1171`) is pinned by the
   restored save clock on `--load`; on the fresh-start path `Timer_EnableFixedDt()` also
   resets `TimerRefHR` so the seed is deterministic there too. This upgrades the baseline
   tolerance to exact (same-platform) and is the prerequisite for replay. See
   `docs/plan/FIXED_DT_PLAN.md`.
3. **Canned playthroughs.** Replay a whole session as a regression fixture — the
   gameplay-regression counterpart to the existing draw-call polyrec. There are two paths:
   - *Demo playthrough (effectively achieved).* The game's built-in attract reel is a
     21-scene scripted tour of the game's locations rooted at cube 201 and ending with
     `LM_THE_END` at cube 218 ("The Dark Monk statue (last)") — see
     [SCENES.md](SCENES.md#demo-scenes-193-221). Under `--demo` + `--fixed-dt`, any of the
     reel's entry cubes replays the same scene chain, dialogue sequence, and gameplay state
     — sequential or parallel. This covers the regression-fixture use case without an
     explicit input layer; the scripted reel *is* the canned input.
   - *Explicit input capture/replay (deferred).* For sessions outside the reel — random
     exploration, tooling-style automation — capture input per tick and replay it.
     Attaches at the same top-of-loop seam as `Control_TickHook`; depends on (2).

Done (independent of the above): `--polyrec <path>` triggers the existing polygon draw-call
recording (`tests/SNAPSHOT/`, previously a manual Alt+F9) at a scripted `--load X --tick N`
state, making ASM↔CPP captures reproducible. Requires an `ENABLE_POLY_RECORDING` build.

Done (UI side of the regression net): six `ui <surface>` console verbs (see "UI capture"
above) cover every modal UI surface — inventory wheel, planet globe, dialogue bubble,
options menu, main menu, found-object cinematic — with byte-identical PNG goldens
committed under `tests/savegame/corpus/baselines/ui/`. Pairs with (1)'s world-space
guardrail: that one catches simulation perturbations from rendering changes; this one
catches UI rendering changes directly. Most useful for the widescreen work.
