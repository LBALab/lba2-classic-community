# Hosting LBA1 on the lba2cc engine: paths, cost, and feasibility spikes

**Status:** Research + plan. A proposal awaiting go/no-go on the §7 decisions. The §6.2 body
transcoder spike (`scripts/dev/lba1_body_probe.py`) passes: the LBA1 body is decoded and
validated on retail bytes and its geometry transcoded to a valid LBA2 body; the on-engine
render (face-normal synthesis + a load/snapshot harness) is the remaining sub-step. The §6.4
script spike (`scripts/dev/lba1_script_remap.py`) also passes: the quest's set/test/branch
transcodes across the byte-to-S16 flag-width seam, and it corrected the opcode-alignment claim
in `LBA1_PORTING_SURFACE.md` §8. The §6.5 background spike (`scripts/dev/lba1_bkg_repack.py`)
passes too: LBA1's 3 background HQRs re-index into LBA2's merged container by offset arithmetic.
The §6.6 audio spike (`scripts/dev/lba1_voc_probe.py`) confirms LBA1 VOC audio is drop-in through
lba2cc's existing VOC decoder (one-line gate widening needed for 4 `0x01`-prefixed voice entries).
This is the **plan**; the per-subsystem **cost survey** it builds on is
[LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md), and the **north-star** it serves is
[ARCHITECTURE.md](ARCHITECTURE.md) ("one engine, two games").

**Scope:** Decide *how* to bring LBA1 to this groundwork, cost the candidate paths against
each other, and de-risk the recommended path with cheap spikes. Three end-states were
considered (§2). The recommendation is **Path B (host LBA1 content on the lba2cc engine),
built in-repo and game-id-gated, with extraction to a separate repo deferred**.

**Evidence tags:** `[verified-from-code]` (read the exact lines, in this repo, the
`lba1-classic` tree, or the `twin-e` tree), `[verified-from-docs]` (checked against another
doc here), `[verified-from-data]` (checked against local LBA1/LBA2 retail HQRs), `[assumed]`
(inference / estimate), `[design]` (a design choice, not a code finding). `path:line`
citations are against the trees at audit time.

---

## 0. Decision-ready summary

- **The cheap path is also the documented one.** The lba2cc engine is a strict superset of
  LBA1's (`LBA1_PORTING_SURFACE.md`): same HQR, same Life/Track VM, same brick-iso world,
  same anim math. Hosting LBA1 is **format transcoders (body, scene) + a script opcode-remap
  + GPLv2 FLA/MIDI integration**, not re-architecting. Path B reuses the *actual* engine
  (SDL3, rasteriser, tests, CI, multiplatform); Paths A and C re-pay or re-package that work.
- **LBA1's quest is data, not C.** `[verified-from-code]` Progression lives in Life-script
  bytecode + three flag arrays (`ListFlagGame[255]`, `ListFlagCube[80]`,
  `ListFlagInventory[28]` in `lba1-classic/SOURCES/GLOBAL.C`), mutated by VM opcodes
  (`LM_SET_FLAG_GAME`, `LM_INC_CHAPTER`, `LM_CHANGE_CUBE`, `LM_FOUND_OBJECT`,
  `LM_SET_HOLO_POS`) and branched on by `LF_FLAG_GAME`/`LF_CHAPTER`/`LF_USE_INVENTORY`. The
  whole quest rides inside `SCENE.HQR` once the VM runs LBA1 bytecode. **Only two hardcoded
  story "rustines"** exist: the intro (`PERSO.C:213-248`, `NewCube==0 && Chapitre==0`) and
  one redirect (`OBJECT.C:369-372`, `NewCube==4 → 118 if ListFlagGame[30]`). This is what
  makes Path B small, and it puts the opcode-remap (and flag *width*) at the centre.
- **The PAL is independent.** `[verified-from-docs]` B/C reuse SDL3 as it ships; the headless
  determinism harness they test against (`Control_*` + fixed-dt) **already exists today**, pre-
  PAL ([PLATFORM_PAL_PLAN.md](PLATFORM_PAL_PLAN.md) §0). PAL only matters for a *non-SDL* LBA1
  host, the same §5.1 story as for LBA2. Do them in either order; neither blocks the other.
- **The menu is the same engine in both trees, so "agnostic menu" is a facade, not a
  rewrite.** `[verified-from-code]` Both define menus as a `U16` array
  `[selected, nb_entries, y_center, dia_num, (type, text_id)*]` driven by a generic
  `DoGameMenu()` returning the selected text-id; both use the template→build→drive flow and a
  plasma background. The divergence is *data* (entry text-ids, inventory grid size, behaviour
  set). See §5.
- **The LBA1 source of truth is `../lba1-classic`; `twin-e` is only a helper.**
  `[verified-from-code]` The canonical LBA1 behaviour and on-disk formats are the original
  Adeline source (`lba1-classic`: opcode numbering in `SOURCES/COMMON.H`, the body parser
  `LIB386/LIB_3D/P_OBJET.ASM`, etc.). `twin-e` is a pure-LBA1 GPLv2 *reimplementation* used as
  a secondary, more-readable cross-check: named opcode tables (`twin-e/src/script.life.c:1511+`,
  `script.move.c:539+`), all 255 flags named (`twin-e/docs/GameFlags.md`), a standalone
  XMIDI→MIDI converter, and the FLA decoder. The opcode-remap table is `lba1-classic`'s
  numbering diffed against lba2cc's, with twin-e's names as a legend; where the two disagree,
  `lba1-classic` wins. See §6.3.
- **Assets stay native on disk; the `GameProfile` adapts them in-engine.** `[design]` Not offline
  asset conversion but a game-id-selected loader layer that reads each game's native format and
  builds the engine's shared in-memory structures at load (§7 decision 9). Engine init is
  game-neutral and brings up all capabilities (incl. MIDI + FLA); the `GameProfile` only routes
  usage, so games can even be toggled at runtime (§5). The media stack is the user's own GPLv2
  projects vendored behind AIL (§6.7).

