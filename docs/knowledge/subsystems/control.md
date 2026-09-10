---
type: Subsystem
title: Control harness
description: The CLI harness boots the engine, restores a save, runs console commands at chosen ticks, advances the simulation, dumps state and exits in one invocation, reusing seams the engine already has rather than adding a scripting runtime; a loopback socket is the one exception, for probe loops.
status: draft
subsystem: control
as_of: 9f3750f5
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T09:00:00Z }
relates_to:
  - /subsystems/console.md
  - /subsystems/recording.md
  - /subsystems/timing.md
  - /subsystems/input.md
sources:
  - id: control-doc
    resource: ../../CONTROL.md
    title: CLI control harness (docs/CONTROL.md)
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the harness
  - id: perso-cpp
    resource: ../../../SOURCES/PERSO.CPP
    title: PERSO.CPP, the boot path and the tick hook's call site
  - id: android-doc
    resource: ../../ANDROID.md
    title: Android (docs/ANDROID.md)
  - id: record-cpp
    resource: ../../../SOURCES/RECORD.CPP
    title: RECORD.CPP, the simulated audio clock and the replay-alone refusal
---

The principles are structural. The contracts and the seams are read at `as_of`.

# Principles

- **Outside-in.** The harness reuses seams the engine already has: the console command bus for `--exec`, the normal save-load sequence for `--load`, the existing PNG writer for `--screenshot`, and the main loop's own iteration for `--tick`. It adds no scripting runtime and changes no game logic.[^control-doc]
- **The whole plan is fixed before boot.** One invocation either does what it was asked or does not, which is what automation and regression need and what makes a run replayable. A probe loop whose next step depends on the last result is the one shape this cannot do; the socket exists for it and gives up reproducibility on purpose. See [the socket decision](/decisions/the-socket-is-a-transport.md).
- **Headless is the only supported mode for automation.** A window pauses the game on focus loss by design, and a live audio thread branches the simulation, so a windowed run's clock depends on what the rest of the desktop is doing. Six identical pinned runs in parallel produced five sim states windowed and one headless.[^control-doc] The one carve-out is a recorder-armed run: recording and replay both switch voice lifetime onto the game clock, answered from the sample's own length, so an audio-on session recorded with a window replays clean. The batch warning in the parser predates that switch.[^record-cpp]
- **Determinism is a property of the run's configuration, not of the engine.** `--headless` and `--fixed-dt` pin the clock, the null sound backend removes the audio thread, and a fresh process per run keeps global state from leaking between loads. Drop any one and a run that looks deterministic is not.[^control-doc]
- **Two front-ends to one engine.** The CLI owns launch time: discovery, video and audio, the batch clock, captures, self-tests. The console owns runtime verbs. `--exec` and `--exec-at` are the one-directional bridge, and the CLI points at the console's own help rather than documenting its verbs twice. See [one flag table](/decisions/one-flag-table-two-tier-help.md).

# Contracts

| Contract | Statement |
|---|---|
| The flag table | Every flag the engine accepts is a row in one table. An unknown flag, a missing value, or a flag standing where a value belongs is rejected before SDL starts. A flag that asks the engine to do something takes the automated boot path and skips the logos; a flag that only describes the environment does not, so `--headless` alone is still the game. |
| What arms the harness | `--load`, `--exec`, `--exec-at`, `--tick`, `--fixed-dt`, `--fixed-timestep`, `--dump-state`, `--screenshot`, `--polyrec`, `--capture-projection`, `--projection-hash`, `--exit`, `--demo`, `--verbose`, `--record-telemetry` and `--res-switch-test` set the harness active. `--listen`, `--replay`, `--record`, `--headless`, `--no-audio`, `--no-autosave`, `--resolution`, `--language`, `--vsync`, `--log-level` and `--ignore-focus` do not. The two that surprise are the recorder's: a replay or a recording is armed independently of the harness.[^control-cpp] |
| `--replay` alone | Does not arm the harness and does not supply a scene. With harness flags the run is a harness run, and a replay of a player's session then behaves as one; without them the replay refuses, "replays into a game, and none is in progress", because there is no live scene to replay into. The refusal was chosen over booting a game from the snapshot and putting the menu back, which is the demo reel's shape and a larger piece than a guard.[^record-cpp] |
| The load's timer bracket | A harness `--load` returns with the engine's timer bracket one level open, and the harness closes it itself at the first tick the step is armed. A run that never arms a step keeps it open, by design. See [the bracket decision](/decisions/harness-load-bracket-closes-at-arming.md). |
| Order | `--load` or a fresh start; `--exec` on the first tick; each `--exec-at` at its tick; advance to the budget; dump and screenshot; exit. A natural end before the budget still writes the artifacts. A bad `--load` fails the run and writes nothing, so a typo cannot hand back plausible output of the wrong scene.[^perso-cpp] |
| A tick | One `MainLoop` iteration, normally one rendered frame. A cube change restarts the loop body to load the new cube, which counts as a tick with no frame, so `--tick N` over-counts by the number of scene changes crossed.[^control-doc] |
| `--exec` races a scene change | A `cube` change applies on the next frame, so a command after it in the same `--exec` runs in the old cube and the pending change then resets the hero, silently. Schedule the dependent command with `--exec-at`. There is one `--exec` buffer; `--exec-at` is repeatable up to sixteen entries.[^control-cpp] |
| `--tick` bounds the work, `--exit` ends the process | Without `--exit` the run sits in the game once the budget is spent, and a timeout then reads as a hang. |
| Determinism | `--fixed-dt` pins the per-tick clock step, and only in a `--headless` run; exact exterior determinism also needs the null sound backend. The step is a choice, 16 ms for goldens, and a different step gives a different but reproducible trajectory. Exactness is per platform and per build.[^control-doc] |
| Headless and audio are one switch | `--headless` puts both SDL drivers on dummy and turns the audio device off unconditionally; there is no flag combination that gives a windowless run with audio. Audio is a simulation input, and the recording header writes `mode.headless` from the flag, so a headless replay of an audio-on recording takes a different branch. A windowless run that keeps audio is `SDL_VIDEODRIVER=dummy` in the environment without the flag.[^control-cpp] |
| Two input layers | `input` ORs a mask into `Input` from `MainLoop`, metered in sim ticks, so a modal spinning in its own poll loop never sees it. `key` writes a scancode into `TabKeys` from the keyboard layer's harness hook on every poll, so modals see it; it is metered in input polls, capped at eight holds, and a modal masks whatever is held when it opens, so a press has to be armed to land inside it.[^control-cpp] |
| Autosave | A harness run autosaves into the live slot as ticks advance unless `--no-autosave` is passed. A loaded save is not read-only. |
| Output channels | `stdout` is data: the dump and the `--exec` result mirror. `stderr` is the log and the `[control]` diagnostics. A line emitted while the console is driving the run reaches both, so merging the two duplicates every line across two differently buffered streams and reads like the clock running backwards. Capture one.[^control-doc] |
| The dump | Schema 1, hand-written, all integer. `timer_ref_hr` is the save's play time and is not monotonic across a load or a scene change; `wall_ms` counts from process start; `clock_src_ms` is what `ManageTime` would read now and is monotonic under a pinned step. The `log` field holds console scrollback only.[^control-doc] |

