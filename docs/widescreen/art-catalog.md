# Widescreen art catalog — `SCREEN.HQR`

Inventory of every full-screen bitmap shipped in retail `SCREEN.HQR`. This is
research output for the [widescreen plan](../WIDESCREEN.md) — concretely, the
"what art do we have to deal with?" deliverable. It does **not** propose any
re-authoring; copyrighted bitmaps stay where they are.

## How to reproduce

```bash
scripts/dev/art_catalog_screen.py /path/to/retail/data/SCREEN.HQR \
    docs/widescreen/catalog-dumps/
```

The `catalog-dumps/` directory has a `.gitignore` that excludes everything
except itself, so the rendered PNGs stay local-only. Don't `git add` anything
under it.

## File layout

`SCREEN.HQR` carries **39 pairs** — 78 entries total. Each pair is a 640×480
indexed 8-bit bitmap (entry `2k`) followed by its 256×3 RGB palette (entry
`2k+1`). Several call sites bake the `* 2` index conversion in by hand
(`numpcx *= 2 ; // car il y a les palettes intercalées` —
[GAMEMENU.CPP:4597](../../SOURCES/GAMEMENU.CPP)); others pass the raw entry
index. The script normalises 6-bit palette values (DOS-VGA legacy) so PNGs
look the same as on-screen.

Sixteen entries have named `PCR_*` defines in
[SOURCES/COMMON.H:163-179](../../SOURCES/COMMON.H); the rest are addressed
positionally from script bytecode (`GERELIFE.CPP:2137` reads `num` from
`*PtrPrg++`) or from the dynamic `ListArdoise[]` slate inventory
([INVENT.CPP:530-532](../../SOURCES/INVENT.CPP)).

## Inventory

Dimensions are uniform — every bitmap is 640×480, every palette is 768 bytes.
"Use" is "what game code path is known to surface this entry" — orphan entries
appear in retail data but no live code path references them.

| # | PCR define | Tag | What it depicts | Use |
|---|---|---|---|---|
| 0  | `PCR_LOGO`       | logo       | Adeline logo splash                                      | Boot splash ([GAMEMENU.CPP:4318](../../SOURCES/GAMEMENU.CPP)) |
| 2  | `PCR_BUMPER`     | bumper     | "LBA2" bumper card                                       | Boot splash ([GAMEMENU.CPP:4463, 4551, 4581](../../SOURCES/GAMEMENU.CPP)) |
| 4  | `PCR_MENU`       | menu       | Cloudy-sky main-menu background                          | Main menu + save thumbnails ([GAMEMENU.CPP:2375, 2486, 3444, 4133](../../SOURCES/GAMEMENU.CPP)) |
| 6  | `PCR_ARDOISE`    | ardoise    | Slate (wooden-framed chalkboard, grid)                   | Slate dialog backdrop ([INVENT.CPP:2147](../../SOURCES/INVENT.CPP)) |
| 8  | `PCR_PEGOUT`     | pegout     | Sewer map — paper-on-wood ("Petit Égouts")               | Orphan in current code; loadable by script |
| 10 | `PCR_AEGOUT`     | aegout     | Sewer map — black slate grid ("Agrandi Égouts")          | Slate inventory ([INVENT.CPP:2120-2159](../../SOURCES/INVENT.CPP) via `ListArdoise`) |
| 12 | `PCR_PLABYRT`    | plabyrt    | Labyrinth map — paper                                    | Orphan |
| 14 | `PCR_ALABYRT`    | alabyrt    | Labyrinth map — slate                                    | Slate inventory |
| 16 | `PCR_PLUNE`      | plune      | Moon map — paper                                         | Orphan |
| 18 | `PCR_ALUNE`      | alune      | Moon map — slate                                         | Slate inventory |
| 20 | `PCR_HACIENDA`   | hacienda   | Hacienda cliff-cove view (framed circular)               | Orphan in current code (named, no caller) |
| 22 | `PCR_VUPHARE`    | vuphare    | Lighthouse establishing shot (framed)                    | Orphan in current code (named, no caller) |
| 24 | `PCR_VUPHARE2`   | vuphare2   | Lighthouse, porthole-vignette second take                | Orphan |
| 26 | —                | unnamed    | Planet + comet through space (cinematic still)           | Script-driven cinematic |
| 28 | —                | unnamed    | Dr Funfrock & lieutenants in lab interior                | Script-driven cinematic |
| 30 | —                | unnamed    | Picnic with friends + alien hand (cinematic still)       | Script-driven cinematic |
| 32 | —                | unnamed    | Emerald Moon close-up through porthole vignette          | Script-driven cinematic |
| 34 | —                | unnamed    | Dark interior with green-glow Sendell orbs               | Script-driven cinematic |
| 36 | —                | unnamed    | Celestial diagram — planet sliced into labelled sectors  | Script-driven cinematic |
| 38 | —                | unnamed    | Mona-Lisa pastiche portrait (gallery)                    | Script-driven cinematic |
| 40 | —                | unnamed    | Volcano world — paper-style top-down                     | Script-driven cinematic |
| 42 | —                | unnamed    | Volcano world — black slate-grid top-down                | Script-driven cinematic |
| 44 | —                | unnamed    | Hand turning album page in front of bald guard           | Script-driven cinematic |
| 46 | —                | unnamed    | Bicorn-hatted bust on classical plinth                   | Script-driven cinematic |
| 48 | —                | unnamed    | Three soldier-musicians on parade                        | Script-driven cinematic |
| 50 | —                | unnamed    | Slaves in checkered prison cell (top-down)               | Script-driven cinematic |
| 52 | —                | unnamed    | Twinsen vs dragon, fire breath                           | Script-driven cinematic |
| 54 | —                | unnamed    | Temple/forum with red sky and dragon silhouette          | Script-driven cinematic |
| 56 | —                | unnamed    | Twinsen's spaceship approaching pink-cloud volcano world | Script-driven cinematic |
| 58 | —                | unnamed    | Coin/marble explosion celebration                        | Script-driven cinematic |
| 60 | `PCR_CDROM`      | cdrom      | "Insert CD" plate                                        | Disc-check fallback ([MESSAGE.CPP:592](../../SOURCES/MESSAGE.CPP), [PERSO.CPP:2432](../../SOURCES/PERSO.CPP), [CONFIG.CPP:1444](../../SOURCES/CONFIG.CPP)) |
| 62 | —                | unnamed    | Glowing Sendell-baby figure inside dark orb              | Script-driven cinematic |
| 64 | —                | unnamed    | Twinsen's house — paper architectural sketch             | Script-driven cinematic |
| 66 | —                | unnamed    | Twinsen's house — slate-grid blueprint                   | Script-driven cinematic |
| 68 | —                | unnamed    | Twinsen close-up with medallion, cloudy backdrop         | Script-driven cinematic |
| 70 | —                | unnamed    | Sendell-form glowing Twinsen, ocean horizon              | Script-driven cinematic |
| 72 | `PCR_ACTIVISION` | activision | Activision distributor bumper                            | Boot splash, US version ([GAMEMENU.CPP:4443](../../SOURCES/GAMEMENU.CPP)) |
| 74 | `PCR_EA`         | ea         | EA distributor bumper                                    | Boot splash, EA version ([GAMEMENU.CPP:4447](../../SOURCES/GAMEMENU.CPP)) |
| 76 | `PCR_VIRGIN`     | virgin     | Virgin distributor bumper                                | Boot splash, Virgin version ([GAMEMENU.CPP:4452](../../SOURCES/GAMEMENU.CPP)) |