**Bottom line:** Proceed on **Path B**, in-repo and gated behind a new game-id, with the
agnostic-menu/shell extraction and any separate-repo packaging (Path C) deferred until LBA1
content actually loads. Start with the §6 spikes; they touch zero LBA2 code paths.

---

## 1. The two trees today `[verified-from-code]` `[verified-from-data]`

| | `lba2-classic-community` (the groundwork) | `lba1-classic` (the target) |
|---|---|---|
| Game / era | LBA2, 1997 + community port | LBA1 (Relentless), 1994 |
| Platform | SDL3; Linux/macOS/Windows/Android | **DOS only** (DOS4GW/DPMI, VESA/MCGA), OpenWatcom 2 + JWasm → `LBAD.EXE`/`LBADC.EXE` |
| Language | C++98, much ASM ported to C++ | C90; **~59% still ASM** (`LIB_SVGA`, `LIB_3D` entirely ASM) |
| Infra | host + ASM-equiv + Docker tests, CI tiers, ~50 docs, debug console, headless `Control_*` harness w/ fixed-dt, ABI/64-bit, perftrace, polyrec | no `tests/`, no `docs/` folder, no SDL, no 64-bit, release-only CI |
| Shared lineage | HQR, Life/Track VM (`GERELIFE`/`GERETRAK`), `P_OBJET`/`P_ANIM`, `GRILLE`, `INCRUST`: same Adeline DNA one generation apart | |

Both games' retail data is available locally for verification (`../LBA/` = LBA1,
`../LBA2-GOG/` + `../LBA2/Common/` = LBA2) `[verified-from-data]`.

**Reference projects.** Per-repo provenance and role (all `github.com/LBALab/*` except `flade`):

| Project | URL | Role |
|---|---|---|
| `lba1-classic` | `github.com/LBALab/lba1-classic` | LBA1 original Adeline source: **source of truth** for LBA1 formats/behaviour |
| `lba2-classic-community` | `github.com/LBALab/lba2-classic-community` | this repo: the host engine |
| `twin-e` | `github.com/LBALab/twin-e` | GPLv2 LBA1 reimplementation: **readable helper/legend** (LBA1-only), never authoritative |
| `lba2remake` | `github.com/LBALab/lba2remake` | TS reimplementation of **both** games: **bilingual reference** (per-game opcode/operand tables); evidence for §8 |
| `lba-midi-play` | `github.com/LBALab/lba-midi-play` | XMIDI→SMF + TSF synth: **vendored** for the AIL MIDI backend (§6.7 Track A) |
| `cueplay` | `github.com/LBALab/cueplay` | BIN/CUE + ISO9660 reader: **reference** for the disc-image resource source (§6.7) |
| `flade` | `github.com/noctonca/flade` | FLA player (+ opcodes, MIDI, ISO9660): **vendored** for FLA video (§6.7 Track B) + disc-image source |

Precedence: `lba1-classic` and `lba2-classic-community` are canonical; the reimplementations
(`twin-e`, `lba2remake`) and media tools are helpers/vendored code, never overriding the original
source where they differ.

**Scope boundary, Time Commando.** Whether this generalises to other Adeline titles: Time Commando
(1996, between LBA1 and LBA2) shares the **engine substrate** (HQR, LZSS/LZMIT, the 3D libs; its
XCF cinematic codec is even in-tree as `DEC_XCF`, decodable by `scripts/dev/acf_decode.py`), so its
*data* is readable today. But TC is a **different game** (pre-rendered XCF backdrop + a live 3D
actor in a `Recouvre` window + a scripted rail camera + STAGE/RUN levels), not the brick-iso +
Life/Track world LBA1 and LBA2 share. Hosting TC would mean building a TC *game layer* on the
shared substrate, not a content port. The realistic north-star is **one engine substrate, two LBA
games**; the `GameProfile` could host a third title, but each non-LBA game needs its own game
layer. TC's value here is as the SDK Rosetta stone (the dual-path format seams were verified across
LBA1/TC/LBA2; see `ENGINE_GAME_SEAM.md` / `TIME_COMMANDO.md`).

---

## 2. The three paths, costed

Effort is order-of-magnitude (one experienced engineer), for *comparison*, not scheduling.

