---
okf_version: "0.2"
---

# lba2cc knowledge bundle

What is true about this engine and why, one concept per file, for agents and for the people who review them. [SCHEMA.md](SCHEMA.md) is the house profile and the first thing to read before adding a concept. [log.md](log.md) is the change history, newest first.

## Subsystems

* [Knowledge bundle](subsystems/knowledge.md) - Design principles, contracts, and seams for the docs/knowledge OKF bundle.
* [Session recording](subsystems/recording.md) - The recorder captures a played session at the input waist and replays it into the same simulation, with a per-tick digest that names the first tick that stops matching.
* [Engine timing](subsystems/timing.md) - Two millisecond clocks, one function that advances them, two bracket pairs with different intents, and a harness-only virtual clock source laid over the wall clock.
* [Control harness](subsystems/control.md) - The CLI harness boots the engine, restores a save, runs console commands at chosen ticks, advances the simulation, dumps state and exits in one invocation, reusing seams the engine already has rather than adding a scripting runtime; a loopback socket is the one exception, for probe loops.
* [Debug console](subsystems/console.md) - The always-compiled drop-down console is the engine's runtime command bus; its verbs call existing engine functions, its output lines are a parsed contract, and the same bus answers the keyboard, the CLI's --exec, the --listen socket and a replay.

## Decisions