# Seams

- `Control_TickHook`, one call site, at the top of the `MainLoop` body: the per-tick work, the `--exec-at` schedule, the recorder's tick and exec hooks, the deferred bracket close. It is dark for every modal, cinematic and menu, which is why the recorder starts at the input poll and the socket is polled from the present path.[^perso-cpp]
- `Control_Begin`, entered when any acting flag is present: arms the pinned step before the load, performs `--load`, and on failure ends the run without writing artifacts. The harness then calls `MainLoop` directly, so the boot menu and the intro are never entered; a return for the in-game menu re-enters through the menu's own loop, since a menu is a pause and not the end of a session, and any other return reaches the harness's own reporting.[^perso-cpp]
- The console bus, `Console_Execute`, for `--exec` and `--exec-at`; the same bus the socket and a replay feed.
- The keyboard layer's harness hook, where `key` holds land in `TabKeys`, beside the recorder's weak hooks.
- What the harness cannot reach. The game menus, since `input` injects into `MainLoop` only and no committed driver reaches the main menu or the save-name screen except through `key`. Android, where argv is always empty, the socket is not built, the app has no network permission and stderr is discarded, so only the console with a hardware keyboard and the file sink survive.[^android-doc]

# What it does not tell you

- Whether a run did what you asked. The exit code is not a verdict. A timeout is most often a modal waiting for input that never comes, or a run with `--tick` and no `--exit` sitting in the game; `--verbose` names the modal on stderr before the hang. A stack says where a run stopped, never whether it should have been there.
- Whether a green run is yours. A fixture that does not isolate `LBA2_USER_DIR` reads the developer's own `lba2.cfg`, and four keys move captures: fullscreen, vsync, the auto camera and the detail level. A hash divergence from a local setting is indistinguishable from a rendering regression. Isolation per test is the rule inside a fixture and exactly wrong around the whole suite, since setting the variable once puts every fixture back on one profile.
- Whether `--demo` did anything. It drives only an authored reel scene, cubes 193 to 221; elsewhere the hero idles and a long run reads as evidence of absence. The harness warns.
- Whether the first frame exercised the normal path. The first frame after a load is a full redraw, which hides the partial-frame bugs the harness is good at catching; drive the event a few frames in.

[^control-doc]: [docs/CONTROL.md](../../CONTROL.md), "Usage", "Notes and limits", "--dump-state JSON" and "Determinism".
[^control-cpp]: [SOURCES/CONTROL.CPP](../../../SOURCES/CONTROL.CPP), the argument parser, `Control_KeyHold` and `Control_TickHook`.
[^perso-cpp]: [SOURCES/PERSO.CPP](../../../SOURCES/PERSO.CPP), the `Control_IsActive` branch in `main` and the `Control_TickHook` call in `MainLoop`.
[^android-doc]: [docs/ANDROID.md](../../ANDROID.md), the console and hardware keyboard limitation.
[^record-cpp]: [SOURCES/RECORD.CPP](../../../SOURCES/RECORD.CPP), the `Timer_SetSimAudioClock` calls on the record and play paths, and the `Control_HasLiveScene` guard in `Record_Play`.
