# GPU Renderer (OpenGL 2.1 / GLES 2.0)

Optional hardware-accelerated 3D rendering backend alongside the existing software rasterizer. The GL code is always compiled in (on platforms with GL headers). The user selects between Software and OpenGL at runtime via the Display menu. Vulkan is a future goal (not in scope for this work).

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
  └─ 2D sprites/UI: AffGraph(), text, menu → writes to Log (8-bit indexed)
      └─ Composited as a 2D overlay quad on top of the GL 3D scene
```

The key insight: 3D is fully GPU-rendered, 2D elements (sprites, HUD text, menu) still use the existing SW path writing to `Log`, then get composited as a textured quad.

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

### Animated water tiles

`GroundTexture` is mutated in-place every 250ms: 32×32 blocks are copied from `SkySeaTexture` into specific locations in `GroundTexture` (`TERRAIN.CPP:1019-1053`). The GL backend must detect this and re-upload the dirty region via `glTexSubImage2D`, or re-upload the whole atlas (64KB, negligible).

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

The CLUT is a 16×256 table: `[intensity_level][texel] → palette_color_index`. It encodes the combination of Gouraud lighting intensity and texture color.

- Base pointer: `PtrCLUTFog` (loaded from HQR resource, `SOURCES/AMBIANCE.CPP:472`)
- Active region: `PtrCLUTGouraud` (points into `PtrCLUTFog`, 4096 bytes = 16 rows × 256 cols)
- Updated by `SetCLUT(U32 defaultline)` (`LIB386/pol_work/POLY.CPP:272-295`)

`SetCLUT` is called **per object** based on distance fog level. The `defaultline` parameter selects which 256-byte row of the CLUT to use. `PtrCLUTGouraud` is recomputed as `PtrCLUTFog + ((defaultline << 8) & 0xF000)`.

### Fill_Logical_Palette

A 256-byte copy of the current palette row (`POLY.CPP:251`), updated by `SetCLUT()`. Used by the SW rasterizer for palette lookups. The GL path uses the CLUT texture instead.

## GLSL shader

The fragment shader replicates the CLUT-based lighting:

```glsl
// Vertex shader (GL 2.1 / GLES 2.0 compatible)
attribute vec2 aPos;
attribute vec2 aTexCoord;
attribute float aLight;
attribute float aZ;
varying vec2 vTexCoord;
varying float vLight;

void main() {
    gl_Position = vec4(aPos, aZ, 1.0);
    vTexCoord = aTexCoord;
    vLight = aLight;
}

// Fragment shader
uniform sampler2D uAtlas;   // 256×256, 8-bit (R8 or LUMINANCE)
uniform sampler2D uCLUT;    // 16×256 (intensity × texel → palette index)
uniform sampler2D uPalette; // 256×1 (palette index → ARGB)
uniform vec2 uRepMask;

varying vec2 vTexCoord;
varying float vLight;

