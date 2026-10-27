# GPU Renderer (OpenGL 2.1 / GLES 2.0)

Optional hardware-accelerated rendering backend alongside the existing software rasterizer. The GL code is always compiled in (on platforms with GL headers). The user selects between Software and OpenGL at runtime via the Display menu. Vulkan is a future goal (not in scope for this work).

When OpenGL is active, the whole world is rendered through it: exterior terrain, decors, models and sprites, and the isometric interior brick layer, models and world sprites. The 8-bit `Log` path remains for UI, text and menus, then composites over the GL frame.

**Scope:** OpenGL 2.1 for desktop, OpenGL ES 2.0 for Android. Vulkan is a future goal (not in scope for this work).

## Goals

- Bilinear/trilinear texture filtering to reduce the blocky aliased look of the SW rasterizer's nearest-neighbor sampling.
- GPU-accelerated 3D polygon rasterization (the CPU no longer fills scanlines for 3D geometry).
- Keep the software rasterizer always available as a runtime fallback (user picks in Display menu).
- Target broad hardware compatibility: older desktop GPUs (GL 2.1) and Android (GLES 2.0).

## Architecture

### Where the GL path plugs in

All 3D geometry flows through `Fill_Poly()` (`LIB386/pol_work/POLY.CPP:143`). The GL backend intercepts here:

```
Fill_Poly(type, color, n, points)
  ├─ if (GPURendererActive) → GL_RenderTriangleList(type, color, n, points)
  └─ else                   → existing SW dispatch (Fill_Saut_Normal[])
```

`GPURendererActive` is a runtime global toggled by the Display menu. Both the SW and GL paths are always compiled and available.

### Rendering flow (GL mode)

```
Game logic (SOURCES/)
  │
  ├─ 3D geometry: Fill_Poly() → GL backend
  │   ├─ Collects triangles into batch
  │   ├─ Uploads atlas/CLUT/palette textures on change
  │   ├─ Renders via GLSL shader
  │   └─ Writes to GL default framebuffer (depth + color)
  │
  ├─ World sprites/extras: ScaleSprite GL quads into the FBO
  └─ UI/text/menu: AffGraph(), text, menu → writes to Log (8-bit indexed)
      └─ Composited as a 2D overlay quad on top of the GL scene
```

The key insight: every world pixel is rasterized by GL, including the isometric interior. UI and text still use the indexed `Log` buffer and are composited as a textured quad.

### Isometric interior

Interior worlds do not use the exterior perspective terrain path. `AffGrille` clears the scene FBO and draws each brick tile as a textured GL quad (`DrawBrickGL` in `SOURCES/GRILLE.CPP`). The brick RLE walk is reused through `AffGraph` into a scratch bitmap on first use and cached; the visible tile is then sampled from a GL texture. `GL_SetWorldGL` lets the interior object polygons enter `GL_RenderTriangleList` without a terrain depth buffer. In that world pass `ProjectListIso` has already replaced each body point's Z with its painter sort key, so `GetZO3`/`GetZO4` derive a view depth from that key and the object's camera-relative position. Opaque body batches then write FBO depth, which lets the body-model lines, spheres and sprite overlays depth-test against the body instead of x-raying through it. Lines and spheres take the same isometric depth and draw into the FBO; the Log overlay remains UI.

`DrawOverBrick*` keeps foreground bricks above models, but it must restore the background, not repaint the selected brick. The software `CopyMask` copies the pristine background through the brick mask; a foreground brick can share its mask area with a brick painted later in the base order, so drawing the selected brick's texture would bring that buried brick back over the visible one. The GL path mirrors `CopyMask`: `OverlayBegin`/`OverlayReplayBase` re-walk the base brick order inside the object's clip window, `OverlayMark` masks that reconstruction per brick, and `OverlayFlush` draws the result into the FBO as one indexed patch. The patch uses nearest sampling through `GL_SetSpriteFilterOverride` because its pixels are already composed at screen resolution, and `GL_SetSpriteUploadAlways` forces each reused tile buffer through a fresh texture upload.

The interior full flip copies `Screen` over `Log` every frame, which would erase an open dialogue strip. `SpeakAnimation` snapshots the strip before the scene pass and `AffScene` restores it after the copy, so the typewriter text, border and shade survive the full redraw in both mono and stereo.

Interior palettes are selected by `ChoicePalette`; it now syncs the video/GL palette LUT as well, which is what keeps indexed brick and sprite textures on the room's palette rather than the previous scene's.

