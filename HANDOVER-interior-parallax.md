# Handover — interior stereo parallax (next session)

Paste this file's path into a new session and say: "continue this work".

## Goal

Interiors are a flat isometric brick layer with 3D models on top. In the Quest
stereo view the background read as a painting: it had **zero** eye offset while
the models shifted. The goal is a 2.5D interior: background parallax that is
consistent with the models and free of artefacts (no black seams, no smearing).

## Current state (working, installed on Quest, committed)

Branch `quest-good-perf`, HEAD `5cb7a91c`:

- `914cf7a4 feat(gl): give the isometric interior stereo depth` — depth scale
  for the interior, shared by models, sprites, shadows and the background.
- `3efb2162 feat(gl): tilt the interior backdrop instead of splitting it` —
  backdrop drawn as **one** quad whose corners carry the far and near ends of
  the room's depth range (top = far wall, bottom = floor at the feet). No seams
  (one image), no underlay, mono pixel-identical.
- `5cb7a91c feat(gl): double the interior stereo budget` — `ISO_DEPTH_AZ_DENOM`
  16 → 32.

User verdict on the tilt: "doesn't look well, do per-pixel warp".

Parked attempts (bands, underlay, first per-pixel try) are on branch
`wip-interior-stereo`. Do not merge it; mine it for parts.

## Key mechanics

- Interior "depth" is the isometric painter key `(x + z) / 2` in world units
  from the camera. `IsoDepthToAz()` / `IsoAzGain()` in
  `LIB386/H/3D/PROJ.H` map it to `aZ` and scale it by
  `ISO_DEPTH_AZ_DENOM`. The map is monotonic, so GL depth ordering is safe.
- The polygon stages turn `aZ` into an eye offset in the **vertex** stage:
  `sx = -eye.x * focal.x * eyeZScale / aZ + proj.x` (see `s_clutVertSrc`,
  `GL_SetClutEyeUniforms`, `GL_SetSinglePassEyeDelta`).
  So offset ∝ 1/aZ: halving aZ doubles the parallax.
- Consumers that must stay on one scale: `IsoDepth` (AFF_OBJ.CPP),
  `SpriteGLDepth` (OBJECT.CPP), `InteriorShadowAz` (GRILLE.CPP ProjectShadow),
  `BrickDepthAz` (GRILLE.CPP).
- Interior shadows need `GL_DrawShadowPoly(..., terrainBias = 0)`: the bias is
  a terrain-only trick and lifts the shadow off the floor (right-eye-only
  offset, since eye 0 is the reference).
- The layer draw is `GL_DrawIndexedLayer()` in `LIB386/GL/RENDER_GL.CPP`; the
  composed image is `s_baseLayer` in `SOURCES/GRILLE.CPP`.
  `LIB386/H/GL/RENDER_GL.H` holds the API doc.

## Task: replace the tilt with a per-pixel depth warp

1. **Depth map at compose time** (`SOURCES/GRILLE.CPP`): alongside
   `s_baseLayer`, compose an `U8` map where each painted pixel gets its tile's
   depth, normalized to `[0,255]` over the frame's `azMin..azMax`. Stamp in
   painter order, where the decoded brick texel is non-zero.
   A working `BaseLayerDepthStamp()` exists on `wip-interior-stereo`; the
   `T_BRICK_ORDER_ENTRY.Az` field and `BrickDepthAz()` already exist in HEAD.
2. **Upload the map** next to the colour texture, generation-keyed the same way
   (the parked branch extends the layer slot struct with `depthTex`).
3. **Warp in the fragment stage**: replace the per-quad depth with
   ```glsl
   float dn = texture2D(uLayerDepth, vec2(tc.x / uAtlasScale.x,
                                          tc.y / uAtlasScale.y)).r;
   float az = uLayerAzMin + dn * uLayerAzSpan;
   tc -= uLayerShiftPx + uLayerShiftPerAz / max(az, 1.0);
   ```
   with `uLayerShiftPerAz = -eye1.x * FRatioX * eyeZScale` (plus the y term)
   set per eye exactly as `GL_SetClutEyeUniforms` does, inside the existing
   two-eye scissor loop.
4. **Delete the ramp/bands**: pass `azTop/azBottom` out, drop the band state
   and the underlay.

### Unresolved bug from the first attempt

With the depth-map layer wired in, the **whole frame went black** (exterior
too), even with a solid-colour fragment, no GL error, program linked, frames
still presenting (fewer than the 300 needed for `[GLPerf]`). Setting
`LBA2_NO_LAYER=1` (per-brick fallback) rendered fine, so the layer draw
corrupted the frame.

Suspects, in order:

- the extra `glBindTexture` on **unit 3** — that is the texture-array unit the
  batch programs sample `uAtlasArr` on; a 2D texture there can desync the
  array path's bound-texture cache;
- the layer vertex attribute set (6 floats/vertex) versus the batch path's
  16-byte stride and its attribute-pointer cache (`s_clutAttribsDirty`);
- the depth texture upload path.

Suggested first experiment: bind the depth map to unit 0 instead of 3 for one
build (sample it only in the layer program, which owns unit 0's binding for its
whole draw) and see whether the frame survives.

## Verification available without the headset

- Mono must stay pixel-identical: `--no-autosave --load <save> --exec-at 2
  "cube 15" --exec-at 4 "teleport 25856 768 29000" --fixed-dt 16 --tick 120
  --screenshot out.png --exit` under `xvfb-run` (no `--headless`).
  Compare with `ImageChops` — expect ~0.005/255.
- Shader validation: extract the layer vertex/fragment from
  `RENDER_GL.CPP`, convert to `#version 300 es`, run `glslangValidator
  -S vert|frag` (both ES 1.00 and 3.00 pass today).
- Expected per-eye offset: `33150 / aZ` px, aZ = interior key / DENOM.
  At DENOM 16 the room spans ~6.5–8.8 px; at 32, double.
- Host tests: build `lba2cc` then `host_tests` in `out/build/hosttest`, run
  `ctest -L host_quick`; 48/50 pass, `test_poly_geometry` and
  `test_move_framerate` fail before this work too.

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

## Editing hazards hit this session

- Do **not** rewrite regions of `RENDER_GL.CPP` by string slicing; three
  separate attempts duplicated or truncated shader/functions. Use the edit tool
  with exact anchors, build after each change, and prefer a dedicated fragment
  shader over injecting into the shared CLUT source.
