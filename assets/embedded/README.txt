Community splash (optional build)
---------------------------------

When CMake option LBA2_EMBEDDED_COMMUNITY_STARTUP is ON, DistribLogo shows one
embedded full-screen slide before AdelineLogo:

  * community_splash_* — from assets/splash.png (see prepare script)

Required filenames (fixed for CMake / bin2c):

  community_splash_640x480.lim
  community_splash_640x480.pal

Automated:

  python3 tools/prepare_embedded_startup_assets.py

Default mapping uses ``contain`` so the whole image fits on 640x480 (letterbox
if needed). Override: --fit cover|contain|stretch

Optional ``--content-scale S`` (0..1] shrinks the fitted bitmap before
centering (adds margin even for 4:3 sources that would otherwise fill the frame).
Same flag exists on tools/probe_community_assets.py --export.

AdelineLogo still uses retail screen.hqr (PCR_LOGO).

CD-ROM insert-disc screen is not used when LBA2_SKIP_CDROM_CHECK=ON (default).
Turn OFF to restore classic CD check:

  cmake -B build -DLBA2_SKIP_CDROM_CHECK=OFF

Configure + build:

  cmake -B build -DLBA2_EMBEDDED_COMMUNITY_STARTUP=ON
  cmake --build build