`DisplayTexFilter` is also honoured by gameplay sprite quads, including the decoded brick tiles: nearest, palette-resolved bilinear, Scale2x, and xBR are selected per draw through the CLUT shader. Filtering happens on palette indices and resolves through the palette texture, so the result does not interpolate raw palette indices. Sprite quads carry `RepMask = 0xFFFF` so the filter neighbourhood clamps at the texture edge instead of wrapping to the opposite edge of the tile; without that clamp, xBR drew a dark seam at every brick boundary. Chroma-keyed neighbour taps resolve to the centre texel instead of palette[0], so a filtered edge does not darken against the transparent key.

### GL 2.1 vs GLES 2.0

| Aspect | GL 2.1 (desktop) | GLES 2.0 (Android) |
|--------|-------------------|---------------------|
| Headers | `<GL/gl.h>` | `<GLES2/gl2.h>` |
| Texture format (8-bit) | `GL_R8` + swizzle | `GL_LUMINANCE` |
| Shader version | `#version 120` | `#version 100` |
| Index type | `GL_UNSIGNED_SHORT` | `GL_UNSIGNED_SHORT` |
| FBO | `GL_ARB_framebuffer_object` | Core |
| Context creation | SDL3 `SDL_GL_CreateContext` | SDL3 (handles EGL internally) |

SDL3 manages the EGL context on Android when `SDL_WINDOW_OPENGL` is set. No manual EGL code needed.

## Texture system

### Atlas pages (4 pages, each 256×256, 8-bit indexed)

| Page | Variable | Loaded | Used for |
|------|----------|--------|----------|
| Ground | `GroundTexture` | Per island | Terrain tiles (mutated in-place for animated water) |
| Object decor | `ObjTexture` | Per island | Static scenery (buildings, props) |
| Actor/hero | `BufferTexture` | Once globally | Heroes, NPCs, enemies |
| Sky/sea | `SkySeaTexture` | Per sky switch | Sky + sea halves |

Source: `SOURCES/MEM.CPP:245-255`, `SOURCES/3DEXT/LOADISLE.CPP:176-179`, `SOURCES/PERSO.CPP:3250`.

### How PtrMap is set (per-polygon)

`PtrMap` is a global pointer set immediately before each `Fill_Poly()` call. It points into one of the four atlas pages, often offset into a sub-region:

- **Terrain:** `PtrMap = GroundTexture` (set once before the terrain loop, `TERRAIN.CPP:844`)
- **Sky:** `PtrMap = SkySeaTexture + 128` (the sky half, `DRAWSKY.CPP:39`)
- **Sea:** `PtrMap = SkySeaTexture` (the sea half, `DRAWSKY.CPP:88`)
- **Objects:** `PtrMap = ObjPtrMap + (textureInfo & 0xFFFF)` (per-polygon offset, `AFF_OBJ.CPP:443`)
- **Actors:** `PtrMap = BufferTexture` (set globally, `OBJECT.CPP:5783`)

### RepMask (UV wrapping mask)

`RepMask` encodes texture tile size as `(height-1 << 8) | (width-1)`:

| Value | Size | Usage |
|-------|------|-------|
| `0xFFFF` | 256×256 | Full atlas (terrain, objects, actors) |
| `0x7F7F` | 128×128 | Sky, sea |
| `0x3F3F` | 64×64 | Menu effects (plasma/fire) |
| Per-polygon | Varies | Object body textures (from `PtrTextures[]` table) |

Object polygons have individual RepMask values from the body's texture handle table (`AFF_OBJ.CPP:442`). This means RepMask can change per-polygon within a batch.

The GL backend uploads only the RepMask-sized region of each page (`GL_BindAtlasFor` in `LIB386/GL/RENDER_GL.CPP`): a page lives in memory at a 256-byte row stride, so a (w,h) sub-rect upload must set `GL_UNPACK_ROW_LENGTH` to 256 — tight packing scrambles every row past the first (diagonal-stripe artifacts). On GLES 2.0 builds without `GL_UNPACK_ROW_LENGTH` the upload falls back to the full 256×256 page.

### Sea and sky clipping

`Draw_Poly` (`SOURCES/3DEXT/DRAWSKY.CPP`) clips sea and sky quads against the near and far planes, then against the screen rectangle. The screen clip interpolates the UV and camera depth in the same perspective space as the batch shader (UV/Z and 1/Z), so a corner that projects far outside S16 no longer shifts the visible texture. `SkySeaQuadVisible` rejects a quad only when a circumscribed sphere around it is wholly off-screen or entirely past the far plane; a failed projection or a near quad is kept for the exact clipper. `LBA2_NO_SEA_CLAMP=1` and `LBA2_NO_SEA_CULL=1` restore the previous paths for comparison.

