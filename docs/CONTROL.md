# CLI control harness

Drive the engine from the command line for automation and regression: boot, restore a
save, run console commands, advance the simulation by N ticks, dump engine state to JSON,
take a screenshot, and exit — in one invocation. The foundation for an agentic dev loop
and for record/replay regression nets.

This is an outside-in harness. It reuses existing engine seams — the
[console command bus](CONSOLE.md) for `--exec`, the normal save-load sequence for
`--load`, the existing `SavePNG` for `--screenshot` — rather than changing game logic. It
is not a headless mode (the engine still opens a window), not an IPC/REPL, and not a
scripting runtime. See `AUTOMATION_RESEARCH.md` and `AUTOMATION_PLAN.md` for the design.

## Build

Always compiled into desktop builds; no CMake flag. Run a normal build:

```bash
make build      # -> build/SOURCES/lba2cc
```

## Usage

```
lba2cc --load <slot>          restore a save before the loop starts
       --exec "<cmd>;<cmd>"   run console commands (';'-separated) on the first tick
       --tick <N>             advance N simulation ticks
       --dump-state <path>    write a JSON snapshot of engine state
       --screenshot <path>    write a PNG of the rendered frame
       --exit                 exit cleanly after the above
```

All flags are optional and composable. With none, the game launches normally. Passing any
of them takes the automated boot path (the distributor/Adeline logos are skipped).

Order of operations: `--load` (or a fresh start) → first tick runs `--exec` →
advance to N ticks → `--dump-state` + `--screenshot` → `--exit`.

### Examples

```bash
export LBA2_GAME_DIR=/path/to/data

# Restore a save, advance 30 ticks, snapshot state, exit.
lba2cc --load "021 Palace" --tick 30 --dump-state state.json --exit

# Jump to a cube from a fresh start and screenshot it.
lba2cc --exec "cube 100" --tick 5 --screenshot shot.png --exit

# Batch several commands.
lba2cc --load mysave --exec "cube 100; give clover 3" --tick 5 --dump-state s.json --exit
```

`--load` resolves its argument as a direct file path first, then as a save name in the
save directory, then with a `.lba` suffix — so both `--load "021 Palace"` and
`--load /full/path/021 Palace.LBA` work.

### Notes and limits

- **A rendered artifact needs a tick.** `--screenshot` forces a minimum of one tick (you
  can't screenshot a frame that was never drawn). A `--dump-state` with `--tick 0` reflects
  the loaded state before any simulation step.
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
- A bad `--load` path exits cleanly with a `save not found` diagnostic.

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
  "inventory": { "magic_level": 3, "magic_point": 60,
                 "gold": 0, "zlitos": 142, "keys": 1, "clover_boxes": 6 },
  "actors": [ { "index": 0, "x": 5926, "y": 256, "z": 4935, "beta": 2337,
                "life": 250, "body": 2, "anim": 66, "move": 1, "flags": 2119 } ],
  "vars": { "count": 256, "nonzero": { "0": 1, "14": 5, "88": 25 } },
  "log": [ "Cube 100 (change on next frame)" ]
}
```

- `tick` — completed ticks (the hook is inside the loop body, so this counts real
  iterations). `timer_ref_hr` — the game clock in ms.
- `scene` — island/cube and live object/zone/track/patch counts.
- `hero` — `ListObjet[0]`: world position, local angles, life, current behaviour and
  weapon, body/anim ids, animation frame, movement and zone state, flag words.
- `inventory` — magic, money (gold/zlitos), keys, clover boxes.
- `actors` — every live object (`0..num_objects-1`); the hero is also `actors[0]`.
- `vars` — `ListVarGame[]` (the saved script-variable array), emitted sparsely as
  `{count, nonzero}` to keep the file small.
- `log` — recent console scrollback lines.

## Determinism

The same `--load X --exec Y --tick N` does **not** produce a byte-identical `state.json`
across runs today. The simulation is wall-clock driven, and the RNG is reseeded from the
clock per cube change. For stable results:

- Build/run with `-DSOUND_BACKEND=null` (or `SDL_AUDIODRIVER=dummy`) to drop the audio
  thread and sample-timed script branches.
- Assert on **discrete** fields (cube, island, life, inventory counts, var indices),
  **not** exact coordinates. For time/animation fields use relational checks ("advanced",
  "changed").

A future deterministic mode (pinning the timer and RNG seed) can build on the existing
timer hooks; it is out of scope here.

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
