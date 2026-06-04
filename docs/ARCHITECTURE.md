# Engine architecture — overview and roadmap

> **Status: living index (v0).** Entry point for the architecture-understanding effort.
> This is groundwork: *understanding first, no engine changes yet.* Truth hierarchy:
> code > this document > external sources.

## North-star: one engine, two games

The LBA1 community source (`lba1-classic`, Relentless, 1994) and this repo (LBA2, 1997 +
community port) share the Adeline lineage — same `LIB386/`+`SOURCES/` layout, same HQR
format, same Life/Track script model. The long-term prospect is to understand the engine
well enough that **LBA1 content could run on the lba2cc engine**. That port is not the
immediate work; the immediate work is the *understanding* that would make it tractable —
and which pays off now by telling us where to draw module boundaries (engine/game
separation, strangler-fig). Architecture is the lever; comprehension is the fulcrum. The
concrete cost of that port is assessed in [LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md).

## The three axes

The engine is mapped along three orthogonal axes. Read them together — the payoff is the
convergence.

| Axis | Doc | Question it answers |
|---|---|---|
| **Structure** | [ARCHITECTURE_GLOBALS.md](ARCHITECTURE_GLOBALS.md) | Which domain owns which of the ~200 globals? Where is the shared-state bus? |
| **Layer** | [ENGINE_GAME_SEAM.md](ENGINE_GAME_SEAM.md) | What is engine vs game vs platform? (validated against the LBA1 source) |
| **Time** | [LIFECYCLES.md](LIFECYCLES.md) | When is state created, mutated, destroyed? (main loop, `ChangeCube`) |

Underneath all three sits the **data contract** the engine reads:
[ENGINE_FILE_FORMATS.md](ENGINE_FILE_FORMATS.md) specs the on-disk formats (HQR, LZ, 3D body,
animation, sprite, samples, XMIDI, the XCF cinematic codec) at engine altitude, each with its
cross-title (LBA1/TC/LBA2) version timeline.

**The convergence is the signal.** The same small set of things surfaces on all three axes:
`ListObjet`, `CubeMode`, `Comportement`, `ChangeCube`, and the Life/Track script VM. They
are simultaneously the shared-bus offenders (structure), the engine/game membrane and
mode switches (layer), and the lifecycle pivots (time). Where the axes agree is where the
architecture actually lives — and where the first boundaries should be drawn.

## The eight domains and their deeper docs

Each domain in the structural map has a layer tag and, often, an existing subsystem doc
that goes deeper. This table is the map-of-maps.

| Domain | Layer | Deeper references |
|---|---|---|
| Gameplay / simulation | Game on engine substrate | [IMPACT_SCRIPTS.md](IMPACT_SCRIPTS.md), [LBA_EDITOR.md](LBA_EDITOR.md), [GLOSSARY.md](GLOSSARY.md) |
| Rendering | Engine | [CAMERA.md](CAMERA.md), [SPRITES.md](SPRITES.md), [GFX_OPTIONS.md](GFX_OPTIONS.md), [POLYREC.md](POLYREC.md) |
| World / cube transform | Engine | [SCENES.md](SCENES.md) |
| Resources / assets | Engine | [GAME_DATA.md](GAME_DATA.md), [ABI.md](ABI.md) |
| Audio | Engine + game scheduling | [AUDIO.md](AUDIO.md) |
| Platform / IO | Platform | [PLATFORM.md](PLATFORM.md), [TIMING.md](TIMING.md), [CONFIG.md](CONFIG.md), [CONTROL.md](CONTROL.md) |
| UI / text | Engine machinery + game content | [MENU.md](MENU.md), [GLOSSARY.md](GLOSSARY.md) |
| Orchestration / lifecycle | Engine framework | [LIFECYCLES.md](LIFECYCLES.md), [SAVEGAME.md](SAVEGAME.md), [TIMING.md](TIMING.md) |

## The engine/game membrane: the script VM

The single most important seam. The engine runs a fixed main loop; the *game* is Life
scripts (behaviour AI, dispatched at main-loop step 6, `DoLife`) and Track scripts
(pathing, step 3, `DoTrack`), plus assets. `GERELIFE`/`GERETRAK`/`FUNC` implement the VM
and are present in all three Adeline trees. LBA1 uses the same VM (confirmed), so this
interface is what lets its content run here. Detailed API spec:
**[ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md)**.

## Gaps — what's mapped, what's left

1. **The engine↔game interface spec** — *mapped:* [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md).
   The Life/Track bytecode VM: opcode tables, the `switch`-on-cursor dispatch (*not* the
   `EXTFUNC`/`PTRFUNC` tables — those turned out to be engine-internal / editor tooling),
   the syscall surface, and script-variable storage. The keystone for LBA1 portability.
2. **The LBA1 ↔ LBA2 porting surface** — *mapped:* [LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md).
   Per-subsystem compatible/shim/different verdicts. Headline: the engine is a strict
   superset, so hosting LBA1 is format transcoders + a script opcode-remap + integrating
   GPLv2 FLA/MIDI decoders (lba-midi-play, twin-e) — not re-architecting.
3. **The call-graph / data-flow axis** — *not yet mapped.* Who mutates the shared-bus
   offenders, and from where. The fourth axis; lives only in the code and the
   codebase-memory graph (weak on exactly this — sparse function→function edges).

## Strangler-fig roadmap (understanding → action, later)

When the maps are solid enough to act on, the order the three axes agree on:

1. **Keystone: the script VM facade** — the engine/game interface. Highest leverage;
   aligns with the Life/Track tooling thread (#181).
2. **First structural target: World / cube transform** — it dominates the shared-bus
   offender list, and `ChangeCube`'s phases give the exact sequence a facade must preserve.
3. **Platform: already underway** — the community layer is the PAL; PR #284–#286 formalise
   the platform seam. Keep pushing host concerns there.

Each step is a facade drawn *over* frozen code, proven behaviour-identical against the
polyrec/projrec harnesses before the next. No rewrite.

## How to use these docs

Start here, then the three axes (structure → layer → time), then the per-domain subsystem
doc when you need depth. The maps are a reasoning overlay; when they disagree with the
code, the code wins.