void main() {
    vec2 uv = mod(vTexCoord, uRepMask);
    float texel = texture2D(uAtlas, uv).r;
    float palIdx = texture2D(uCLUT, vec2(texel, vLight / 16.0)).r;
    gl_FragColor = texture2D(uPalette, vec2(palIdx, 0.5));
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

- Atlas page changes (different `PtrMap` base pointer)
- CLUT changes (different fog/brightness level via `SetCLUT`)
- RepMask changes (different UV wrapping)
- Batch buffer full (~4096 vertices)
- End of frame (before present)

### Vertex format

```c
struct GLVertex {
    S16 x, y;          // screen-space position (from Struc_Point::Pt_XE/YE)
    U16 texU, texV;    // texture coordinates (from Struc_Point::Pt_MapU/V)
    U16 light;         // Gouraud intensity (from Struc_Point::Pt_Light)
    U16 zo;            // normalized Z (from Struc_Point::Pt_ZO)
};  // 12 bytes per vertex
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

1. **Animated water dirty-tracking:** `GroundTexture` is mutated in-place (32×32 copies from `SkySeaTexture`). Need to detect changes and re-upload. Options: dirty-flag per block, CRC check, or unconditional re-upload of the whole atlas each frame (64KB — negligible).

2. **Per-polygon RepMask for objects:** Object body polygons have individual RepMask from the texture handle table. This forces a batch flush per-polygon for objects, or RepMask must be encoded as a vertex attribute and handled in the shader via `mod()`.

3. **Dithered shading visual difference:** The SW renderer has dithered Gouraud (checkerboard pattern). GL bilinear filtering produces a smoother look. This is a deliberate improvement, but it's a visual difference from the original.

4. **Screen-space vertices:** Geometry arrives pre-projected (`Struc_Point` has screen-space X/Y). The GL vertex shader is a passthrough for position — all interesting work is in the fragment shader (CLUT lookup, palette mapping). This keeps the shader simple.

5. **SDL3 GL context + SDL_Renderer coexistence:** When GL mode is active, the `SDL_Renderer` (used for 2D compositing overlays) and the raw GL context need to share the window. Options: render 3D to an FBO then blit through SDL, or manage the GL context directly and use SDL only for window/input.

6. **GLES 2.0 `GL_LUMINANCE` vs desktop `GL_R8`:** Desktop GL deprecated `GL_LUMINANCE` but it still works. Could use `GL_LUMINANCE` everywhere for simplicity, or `#ifdef` branch for correctness.

7. **Batch flush frequency:** Objects with many individually-textured polygons (each with different PtrMap offset + RepMask) will flush the batch frequently. Profile to see if this is a real problem — the batch is small (4096 verts) and GPU draw calls are cheap on modern hardware, but may matter on very old GLES 2.0 devices.

## Implementation phases

### Phase 1: Build system + renderer abstraction

- Add `GPURENDERER` CMake option (default ON)
- Create `LIB386/GL/` directory with CMakeLists.txt (conditional on GPURENDERER)
- Add `GPURendererActive` global and renderer dispatch at `Fill_Poly()`
- Define `GL_RenderTriangleList()` stub
- `USE_GPURENDERER` compile definition
- Verify SW builds are unaffected when GPURENDERER=OFF

### Phase 2: GL context + basic triangle rendering

- SDL3 GL context creation in `WINDOW.CPP` (lazily, on first switch to OpenGL)
  - `SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true)`
  - `SDL_GL_CreateContext()`
- Basic GL state setup (`glViewport`, `glClearColor`, etc.)
- Minimal vertex + fragment shader (texture sampling only, no CLUT)
- Render a single test triangle with atlas texture
- Verify triangle appears correctly on screen

### Phase 3: CLUT-based Gouraud shading

- Implement the full CLUT shader (atlas + CLUT + palette textures)
- Upload `PtrCLUTGouraud` region as 16×256 GL texture
- Upload `paletteLUT` as 256×1 GL texture
- Pass `Pt_Light` as vertex attribute
- Detect `SetCLUT()` changes and re-upload CLUT texture
- Verify Gouraud-shaded textured polygons match SW output

### Phase 4: Z-buffer + Fog

- Enable `GL_DEPTH_TEST` with `GL_LEQUAL`
- Pass `Pt_ZO` as vertex Z
- Clear depth buffer each frame
- Verify depth sorting matches SW Z-buffer behavior
- Fog works via CLUT rows (no separate GL fog needed)

### Phase 5: Texture atlas management

- Upload all four 256×256 atlas pages as GL textures
- Detect `PtrMap` page changes and switch atlas binding
- Handle `RepMask` UV wrapping (pass as uniform, `mod()` in shader)
- Handle animated water tile mutations in `GroundTexture`
- Per-polygon RepMask handling for object body textures

### Phase 6: Full 3D scene rendering

- Wire `GL_RenderTriangleList()` to receive all `Fill_Poly()` calls
- Handle all polygon types (flat, textured, Gouraud, fog, Z-buffer, NZW)
- Batch triangles and flush on state changes
- End-of-frame flush before present
- Verify complete 3D scenes render correctly

### Phase 7: 2D/3D compositing + filtering

- Composite `Log` buffer (2D sprites, UI text) as overlay quad on top of GL 3D scene
- Bilinear filtering toggle (`GL_NEAREST` ↔ `GL_LINEAR` on atlas texture)
- Trilinear filtering option (`glGenerateMipmap` + `GL_LINEAR_MIPMAP_LINEAR`)
- Menu entries for Renderer and Texture Filter toggles
- Config read/write for `Renderer` and `TextureFilter` keys

### Phase 8: GLES 2.0 + Android + runtime switching

- GLES 2.0 compatibility: `#version 100`, `GL_LUMINANCE`, header differences
- Android GL context via SDL3 (EGL handled internally)
- Android-specific: fullscreen creation, MTE staging buffer bypass
- Runtime SW ↔ GL switching (tear down GL context, reinit SW present path)
- Verify on Android arm64-v8a and armeabi-v7a

## Files to modify/create

| File | Change |
|------|--------|
| `CMakeLists.txt` (root) | Add `GPURENDERER` option (default ON) |
| `LIB386/GL/` (new directory) | GL renderer implementation |
| `LIB386/CMakeLists.txt` | Conditionally add `GL/` subdirectory |
| `LIB386/pol_work/POLY.CPP` | Add GL dispatch branch in `Fill_Poly()` |
| `LIB386/SYSTEM/WINDOW.CPP` | Lazy GL context creation |
| `LIB386/SVGA/SDL.CPP` | Bypass palette-scan in GL mode |
| `SOURCES/GAMEMENU.CPP` | Add renderer + filter menu entries |
| `SOURCES/MENU_LABELS.H` | Add new label enums |
| `SOURCES/MENU_LABELS.CPP` | Add 6-language translations |
| `SOURCES/PERSO.CPP` | Read/write new config keys |
| `SOURCES/C_EXTERN.H` | Declare `GPURendererActive`, `DisplayTexFilter` |
| `SOURCES/LBA2.CFG` | Add default `Renderer` and `TextureFilter` keys |
| `SOURCES/EMBEDDED_CFG_WRITE.CPP` | Include new keys in embedded default |