| | **Path A: native LBA1** | **Path B: LBA1 on lba2cc** | **Path C: hybrid repo** |
|---|---|---|---|
| Reuses | lba2cc's *patterns* | lba2cc's *actual engine* | lba2cc's engine, vendored |
| Builds | DOS→SDL3 for LBA1, port ~53 graphics/3D ASM files, ABI/64-bit, then graft on infra | body/scene transcoders + opcode-remap + GPLv2 FLA/MIDI + a game-id content layer | B's engineering, packaged as a forked repo vendoring the engine |
| `lba1-classic` is | the codebase you port | a reference oracle | a reference oracle |
| End artifact | faithful standalone **LBA1 engine** | **lba2cc that also runs LBA1** | a distinct `lba1-classic-community` riding the shared engine |
| Rough total | ~12–24+ person-months | **~3–6 person-months** | B + ~1–2 mo topology; **+6–12 mo** if the clean two-game seam is built |
| Relative to B | ~4–6× | 1× | ~1.3× (light) … ~3×+ (clean seam) |
| Head start banked | low (patterns only) | **high** (body format decoded, HQR/LZSS verified, opcode collisions mapped, twin-e as legend) | high (same as B) |
| Fidelity | authentic LBA1 | LBA1 *on the LBA2 engine* (verify sort-tree vs painter's, SVGA vs MCGA) | same caveat as B |

**Per-subsystem reuse** (✅ reuse ≈0 · 🟢 small · 🟡 medium · 🔴 large):

| Subsystem | A | B | C |
|---|---|---|---|
| Platform (window/video-out/input/timing) | 🔴 new SDL3 layer for LBA1 | ✅ reuse | ✅ |
| Software rasteriser (`LIB_SVGA`+`LIB_3D`, ~53 ASM) | 🔴 port to C++ | ✅ `pol_work` (LBA1 is a subset) | ✅ |
| HQR container | 🟢 ABI cleanup | ✅ drop-in (`[verified-from-data]`) | ✅ |
| 3D body format | ✅ keep LBA1's | 🟡 transcoder (format decoded; §6) | 🟡 |
| Animation | ✅ keep | 🟢 drop-in once groups align | 🟢 |
| Scene/grid/cube | ✅ keep | 🟡 repackaging shim (3 HQR → merged) | 🟡 |
| Sprites/palettes | ✅ keep | 🟢 byte-diff + small shim | 🟢 |
| Script VM (Life/Track) | 🟢 keep + ABI | 🟡 **opcode-remap + flag width** | 🟡 |
| Audio samples | 🔴 rewrite DMA/GUS/SB | ✅ existing VOC path (1-line gate widening; §6.6) | ✅ |
| MIDI music | 🔴 rewrite | 🟡 new AIL MIDI backend (vendor `lba-midi-play`; §6.7) | 🟡 |
| Video (FLA) | 🟡 vendor twin-e `flamovies.c` | 🟡 FLA driver (vendor `flade`; writes `Log`; §6.7) | 🟡 |
| Save/load | 🟡 fix + modernise | 🟡 LBA1 save layout in engine | 🟡 |
| Build / ABI / 64-bit | 🔴 replace OpenWatcom/DOS4GW | ✅ reuse | ✅ |
| Two-games plumbing (game-id, menu/HUD/holomap/save) | ✅ N/A | 🟡 new (§5) | 🟡–🔴 plus seam work |
| Tests / CI / docs / console / headless | 🟡 re-instantiate patterns; **no LBA1 equiv-oracle** | ✅ inherit; add LBA1 cases | ✅ inherit + repo CI |
| Repo & upstream-sync | 🟢 standalone | ✅ in-repo | 🟡 vendor + sync burden |

**Why not A:** it re-pays the entire lba2cc port on an older, more ASM-laden codebase, and has
**no equivalence oracle**. lba2cc's confidence comes from ASM↔C++ tests against the original;
a fresh native port that also de-ASMs the rasteriser has nothing to diff against but "looks
right." Only choose A for a purist standalone LBA1 *engine*.

**Why B over C first:** the engine/game facades that would make C clean are *understood but
not built* (`ENGINE_GAME_SEAM.md`, `ARCHITECTURE.md` defer them). C still has to touch the
same shared-core points B does (body loader, VM operand widths, flag storage), but across a
repo boundary, with the LBA2 regression tests in the *other* repo. So C-first can *raise* the
"break LBA2" risk while adding an engine-sync burden. Build B in-repo, prove content runs,
*then* decide whether to extract to C.

---

## 3. Will Path B break LBA2? `[design]` `[verified-from-docs]`

The fear is legitimate; the design neutralises most of it **in-repo**:

- B's work is overwhelmingly **additive and game-id-gated**: new transcoders, a remap table
  selected by game-id, an LBA1 content layer. The shared engine core stays behaviour-identical
  for LBA2, and lba2cc already has the harness to prove it: **polyrec byte-for-byte, fixed-dt
  determinism, ASM-equivalence**.
- The engine was **already built multi-title-aware.** `ENGINE_GAME_SEAM.md` (SDK-timeline)
  lists dual-path seams it carries today: **LZSS+LZMIT**, **VOC+WAV**, **16-bit vs 32-bit body
  coords**, XCF/Smacker. Adding LBA1 paths follows an existing pattern.
- The narrow shared-code touch points to guard with tests: the **body loader** (16↔32-bit,
  *already* dual-path), **VM operand widths**, **flag-storage width**.

So the regression guard is the test harness, and it is strongest when everything is in one
repo, which is the core argument for B-first over C-first (§2).

---

## 4. PAL dependency `[verified-from-docs]`

None. B/C run on today's SDL3 engine. The headless, deterministic, screenshot-comparable
execution the spikes need (`Control_*` harness + fixed-dt virtual clock + `SNAPSHOT`) **ships
today**, pre-PAL ([PLATFORM_PAL_PLAN.md](PLATFORM_PAL_PLAN.md) §0). The PAL becomes relevant
only for a non-SDL LBA1 host (libretro/GameCube): the §5.1 story shared with LBA2. The two
efforts are synergistic (both isolate the platform/IO/media layer where the LBA1↔LBA2
divergence concentrates) but not coupled; the only interaction is mild merge coordination if
the PAL present-path PR and the LBA1 FLA work land together, and FLA writes the engine's
existing `Log` framebuffer the way Smacker does, so even that misses the PAL seam.

---

## 5. The agnostic menu + engine shell + game-id `[verified-from-code]` `[design]`

The two menus are **the same Adeline menu engine, one generation apart**:

| | LBA1 (`lba1-classic/SOURCES/GAMEMENU.C`) | LBA2 (`SOURCES/GAMEMENU.CPP`) |
|---|---|---|
| Menu data | `U16` array `[sel, nb, ycenter, dia, (type,text_id)*]` | identical format (`MENU.md`, `:82-86`) |
| Driver | `DoGameMenu()` → selected text-id | `DoGameMenu()` → text-id or `1000` (ESC) |
| Build flow | `GameMainMenu[]` filtered | `RealGameMainMenu`→`BuildGameMainMenu`→`GameMainMenu` |
| Main entries | New/Continue/Options/Quit (text 20–23) | Resume/New/Load/Save/Options/Quit (text 70–75) |
| Background | plasma/fire | plasma/fire ("a hallmark of LBA1 and LBA2 menus") |

So "make `GAMEMENU` agnostic" is **formalising a seam already latent in both codebases**: a
facade, not a rewrite (~20–30% of `GAMEMENU.CPP` surface, low risk if menu-nav + screenshot
tests cover it). The design, three layers:

1. **Menu VM (already agnostic, both trees):** `DoGameMenu`/`DrawGameMenu`/slider types/save
   picker/plasma. Keep its behaviour byte-identical.
2. **Port/engine options (shared, belong to neither game):** volume, language, display
   (resolution/vsync/fullscreen), gamepad config, follow-camera, audio balance. These live in
   LBA2's `GAMEMENU.CPP` today but are *port* options, shared across both games.
3. **Per-game content descriptor (the only game-specific part):** main-menu entry table +
   text-id base + save schema, selected by **game-id**.

**Scope the agnostic work to the start/load/save/options *shell***: that is where "they look
the same" is genuinely true and the risk is lowest. The **in-world HUD** diverges more and
stays per-game (riding shared draw primitives): inventory (LBA1 4×4=16 vs LBA2 7×5=35),
behaviour selector (LBA1 CTRL+arrows overlay vs **none in LBA2's menu UI**), holomap (LBA1's
single `HOLOMAP.C` vs LBA2's four `HOLO*` modules).

**The keystone is a game-id, which does not exist yet** `[verified-from-code]`:
`RES_DISCOVERY` only knows `lba2.hqr`; `DistribVersion` is publisher-logo only; `Version` is a
build string. A `GameId`/`GameProfile` is foundational: it is the **one dispatch point** that
selects the per-game **native-format asset loaders/resolvers** (the in-engine adaptation of §7
decision 9: body loader, background grid/block/brick resolver, sprite/palette reader, script VM
mode), plus the opcode-remap table, asset-name map, menu descriptor, and save schema. So
"reading LBA1 assets unmodified" is not a separate mechanism: it *is* the `GameProfile` choosing
LBA1 loaders. The retail files stay native on disk; only the in-memory structures are shared. **The game-select menu is its natural first
consumer and belongs to the boot shell, not either game:** boot → `RES_DISCOVERY` finds data →
if both an LBA1 marker (`lba.hqr`/`ress.hqr`) and `lba2.hqr` are present, show a 2-entry
chooser on the existing `DoGameMenu`; single-game installs auto-select (today's LBA2 UX
preserved); `--game lba1` forces it. This reframes the work as **"an Adeline engine shell that
discovers data, picks a GameProfile, and runs"**: the north-star at the boot/UX layer.

**Engine init is game-neutral; the `GameProfile` is a runtime-swappable routing object.**
`[design]` Capabilities come up once, unconditionally, at engine init: SDL, the full AIL audio
stack (samples, streams, CD, *and* MIDI), the video decoders (Smacker + FLA), the framebuffer,
input. The `GameProfile` does not gate capability *init*; it routes *usage* (which loaders, opcode
table, menu, save schema, and which music/video driver a game drives). So the MIDI synth and FLA
decoder initialise regardless of the active game, and switching games at runtime is a **soft
reset** (tear down game state, reload that game's data, swap the active `GameProfile`), not an
engine re-init. The constraint: keep all game-specific state behind the `GameProfile` and out of
the one-time init path, so the engine hosts either title and can toggle between them at runtime.

**Sequencing:** the feasibility spike (§6) needs *no menu*. Introduce the `GameProfile` scaffold
with the first real LBA1-loading PR; do the shell extraction + game-select UI *after* LBA1
content loads, when there are **two concrete menu content sets** to extract from (principled
refactoring, not premature abstraction, the strangler-fig pattern the docs already prescribe).

---

## 6. Feasibility spikes (de-risk B; touch zero LBA2 code)

All spikes lean on groundwork already landed (the `Control_*` harness + `SNAPSHOT`) and run
against local retail data, so they double as proof of how additive B is. **All four spikes below
have passed** (§6.2 body, §6.4 script, §6.5 background, §6.6 audio); the media stack (§6.7) is
planned, not spiked.

**Offline vs production (decision 9, §7):** the spike tools transcode/repack *offline* in Python
to validate the format mappings cheaply. That is **not** the recommended production approach,
which is **in-engine adaptation**: a game-id-gated loader layer that reads each game's native
on-disk assets and builds the engine's in-memory structures at load, leaving the retail files
untouched (the same way the engine already consumes LBA2 data). So treat these tools as the
validated mapping reference + test oracles, not an asset-conversion pipeline.

**Spike ladder, in order:**

1. **Body transcoder (the dominant unknown).** Transcode one LBA1 body → `T_BODY_HEADER`, load
   on lba2cc, render headless, eyeball. *Transcode + standalone render done (§6.2); on-engine
   render pending.*
2. **Opcode-remap + flag-width (the quest linchpin).** Run one LBA1 `SCENE.HQR` script blob on
   the lba2cc VM through a remap table + an LBA1 flag-storage shim; confirm a
   `set flag → test flag → branch` executes. This is where the byte-vs-`S16` subtlety bites.
3. **Scene repackaging.** Re-index LBA1's three background HQRs (`LBA_GRI/BLL/BRK`) into one
   `T_BKG` + synthesized `T_GRI_HEADER`; render one cube.
4. *(optional)* **VOC sample.** Confirm an LBA1 `SAMPLES.HQR` VOC plays through lba2cc's
   existing VOC path.

Spikes 1–3 reach **"one LBA1 scene renders and one script branch runs on the lba2cc engine"**,
the minimal end-to-end proof that B is real (~2–4 weeks).

### 6.1 Body format, decoded for the spike `[verified-from-code]` `[verified-from-data]`

LBA1 body stream (`lba1-classic/LIB386/LIB_3D/P_OBJET.ASM:211-248`, `AffObjet`): `Infos` (U16)
→ ZV bbox (12 B = 6×`s16`) → `U16` zone-info size + that many bytes → point/normal/group cloud
(consumed by `RotateNuage`/`AnimNuage`) → `U16` nb polys + poly records → lines → spheres.
Per-poly records and the n-gon/material encoding are specified in
[LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md) (§ "Per-poly record").

LBA2 target (`LIB386/H/OBJECT/AFF_OBJ.H:96-122`, `T_BODY_HEADER`, 96 B): `S32 Info`,
`S16 SizeHeader`/`Dummy`, 6×`S32` bbox, then count/offset pairs for
groups/points/normals/normFaces/polys/lines/spheres/textures. Transcode = widen 16→32-bit,
synthesize `SizeHeader`=96 + the offset table, type-group the polys, triangulate n-gons,
convert the pre-multiplied index stride, synthesize face normals; textures empty for LBA1.

The spike tool reads LBA1 and LBA2 bodies with the **same** HQR/LZSS reader (`hqr_inspect.py`),
validates the LBA1 header against the known retail values (`Info=0x0003`, bbox
`-253,260,0,1240,-153,200` for body 1), decodes the full LBA2 `T_BODY_HEADER`, and emits a
first-cut transcoded header, pinpointing the remaining inner-loop (the nuage+poly walk) as the
next work. Run results are recorded in §6.2.

### 6.2 Spike 1 result: body transcoder on retail bytes `[verified-from-data]`

`scripts/dev/lba1_body_probe.py` against local retail data (`../LBA/BODY.HQR`,
`../LBA2-GOG/BODY.HQR`), entry 1 (Twinsen). **SPIKE PASS.** The full LBA1 body is decoded and
its geometry transcoded to a structurally valid LBA2 `T_BODY_HEADER`; the LBA1 decode is
checked by independent invariants, not by eyeballing.

LBA1 body 1: 6858 B LZSS; `Info=0x0003` (**animated**); **180 points, 19 groups, 135 normals,
230 polys (210 tri, 20 quad), 21 lines, 5 spheres**; section offsets
`points@26 groups@1108 normals@1832 polys@2914 lines@6644 spheres@6814 end@6856`.

LBA1 decode validation (all PASS), each an independent invariant against the source-of-truth
layout (`P_OBJET.ASM` / `P_ANIM.ASM`):

- **parse consumed the body** (6856/6858; 2 trailing pad bytes `AffObjet` never reads).
- **header matches the documented retail values** (`Info=0x0003`, bbox per
  `LBA1_PORTING_SURFACE.md`).
- **all point indices in `[0,180)`** after dividing out the `SIZE_LIST_POINT`=6 pre-multiplier.
- **`sum(group[+18]) == nbNormals` (135)** validates the 38-byte group decode against
  `ComputeAnimNormal` (which reads per-group normal counts from group offset 18).
- **a group word sums to `nbPoints` (offset 2)** recovers the per-group point count, so
  point/group **skinning is reconstructed, not guessed**.

(The bbox-equals-points check is N/A here: animated bodies store group-local points, so the
header ZV is the assembled-model box, not the raw point extent.)

Transcode to LBA2: emitted 5768 B; the 230 LBA1 polys triangulate and type-group into LBA2
`_LIGHT` records across 4 type-groups; per-group skinning applied; anim bit translated
(`INFO_ANIM`=2 to `MASK_OBJECT_ANIMATED`=`0x100`). Emitted-body validation (all PASS):
`SizeHeader==96`, section offsets monotonic and in-bounds, every emitted poly index `< nbPoints`.

**What this proves:** the LBA1 body format is decoded end to end and validated on retail bytes
against the original Adeline source, and its geometry transcodes into a well-formed LBA2 body.
**What remains before an on-engine render (flagged stubs, not faked):** synthesize per-poly
face normals + the `NormFaces` section (LBA2 body 1 has `NormFaces=8`); fine material to
LBA2-type mapping (dither/trame/env); the normal `range` field; the group parent index. Then
load the body on the engine and snapshot it headless via the `Control_*` / `SNAPSHOT` harness
(the remaining, separate sub-step of spike 1).

**Standalone visual check (`scripts/dev/lba1_body_render.py`).** A dependency-free software
rasteriser (stdlib `zlib` only, no engine build) renders decoded geometry to PNG. Scanning the
whole archive: **all 132 LBA1 bodies parse cleanly** (`static=0 animated=132 unparsed=0`), so
the decoder is validated across the entire retail set, not just body 1. Every body is animated
(points group-local), so a bind-pose assembly through the bone hierarchy is needed.

The full 38-byte group is decoded from `RotateGroupe` / `RotList`: `+0` start(x6), `+2` nbPoints,
`+4` pivot(x6), `+6` parent(x38, -1=root), `+8` type, `+10/12/14` angles, `+18` nbNormal, `+20` a
3x3 Q14 matrix. The body's per-group angles are all 0 (rest), so the engine's
`RotMat`+`RotList` reduce to translation, and bind-pose assembly is
`assembled[i] = rawPoint[i] + assembled[pivot]` (parents first). With that, **body 1 renders as
an unmistakable Twinsen** (head, torso, arms, legs), and the **assembled bounding box reproduces
the header ZV** (`Y[0,1238]` vs `[0,1240]`), an independent correctness check on top of the
visual one; body 51 reads as a coherent multi-part craft, body 80 as a faceted disc. Per-frame
limb poses come from `ANIM.HQR` (a separate concern, not needed to validate body geometry).

**What remains of spike 1:** the on-engine render (face-normal synthesis + a load/snapshot
harness). The format reverse-engineering (decode, skeleton, transcode) is closed.

### 6.3 `twin-e` as a secondary helper `[verified-from-code]`

The LBA1 source of truth is `../lba1-classic` (the original Adeline source). `twin-e` (GPLv2,
pure-LBA1 *reimplementation*, `../twin-e`) is a more-readable cross-check, not code to copy and
not authoritative where it disagrees with `lba1-classic`:

| Need | twin-e artifact | Use |
|---|---|---|
| LBA1 Life opcodes | `src/script.life.c:1511+` (named dispatch 0x00–0x69) | LBA1 side of the remap table (§ spike 2) |
| LBA1 Move opcodes | `src/script.move.c:539+` | Track-VM remap |
| Conditions / operators | `src/script.life.c:71-112` | operand semantics |
| All 255 game flags named | `docs/GameFlags.md` | quest/flag mapping + save schema |
| LBA1 save layout | `src/gamestate.h` (`gameFlags[256]`, `inventoryFlags[28]`, `holomap_flags[150]`) | save-format shim |
| XMIDI→MIDI | `src/utils/xmidi.c` (standalone, from ScummVM/Exult) | MIDI integration |
| FLA decoder | `src/flamovies.c` + `docs/FLA.md` | FLA integration (also twin-e is the GPLv2 source cited in `LBA1_PORTING_SURFACE.md` §7) |

Critically twin-e models **only** LBA1, and it is a reimplementation: where it and
`lba1-classic` differ, **`lba1-classic` is authoritative**. The remap table is `lba1-classic`'s
opcode numbering (`SOURCES/COMMON.H`) diffed against lba2cc's; twin-e's named tables and flag
list are a human-readable legend over those numbers, and the LBA2 numbers come from this repo.

### 6.4 Spike 2 result: opcode remap + flag-width on real source `[verified-from-code]`

`scripts/dev/lba1_script_remap.py` parses both `COMMON.H` opcode tables (LBA1
`../lba1-classic/SOURCES/COMMON.H` as source of truth; LBA2 this repo) and exercises a
`set flag -> if flag==v -> branch` transcode on a mini Life-VM. **SPIKE PASS**, plus a
correction to the surface doc.

The flag-width linchpin works. From the engines' own `GERELIFE`: LBA1 `LF_FLAG_GAME` reads a
`UBYTE` (`TypeAnswer=RET_BYTE`, 1-byte compare) and `LM_SET_FLAG_GAME` writes 1 byte; LBA2
`LF_VAR_GAME` reads an `S16` (`RET_S16`, 2-byte compare) and `LM_SET_VAR_GAME` writes 2. Because
`LM_IF` jumps are absolute 2-byte offsets, widening operands shifts every jump. The transcoder
remaps opcodes, widens the flag/var operands (byte to S16), and recomputes the jump offsets; the
LBA1 script (20 B) and its transcode (24 B) then branch **identically** on the mini-VM for both
the true and false paths (`v6=42` / `v6=99`).

**FINDING (corrects `LBA1_PORTING_SURFACE.md` §8):** the `LM_` tables do **not** align 0..56.
Verified on real source: of LBA1's 101 Life ops, 68 are identical-slot, 2 are FLAG->VAR renames
(31, 36), and 30 collide, **including inside 0..56**: `LM_LABEL`(10)->`LM_PALETTE`,
`LM_SET_LIFE`(21)->`LM_SET_CAMERA`, `LM_SET_LIFE_OBJ`(22)->`LM_CAMERA_CENTER`. So divergence is
interspersed, not "begins at 57". A by-name remap (name match + the FLAG->VAR rename) covers
71/101 automatically; the rest need a manual alias (e.g. `LM_LABEL`->`LM_COMPORTEMENT`, which
shared LBA1's handler) or a real handler (media `LM_PLAY_FLA`/`LM_PLAY_MIDI`/`LM_PLAY_CD_TRACK`,
the fades, `LM_PROJ_ISO/3D`). The control-flow + flag spine still aligns by number (`END`=0,
`IF`=12, `ELSE`=15, `ENDIF`=16, `BODY`=17, `COMPORTEMENT`=32, `SET_*_GAME`=36), which is why the
quest mechanism transcodes.

**What this means:** the opcode-remap is still a contained shim, but a real per-op table with
operand-width rewriting and a handful of handlers, not the near-identity the surface doc implied.
The quest-is-data thesis holds: progression rides the VM, and the VM's set/test/branch crosses
the flag-width seam cleanly. **Remaining:** a full operand-table transcoder (every opcode's
operand layout) plus the media-op handlers, then a real `SCENE.HQR` Life script run end to end.

### 6.5 Spike 3 result: background repackaging on real data `[verified-from-data]`

`scripts/dev/lba1_bkg_repack.py`. **SPIKE PASS** (confirms verdict #4, scene/grid/cube, on real
data from both games).

LBA2's `LBA_BKG.HQR` (18101 entries) decodes as the documented 4-partition merged container via
the `T_BKG_HEADER` at entry 0: `Gri_Start=1, Grm_Start=149, Bll_Start=179, Brk_Start=197,
Max_Brk=17903` (grids | GRMs | block-libs | bricks, ascending and in-bounds). A sample grid's
`T_GRI_HEADER` reads `My_Bll=0, My_Grm=0, UsedBlock` popcount 82/256, and its block library
resolves at `Bll_Start+My_Bll=179`, inside the BLL partition.

LBA1 keeps the three HQRs separate with matched counts (`LBA_GRI`=134 grids, `LBA_BLL`=134
block-libs, `LBA_BRK`=8715 bricks). The re-index into one container is offset arithmetic:
synthesized `T_BKG_HEADER` `Gri_Start=0, Grm_Start=134, Bll_Start=134, Brk_Start=268,
Max_Brk=8715` (8983 entries total), with a per-grid `T_GRI_HEADER` whose `My_Bll` = the grid
index (LBA1 pairs grid N with style N 1:1, whereas LBA2 shares 18 styles across 148 grids; the
1:1 mapping is still valid since the engine resolves `Bll_Start+My_Bll`). The arithmetic and a
sample grid resolution validate.

**Production note (see §7 decision 9):** this spike repacks *offline* to validate the mapping,
but the preferred end-state is **in-engine adaptation**, not a converted asset. A per-game
resource resolver routes grid/block/brick lookups to LBA1's three HQRs and computes `UsedBlock`
at load, exactly as LBA1's own `LoadUsedBrick` already does (LBA1 computes the bitmap; only LBA2
stores it in `T_GRI_HEADER`). That keeps the retail assets untouched and is closer to LBA1's
real behavior than repacking. Either way the brick/block/column payloads are unchanged.

### 6.6 Spike 4 result: VOC audio is (near) drop-in `[verified-from-data]` `[verified-from-code]`

`scripts/dev/lba1_voc_probe.py`. **SPIKE PASS.** lba2cc's `PlaySample`
(`LIB386/AIL/SDL/SAMPLE.CPP`) already decodes Creative Voice (`ParseVocIfPresent`), a path the
community port added for LBA2's own VOX voices. LBA1's audio is the same format, so it rides the
existing AIL path with no new decoder.

- **SFX** (`SAMPLES.HQR`, 230 entries): 230/230 are VOC, 8-bit PCM, `C`-prefixed, rates ~4 to
  17 kHz (mostly 11111 Hz). Fully drop-in.
- **Voices** (`VOX/EN_000`, 191 entries): 191/191 are VOC, 8-bit PCM, 11111 Hz; 187 are
  `0x00`-prefixed (which `ParseVocIfPresent` accepts) and **4 are `0x01`-prefixed** (entries
  39/47/82/83).

**FINDING:** lba2cc's VOC magic gate (`SAMPLE.CPP:103`, `buffer[0] in {C,W,0x00}`) rejects the 4
`0x01`-prefixed voice entries, even though they are valid VOC (the `reative Voice File` magic
matches at offset 1). The fix is **one line**: accept any first byte (the offset-1 magic is the
real signature) or add `0x01`. With that, LBA1 audio (SFX + voices) is fully drop-in via the
`GameProfile`'s sample loader, and the widening is also a small robustness improvement for the
existing LBA2 voice path. (MIDI music is separate: it is the one new audio capability, planned
in §6.7.)

### 6.7 Media stack plan (verdict #7): MIDI + FLA + CD via AIL `[verified-from-code]` `[design]`

Not a spike: a re-implementation plan, since the user's own GPLv2 projects supply the
implementations (`../lba-midi-play` MIDI; `../flade` FLA + its sample/MIDI cues + an ISO9660
reader; `../cueplay` CD audio from BIN/CUE images). All vendor cleanly into GPLv2 lba2cc (`xmidi.c`
GPLv2, TSF MIT, TML zlib, `fla.c` GPLv2; share one copy of `xmidi`/`tsf`/`tml`).

**The LBA1 sound driver is "the LBA2 stack + MIDI".** The AIL stack already covers nearly all of it:

| Capability | lba2cc today | LBA1 need | Work |
|---|---|---|---|
| SFX / voices | `PlaySample` (WAV / VOC) | VOC | none (§6.6) + the 1-line VOC gate fix |
| CD music | `PlayCD` reads extracted `TrackNN.wav` | track numbers (`PlayCdTrack(8/9)`, jingles) | drop-in if extracted (Steam); add a disc-image source for raw GOG (below) |
| MIDI music | **stub** (`InitMidiDriver` returns TRUE; LBA2 unused) | `MIDI_MI.HQR`, 33 tracks | **the one real new audio capability** |
| Video | Smacker (`PlayAcf`) | FLA | **new FLA video driver** |

**Track A, MIDI (the one new capability).** Vendor the synth core once into AIL (`xmidi.c`
XMIDI->SMF + `tsf.h` + `tml.h`; make the converter reentrant). Render-then-stream (as `flade`
does): convert -> TML parse -> `tsf_render` to a PCM buffer -> play via the existing AIL SDL stream
path (`STREAM.CPP`). Extend `MIDI.H` with `PlayMidi/StopMidi/SetMidiVolume`. **Init the MIDI driver
unconditionally** (an engine capability, not per-game, see §5).

**Track B, FLA video.** Vendor `flade`'s SDL-free decoder (`fla.c`/`voc.c`/`util.h`); it emits a
320x200 8-bit paletted frame + 256-colour palette, the `Log` framebuffer's native form. Add
`PlayFla()` mirroring `PlayAcf()` (loop `fla_step`, blit to `Log`, `set_palette`, pace by fps,
ESC-skip). Route FLA cues to AIL: `OP_PLAY_SAMPLE`/`OP_STOP_SAMPLE` -> `PlaySample` (VOC); `OP_INFO`
MIDI triggers -> Track A. Both decoders init; the **`GameProfile` selects the active video driver**
(LBA1 `PlayFla`, LBA2 `PlayAcf`) and music source.

**Disc-image resource source (orthogonal, useful for raw GOG).** `flade`'s ISO9660 reader and
`cueplay`'s BIN/CUE parser (audio tracks are raw 16-bit/44.1k PCM in 2352-byte sectors, the same
layout `scripts/dev/iso_bin.py` already reads for data) can back a **resource-source driver** that
reads HQRs *and* CD tracks straight from a `.bin/.cue/.gog` image, no pre-extraction. This is
**orthogonal to the `GameProfile`**: the GameProfile picks per-game loaders; the resource-source
layer picks where bytes come from (a data directory, today's path, or a disc image). It lets
`PlayCD` and HQR loads work off a raw GOG disc without the user extracting tracks first.

**Soundfont (decided):** bundle one small, license-clean, good-quality GM soundfont and let the
user supply their own (config/CLI override). `flade`'s ~9 KB flute SF2 covers the FLA cutscene
MIDI; the in-game soundtrack (`MIDI_MI.HQR`) wants a small GM bank. Sourcing the bundled font
(CC0/MIT) is the only to-do; the FM `MIDI_SB.HQR` variant is unneeded when synthesising `MIDI_MI`.

**Sequencing:** (1) vendor + decouple the synth core; (2) AIL MIDI contract + render-to-stream,
validate an `MIDI_MI.HQR` track headless; (3) FLA decoder + `PlayFla` (silent), then its cues to
AIL; (4) optional disc-image resource source (`flade`/`cueplay`); (5) `GameProfile` routes the
per-game video/music drivers. Assets stay native throughout.

---

## 7. Open decisions for go/no-go

1. **Path:** confirm **B (in-repo, game-id-gated)**, with C extraction deferred. *Recommended.*
2. **Game-id keystone:** confirm a `GameProfile`/game-id is introduced with the first
   LBA1-loading PR (selects opcode table, asset names, menu descriptor, save schema). *Rec.*
3. **Menu:** confirm the agnostic-shell extraction + game-select UI come *after* LBA1 content
   loads (two real content sets), scoped to the start/load/save/options shell; in-world HUD
   stays per-game. *Recommended.*
4. **Fidelity bar:** accept LBA2-engine behaviour for LBA1 content (sort-tree vs painter's
   order, SVGA vs MCGA), to be verified per-scene, rather than bit-faithful LBA1 rendering.
5. **Flag width:** confirm the remap shim bridges LBA1 `UBYTE` flags ↔ LBA2 `S16` vars
   (not just opcode renumbering), the trickiest correctness point.
6. **Spikes:** confirm the §6 ladder (body → opcode/flag → scene; VOC optional) as the
   de-risking sequence before any committed PR plan.
7. **Doc home:** this doc is the plan; keep [LBA1_PORTING_SURFACE.md](LBA1_PORTING_SURFACE.md)
   as the cost survey and extend [ENGINE_GAME_INTERFACE.md](ENGINE_GAME_INTERFACE.md) /
   [ENGINE_FILE_FORMATS.md](ENGINE_FILE_FORMATS.md) with the remap table + format deltas as
   they are pinned. Confirm.
8. **Script path: transcode vs dual-table VM** (§6.4, raised by `../lba2remake`): run LBA1 Life
   scripts by either **(A) transcode** LBA1 bytecode to LBA2 bytecode so the existing `GERELIFE`
   runs it unchanged (no VM change, safest for LBA2; the spike-2 approach), or **(B) dual-table**
   the engine VM, i.e. teach `GERELIFE` a game-id-gated LBA1 opcode+operand table plus LBA1-only
   handlers (touches the shared VM, but proven at full-game scale by `lba2remake`, which keeps
   separate `data/lba1` + `data/lba2` tables and switches at parse time). Both need handlers for
   LBA1-only/media ops. *Lean: A first (zero VM risk); reconsider B if transcode edge cases
   (jump recompute, missing ops) pile up.* `lba2remake`'s per-game tables are the operand-layout
   reference for either path. Open.
9. **Asset strategy: offline convert vs in-engine adapt** (raised re spike 3; generalizes §8 to
   every asset type). Producing converted LBA2-format asset files would modify the user's retail
   data, add a conversion pipeline, and create a derivative (non-redistributable) copy. The
   preferred end-state is **in-engine adaptation, realized as the `GameProfile`'s per-game
   loader/resolver set** (decision 2 / §5): each game's native on-disk format is read and adapted
   to the engine's shared in-memory structures at load, leaving the retail assets untouched (the
   same model the engine already uses for LBA2). This makes asset adaptation a **facet of the
   `GameProfile` keystone, not a separate mechanism**: the one game-id that picks the opcode
   table, menu descriptor, and save schema also picks the body/background/sprite/script loaders.
   It keeps the LBA2 load path byte-identical (gate + test-guard) while reading LBA1 assets as-is. For background
   (§6.5) the abstraction is both simpler and more faithful than repacking: route grid/block/brick
   lookups to LBA1's three HQRs and compute `UsedBlock` at load, exactly as LBA1's own
   `LoadUsedBrick` does. The §8 script choice (in-memory transcode vs dual-table VM) is a sub-case;
   both keep assets untouched. The spike transcoders stay as the validated mapping reference +
   test oracles, not the pipeline. *Lean: in-engine adaptation; never modify the retail assets.*
   Open (recommended).

---

## 8. Traceability & preservation `[design]`

Path B confines all new work to the community SDL3 layer + new transcoder/remap code; the
upstream-traceable core (ASM-equivalence pairs, the Life/Track VM, the format parsers) is
**never moved or modified**, the same guarantee as [PLATFORM_PAL_PLAN.md](PLATFORM_PAL_PLAN.md)
§5.2. LBA1 content is *data* read by the existing engine; the LBA1 source (`lba1-classic`) and
`twin-e` are references (`lba1-classic` is the source of truth, twin-e a helper), not vendored engine code. The shared Adeline lineage is exactly
why this is cheap, and isolating LBA1 behind a game-id keeps both titles' provenance legible.

---

*Prepared read-only. The only changes are additive dev tooling under `scripts/dev/`
(`lba1_body_probe.py`, `lba1_body_render.py`, `lba1_script_remap.py`, `lba1_bkg_repack.py`,
`lba1_voc_probe.py`) plus this doc; no engine, build, or game behaviour changed. Awaiting
go/no-go on §7.*
