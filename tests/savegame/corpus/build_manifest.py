#!/usr/bin/env python3
"""
Build a savegame corpus manifest from probe.ndjson.

Output: manifest.json — one entry per save with classification + slots for the
expected engine load outcome (filled in by humans/the harness, not the probe).

Classification rules (probe-only signals):
  - num_version != 36                       -> "unknown_version"
  - compressed and !save_decompress_ok      -> "lz_corrupt"
  - probe_abi in (32, 64) and not ambiguous -> "abi_<n>"
  - both s32 and s64 negative (no offset
    yields a plausible NbPatches)           -> "stride_unresolved"
  - both s32 and s64 plausible (>= 0)       -> "abi_ambiguous_plausible"
  - else                                    -> "abi_ambiguous"

The classification is a starting point for the harness; the real source of
truth is `expected_load` once the engine harness fills it in.
"""
import json, os, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
NDJSON = HERE / "probe.ndjson"
OUT = HERE / "manifest.json"

def classify(r):
    if r.get("num_version") != 36:
        return "unknown_version"
    if r.get("compressed") and r.get("save_decompress_ok") is False:
        return "lz_corrupt"
    # Forward-simulation predictor (preferred): walks LoadContexte landmarks
    # under each ABI and picks the one whose values stay in MAX_ ranges.
    pred = r.get("predicted_abi")
    if pred in ("32", "64"):
        return f"abi_{pred}"
    if pred == "ambiguous":
        return "abi_ambiguous_forward_sim"
    if pred == "corrupt":
        return "corrupt"
    # Fallback to the older heuristic-based classification (for older probes).
    abi = r.get("obj3d_abi")
    if abi in ("32", "64"):
        return f"abi_{abi}"
    s32 = r.get("score_32")
    s64 = r.get("score_64")
    try:
        s32_ok = isinstance(s32, int) and s32 >= 0
        s64_ok = isinstance(s64, int) and s64 >= 0
    except Exception:
        s32_ok = s64_ok = False
    if not s32_ok and not s64_ok:
        return "stride_unresolved"
    if s32_ok and s64_ok:
        return "abi_ambiguous_plausible"
    return "abi_ambiguous"

def main():
    save_root = os.environ.get("LBA2_SAVE_TEST_DIR")
    if not save_root:
        sys.stderr.write(
            "LBA2_SAVE_TEST_DIR not set — point it at your save directory.\n"
            "(Example: export LBA2_SAVE_TEST_DIR=$HOME/.local/share/Twinsen/LBA2/save)\n"
        )
        sys.exit(2)
    rows = [json.loads(l) for l in NDJSON.read_text().splitlines() if l.strip()]
    rows.sort(key=lambda r: r["path"])
    entries = []
    for r in rows:
        entries.append({
            "name": os.path.basename(r["path"]),
            "rel_path": os.path.relpath(r["path"], save_root),
            "size": r["size"],
            "num_version": r.get("num_version"),
            "compressed": r.get("compressed"),
            "cube": r.get("cube"),
            "nb_objets": r.get("nb_objets"),
            "probe_abi": r.get("obj3d_abi"),
            "predicted_abi": r.get("predicted_abi"),
            "predicted_outcome": r.get("predicted_outcome"),
            "score_32": r.get("score_32"),
            "score_64": r.get("score_64"),
            "lz_ok": r.get("save_decompress_ok"),
            "classification": classify(r),
            # filled in by harness / hand:
            "expected_load": None,        # "ok" | "reject" | "ctxerr" | "segv"
            "notes": "",
        })
    by_class = {}
    for e in entries:
        by_class.setdefault(e["classification"], 0)
        by_class[e["classification"]] += 1
    manifest = {
        "save_root": save_root,
        "n_saves": len(entries),
        "by_classification": by_class,
        "entries": entries,
    }
    OUT.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"wrote {OUT}: {len(entries)} entries")
    for k, v in sorted(by_class.items()):
        print(f"  {k}: {v}")

if __name__ == "__main__":
    main()
