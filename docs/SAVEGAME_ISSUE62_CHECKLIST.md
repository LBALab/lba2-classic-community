# Issue #62 — Legacy saves & load safety (tracked checklist)

Working document for [GitHub issue #62](https://github.com/LBALab/lba2-classic-community/issues/62): **safe load + compatibility** for legacy / wrong-layout `.lba` streams on **64-bit** builds, without changing default gameplay.

**Normative format / pipelines** stay in [SAVEGAME.md](SAVEGAME.md). When #62 behavior stabilizes, fold **limits, error paths, and test pointers** back into SAVEGAME.md and trim this file or keep it as an implementation log—maintainer choice.

---

## Goals (from issue)

- [x] No silent corruption / **no SIGSEGV** on corrupt or legacy-layout streams: validate, reject, or defined fallback (bounded reads + `LoadContexte` returns **`-1`** on truncation).
- [x] Where feasible: **load legacy 32-bit wire layout** — heuristic + **`LBA2_SAVE_LOAD_ABI=32`** override; **`T_OBJ_3D`** wire migration in [SAVEGAME.CPP](../SOURCES/SAVEGAME.CPP).
- [x] **Hardening** that benefits everyone: bounded counts, capped file read, decompressed size vs buffer, etc.
- [x] **docs/SAVEGAME.md** updated (limits + env + `flagload`; canonical serializer still future).
- [x] **Tests** — `test_savegame_load_bounds` host harness ([tests/savegame/test_load_bounds.cpp](../tests/savegame/test_load_bounds.cpp)); do **not** relax unrelated ASM↔CPP tests ([AGENTS.md](../AGENTS.md)).

**Non-goals (issue):** portable `NUM_VERSION` 37+ on-disk format—separate follow-up.

---

## Codebase Memory MCP (optional but recommended)

Use the **codebase-memory** MCP (Cursor: `project-0-lba2-classic-community-codebase-memory-mcp`) to speed up **call-site discovery** and impact analysis. Indexed project id:

**`home-ivan-code-lba-hacking-lba2-classic-community`**

| Tool | Use for #62 |
|------|----------------|
| **`search_code`** | Find `LbaRead`, `LoadContexte`, `LoadGame`, `PtrSave`, `NbObjets`, `NbPatches` with `path_filter` (e.g. `SAVEGAME` or `SOURCES`). Prefer `mode=compact` first. |
| **`trace_path`** | From `LoadGame` / `LoadContexte` **outbound** calls (depth 2–4) to see who else assumes save buffer layout. |
| **`query_graph`** / **`get_architecture`** | Broader “what touches save load” when refactoring. |
| **`index_repository`** | If graph looks stale after large pulls. |

**Workflow:** after each audit pass, optionally **`ingest_traces`** with short structured notes (function, finding, risk) so future sessions keep context.

---

## Phase 0 — Audit (read-only) — done

| Field / region | Max / source | Behavior shipped | Decision |
|----------------|--------------|-------------------|----------|
| Whole `.lba` file | `640×480 + RECOVER_AREA` (`SaveLoadScreenBufferBytes`) | `LoadSize` caps read; oversize rejected | **Reject** |
| Player name (header) | `MAX_SIZE_PLAYER_NAME` + NUL | Bounded scan; no NUL → fail | **Reject** |
| `sizefile` + compressed tail | Buffer geometry | `SaveLoadValidateCompressedStaging` before `ExpandLZ` | **Reject** |
| `LoadContexte` cursor | `SaveLoadSetReadLimit(end)` | Guarded `LbaRead*` return **`-1`** (`SAVELOAD_CTX_ERR`) | **Reject** |
| `NbObjets` | `MAX_OBJETS` (100) | Range check; room `nb × stride` before object loop | **Reject** |
| Per-object stride | 278 (32-bit wire) vs native (`142 + offsetof(CurrentFrame)`) | Heuristic + `LBA2_SAVE_LOAD_ABI=32` | **Reject** if ambiguous uses native |
| `T_OBJ_3D` blob (32 wire) | 136 bytes | `T_OBJ_3D_WIRE32` + `SavegameObj3dFromWire32` | **Migrate** |
| `NbPatches` | `MAX_PATCHES` (500) | Range check | **Reject** |
| Patch apply | `PtrSceneMem` | Offset+size scene bounds | **Reject** |
| Extras count byte | `MAX_EXTRAS` (50) | Upper bound | **Reject** |
| `NbZones` | `MAX_ZONES` (255) | Range check | **Reject** |
| Incrust count | `MAX_INCRUST_DISP` (10) | Upper bound; fixed rain loop (`n++` only) | **Reject** / **bugfix** |
| Flow count | `MAX_FLOWS` (10) | Upper bound | **Reject** |
| Flow dots `wbyte2` | `MAX_FLOW_DOTS` (100) | Upper bound | **Reject** |
| Valid-pos tail `SizeOfBufferValidePos` | `SIZE_BUFFER_VALIDE_POS` | Checked in `LoadGame` | **Reject** |
| Checkpoint `LoadContexte` | `BufferValidePos` allocation | `SaveLoadSetReadLimit` in [VALIDPOS.CPP](../SOURCES/VALIDPOS.CPP) | **Reject** |
| Post-load branch | `OBJECT.CPP` | `flagload < 0 \|\| !flagload` → `InitLoadedGame` | **Fail safe** |

DEBUG / `NumVersion` < 34 extra fields: unchanged (still only in DEBUG/TEST/EDIT builds); retail path unchanged.

---

## Phase 1 — Invariants — merged into table above

---

## Phase 2 — Implement

| Step | Content | Done |
|------|---------|------|
| 2a | **Generic bounds** on hot paths | [x] |
| 2b | **Failure path** — `LoadContexte` **`-1`**, `LoadGame`/`RestartValidePos`/`ChangeCube` handle | [x] |
| 2c | **Legacy 32-bit object stride** | [x] |
| 2d | **SAVEGAME.md** + this checklist | [x] |

---

## Phase 3 — Tests

| Tier | Purpose | Fixtures |
|------|---------|----------|
| **Unit / harness** | LZ staging + stride heuristic | `test_savegame_load_bounds` |
| **Integration** | “Good” load still works after changes | Optional future committed `.lba` |
| **Optional local** | Full **numbered / fan** pack | Env dir (e.g. `LBA2_TEST_SAVES`); **skip** in public CI if not committed |

- [x] CTest `test_savegame_load_bounds`; see [TESTING.md](TESTING.md).
- [ ] Document optional local pack when env hook exists.

**Regression definition:** same integration assertions **before/after** #62 patches (load success + a few invariants you can read cheaply).

---

## Tooling (already in repo)

- [scripts/save_probe.py](../scripts/save_probe.py) — NDJSON inspection, LZ via `save_decompress`, ABI heuristic (not a substitute for engine proof).
- [scripts/save_probe_lz_selftest.py](../scripts/save_probe_lz_selftest.py) + **`make save-probe-lz-selftest`** — LZ round-trip sanity.

Use **`save_probe`** while debugging failed loads; **acceptance** for #62 remains **engine tests + docs**.

---

## PR split (suggested)

1. **PR1 — Audit doc only** (optional): paste Phase 0 table into issue + link here.  
2. **PR2 — Bounds** (smallest risky reads first: name length, decompressed size).  
3. **PR3 — Tail sections** (patches / extras / …).  
4. **PR4 — Legacy 32** (if still in scope).  
5. **PR5 — SAVEGAME.md merge** + trim this file.

---

## Open questions (fill in as you go)

- …

---

## Changelog (optional maintainer log)

| Date | Note |
|------|------|
| 2026-05-04 | Initial hardening + 32-bit object wire + `SAVEGAME_LOAD_BOUNDS` + host test + doc merge. |