### Animated water tiles

`GroundTexture` is mutated in place every 250 ms: `DoAnimatedPolys()` copies 32×32 blocks from the sky/sea page into specific locations in it (`TERRAIN.CPP`). The mutation sets `AtlasTextureDirty` and adds the page to the renderer's dirty-page list, along with `BufferTexture` while `DoTextureAnimation()` is active, because that rebakes effects there between ticks. At the next textured polygon the GL renderer marks each listed page stale: its 2D cache entry and any array layers that sample it are re-uploaded at flush time by `GL_BindAtlasFor` or `GL_ArrayBind`, while the other cached pages stay resident. A pointer inside a 64 KB page also invalidates the page-base entry, because the inventory draws `BufferTexture` while its effects live at offsets. A null page, or more distinct pages than the list holds, falls back to bumping the global content version and re-uploading every cached page.

The menu fire bar calls `DoTextureAnimation()` and then `FixePlasmaPtrMap()`; the latter names the menu plasma page and `BufferTexture`, which holds the scanned fire/plasma effects. The plasma page and the effect page can be refreshed independently in one interval.

### UV coordinates

Texture coordinates are 8.8 fixed-point. The filler extracts the integer part via `>> 8` and applies `RepMask`:

```cpp
// POLYTZG.CPP:107-108
U32 texU = (curMapU >> 8) & (RepMask & 0xFF);
U32 texV = (curMapV >> 8) & ((RepMask >> 8) & 0xFF);
```

## Palette and CLUT system

### Palette

256-entry RGB palette. Set via `SetVideoPalette()` (`LIB386/SVGA/SDL.CPP:169`). The `paletteLUT[256]` array maps each index to ARGB8888 (`SDL.CPP:37`). In the GL path, this becomes a 256×1 texture.

### CLUT (Gouraud shading lookup)

The CLUT is a 256×256 table: `[gouraud_row][texel] → palette_color_index`. It encodes the combination of Gouraud lighting intensity and texture color.

- Base pointer: `PtrCLUTFog` (loaded from HQR resource, `SOURCES/AMBIANCE.CPP:472`)
- Active region: `PtrCLUTGouraud` (points into `PtrCLUTFog`, 4096 bytes = 16 rows × 256 cols)
- Updated by `SetCLUT(U32 defaultline)` (`LIB386/pol_work/POLY.CPP:272-295`)

The CLUT index formula: `gouraudIndex = ((gouraudValue >> 8) & 0xFF00) | texel`

In the GL renderer, the full 64 KB fog table is uploaded once as a 256×256 LUMINANCE texture. The gouraud value's high byte (0–255) indexes the row directly — no need to track `SetCLUT` changes, since the shader samples the correct 16-row block within the full table.

The upload is generation-tracked: `InitPalette()` bumps `CLUTGeneration` when it selects a new XPL fog table, and `GL_UploadCLUT` re-sends the table only when that generation or `PtrCLUTFog` changes. `SetVideoPalette()`/`SetVideoPaletteCol()` similarly bump a palette generation so the 1 KB palette texture is uploaded only when the palette actually changes.

### Fill_Logical_Palette

A 256-byte copy of the current palette row (`POLY.CPP:251`), updated by `SetCLUT()`. Used by the SW rasterizer for palette lookups. The GL path uses the CLUT texture instead.

## GLSL shader

The CLUT fragment shader replicates the SW CLUT pipeline: atlas → CLUT → palette.
Upload the full 64 KB fog table as a 256×256 LUMINANCE texture. The gouraud
value's high byte indexes the correct row directly (no need to track `SetCLUT`
changes).

