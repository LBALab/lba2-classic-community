# Purpose tasks for the knowledge bundle

The bundle under `docs/knowledge/` exists so that an agent or a contributor about to
change the engine leaves load-bearing behaviour alone. Its lint checks that the bundle is
well formed; nothing checks that it works. These four tasks do. Each is a refactor that
has happened or nearly happened in this repository, so the right answer is known.

The test is manual and takes an afternoon. Run it before trusting a review of the bundle
that only read it, and again whenever a slice lands that changes what an agent would find.

## How to run

Two arms per task, same agent, same prompt, fresh session each time:

- **With the bundle.** A checkout where `docs/knowledge/` exists. Say nothing about it.
- **Without the bundle.** A checkout with `docs/knowledge/` removed from the working copy
  (`git rm -r --cached docs/knowledge` on a scratch branch, or a worktree at a commit
  before it landed). Everything else identical.

Give the prompt verbatim. Do not point at the bundle, the docs, or the source comments.
Record three things per arm: whether the agent opened a file under `docs/knowledge/`,
whether it made the change, and what reason it gave.

Score a pass when the agent declines the change and gives the load-bearing reason, with
or without naming the concept. Score a fail when it makes the change. An agent that
declines without the reason, or asks permission first, is a partial and is recorded as
such. The expected result is that the arm with the bundle passes all four and the arm
without fails at least one. If both arms pass all four, the source comments are doing
the work and the bundle's marginal value on these tasks is nil, which is a finding
about the bundle, not about the agent.

One confound to state in the results: the source comments already carry part of this
knowledge. `PLAYACF.H` declares the leave-black contract, `FadePal` explains its
`BoxBlit`, and `SAVEGAME.CPP` explains the native-first trial. The arm without the bundle
is therefore not knowledge-free, and what the test measures is the bundle's value over
the comments, which is the value that matters.

## The tasks

### 1. Symmetric fade guards

Prompt: "In `SOURCES/AMBIANCE.CPP`, `FadeToBlack` returns early when `FlagBlackPal` is
set but `FadeToPal` has no matching guard. Make the pair symmetric."

Load-bearing concept: `docs/knowledge/quirks/a-fade-to-black-on-a-black-palette-is-a-no-op.md`.

Expected: decline. A guard on the fades to a palette skips the reveal after a black
present and leaves a scene that never lights; removing the guard from the fades to black
ramps an already-black palette for 200 ms at every call site written for the return,
the video player's entry among them. Either direction changes the game.

### 2. Bump the save version for a clean reader

Prompt: "`LoadContexte` in `SOURCES/SAVEGAME.CPP` discovers the per-object stride by
trial. Bump `NUM_VERSION` to 37 so the reader can dispatch on the version byte and the
trial can go."

Load-bearing concept: `docs/knowledge/decisions/the-save-version-stays-36.md`.

Expected: decline. The version stays 36 by decision so the writer emits true retail
bytes under the retail version number; a 37 is a layout change for every existing file,
the reader's trial is the price of that decision, and its order is the one measured not
to crash.

### 3. Remove a pump that looks like a no-op

Prompt: "Several wait loops in the engine call `Timer_FixedDtPump()` at the top of the
loop and never present a frame. The call does nothing visible. Remove it from those
loops."

Load-bearing concepts: `docs/knowledge/quirks/a-clock-wait-mints-its-own-step.md` and
`docs/knowledge/decisions/two-pumps-polled-and-unpolled.md`.

Expected: decline. Under a pinned step the pump is the loop's only clock source; a wait
that ends on the game clock without it never ends, and the harness and every replay run
under a pinned step.

### 4. Do not leave the player on black after a video

Prompt: "`PlayAcf` in `SOURCES/PLAYACF.CPP` leaves the screen black when a video ends.
Restore the screen that was there before the video so the player is not left on black."

Load-bearing concept: `docs/knowledge/quirks/playacf-leaves-the-screen-black.md`.

Expected: decline. Leaving the screen black is the contract every caller is written for;
the reveal belongs to the caller, and restoring the screen lit under the callers that
arm a fade presents the next scene bright for a frame before the fade-in forces it
black. That regression shipped once and was reverted.

## Recording a run

Add a dated section below with the agent and model, the commit of the bundle, and a
four-row table per arm: task, opened the bundle, made the change, reason given. Keep
the prompts above unchanged between runs so results compare.
