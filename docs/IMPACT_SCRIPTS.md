# IMPACT scripts and FLOW particles — Adeline's effects-scripting system

IMPACT is the engine's small bytecode language for **visual/audio effects at a point in space**:
explosions, hits, thrown debris, particle bursts, positional sounds. FLOW is the companion
**particle-emitter definition** table that IMPACT (and other code) references by index. Together
they are the effects layer — *not* gameplay or input, and distinct from the per-object Life
(`LM_*`) and Track (`TM_*`) scripts. Reversed during the input-replay research
(`docs/INPUT_REPLAY_RESEARCH.md`); this documents both formats and the tooling.

## Runtime

`DoImpact(num, x, y, z, owner)` (`SOURCES/IMPACT.CPP`) executes impact `num` at world position
`(x,y,z)`, walking a compiled command stream and spawning the effects. The compiled blob is
loaded once from `ress.hqr` resource **`RESS_IMPACT` (47)** into `BufferImpact`
(`PERSO.CPP:2592`). The `THROW`/`FLOW` commands feed the extra/particle systems
(`ThrowExtra*`, `CreateExtraParticleFlow`, `InitExtraPof`); `SAMPLE` plays a positional sound;
`SPRITE` spawns an animated sprite. The editor-only `CompileImpact()` turns human-readable
`.IMP` text into this bytecode (`#ifdef LBA_EDITOR`); the shipped blob was produced by Adeline's
`COMPILATOR` tool, which is why object references are S16 body indices, not names.

## On-disk format (the compiled blob)

A header offset table followed by per-impact command streams — the same container shape as HQR:

- **Offset table:** `count = firstU32 / 4` little-endian U32 slots; slot `i` is the byte offset
  (within the blob) of impact `i`'s stream. Consecutive offsets bound each stream; a slot equal
  to the next (or to the blob end) is an empty impact.
- **Command stream:** one **command byte** (`IMP_*`, `SOURCES/IMPACT.H`) then its operands, until
  `IMP_END` (0). `IMP_REM`/`;` are compile-time comments — they do not appear in the blob.

Operand layouts (`h` = s16, `i` = s32, `B` = u8), taken from `DoImpact`'s runtime reads:

| Cmd | Code | Operands |
|-----|------|----------|
| `END` | 0 | — |
| `SAMPLE` | 2 | `sample:h freq:h decalage:h vol:B` |
| `FLOW` | 3 | `flow:B` |
| `THROW` | 4 | `sprite:h alpha:h beta:h speed:h poids:h force:B` |
| `THROW_POF` | 5 | `pof:h alpha:h beta:h speed:h poids:h scale:i` |
| `THROW_OBJ` | 6 | `body:h alpha:h beta:h speed:h poids:h rot:h force:B` |
| `SPRITE` | 7 | `deb:h fin:h tempo:i scale:i transp:B force:B` |
| `FLOW_SPRITE` | 8 | `flow:B deb:h fin:h tempo:i scale:i transp:B force:B` |
| `FLOW_OBJ` | 9 | `flow:B body:h force:B` |
| `FLOW_POF` | 10 | `flow:B pof:h scale_deb:i scale_fin:i tempo:i speedrot:h` |

(`alpha`/`beta` are Adeline angles 0–4095; `beta` is added to the owner's facing at runtime.)

## The shipped impacts (retail data)

The retail `ress.hqr` blob is 2160 bytes, **42 impacts** (slot 42 empty). Command frequency:
`THROW_OBJ` 56, `SAMPLE` 34, `FLOW` 25, `FLOW_SPRITE` 20, `SPRITE` 13, `FLOW_OBJ`/`FLOW_POF` 8
each, `THROW` 8, `THROW_POF` 3. They read as hit/explosion effects — e.g. impact 0 plays a
sound, emits a sprite flow and two particle flows, hurls seven copies of body 62 (debris) at
varied angles/speeds, and finishes with an animated sprite.

## Tooling

- `scripts/dev/hqr_inspect.py <hqr> --entry 47 --dump impact.bin` — extract + decompress the
  blob from `ress.hqr`.
- `scripts/dev/impact_disasm.py impact.bin` — disassemble all impacts.
- `scripts/dev/impact_disasm.py impact.bin --check` — round-trip: reassemble and verify
  **byte-identical** to the original. This passes on the retail blob, which proves the codec is
  complete in both directions — i.e. `assemble()` is a working IMPACT compiler, so we can author
  and emit our own impact bytecode that the engine would run.

## FLOW — particle-emitter definitions (`RESS_FLOW` = 45)

The `FLOW`/`FLOW_SPRITE`/`FLOW_OBJ`/`FLOW_POF` commands above reference a particle emitter by
index. Those emitters live in `ress.hqr` resource **`RESS_FLOW` (45)**, loaded raw by `Load_HQR`
into `TabPartFlow` (`FLOW.CPP:112`) — a **flat array of `T_FLOW`**, no offset table, so flow `N`
is simply the Nth record. `CreateParticleFlow`/`CreateExtraParticleFlow` instantiate it.

`T_FLOW` (`SOURCES/FLOW.H`) is a thin fixed-width struct of **36 bytes, no padding**: fifteen
`s16` then two `u8` then one `u32`:

`Alpha Beta OuvertureAlpha OuvertureBeta XMin YMin ZMin XMax YMax ZMax Delay Speed Weight NbDot
Bank` (s16×15), `Coul Range` (u8×2), `Flags` (u32).

`Alpha`/`Beta` are the emission angles (Adeline 0–4095); `OuvertureAlpha`/`OuvertureBeta` the
cone spread in each axis; `NbDot` the particle count (≤ 100); `Speed`/`Weight`/`Delay` the
per-dot dynamics; `Bank`/`Coul`/`Range` select the colour ramp.

Retail data: **96 flows** (3456 bytes / 36). Impacts reference flows 11–95; `flow 64` is the
most-used (9×). Example — impact 0's burst combines flow 65 (full-360 azimuth, 100 dots),
flow 81 (wide cone, 56 dots) and flow 55 (narrow, 4 dots).

- `scripts/dev/hqr_inspect.py <hqr> --entry 45 --dump flow.bin` — extract + decompress.
- `scripts/dev/flow_dump.py flow.bin` — decode all flows; also round-trips byte-identical
  (confirming the 36-byte layout is exact and the codec complete).
