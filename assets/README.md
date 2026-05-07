# Source art for optional community branding

This folder holds **authoring** PNGs for features that are **off by default** in CMake so retail behaviour stays easy to compare.

| Path | Role |
|------|------|
| `splash.png` | Full-screen **DistribLogo** image (8-bit indexed at build time). |
| `embedded/` | **Generated** `community_splash_640x480.{lim,pal}` land here after you run the prepare script (see below). Not required unless `LBA2_EMBEDDED_COMMUNITY_STARTUP=ON`. |

## Quick workflow

1. Edit `splash.png` (640×480 target art; script scales/fits — default **contain** keeps the whole picture visible). Use `--content-scale 0.85` (or similar in `(0,1]`) if you want extra letterboxing even when the PNG is already 4:3.
2. From the repo root (Python venv with Pillow if you use one):

   ```bash
   python3 tools/prepare_embedded_startup_assets.py
   ```

3. Configure with embedding enabled and build:

   ```bash
   cmake -B build -DLBA2_EMBEDDED_COMMUNITY_STARTUP=ON
   cmake --build build
   ```

**CMake options**

- **`LBA2_EMBEDDED_COMMUNITY_STARTUP`** — embed splash into the binary (default **OFF**).
- **`LBA2_SKIP_CDROM_CHECK`** — skip the legacy insert-disc UI and call `InitCD("")` (default **ON** for this port).

## Details

- Full filenames, edge cases, and links to engine behaviour: **[embedded/README.txt](embedded/README.txt)**.
- Implementation: `tools/prepare_embedded_startup_assets.py`, `tools/bin2c.py`, `SOURCES/GAMEMENU.CPP` (`DistribLogo`), `SOURCES/CMakeLists.txt` (embedded blobs).

**Note:** Retail game data (`screen.hqr`, etc.) are not redistributable; keep them out of the repo. Splash conversion is **256-colour**; complex gradients may band — adjust art or `--fit` if needed.