## Observations

**Map P/A pairs.** The first ten map bitmaps are paper / slate-grid renderings
of the same scene. The slate-grid versions get loaded into the in-game slate
inventory via `ListArdoise[]` ([INVENT.CPP:530-532](../../SOURCES/INVENT.CPP)
seeds the default 5/7/9 → entries 10/14/18 = AEGOUT/ALABYRT/ALUNE). The paper
versions are orphans in the current code path — no live caller, though they
remain reachable from `GERELIFE.CPP`'s cinematic script opcode. Likely
historical artwork or kept for a cut feature.

**Porthole-vignette frame.** Several cinematic stills (24, 32, possibly 22)
share the same circular vignette frame, baked into each bitmap rather than
drawn as a separate overlay. For widescreen this means letterboxing (or
black-bordering) is the only "correct" treatment — extending the vignette
would require re-authoring the frame.

**`PCR_HELP`** is referenced at [GAMEMENU.CPP:4020,4031](../../SOURCES/GAMEMENU.CPP)
but the whole `Help()` function is wrapped in `/* ... */`. The identifier is
not defined anywhere; the dead block is the only reason it survives in `grep`.
Safe to ignore for widescreen.

**Demo bumper.** [GAMEMENU.CPP:4437](../../SOURCES/GAMEMENU.CPP) calls
`ShowLogo(3*2, 5)` under `#ifdef DEMO`, which would load entry 6
(`PCR_ARDOISE`). Likely a stub the demo build never actually uses, since 6 is
the slate not a distributor logo. Not relevant for retail/widescreen.

## Widescreen implications (one-line summary)

Every entry is a fixed 640×480 indexed bitmap. There is no path that
re-renders these dynamically, so any widescreen treatment is one of:
**(a) letterbox** to 4:3 inside the wider framebuffer, **(b) extend** the
existing image by stretching / mirroring / colour-extending edges, or
**(c) ship an HD pack** as a separate (re-authored) `SCREEN.HQR` later. The
strategy doc (TBD) triages each entry into one of those buckets. Until then,
the safe default is (a).
