# Legacy port-64 native-format save fixtures

Two saves in the **pre-portable-writer** on-disk layout: the format a 64-bit
build of this port wrote before the wire converters landed (issue #64). Their
per-object / per-extra / per-flow records use the host-native 64-bit struct
stride (304 B per object, not the 136 B wire object), so they exercise the
reader's native path.

These are the only way to test that path: after the portable writer, no build
produces native-format saves, so the fixtures cannot be regenerated and are
committed here.

They also pin a real regression. The reader reads the native (wider) stride
first, because reading it from shorter wire data reliably over-runs and fails
validation (so wire saves are retried as wire), whereas the reverse order lets a
native save spuriously pass the shorter wire read and then SIGSEGV in
`InitLoadedGame`. `multi_object.lba` is exactly such an ambiguous save: an early
wire-first version of the reader crashed on it. These fixtures catch that.

## Provenance

Not retail data. Both were written by a 64-bit build of `main` at commit-time,
then processed locally. They contain no game assets, only references
(cube/scene/body/animation indices) into the retail HQR archives, same as the
`steam_classic_2023/` corpus.

| File | Layout | Objects | How made |
|------|--------|---------|----------|
| `multi_object.lba` | native 64-bit, compressed (`0xA4`) | 6 | `savebug` after loading a scene with the pre-fix writer |
| `single_object.lba` | native 64-bit, uncompressed (`0x24`) | 1 | derived from `multi_object.lba`: decompressed, object array truncated to the hero, `NbObjets` set to 1, re-emitted uncompressed |

The single-object case is the worst case for detection (object 0's `IndexFile3D`
sits in the stride-invariant prefix and always validates), which is why it is
included; natural saves never have fewer than ~3 objects.

## What they test (via `--save-load-test` / `--load`, retail data required)

- `multi_object.lba` and `single_object.lba` under **auto** load (`flagload=0`)
  and, on the full `--load` path, do **not** SIGSEGV. This is the migration path
  every legacy save takes, and the crash-regression guard.
- both under **`LBA2_SAVE_LOAD_ABI=64`** load: the native read is correct.
- a sample of `steam_classic_2023/` (retail wire) under **`LBA2_SAVE_LOAD_ABI=64`**
  rejects: forced-native cannot read wire records (discriminator sanity).

Driver: `tests/savegame/corpus/native_fallback_check.py`.