* [The harness closes the load's timer bracket at the first armed tick](decisions/harness-load-bracket-closes-at-arming.md) - A harness --load returns with the timer bracket one level open, so the harness closes it itself, at the first tick the deterministic step is armed rather than at the load, so that both ends of a recording close it in the same clock regime.
* [Under the pinned step a present is a tick, and the four mint policies stay four](decisions/presents-mint-the-pinned-step.md) - The harness clock mints a step of simulation time from four policies through one funnel; the funnel is done, the policies cannot collapse inside the timer because only the loop bodies know where an iteration ends, and until they do the minimum stable set is four.
* [Two pumps, for a wait that polls and one that does not](decisions/two-pumps-polled-and-unpolled.md) - Timer_FixedDtPump mints a step and drives the recorder's wait hook, for a wait with no input poll; Timer_FixedDtPumpPolled mints only, for a wait that polls, because a polling loop already gets a fresh reading per iteration and the hook would mint a second step and sleep it out.
* [The socket is a transport, not an arming](decisions/the-socket-is-a-transport.md) - --listen puts a line server in front of the console bus so a driver can steer a running engine, and does nothing else; it does not arm the harness, does not pin the clock, and gives up reproducibility on purpose, so CI stays on --exec-at.
* [Console output is a parsed contract](decisions/console-output-is-a-parsed-contract.md) - Every console command's output lines are matched by harness scripts, socket clients and probe sweeps, so new information is appended after the value and never spliced between an identifier and its value; the formats live in pure formatters pinned by host tests.
* [One flag table drives validation and two-tier help](decisions/one-flag-table-two-tier-help.md) - Every flag the engine accepts is a row in one table with its arity, case rule, player-facing bit and section; validation walks the table before SDL starts and rejects what is not in it, --help prints the player subset and --help-all the grouped whole, and neither names a repository path.
* [Every digest field declares why a replay can establish it](decisions/digest-membership.md) - Four membership classes, passed as an argument to the mixing macro, so a field cannot be hashed without saying whether the load restores it, the file carries it, nothing establishes it, or it is not state at all.
* [A recording is one file](decisions/one-recording-file.md) - Both savegames travel inside the .rec as framed chunks, so a recording cannot be parted from the state it started from, and a torn write is refused rather than loaded.
* [The RNG reproduces glibc, in tree](decisions/rng-reproduces-glibc.md) - Rnd draws from an in-tree reimplementation of glibc's TYPE_3 generator, so one recording replays on every platform and every Linux baseline stays valid, and the single stream is not split for the recorder's sake.

## Formats

* [Control socket line protocol](formats/control-socket-protocol.md) - One command per line in; that command's console output back verbatim and a line reading <<END>>; pushed log records prefixed "! "; loopback only, one client, alive only on a build that enabled it and a run that asked for it.
* [Session recording file (.rec)](formats/rec.md) - One self-framing file per session, format 14 with a read floor of 9, a text header, both savegames as framed chunks, and a binary record stream indexed by input poll.

## Quirks

* [ChangeCube seeds the RNG from the boot clock on a load](quirks/changecube-seeds-from-the-boot-clock.md) - On the loose-clock --load path the engine's only reseed reads TimerRefHR before LoadGame installs the save's clock, so the seed is how many milliseconds the process took to boot, and the recorder has to be handed it inside ChangeCube.
* [MulMatrixF zeroes the destination translation](quirks/mulmatrixf-zeroes-the-translation.md) - The 3x3 float matrix multiply writes zero into the destination's TX, TY and TZ, which a port written from the arithmetic alone leaves untouched.
* [SaveTimer counts up at any depth](quirks/savetimer-counts-up-at-any-depth.md) - SaveTimer increments the bracket depth on every call while RestoreTimer no-ops at zero, so an unmatched save raises the floor for the life of the process and every later restore goes inert, with nothing reported in either direction.
* [RestoreTimer restores one of a coupled pair](quirks/restoretimer-restores-one-of-a-pair.md) - The snapshot SaveTimer takes is TimerRefHR alone and RestoreTimer puts that one variable back; LastTime is whatever the ManageTime call inside the restore left it, so the rewind discards the interval only when the timer is unlocked at that moment.
* [Two modals take their clock from their own present](quirks/two-modals-take-their-clock-from-their-own-present.md) - OpenInventory's box-opening loop and the end-credits scroll each end on the game clock, and their own BoxUpdate is the only thing in the body that moves it; under a pinned step, presents that do not mint leave both unable to end, while nine other modals exit on a keypress and never notice.
* [A wait on the game clock mints its own step](quirks/a-clock-wait-mints-its-own-step.md) - Eight wait loops that end on the game clock without presenting carry a one-line pump call that looks like a no-op; under a pinned step it is the loop's only clock source, and removing it leaves the loop unable to end.
* [The opening scene opens a dialogue about four seconds in](quirks/the-opening-scene-opens-a-dialogue-at-four-seconds.md) - Object 4's Life script in the first cube runs a MESSAGE opcode about four seconds of game time after a fresh start; the dialogue spins in its own poll loop, retires no ticks, and waits for any held input to be released before it will take a press, so a headless run with no --load wedges there and an escape armed early does not help.
* [Sample fades end on the wall clock](quirks/sample-fades-end-on-the-wall-clock.md) - HQ_PauseSamples and HQ_ResumeSamples spin with empty bodies until the fade reports done, and the fade reads SDL_GetTicks directly; that wall-clock read is the loop's only termination, so moving it onto the virtual clock source without a pump leaves the loop unable to end under a pinned step.

## Porting status

Generated by `scripts/dev/knowledge_porting.py`. Regenerate rather than edit.

* [Porting status, LIB386/3D](porting/3d.md) - Per-routine equivalence status of the ASM and C++ pairs under LIB386/3D, projected from ASM_VALIDATION_PROGRESS.md.

## References

* [ASM equivalence suite](references/asm-equivalence-suite.md) - The tests that run each ported routine in its ASM and C++ forms on the same inputs and compare results and side effects byte for byte; the attester behind every equivalence of tested.
* [Harness and console host tests](references/harness-tests.md) - The host tests behind the control harness and the console, covering the socket protocol's three buffers, the CLI flag table's accept and reject shapes and both help tiers, and the console's commands, completion, input, render and output formatters.
* [Recording tests](references/record-tests.md) - The host test for the recording format's readers and frame, and the automation fixtures that record and replay through a real engine; the attesters behind the .rec Format.