```glsl
// Vertex shader (GL 2.1 / GLES 2.0 compatible)
attribute vec2 aPos;
attribute vec2 aTexCoord;
attribute float aLight;
attribute float aZ;
uniform vec2 uScreenScale; // 2.0/resX, 2.0/resY
varying vec2 vTexCoord;
varying float vLight;

void main() {
    float cx = aPos.x * uScreenScale.x - 1.0;
    float cy = 1.0 - aPos.y * uScreenScale.y;
    gl_Position = vec4(cx, cy, aZ / 65535.0, 1.0);
    vTexCoord = aTexCoord;
    vLight = aLight;
}

// Fragment shader
uniform sampler2D uAtlas;   // 256×256, LUMINANCE (8-bit indexed)
uniform sampler2D uCLUT;    // 256×256, LUMINANCE (full fog table)
uniform sampler2D uPalette; // 256×1, BGRA (paletteLUT)
uniform vec2 uRepMask;      // (widthMask, heightMask) for UV wrapping

varying vec2 vTexCoord;
varying float vLight;

void main() {
    vec2 tc = vTexCoord / 256.0;
    float wrappedU = mod(tc.x, uRepMask.x + 1.0);
    float wrappedV = mod(tc.y, uRepMask.y + 1.0);
    vec2 atlasUV = vec2((wrappedU + 0.5) / 256.0, (wrappedV + 0.5) / 256.0);
    float texel = texture2D(uAtlas, atlasUV).r * 255.0;
    float gouraudRow = floor(vLight / 256.0);
    vec2 clutUV = vec2((texel + 0.5) / 256.0, (gouraudRow + 0.5) / 256.0);
    float palIdx = texture2D(uCLUT, clutUV).r * 255.0;
    vec2 palUV = vec2((palIdx + 0.5) / 256.0, 0.5);
    gl_FragColor = texture2D(uPalette, palUV);
}
```

**Filtering ON:** Set `GL_LINEAR` on `uAtlas` (bilinear) or `GL_LINEAR_MIPMAP_LINEAR` (trilinear with mipmaps).
**Filter OFF:** Set `GL_NEAREST` on `uAtlas` (matches original SW look).

## Z-buffer

Use GL's hardware depth buffer instead of the software `PtrZBuffer`:

- Pass `Struc_Point::Pt_ZO` (normalized 16-bit Z) as vertex Z
- `glEnable(GL_DEPTH_TEST)` with `GL_LEQUAL`
- Clear depth buffer each frame: `glClear(GL_DEPTH_BUFFER_BIT)`

The software `PtrZBuffer` (U16 array, `POLY.H:85`) is not used in GL mode.

## Fog

Fog is implemented via the CLUT system — different CLUT rows encode fog-tinted colors. The GL shader handles this naturally through the `uCLUT` texture. `SetFog()` (`POLY.CPP:297`) sets fog distance parameters per cube, which affect which CLUT row is selected via `SetCLUT()`.

No separate GL fog equation needed — the CLUT already encodes the fog blend.

## Geometry batching

Triangles are collected into a VBO and flushed when:

- Atlas page changes (different `PtrMap` base pointer) — only for polys on the 2D path; textured polys inside the decor/object windows sample the texture array instead and skip this trigger
- The batch's class (array vs 2D) changes — non-sampling polys inherit the open batch's class so bodies mixing textured and flat polys do not flip programs per poly
- Alpha mode changes (only TRANS batches enable blending; mode, chroma and perspective differences branch per fragment in the shader, so they share a batch)
- Fog-smooth mode enters or leaves the batch (type 24 pins the depth func to `GL_ALWAYS` under NZW)
- Z-buffer/NZW fill flags change
- Batch buffer full (~4096 vertices)
- End of frame (before present)

The CLUT row is *not* a flush trigger: `SetCLUT()` changes it per terrain tile (fog distance), and it rides per vertex instead — in the material word's high byte for CLUT-textured polygons (mode 0), in the RepMask vertex field for gouraud-table ones (mode 3) — so tiles share draw calls.

### Static terrain VBO path (stereo)

