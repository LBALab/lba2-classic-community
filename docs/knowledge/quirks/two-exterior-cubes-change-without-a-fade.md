---
type: Quirk
title: Two exterior cubes change without a fade when the horizon is drawn
description: LoadScene and ChangeCube both gate the fade on the horizon flag and the sum of the old and new cube modes, so walking between two exterior cubes at the default detail level fades nothing, clears nothing, stops no sample and presents the new cube over the old one at a lit palette.
status: draft
scope: "FlagDrawHorizon set, which is the default and every detail level above the lowest; both cubes CUBE_EXTERIEUR; not the load path"
equivalence: untested
asm_origin: "SOURCES/DISKFUNC.CPP:LoadScene"
generated: { by: claude-code/claude-fable-5-1, at: 2026-09-10T12:00:00Z }
relates_to:
  - /subsystems/transitions.md
  - /quirks/a-fade-to-black-on-a-black-palette-is-a-no-op.md
  - /subsystems/recording.md
sources:
  - id: diskfunc-cpp
    resource: ../../../SOURCES/DISKFUNC.CPP
    title: DISKFUNC.CPP, the gate around the fade in LoadScene
  - id: object-cpp
    resource: ../../../SOURCES/OBJECT.CPP
    title: OBJECT.CPP, the same gate around FlagFade in ChangeCube
  - id: gamemenu-cpp
    resource: ../../../SOURCES/GAMEMENU.CPP
    title: GAMEMENU.CPP, the detail-level switch that sets FlagDrawHorizon
  - id: original
    resource: https://github.com/LBALab/lba2-classic-community/commit/333929ab
    title: The initial import, SOURCES/DISKFUNC.CPP, the same gate
  - id: transitions-doc
    resource: ../../TRANSITIONS.md
    title: Scene and FMV transitions (docs/TRANSITIONS.md), which does not name the exception
  - id: control-cpp
    resource: ../../../SOURCES/CONTROL.CPP
    title: CONTROL.CPP, the settings sweep above Control_DescribeState
---

# Scope

The horizon flag is set at every detail level but the lowest, which is the only one that turns off the drawing of the surrounding cubes, and it defaults to set. `CUBE_EXTERIEUR` is 1 and `CUBE_INTERIEUR` is 0, so the sum of the two modes is 2 only for an exterior cube following an exterior cube. The load path is gated out separately and never fades.[^gamemenu-cpp]

# Evidence

`untested`, read against the original source. The gate is in the initial import with the same three terms.[^original] No test or fixture in the tree walks an exterior boundary with the fade under observation.

# Behaviour

`LoadScene` fades to black, stops the ambient sample, resets the dirty boxes, clears and presents only when `!FlagLoadGame AND (!FlagDrawHorizon OR CubeMode + LastCubeMode != 2)`. `ChangeCube` arms `FlagFade` under the same condition; when it fails and the palette happens to be black with no fade pending, it arms `FlagPal` for an instant sync instead, and otherwise touches neither flag.[^diskfunc-cpp][^object-cpp] So between two exterior cubes the new cube is loaded behind the old frame, rendered, and presented over it with the palette it already had; the ambient sample runs on; the moving box list carries over.

# Why it is load bearing

- The island walk is seamless by design. The neighbouring cubes are already on screen as the horizon, so a fade at the boundary would be a blackout in a continuous world. A reader who takes "black brackets the load" as universal and adds the fade, or the clear, or a `BoxReset`, to the ungated path changes what the game looks like on every outdoor step.[^transitions-doc]
- The exception cuts the other way for an oracle. A check that every cube change fades, or that `FlagFade` is set after every `ChangeCube`, or that the clock is pumped across every load, false-alarms on an exterior walk; and the recorder's carried `FlagFade` and `FlagBlackPal` differ after an exterior change from after an interior one, which is correct and not a divergence.
- The palette is not synced on this path unless it was black. Two exterior cubes are expected to share a palette and nothing checks that they do; a palette difference between neighbours would show as the new cube rendered under the old palette until something else syncs.

# What it does not tell you

- **That the transition is free.** The load still happens synchronously inside `ChangeCube` and the game clock does not advance through it; only the fade and the clear are skipped. A frame-time measurement across an exterior boundary still sees the load.
- **What the lowest detail level does.** With the horizon off, every cube change fades, including the exterior ones, so the same save walked at detail 0 and at detail 3 produces different fade counts and a different clock under a pinned step. A recording carries the level as `settings.DetailLevel`, compares it on replay and reports a difference by name, and does not install it: the replay runs on the operator's level. An island walk replayed at the wrong level therefore diverges for a reason the file recorded and the engine printed with its boot banner, and the verdict line at the end carries no count of such differences, a predicate the coverage gate left out on purpose for its own calibration. The key is in the header because a sweep measured the mismatch diverging at tick 235 through a different mechanism, the rain: the lowest level turns the rain off, and the rain draws from the one shared random stream. The fade is a second mechanism on the same key.[^control-cpp]

[^diskfunc-cpp]: SOURCES/DISKFUNC.CPP, `LoadScene`, the `if` around `FadeToBlackAndSamples`.
[^object-cpp]: SOURCES/OBJECT.CPP, `ChangeCube`, the `if (!FlagLoadGame)` block after `RestartPerso`.
[^gamemenu-cpp]: SOURCES/GAMEMENU.CPP, the `switch (DetailLevel)` that assigns `FlagDrawHorizon`; SOURCES/GLOBAL.CPP gives it the default.
[^original]: Commit 333929ab, the initial import, SOURCES/DISKFUNC.CPP, the same condition in `LoadScene`.
[^transitions-doc]: docs/TRANSITIONS.md, "The core principle: black brackets the load".
[^control-cpp]: SOURCES/CONTROL.CPP, the comment above `Control_DescribeState`, the `DetailLevel 3 -> 0` row of the first sweep, and the last line of the `settings.` block; SOURCES/RECORD.CPP, `mode_line_ignored`, which carries no `settings.` key, and the `replay ended` line, which carries no mode count.
