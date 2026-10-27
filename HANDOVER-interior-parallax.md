# Handover — interior stereo parallax

Paste this file's path into a new session and say: "continue this work".

## Goal

Interiors are a flat isometric brick layer with 3D models on top. In the Quest
stereo view the background read as a painting: it had **zero** eye offset while
the models shifted. The goal is a 2.5D interior: background parallax that is
consistent with the models and free of artefacts (no black seams, no smearing).

## Current state

Branch `quest-good-perf`:

- `914cf7a4 feat(gl): give the isometric interior stereo depth` — depth scale
  for the interior, shared by models, sprites, shadows and the background.
- `5cb7a91c feat(gl): double the interior stereo budget` — `ISO_DEPTH_AZ_DENOM`
  16 → 32.
- `d8a3afa6 feat(gl): warp the interior backdrop per pixel` — the composite
  carries a screen-sized depth map and the layer's fragment stage shifts every
  pixel by the offset its own depth earns. Replaces the earlier tilt.
- `030b392e fix(gl): read the whole run when stamping layer depth` — the depth
  stamp advanced two bytes over a repeat run instead of one, so ~20k painted
  pixels per room kept depth 0 (nearest plane, maximum eye offset) and smeared
  sideways in the right eye. This was the reported smearing, and it also made
  small props (wall candles) look displaced: the left eye has no offset, so it
  looked clean.
- `c2c941bf fix(gl): draw the stereo HUD once, after the scene resolve` — the
  dialog/HUD overlay is now painted into both halves from one band list after
  the resolve, so the pair cannot drift and the text avoids MSAA softening.

Parked attempts (bands, underlay) are on branch `wip-interior-stereo`. Do not
merge it; mine it for parts.

## Key mechanics

- Interior "depth" is the isometric painter key `(x + z) / 2` in world units
  from the camera. `IsoDepthToAz()` / `IsoAzGain()` in `LIB386/H/3D/PROJ.H` map
  it to `aZ` and scale it by `ISO_DEPTH_AZ_DENOM`. The map is monotonic, so GL
  depth ordering is safe.
- The polygon stages turn `aZ` into an eye offset in the **vertex** stage:
  `sx = -eye.x * focal.x * eyeZScale / aZ + proj.x`. The layer warp reproduces
  exactly that in the **fragment** stage (`GL_SetLayerWarpUniforms`).
- `BrickDepthAz` (GRILLE.CPP) and `IsoDepth` (AFF_OBJ) must use the same
  scale — check both before changing either. `SpriteGLDepth` (OBJECT.CPP),
  `InteriorShadowAz` and `BrickDepthAz` (GRILLE.CPP) are the other consumers.
- The warp only applies in **single-pass** stereo. In the two-pass path each
  eye re-renders `AffGrille` with its own camera, so the layer parallaxes by
  construction.
- Interior shadows need `GL_DrawShadowPoly(..., terrainBias = 0)`.
- Layer draw: `GL_DrawIndexedLayer()` in `LIB386/GL/RENDER_GL.CPP`; colour
  composite `s_baseLayer` and depth map `s_baseDepth` in `SOURCES/GRILLE.CPP`.

## Reviewing stereo without the headset

`docs/ANDROID.md` §"Reviewing the stereo pair without the headset" is the
current recipe. Short version:

```
LBA2_FAKE_STEREO=30 [LBA2_FAKE_STEREO_SS=1] [LBA2_FAKE_VR_DIALOG=1] \
xvfb-run -a lba2cc --no-autosave --load <save> \
  --exec-at 2 "cube 15" --exec-at 4 "teleport 25856 768 29000" \
  --exec-at 30 "ui dialog 3 /tmp/dlg.png" \
  --fixed-dt 16 --tick 120 --screenshot /tmp/pair.png --exit
```

`--screenshot` and the `ui dialog` capture both write the pair side by side at
eye resolution. Measured on that pair: far wall 27 px, mid floor 29 px, near
floor 33 px, model beside the near floor 32 px — depth-ordered and consistent
with the models.

A headset screencap is **not** usable for this: each half carries its own pose
and reprojection, so the halves are not comparable pixel by pixel.

## Known limits (judge these in the headset)

- The warp cannot invent content beyond the composed silhouette. Where the
  background ends inside the frame, the near eye keeps that margin black.
- `OverlayFlush` draws a restored foreground-brick patch at one average `az`
  (`s_overlayAz`), not per pixel, so a patch over a model can sit a pixel or
  two off the tiles around it in the right eye.
- Depth discontinuities between tiles still tear by a few pixels (the largest
  adjacent jump is ~7 px of eye offset at a wall/floor boundary). That is the
  honest cost of a backward per-pixel warp over tiles; a forward per-tile
  splat would be exact but reopen the seams the single composite removed.

## Device loop

`cmake --build out/build/quest -- -j$(nproc)` →
`bash scripts/packaging/bundle-android.sh --lib out/build/quest/SOURCES/liblba2cc.so
--version $(cat VERSION) --arch arm64-v8a --build-dir out/build/quest
--sdk-root $HOME/Android/Sdk --sdl3-java-src out/android/SDL3
--sdl3-lib out/android/sdl3-install/lib/libSDL3.so
--cxx-shared-lib /opt/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so
--manifest packaging/android/AndroidManifest.quest.xml
--extra-lib out/build/quest/SOURCES/libopenxr_loader.so
--output-dir out/build/quest/apk` →
`adb install -r out/build/quest/apk/lba2cc-0.13.0-dev-android-arm64-v8a.apk` →
`adb shell am start -n org.lbalab.lba2cc/org.libsdl.app.SDLActivity`.

Screenshot: `adb exec-out screencap` (raw RGBA, 16-byte header, 4128x2208).
The on-device PNG encoder often writes 0 bytes, so convert the raw locally.
The app exits on its own when the headset is not worn, and that exit aborts in
`OpenXR_DestroySession` (pre-existing, unrelated); capture within ~20 s.

## Editing hazards

- Do **not** rewrite regions of `RENDER_GL.CPP` by string slicing. Use the
  edit tool with exact anchors and build after each change.
- `clang-format-18` is the pinned version (`pip install clang-format==18.1.8`).
  `LIB386/SYSTEM/OPENXR.CPP` is **not** format-clean in its Android-only
  region, so touching it drags a ~300-line reformat into the commit; put
  desktop-only shims somewhere else (the `LBA2_FAKE_VR_DIALOG` knob lives in
  `NormalWinDial` for exactly this reason).