Exterior terrain bypasses the per-tile batch in stereo. `AffichageTerrainZBuf` uploads each cube once as a `T_GL_TerrainVertex` VBO (cube-local position, 8.8 atlas UV, light, RepMask, material, and the tile's top-left corner for the fog row) and draws the whole cube with one call through `s_terrainVertSrc`. The vertex shader applies `MatriceWorld` and the cube-local `CameraXr/Yr/Zr` and reproduces `LongProjectPoint3D`; `w` is the camera-space depth, so the GPU clips behind the eye, and the fragment shader discards the sub-`NearClip` sliver and anything past `ClipZFar`. The batch shader's screen-linear interpolation comes from `w = 1`, so the terrain shader premultiplies UV and depth by `w` and divides by the interpolated `w` in the fragment stage.

The cache is keyed by cube `(x, y)`, holds 16 entries with LRU eviction, and is dropped when `LoadIsland()` or `InitSeaCube()` replaces the per-cube HQR data. Animated water/flower tiles remain in the VBO as their current frame and are also projected on the CPU into `PtrAniPoly` so the animated-poly pass is unchanged. The path is active only for a stereo eye unless `LBA2_GPU_TERRAIN=1` forces it; `LBA2_GPU_TERRAIN=0` is a kill switch and `LBA2_TERRAIN_NOCULL=1` disables culling for comparison. The mono desktop path stays on the batch.

### Texture array path

Textured polys queued inside the decor/object windows (`GL_ArrayWindowBegin/End`) stamp `0x8000|layer` into the RepMask vertex field and sample a `GL_TEXTURE_2D_ARRAY` on unit 3 through a GLSL-3 variant of the CLUT program. Each layer holds the poly's `(PtrMap, w, h)` region *tiled* to 256×256, so the shader's `mod`-256 UV wrap reproduces the RepMask wrap for UVs pushed outside the region (raw 256×256 windows would sample the neighbouring texture block's bytes instead). This is what lets the decor pass — which interleaves a different page offset per poly group — merge into one batch instead of flushing per offset; `LBA2_NO_ARRAY=1` forces the 2D path for comparison.

### Decor VBO streams

The decor pass collects its screen-space triangles into CPU streams (the same 16-byte batch vertex layout) and draws them at `GL_DecorBatchEnd` instead of flushing the shared batch whenever a decor changes atlas class, alpha or depth-write mode. Array-path decors sample the texture array. Non-array decors use the legacy CLUT program, carry their 2D atlas page and mask, and are drawn before the array stream and static decor VBOs so their ordering matches the shared batch. Fog-smooth decors bind that page and use the shared batch's `GL_ALWAYS` depth function under NZW. Both kinds split into depth-writing opaque, depth-test-only opaque and alpha streams; alpha and non-Z-buffered polygons do not write depth. `LBA2_NO_DECOR_BATCH=1` keeps the per-state batch path for comparison.

The array program is linked separately and Adreno assigns it different attribute slots from the legacy CLUT program, so both layouts are queried and the vertex-array spec is re-issued when the current program changes.

Eye 1 reuses eye 0’s decor screen list and sort order per loaded cube: the pass runs once for the current cube and once for each visible horizon cube, so one snapshot is kept for each cube for the current stereo pair (`GL_GetStereoFrameSeq()`). The cube/decor-count/pair guard prevents a stale list at a cube boundary, and the lateral reject’s frame margins cover the IPD shift.

The GL batch interpolates texture coordinates affinely for the original POLYTEXT/POLYGTEX types (8–15), matching the software filler. On a large, oblique wall this is visible as texture swim/warp, especially in VR. The opt-in Display → Graphics → Perspective toggle stamps the material perspective bit for those types, reusing the batch’s existing per-vertex `1/W` varyings (the same path already used by Z-textured types 16–24). It is off by default; the software/CPU renderer and the default affine look are unchanged.

### Static decor VBO

Opaque, non-animated decor bodies bypass the per-frame CPU projection. The polygon handlers replay each body once in model space through `DecorVBORecord`, which captures positions, point/face normals, 8.8 UVs, the raw body colour and the 2D page `RepMask`. A `T_GL_DecorVertex` VBO is cached per `(body, beta)`; the vertex shader applies the decor's beta rotation (baked at build), its world translation (a per-draw uniform) and `MatriceWorld`, then reproduces `LongProjectPoint3D` and the camera-space light. Flat and flat-textured shading select their palette/CLUT row from that light exactly as the CPU handlers do, and the per-decor fog row rides in the material word.

The VBO is split into per-texture-page groups; each group samples the 2D atlas page cache with its `RepMask` via `GL_BindAtlasFor`, so the static path never touches the texture array. The cache is keyed by `(body, beta)` and dropped on cube/island changes. Bodies that are transparent, use alpha/stipple polygons, environment mapping, lines/spheres, or whose normals do not match their points stay on the per-frame CPU stream path. The static path is used in mono and stereo GL rendering, not only in a stereo eye. `LBA2_NO_DECOR_VBO=1` disables it and `LBA2_DECOR_NOCULL=1` disables back-face culling for comparison.

### Vertex format

```c
struct GLVertex {
    S16 x, y;          // screen-space position (from Struc_Point::Pt_XE/YE)
    U16 texU, texV;    // texture coordinates (from Struc_Point::Pt_MapU/V)
    U16 light;         // Gouraud intensity (from Struc_Point::Pt_Light)
    U16 zo;            // normalized Z (from Struc_Point::Pt_ZO)
    U16 repMask;       // UV wrap mask; gouraud-table mode 3 reuses it for the CLUT row;
                       // 0x8000|layer marks an array-path poly (see Texture array path)
    U16 material;      // low byte: shading mode + alpha/chroma/persp flags,
                       // high byte: flat colour, shadow level, or CLUT row (mode 0)
};  // 16 bytes per vertex
```

## 2D/3D compositing

In GL mode, the 3D scene renders directly to the GL framebuffer. 2D elements (sprites, HUD, menu text) still write to the `Log` buffer via the existing SW path (`AffGraph`, text rendering). These are composited on top as a fullscreen textured quad:

1. Render 3D scene to GL framebuffer (with depth)
2. Convert `Log` 8-bit → ARGB (palette LUT, same as current present path)
3. Upload as a second GL texture
4. Draw fullscreen quad with alpha blending (transparent colors become see-through)

The palette-indexed → ARGB conversion for 2D only happens once per frame, on a much smaller workload than the current full-frame conversion.

## Build system

### Compile-time control

The GL renderer is always compiled on platforms with GL headers. A simple CMake option allows excluding it for minimal builds:

```cmake
option(GPURENDERER "Enable GPU renderer (OpenGL 2.1 / GLES 2.0)" ON)
```

When `OFF`, the GL code is not compiled and the `GPURendererActive` global is always false. When `ON` (default), both SW and GL paths are available and the user picks at runtime.

Compile definition when enabled:

```cmake
if(GPURENDERER)
    target_compile_definitions(... PRIVATE USE_GPURENDERER)
endif()
```

### New directory

```
LIB386/GL/
  RENDER_GL.CPP    — GL context init, texture management, triangle batching
  RENDER_GL.H      — public interface
  SHADER_GLSL.H    — embedded GLSL source (GL 2.1 + GLES 2.0 compatible)
  CMakeLists.txt   — conditionally added when GPURENDERER=ON
```

## Display menu integration

### New menu entries

Added to `DisplayMenu[]` in `GAMEMENU.CPP:146`:

```
DisplayMenu[] = {
    0, 6,        // bumped from 4 to 6 entries
    260, 0,
    0, 26,                        // Back
    0, MENU_ID_RESOLUTION,        // Resolution: 1280x720
    0, MENU_ID_DISPLAY_VSYNC,     // Vsync ON
    0, MENU_ID_DISPLAY_FULLSCREEN,// Fullscreen
    0, MENU_ID_DISPLAY_RENDERER,  // Renderer: Software / OpenGL    ← NEW
    0, MENU_ID_DISPLAY_TEXFILTER, // Texture Filter ON / OFF        ← NEW
};
```

**Renderer toggle** (`MENU_ID_DISPLAY_RENDERER`): cycles between Software and OpenGL. The GL context is created lazily on first switch to OpenGL. Switching back to Software tears down the GL context. This is a heavier operation than a VSync toggle — similar to `Res_Switch()`.

**Texture filter toggle** (`MENU_ID_DISPLAY_TEXFILTER`): flips `GL_NEAREST` ↔ `GL_LINEAR` on the atlas texture. **Only active when the OpenGL renderer is selected** — when Software is active, this row is greyed out (non-selectable). Switching the renderer to OpenGL enables it; switching back to Software greys it out again. Trivial to apply at runtime.

The shipped option now cycles four values (`DisplayTexFilter`: 0 = Nearest, 1 = Bilinear, 2 = Scale2x, 3 = xBR Textures). Values 2 and 3 apply their documented edge rules **per texture, inside the 3D sampler** — the CLUT shader gathers the atlas neighbourhood of the sampled texel and runs the rules on it (`clutScale2x` / `clutXbr`, plus `arrayScale2x` / `arrayXbr` in the texture-array variant), so geometry edges, the 2D UI and text render fully native and only world textures are smoothed. The values map 1:1 onto the shader's `uBilinear` uniform (0 = nearest, 1 = bilinear, 2 = Scale2x, 3 = xBR). The 2D overlay also keeps its own present-shader Scale2x pass for mode 2 (`s_fragSrc` / `s_presentIndexedFragSrc`), with exact texel-centre sampling so the indexed path compares raw palette indices.

**Scale2x (2)** compares raw atlas indices — exact integers — and replaces the centre texel with a neighbour only when the two guards of the sample's sub-quadrant agree; a replaced texel costs a single CLUT lookup. **xBR Textures (3)** runs Hyllian's xBR-lv2 rules (MIT, CORNER_C variant — CPU twin `XbrUpscale2x` in `LIB386/SVGA/XBR.CPP`, host-tested in `tests/xbr/`): edge weights come from unlit palette colors, and the blend applies those weights to the CLUT-lit neighbours, so gouraud lighting stays a per-texel lookup and only the smoothing changes. xBR keeps boundaries crisper than bilinear; Scale2x changes texels only where the documented rules fire, staying closest to the original art.

The Renderer row is always selectable. The Texture Filter row's selectability depends on the current renderer. Both rows are always visible in the menu (when `GPURENDERER=ON` at build time). If the user's GPU doesn't support GL 2.1 / GLES 2.0, the OpenGL option is shown but produces an error message on selection.

### Menu labels

New entries in `MENU_LABELS.H` / `MENU_LABELS.CPP`:

```c
MENU_LABEL_RENDERER_SOFTWARE,   // "Software"
MENU_LABEL_RENDERER_OPENGL,     // "OpenGL"
MENU_LABEL_TEXFILTER_ON,        // Texture Filter ON
MENU_LABEL_TEXFILTER_OFF,       // Texture Filter OFF
```

Each needs 6-language translations (EN, FR, DE, ES, IT, PT). The existing `tests/menu_labels` test automatically validates new entries.

### Config persistence

New keys in `lba2.cfg`:

```
Renderer=0          ; 0=Software, 1=OpenGL
TextureFilter=1     ; 0=Off (nearest), 1=On (bilinear)
```

Both are runtime settings, not build-dependent. Read in `ReadConfigFile()` (`PERSO.CPP:2314`), written in `WriteConfigFile()` (`PERSO.CPP:2478`).

## Risks and open questions

1. **Animated water dirty-tracking:** resolved by the named-page dirty flag in "Animated water tiles". A caller that sets `AtlasTextureDirty` without a page still takes the conservative full-cache upload path.

2. **Per-polygon RepMask for objects:** Object body polygons have individual RepMask from the texture handle table. This forces a batch flush per-polygon for objects, or RepMask must be encoded as a vertex attribute and handled in the shader via `mod()`.

3. **Dithered shading visual difference:** The SW renderer has dithered Gouraud (checkerboard pattern). GL bilinear filtering produces a smoother look. This is a deliberate improvement, but it's a visual difference from the original.

4. **Screen-space vertices:** Geometry arrives pre-projected (`Struc_Point` has screen-space X/Y). The GL vertex shader is a passthrough for position — all interesting work is in the fragment shader (CLUT lookup, palette mapping). This keeps the shader simple.

5. **SDL3 GL context + SDL_Renderer coexistence:** When GL mode is active, the `SDL_Renderer` (used for 2D compositing overlays) and the raw GL context need to share the window. Options: render 3D to an FBO then blit through SDL, or manage the GL context directly and use SDL only for window/input.

6. **GLES 2.0 `GL_LUMINANCE` vs desktop `GL_R8`:** Desktop GL deprecated `GL_LUMINANCE` but it still works. Could use `GL_LUMINANCE` everywhere for simplicity, or `#ifdef` branch for correctness.

7. **Batch flush frequency:** Objects with many individually-textured polygons (each with different PtrMap offset + RepMask) used to flush the batch per offset — measured at 240 atlas flushes per frame on an island view. The texture array path (see Geometry batching) merged these: 46 flushes total on the same view, pixel-identical output.

## Implementation phases

### Phase 1: Build system + renderer abstraction

- ✅ Add `GPURENDERER` CMake option (default ON)
- ✅ Create `LIB386/GL/` directory with CMakeLists.txt (conditional on GPURENDERER)
- ✅ Add `GPURendererActive` global and renderer dispatch at `Fill_Poly()`
- ✅ Define `GL_RenderTriangleList()` stub
- ✅ `USE_GPURENDERER` compile definition
- ✅ Verify SW builds are unaffected when GPURENDERER=OFF

### Phase 2: GL context + basic triangle rendering

- ✅ SDL3 GL context creation in `WINDOW.CPP` (lazily, on first switch to OpenGL)
  - `SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true)`
  - `SDL_GL_CreateContext()`
- ✅ Basic GL state setup (`glViewport`, `glClearColor`, etc.)
- ✅ Minimal vertex + fragment shader (texture sampling only, no CLUT)
- ✅ Render a single test triangle with atlas texture
- ✅ Verify triangle appears correctly on screen

### Phase 3: CLUT-based Gouraud shading + Z-buffer + batching

- ✅ CLUT shader: atlas (256×256 LUMINANCE) → CLUT (256×256 LUMINANCE, full 64 KB fog table) → palette (256×1 BGRA)
- ✅ Upload full fog table as CLUT texture; gouraud value's high byte indexes correct rows directly
- ✅ Upload `paletteLUT` as 256×1 BGRA texture (palette index 0 has alpha=0 for chroma key)
- ✅ Pass `Pt_Light` as vertex attribute; `RepMask` as uniform for UV wrapping via `mod()`
- ✅ Pass `Pt_ZO` as vertex Z with `GL_DEPTH_TEST` / `GL_LEQUAL` (16-bit depth buffer requested at context creation)
- ✅ Batch triangles into a dynamic VBO; flush on RepMask/type change or batch full (4096 verts)
- ✅ End-of-frame flush in `GL_Present` before 2D overlay quad
- ✅ Triangle fan decomposition for N-gons (N≥3)
- ✅ `Fill_Poly` falls back to SW when `GL_RenderTriangleList` returns 0
- ✅ `GL_GetPaletteLUT()` getter in SDL.CPP exposes palette data to the GL renderer

### Phase 4: Z-buffer + Fog

- ✅ Implemented in Phase 3 (GL_DEPTH_TEST + Pt_ZO)
- ✅ Fog works via CLUT rows (no separate GL fog needed)

### Phase 5: Texture atlas management

- ✅ Atlas pointer tracked; re-uploaded to GL when `PtrMap` changes (single 256×256 page at a time)
- ✅ `RepMask` UV wrapping handled via per-vertex attribute in fragment shader (no batch-flush needed)
- ✅ Handle animated water tile mutations in `GroundTexture` — the named dirty page in `DoAnimatedPolys()` forces a re-upload of that page
- ✅ Per-polygon RepMask handling for object body textures (via `Pt_RepMask` vertex attribute)

### Phase 6: Full 3D scene rendering

- ✅ `GL_RenderTriangleList()` receives all textured `Fill_Poly()` calls (types 8–24)
- ✅ Non-textured polygon types 0/1 (SOLID/FLAT), 2/3 (TRANS/TRAME), 4/5 (GOURAUD/DITHER), 6/7 (GOURAUD_TABLE/DITHER_TABLE) in GL
- ✅ TRANS: flat-shaded polygon with alpha blending (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`)
- ✅ TRAME: flat-shaded polygon with stipple discard pattern (50% coverage checkerboard)
- ✅ Batch triangles and flush on state changes (type, color, batch full)
- ✅ End-of-frame flush before present

### Phase 7: 2D/3D compositing + filtering

- ✅ Composite `Log` buffer (2D sprites, UI text) as overlay quad on top of GL 3D scene
- ✅ Bilinear filtering toggle (`GL_NEAREST` ↔ `GL_LINEAR` on overlay texture)
- ❌ Trilinear filtering option — not viable with CLUT pipeline (indexed atlas can't be mipmapped correctly)
- ✅ Menu entries for Renderer and Texture Filter toggles (6 languages)
- ✅ Config read/write for `Renderer` and `TextureFilter` keys

### Phase 8: GLES 2.0 + Android + runtime switching

- ✅ GLES 2.0 compatibility: `#version 100`, `GL_LUMINANCE`, header differences
- ✅ Runtime SW ↔ GL switching (tear down GL context, reinit SW present path)
- ❌ Android GL context via SDL3 (EGL handled internally)
- ❌ Android-specific: fullscreen creation, MTE staging buffer bypass
- ❌ Verify on Android arm64-v8a and armeabi-v7a

## Files to modify/create

| File | Change |
|------|--------|
| `CMakeLists.txt` (root) | Add `GPURENDERER` option (default ON) |
| `LIB386/GL/RENDER_GL.CPP` | GL renderer: present shader, CLUT shader, batch rendering, texture management |
| `LIB386/GL/RENDER_GL.H` | Public interface (Init, Shutdown, Resize, Present, RenderTriangleList) |
| `LIB386/GL/CMakeLists.txt` | `gly` library, `USE_GPURENDERER` define |
| `LIB386/CMakeLists.txt` | Conditionally add `GL/` subdirectory |
| `LIB386/pol_work/POLY.CPP` | GL dispatch with SW fallback in `Fill_Poly()` |
| `LIB386/SVGA/SDL.CPP` | `GL_GetPaletteLUT()` getter; GL present path |
| `LIB386/H/SVGA/VIDEO.H` | Declare `GL_GetPaletteLUT()` |
| `LIB386/SYSTEM/WINDOW.CPP` | Lazy GL context creation, window recreation |
| `SOURCES/GAMEMENU.CPP` | Add renderer + filter menu entries |
| `SOURCES/MENU_LABELS.H` | Add new label enums |
| `SOURCES/MENU_LABELS.CPP` | Add 6-language translations |
| `SOURCES/PERSO.CPP` | Read/write new config keys, boot-time GL init |
| `SOURCES/C_EXTERN.H` | Declare `GPURendererActive`, `DisplayTexFilter` |
| `SOURCES/LBA2.CFG` | Add default `Renderer` and `TextureFilter` keys |
| `SOURCES/EMBEDDED_CFG_WRITE.CPP` | Include new keys in embedded default |
