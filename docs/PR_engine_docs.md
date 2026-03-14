# Add engine docs: glossary, lifecycles, scene index

## Why

Give contributors and modders a single, code-anchored reference for engine terms, lifecycles, and scenes so they can onboard and build tooling without reverse‑engineering from scratch.

- **Onboarding** — New contributors and modders need a map of the engine (cubes, zones, scripts, lifecycles) without digging through the whole codebase or guessing names.
- **Single reference** — One place for terms, code locations, and scene layout. “Code > this doc” keeps the doc from replacing the source; it points to it.
- **Modding and tooling** — Scene index, script/zone concepts, and constants (e.g. Comportement, LM_*) support level design, script editors, and other tools that need stable, code-backed definitions.
- **Preservation** — Engine behaviour and naming live mostly in code and a few heads; these docs capture that and tie it to files/symbols so it’s findable and maintainable.

## What

- **GLOSSARY.md** — Domain terms (Cube, Zone, T_OBJET, scripts, hero, collision grid, enums) with code locations and refs.
- **LIFECYCLES.md** — Main loop order, scene load phases, object/hero/animation lifecycles and where they live in code.
- **SCENES.md** — All 223 cubes by island with location names (from [LBALab/metadata](https://github.com/LBALab/metadata)), interior/exterior, obj/zone counts.

## Conventions

- Code locations and constants checked against the repo; truth hierarchy: **code > this document > external sources**.
- Cross-links only (e.g. Comportement in LIFECYCLES → GLOSSARY) to avoid duplication.
- Scene names and similar human-readable data attributed to [LBALab/metadata](https://github.com/LBALab/metadata) in the intro where used.
- In-game scene inspection: [CONSOLE.md](CONSOLE.md) (`cube`, `status`).

## Notes

- HQR line ref is 66–80 in COMMON.H; life script opcode count clarified (131 `LM_*` in code, “155” often cited elsewhere).
- Filename casing in doc paths matches repo (e.g. `SOURCES/COMMON.H`, `SOURCES/3DEXT/LOADISLE.CPP`).

---

*Worth a quick pass for consistency with the code; no behaviour changes.*
