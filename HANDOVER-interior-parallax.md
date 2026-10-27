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
- `3efb2162 feat(gl): tilt the interior backdrop instead of splitting it` —
  backdrop as one quad whose corners carried the far and near ends of the
  room's depth range. Superseded below.
- `5cb7a91c feat(gl): double the interior stereo budget` — `ISO_DEPTH_AZ_DENOM`
  16 → 32.
- `feat(gl): warp the interior backdrop per pixel` — replaces the tilt. The
  composite now carries a screen-sized depth map next to its colour, and the
  layer's fragment stage shifts every pixel by the offset its own depth earns.

Parked attempts (bands, underlay) are on branch `wip-interior-stereo`. Do not
merge it; mine it for parts.

## Key mechanics

- Interior "depth" is the isometric painter key `(x + z) / 2` in world units
  from the camera. `IsoDepthToAz()` / `IsoAzGain()` in `LIB386/H/3D/PROJ.H` map
  it to `aZ` and scale it by `ISO_DEPTH_AZ_DENOM`. The map is monotonic, so GL
  depth ordering is safe.
- The polygon stages turn `aZ` into an eye offset in the **vertex** stage:
  `sx = -eye.x * focal.x * eyeZScale / aZ + proj.x` (see `s_clutVertSrc`,
  `GL_SetClutEyeUniforms`, `GL_SetSinglePassEyeDelta`). Offset ∝ 1/aZ.
- The layer warp reproduces exactly that expression in the **fragment** stage:
  `GL_SetLayerWarpUniforms` pushes `uLayerShiftPx` (= eye * proj) and
  `uLayerShiftPerAz` (= -eye.x * FRatioX * eyeZScale, plus the y term) per eye,
  and the shader does `tc -= uLayerShiftPx + uLayerShiftPerAz / az`.
- Consumers that must stay on one scale: `IsoDepth` (AFF_OBJ.CPP),
  `SpriteGLDepth` (OBJECT.CPP), `InteriorShadowAz` (GRILLE.CPP ProjectShadow),
  `BrickDepthAz` (GRILLE.CPP).
- The warp only applies in **single-pass** stereo. In the two-pass path each
  eye re-renders `AffGrille` with its own camera, so the layer parallaxes by
  construction and the uniforms stay zero.
- Interior shadows need `GL_DrawShadowPoly(..., terrainBias = 0)`.
- The layer draw is `GL_DrawIndexedLayer()` in `LIB386/GL/RENDER_GL.CPP`; the
  composed image is `s_baseLayer` and its depth map `s_baseDepth`, both in
  `SOURCES/GRILLE.CPP`. `LIB386/H/GL/RENDER_GL.H` holds the API doc.

## Why the first per-pixel attempt went black

It bound the depth map on texture unit 3 through `GL_BindTexUnit`. That cache
was `s_curBoundTex[3]`, so unit 3 wrote one slot past the end and unit 3 is
also the texture-array unit the batch programs sample. The depth map now lives
on unit 6 (`GL_LAYER_DEPTH_UNIT`); the cache covers units 0-6, and the layer
draw invalidates the cache after creating or uploading textures, because those
uploads bind behind its back.

## Verification

- Mono must stay pixel-identical: `--no-autosave --load <save> --exec-at 2
  "cube 15" --exec-at 4 "teleport 25856 768 29000" --fixed-dt 16 --tick 120
  --screenshot out.png --exit` under `xvfb-run` (no `--headless`). Compare with
  `ImageChops`: after the warp, only the live frame counter at ~(856, 90)
  differs run to run.
- Shader validation: extract the layer vertex/fragment from `RENDER_GL.CPP`,
  inject `#define LBA_LAYER_NO_WRAP 1` after the header, run
  `glslangValidator` on it as-is and converted to `#version 300 es`.
- Host tests: build `lba2cc` then `host_tests` in `out/build/hosttest`, run
  `ctest -L host_quick`; 48/50 pass, `test_poly_geometry` and
  `test_move_framerate` fail before this work too.
- Room depth observed on device: cube 15, `azMin=904 azMax=1320`, so the eye
  offset spans `33150/1320 ≈ 25 px` to `33150/904 ≈ 37 px` — 12 px of
  differential across one room. The depth byte is ~1.6 aZ per step there, well
  under a tenth of a pixel.

## Known limits (judge these in the headset)

- A compositor screenshot cannot be used to measure disparity: each eye half
  carries a different pose/warp, so the two halves are not comparable pixel by
  pixel. Judgement needs eyes in the headset.
- The warp cannot invent content beyond the composed silhouette. Where the
  background ends inside the frame (black margin), the near eye keeps that
  margin black; it does not smear, but it also does not extend the room.
- `OverlayFlush` draws a restored foreground-brick patch at one average `az`
  (`s_overlayAz`), not per pixel, so a patch over a model can sit a pixel or
  two off the tiles around it in the right eye.

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

Screenshot: `adb exec-out screencap` (raw RGBA, 16-byte header, 4128x2208);
the on-device PNG encoder often writes 0 bytes, so convert the raw locally.
The app exits on its own when the headset is not worn, and that exit aborts in
`OpenXR_DestroySession` (pre-existing, unrelated); take the screenshot within
~20 s of launching.

## Editing hazards

- Do **not** rewrite regions of `RENDER_GL.CPP` by string slicing; three
  separate attempts duplicated or truncated shader/functions. Use the edit tool
  with exact anchors, build after each change, and prefer a dedicated fragment
  shader over injecting into the shared CLUT source.
- `GRILLE.CPP` is ASCII-art heavy; `clang-format` (18) leaves it alone, but the
  four `if (x < bx) bx = x;` lines in `BaseLayerCompose` had to be reformatted
  when the file was touched.
