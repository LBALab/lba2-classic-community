#include <GL/RENDER_GL.H>

#include <3D/CAMERA.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/WINDOW.H>
#include <SYSTEM/OPENXR.H>
#include <SVGA/DIRTYBOX.H>

#ifdef USE_GPURENDERER

/* ── GL headers ─────────────────────────────────────────────────────────── */
#if defined(__ANDROID__)
#ifdef __ANDROID__
#include <GLES3/gl3.h> /* blitFramebuffer + pixel-pack PBOs (depth readback) */
#else
#include <GLES2/gl2.h>
#endif
#else
#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL_opengl.h>
#endif

#include <SDL3/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <FILEIO/SAVEPNG.H>

/* SMAA lookup textures and shader core (generated / vendored, see the
 * attribution at the top of each). */
#include "SMAA_TEX.H"
#include "SMAA_HLSL.H"

/* ── External palette getter (defined in SDL.CPP) ───────────────────────── */
extern "C" const U32 *GL_GetPaletteLUT(void);

/* ── External CLUT / texture globals (from POLY.CPP) ────────────────────── */
extern "C" {
extern PTR_U8 PtrCLUTFog;
extern PTR_U8 PtrCLUTGouraud;
extern PTR_U8 PtrMap;
extern S32 DisplayBrightness;
extern S32 DisplayContrast;
extern S32 DisplaySaturation;
extern S32 RepMask;
extern U8 Fill_Flag_ZBuffer;
extern U8 Fill_Flag_NZW;
extern U32 Fill_Fog_Factor;
extern U32 Fill_ScaledFogNear;
}

/* ── External dirty flag (from VAR_EXT.CPP) ─────────────────────────────── */
extern "C" S32 AtlasTextureDirty;

/* Temporary perf instrumentation: why the last poly fell back to SW
   (1=renderer down, 2=type range, 3=no zbuffer flag, 4=NZW without
   terrain, 5=too few points, 6=menu plasma mask, 7=unprojectable vertex;
   0=accepted). */
extern "C" S32 GL_LastRejectReason;
S32 GL_LastRejectReason = 0;

/* ── External texture filter toggle (from GLOBAL.CPP) ────────────────────── */
extern "C" S32 DisplayTexFilter;
extern "C" S32 DisplayAntiAlias;
extern "C" S32 DisplayStereo3D;

/* Temporary perf instrumentation: draw calls flushed since GL_BeginFrame */
extern "C" S32 GL_BatchFlushCount;
S32 GL_BatchFlushCount = 0;

/* ── Shader sources (GL 2.1 / GLES 2.0 compatible) ─────────────────────── */

#if defined(__ANDROID__)
/* highp is mandatory here: the fragment shader feeds 16-bit values (material
   word, gouraud light, Z, RepMask) through mod()/floor()/divide — mediump's
   11-bit mantissa cannot represent those integers exactly, which on Adreno
   showed up as grayscale output, wrong texture pages and wrong colours.
   Desktop GLSL has no default precision limit and was never affected. */
#define GLSL_VERT_HEADER "#version 100\nprecision highp float;\n"
#define GLSL_FRAG_HEADER "#version 100\nprecision highp float;\n"
#else
#define GLSL_VERT_HEADER "#version 110\n"
#define GLSL_FRAG_HEADER "#version 110\n"
#endif

/* ── Present shader (2D overlay quad) ───────────────────────────────────── */

/* User image adjustments (Display menu), applied at composite time in every
 * present-side shader.  The transform is affine in the color channels, so
 * drawing every fragment of a frame with it — the scene quad and the blended
 * overlay quad alike — composes exactly through alpha blending. */
#define GLSL_IMAGE_ADJUST                                                    \
    "uniform float uBrightness;\n"                                           \
    "uniform float uContrast;\n"                                             \
    "uniform float uSaturation;\n"                                           \
    "vec3 ImageAdjust(vec3 c) {\n"                                           \
    "    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, uSaturation);\n" \
    "    c = (c - 0.5) * uContrast + 0.5 + uBrightness;\n"                   \
    "    return c;\n"                                                        \
    "}\n"

static const char *s_vertSrc =
    GLSL_VERT_HEADER
    "attribute vec2 aPos;\n"
    "attribute vec2 aTexCoord;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

static const char *s_fragSrc =
    GLSL_FRAG_HEADER
    GLSL_IMAGE_ADJUST
    "uniform sampler2D uTex;\n"
    "uniform vec2 uTexel;\n"
    "uniform int uFilter;\n" /* 0 = nearest, 1 = bilinear, 2 = Scale2x */
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    vec4 result;\n"
    "    if (uFilter == 2) {\n"
    /* Scale2x (the documented 2x edge rules) evaluated per output pixel:
     * the fragment's position inside its source texel picks the output
     * sub-quadrant (E0..E3), and the rule replaces it with the matching
     * neighbour only when the two neighbours guarding that corner agree.
     * Sampling at exact texel centres with NEAREST keeps the comparisons
     * exact. */
    "        vec2 t = vTexCoord / uTexel;\n"
    "        vec2 f = fract(t);\n"
    "        vec2 c = (floor(t) + 0.5) * uTexel;\n"
    "        vec4 e = texture2D(uTex, c);\n"
    "        vec4 b = texture2D(uTex, c + vec2(0.0, -uTexel.y));\n"
    "        vec4 d = texture2D(uTex, c + vec2(-uTexel.x, 0.0));\n"
    "        vec4 rf = texture2D(uTex, c + vec2(uTexel.x, 0.0));\n"
    "        vec4 h = texture2D(uTex, c + vec2(0.0, uTexel.y));\n"
    "        result = e;\n"
    "        if (!all(equal(b, h)) && !all(equal(d, rf))) {\n"
    "            if (f.x < 0.5 && f.y < 0.5) {\n"
    "                result = all(equal(d, b)) ? d : e;\n"
    "            } else if (f.x >= 0.5 && f.y < 0.5) {\n"
    "                result = all(equal(b, rf)) ? rf : e;\n"
    "            } else if (f.x < 0.5) {\n"
    "                result = all(equal(d, h)) ? d : e;\n"
    "            } else {\n"
    "                result = all(equal(h, rf)) ? rf : e;\n"
    "            }\n"
    "        }\n"
    "    } else {\n"
    "        result = texture2D(uTex, vTexCoord);\n"
    "    }\n"
    "    gl_FragColor = vec4(ImageAdjust(result.rgb), result.a);\n"
    "}\n";

/* ── CLUT shader (3D Gouraud-textured polygons) ─────────────────────────── */
/* Replicates the SW CLUT pipeline:
   1. Sample atlas  → 8-bit palette index (texel)
   2. CLUT lookup   → texel lit by gouraud intensity
   3. Palette lookup → ARGB color
   The CLUT is the full 64 KB fog table (256×256). SetCLUT selects a
   16-row block within it, but the gouraud value's high byte indexes
   into the correct rows directly. */

static const char *s_clutVertSrc =
    GLSL_VERT_HEADER
    "attribute vec2 aPos;\n"
    "attribute vec2 aTexCoord;\n"
    "attribute float aLight;\n"
    "attribute float aZ;\n"
    "attribute float aRepMask;\n"
    "attribute float aMaterial;\n"
    "uniform vec2 uScreenScale;\n" /* 2.0/resX, 2.0/resY */
    /* Perspective-correct interpolation: the SW rasterizer
       (POLYTZF/POLYTEXZ, Z-textured types 16-24) interpolates
       MapU/MapV/ZBuf over W = 1/Z.  w=1.0 in clip space makes varyings
       interpolate affinely in screen space, so premultiply by 1/Z here
       and divide in the fragment shader.  aZ is GET_ZO(Z0) (proportional
       to Z0); the constant of proportionality cancels in the fragment
       division.  uPerspCorrect selects the corrected pair; otherwise the
       raw affine varyings are used (types 8-15, SW POLYTEXT/POLYGTEX). */
    "varying vec2 vTexCoord;\n"
    "varying vec2 vTexCoordOverW;\n"
    "varying float vInvW;\n"
    "varying float vLight;\n"
    "varying float vRepMask;\n"
    "varying float vZ;\n"
    "varying float vZOverW;\n"
    "varying float vMaterial;\n"
    "void main() {\n"
    "    float cx = aPos.x * uScreenScale.x - 1.0;\n"
    "    float cy = 1.0 - aPos.y * uScreenScale.y;\n"
    "    gl_Position = vec4(cx, cy, 1.0 - aZ / 65535.0, 1.0);\n"
    "    float invW = 1.0 / max(aZ, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    vTexCoordOverW = aTexCoord * invW;\n"
    "    vInvW = invW;\n"
    "    vLight = aLight;\n"
    "    vRepMask = aRepMask;\n"
    "    vZ = aZ;\n"
    "    vZOverW = aZ * invW;\n"
    "    vMaterial = aMaterial;\n"
    "}\n";

static const char *s_clutFragSrc =
    GLSL_FRAG_HEADER
    "uniform sampler2D uAtlas;\n"   /* 256×256 LUMINANCE (or sprite page) */
    "uniform vec2 uAtlasScale;\n"   /* texture dims for texel-space UVs */
    "uniform sampler2D uCLUT;\n"    /* 256×256 LUMINANCE (fog table) */
    "uniform sampler2D uPalette;\n" /* 256×1 BGRA (paletteLUT) */
    "uniform float uCLUTBaseRow;\n" /* legacy row offset (now per vertex) */
    "uniform float uScaledFogNear;\n" /* Fill_ScaledFogNear (fog-smooth distance, mode 5) */
    "uniform float uFogRowScale;\n"   /* Fill_Fog_Factor / 65536 (fog-smooth distance, mode 5) */
    "uniform int uBilinear;\n"      /* 0=nearest 1=bilinear CLUT sampling */
    "varying vec2 vTexCoord;\n"
    "varying vec2 vTexCoordOverW;\n"
    "varying float vInvW;\n"
    "varying float vLight;\n"
    "varying float vRepMask;\n"
    "varying float vZ;\n"
    "varying float vZOverW;\n"
    "varying float vMaterial;\n"
    /* Per-vertex material word (flat across each polygon): low byte =
       polyMode (bits 0-2), alphaMode (bits 3-4), chromaKey (bit 5),
       perspCorrect (bit 6); high byte = flat colour / shadow level.
       All three vertices of a triangle carry the same word, so the
       derived values are constant across the fragment. */
    /* Look up a palette index through the CLUT, returning the final RGBA
       color.  `palIdx` is the raw atlas texel (palette index) and
       `gouraudRow` the CLUT row selected by the gouraud intensity. */
    "vec4 clutLookup(float palIdx, float gouraudRow) {\n"
    "    vec2 cuv = vec2((palIdx + 0.5) / 256.0,\n"
    "                     (gouraudRow + 0.5) / 256.0);\n"
    "    float lit = texture2D(uCLUT, cuv).r * 255.0;\n"
    "    return texture2D(uPalette, vec2((lit + 0.5) / 256.0, 0.5));\n"
    "}\n"
    /* Bilinear CLUT sampling: sample 4 atlas texels, CLUT each one, then
       interpolate the resulting RGBA colors.  This avoids the artifact
       where hardware bilinear on palette indices produces garbage colors. */
    "vec4 bilinearCLUT(vec2 tc, float widthMask, float heightMask,\n"
    "                   float gouraudRow, float chroma) {\n"
    "    float u = mod(tc.x, widthMask + 1.0);\n"
    "    float v = mod(tc.y, heightMask + 1.0);\n"
    "    float iu = floor(u);\n"
    "    float iv = floor(v);\n"
    "    float fu = u - iu;\n"
    "    float fv = v - iv;\n"
    "    /* Wrap each of the 4 texel positions */\n"
    "    vec2 p00 = vec2(mod(iu, widthMask + 1.0),\n"
    "                     mod(iv, heightMask + 1.0));\n"
    "    vec2 p10 = vec2(mod(iu + 1.0, widthMask + 1.0),\n"
    "                     mod(iv, heightMask + 1.0));\n"
    "    vec2 p01 = vec2(mod(iu, widthMask + 1.0),\n"
    "                     mod(iv + 1.0, heightMask + 1.0));\n"
    "    vec2 p11 = vec2(mod(iu + 1.0, widthMask + 1.0),\n"
    "                     mod(iv + 1.0, heightMask + 1.0));\n"
    "    /* Sample atlas → CLUT → palette for each texel */\n"
    "    float t00 = texture2D(uAtlas, (p00 + 0.5) / 256.0).r * 255.0;\n"
    "    float t10 = texture2D(uAtlas, (p10 + 0.5) / 256.0).r * 255.0;\n"
    "    float t01 = texture2D(uAtlas, (p01 + 0.5) / 256.0).r * 255.0;\n"
    "    float t11 = texture2D(uAtlas, (p11 + 0.5) / 256.0).r * 255.0;\n"
    /* Chroma key: discard if nearest texel (p00) is index 0.
       Matches SW nearest-neighbor behaviour for INCRUST types. */
    "    if (chroma > 0.5 && t00 < 0.5) discard;\n"
    "    vec4 c00 = clutLookup(t00, gouraudRow);\n"
    "    vec4 c10 = clutLookup(t10, gouraudRow);\n"
    "    vec4 c01 = clutLookup(t01, gouraudRow);\n"
    "    vec4 c11 = clutLookup(t11, gouraudRow);\n"
    "    return mix(mix(c00, c10, fu), mix(c01, c11, fu), fv);\n"
    "}\n"
    "void main() {\n"
    "    vec4 color;\n"
    "    float alphaOverride = 0.0;\n"
    /* Per-vertex material word, flat across the polygon. */
    "    float mFlags = mod(vMaterial, 256.0);\n"
    "    int mMode = int(mod(mFlags, 8.0) + 0.5);\n"
    "    float mAlpha = floor(mod(mFlags, 32.0) / 8.0 + 0.5);\n"
    "    float mChroma = floor(mod(mFlags, 64.0) / 32.0 + 0.5);\n"
    "    float mPersp = floor(mod(mFlags, 128.0) / 64.0 + 0.5);\n"
    "    float mColor = floor(vMaterial / 256.0);\n"
    /* Select perspective-corrected or affine texture/Z varyings.
       The persp bit is set for Z-textured types (16-24), where the SW
       rasterizer divides MapU/MapV/ZBuf by W = 1/Z; types 8-15
       (SW POLYTEXT/POLYGTEX) interpolate affinely. */
    "    vec2 tc = mix(vTexCoord, vTexCoordOverW / vInvW, mPersp);\n"
    "    float vZsel = mix(vZ, vZOverW / vInvW, mPersp);\n"
     "    if (mMode == 0) {\n"
     /* Textured: atlas → CLUT → palette */
     "        tc = tc / 256.0;\n"
     "        float widthMask = mod(vRepMask, 256.0);\n"
     "        float heightMask = floor(vRepMask / 256.0);\n"
     /* SW init: Fill_CurGouraudMin = (Pt_Light << 8) + 0x8000.
        The +128 (0x80) rounding bias shifts the CLUT row by 1 in some
        cases — same bias applied in polyMode 2.  The CLUT base row rides
        in the material's high byte (see GL_RenderTriangleList): SetCLUT()
        changes it per terrain tile, and carrying it per vertex lets
        tiles share one draw call. */
     "        float gouraudRow = floor((vLight + 128.0) / 256.0) + mColor;\n"
    "        if (uBilinear != 0) {\n"
    "            color = bilinearCLUT(tc, widthMask, heightMask, gouraudRow, mChroma);\n"
     "        } else {\n"
     "            float wrappedU = mod(tc.x, widthMask + 1.0);\n"
     "            float wrappedV = mod(tc.y, heightMask + 1.0);\n"
     "            vec2 atlasUV = vec2((floor(wrappedU) + 0.5) / 256.0,\n"
     "                                (floor(wrappedV) + 0.5) / 256.0);\n"
     "            float texel = texture2D(uAtlas, atlasUV).r * 255.0;\n"
    /* Chroma key: atlas texel 0 = transparent (skipped by SW rasterizer
       at the Log buffer level).  Discard before CLUT so the fog table
       cannot remap index 0 to an opaque value.
       Only INCRUST types (12-15, 20-23) have chroma key; non-INCRUST
       types (8-11, 16-19, 24) write all texels including index 0. */
    "            if (mChroma > 0.5 && texel < 0.5) discard;\n"
    "            color = clutLookup(texel, gouraudRow);\n"
    "        }\n"
    "    } else if (mMode == 1) {\n"
    /* Flat solid: single palette color */
    "        float palIdx = mColor;\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    } else if (mMode == 2) {\n"
    /* Gouraud (types 4/5): palette = (color + (light+128)/256).
       SW init: Fill_CurGouraudMin = (Pt_Light << 8) + 0x8000 + (color << 16).
       The +0x8000 rounding bias means the palette index is
       (color + (Pt_Light + 128) / 256), not (color + Pt_Light / 256). */
    "        float palIdx = mod(mColor + floor((vLight + 128.0) / 256.0), 256.0);\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    } else if (mMode == 4) {\n"
    /* Texture without CLUT (types 8 and 12):
       SW Filler_Texture / Filler_TextureChromaKey writes texel as palette
       index directly (*line = texel).  No CLUT lookup — the atlas texel
       IS the palette index.  Chroma key controlled by mChroma. */
     "        tc = tc / 256.0;\n"
     "        float widthMask = mod(vRepMask, 256.0);\n"
     "        float heightMask = floor(vRepMask / 256.0);\n"
      "        float wrappedU = mod(tc.x, widthMask + 1.0);\n"
      "        float wrappedV = mod(tc.y, heightMask + 1.0);\n"
      "        vec2 atlasUV = vec2((floor(wrappedU) + 0.5) / uAtlasScale.x,\n"
      "                            (floor(wrappedV) + 0.5) / uAtlasScale.y);\n"
      "        float palIdx = texture2D(uAtlas, atlasUV).r * 255.0;\n"
    /* Chroma key: atlas texel 0 = transparent.
       Only INCRUST types have chroma key. */
    "        if (mChroma > 0.5 && palIdx < 0.5) discard;\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    } else if (mMode == 5) {\n"
    /* Texture Z Fog Smooth (type 24, e.g. sea/sky):
       SW Filler_TextureZFogSmoothZBuf / ...NZW use a fixed CLUT base
       (PtrCLUTFog + 12*256) plus a per-pixel distance-based fog row,
       IGNORING Pt_Light entirely.  Replicating the SW math:
       zBufVal = interpolated Pt_ZO (the batch aZ)
       fogDist = max(zBufVal - Fill_ScaledFogNear, 0)
       fogRow  = (fogDist * Fill_Fog_Factor) >> 8, masked & 0xF000
       CLUT index = PtrCLUTFog + 12*256 + fogRow + texel
       As a CLUT row: (12*256 + fogRow)/256 = 12 + fogRow/256, where
       fogRow/256 = ((fogDist * Fill_Fog_Factor) >> 16) & 0xF0.
       uFogRowScale = Fill_Fog_Factor / 65536, so
        floor(zDist * uFogRowScale) = (fogDist * Fill_Fog_Factor) >> 16. */
     "        tc = tc / 256.0;\n"
     "        float widthMask = mod(vRepMask, 256.0);\n"
    "        float heightMask = floor(vRepMask / 256.0);\n"
     "        float wrappedU = mod(tc.x, widthMask + 1.0);\n"
     "        float wrappedV = mod(tc.y, heightMask + 1.0);\n"
     "        vec2 atlasUV = vec2((floor(wrappedU) + 0.5) / 256.0,\n"
     "                            (floor(wrappedV) + 0.5) / 256.0);\n"
     "        float texel = texture2D(uAtlas, atlasUV).r * 255.0;\n"
    /* Chroma key: type 24 is non-INCRUST (bit 2 clear), so index 0 is
       written unconditionally — no discard here. */
     "        float fogDist = max(vZsel - uScaledFogNear, 0.0);\n"
    "        float fogRow16 = floor(fogDist * uFogRowScale);\n"
    "        float clutRow = 12.0 + floor(mod(fogRow16, 256.0) / 16.0) * 16.0;\n"
    "        color = clutLookup(texel, clutRow);\n"
    "    } else if (mMode == 6) {\n"
    /* Ground shadow (SW ShadeBoxBlk replacement): constant gray source
       combined with GL_DST_COLOR/GL_ZERO blending multiplies the scene
       color by (15 - level) / 15 — the linear counterpart of the SW
       fade-CLUT row (15 - level)/15, which maps each palette entry to its
       darker ramp value.  The vertex colour byte holds the level. */
    "        float shade = (15.0 - mColor) / 15.0;\n"
    "        color = vec4(shade, shade, shade, 1.0);\n"
    "    } else {\n"
    /* Gouraud table (types 6/7): CLUT lookup, color = column.
       Same +128 rounding bias as polyMode 0.  The CLUT base row rides in
       the vertex RepMask field (see GL_RenderTriangleList). */
    "        float gouraudRow = floor((vLight + 128.0) / 256.0) + vRepMask;\n"
    "        vec2 clutUV = vec2((mColor + 0.5) / 256.0,\n"
    "                           (gouraudRow + 0.5) / 256.0);\n"
    "        float palIdx = texture2D(uCLUT, clutUV).r * 255.0;\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    }\n"
    /* TRAME stipple: discard every other fragment (50% coverage) */
    "    if (mAlpha > 1.5) {\n"
    "        if (mod(floor(gl_FragCoord.x) + floor(gl_FragCoord.y), 2.0) < 1.0)\n"
    "            discard;\n"
    "    }\n"
    "    if (mAlpha > 0.5) {\n"
    "        color.a = 0.5;\n"
    "    }\n"
    "    gl_FragColor = color;\n"
    "}\n";

/* ── GL state — present shader ──────────────────────────────────────────── */

static GLuint s_program = 0;
static GLuint s_vbo = 0;
static GLuint s_fboVbo = 0; /* non-flipped UV quad for FBO→screen draw */
static GLuint s_texture = 0;
static GLint s_uTexLoc = -1;
static GLint s_uTexelLoc = -1;  /* present overlay: 1/width, 1/height */
static GLint s_uFilterLoc = -1; /* present overlay: 0 nearest, 1 bilinear, 2 Scale2x */
static GLint s_aPosLoc = -1;
static GLint s_aTexCoordLoc = -1;
static U32 s_texWidth = 0;
static U32 s_texHeight = 0;
static S32 s_initialized = 0;

/* ── Indexed present (GPU palette expansion) ─────────────────────────────── */

/* Present the 8-bit Log overlay without a CPU palette pass: the raw index
 * bytes are uploaded as a NEAREST-filtered LUMINANCE texture and expanded
 * through a 256x1 palette texture in the shader. Index 0 expands to the
 * palette's entry 0, which PresentFrame forces to alpha 0 so the 3D FBO
 * content shows through — identical output to the CPU path. */
static GLuint s_presentIndexedProgram = 0;
static GLuint s_logIndexTex = 0;
static U32 s_logIndexTexW = 0;
static U32 s_logIndexTexH = 0;
static GLuint s_lutTex = 0;
static GLint s_idx_aPos = -1;
static GLint s_idx_aTexCoord = -1;
static GLint s_idx_uIndices = -1;
static GLint s_idx_uPalette = -1;
static GLint s_idx_uTexel = -1;
static GLint s_idx_uFilter = -1;
/* User image adjustments (Display menu); affine transform applied in every
 * present-side shader so it composes exactly through alpha blending. */
static GLint s_adj_uBrightness = -1;
static GLint s_adj_uContrast = -1;
static GLint s_adj_uSaturation = -1;
static GLint s_idxAdj_uBrightness = -1;
static GLint s_idxAdj_uContrast = -1;
static GLint s_idxAdj_uSaturation = -1;
/* A frame whose Log overlay is screen-space HUD only is identical for both
 * stereo eyes.  Eye 0 scans and uploads it; eye 1 then reuses the texture
 * and the empty/not-empty result instead of walking the same 3 MB again.
 * Moving 2D content (world sprites, dialogue) disables the reuse.
 * GL_SetStereoEye invalidates the cache on every transition away from eye 1. */
static S32 s_overlayScanValid = 0;
static S32 s_overlayEmptyCached = 0;
static S32 s_overlayEye0Dynamic = 0;
static GLint s_fxaaAdj_uBrightness = -1;
static GLint s_fxaaAdj_uContrast = -1;
static GLint s_fxaaAdj_uSaturation = -1;

static void GL_UploadImageAdjust(GLint uB, GLint uC, GLint uS) {
    glUniform1f(uB, (GLfloat)DisplayBrightness / 100.0f);
    glUniform1f(uC, 1.0f + (GLfloat)DisplayContrast / 100.0f);
    glUniform1f(uS, 1.0f + (GLfloat)DisplaySaturation / 100.0f);
}

/* The present composite applies the display sliders on top of the 3D scene
 * texture.  When they are neutral the composite is a pure copy, so a
 * present that only needs the scene can skip it. */
static S32 GL_ImageAdjustNeutral(void) {
    return DisplayBrightness == 0 && DisplayContrast == 0 &&
           DisplaySaturation == 0;
}

/* Double-wide draws the HUD into the scene target; its final pair-to-
 * swapchain pass applies the display transform to the whole composite.
 * Per-eye and mono composites still apply it while each layer is drawn. */
static S32 s_overlayImageAdjust = 1;

static void GL_UploadNeutralImageAdjust(GLint uB, GLint uC, GLint uS) {
    glUniform1f(uB, 0.0f);
    glUniform1f(uC, 1.0f);
    glUniform1f(uS, 1.0f);
}

static const char *s_presentIndexedFragSrc =
    GLSL_FRAG_HEADER
    GLSL_IMAGE_ADJUST
    "uniform sampler2D uIndices;\n"
    "uniform sampler2D uPalette;\n"
    "uniform vec2 uTexel;\n"
    "uniform int uFilter;\n" /* 0 = nearest, 1 = bilinear, 2 = Scale2x */
    "varying vec2 vTexCoord;\n"
    "float idxAt(vec2 uv) {\n"
    "    return floor(texture2D(uIndices, uv).r * 255.0 + 0.5);\n"
    "}\n"
    "void main() {\n"
    /* NEAREST filtering makes v exactly i/255; rebuild the texel index and
     * step to the palette texel centre the same way the CLUT shader does. */
    "    vec2 t = vTexCoord / uTexel;\n"
    "    vec2 f = fract(t);\n"
    "    vec2 c = (floor(t) + 0.5) * uTexel;\n"
    "    float e = idxAt(c);\n"
    /* Scale2x on the raw palette indices — exact integer comparisons, then
     * a single palette lookup for the winning texel. */
    "    if (uFilter == 2) {\n"
    "        float b = idxAt(c + vec2(0.0, -uTexel.y));\n"
    "        float d = idxAt(c + vec2(-uTexel.x, 0.0));\n"
    "        float r = idxAt(c + vec2(uTexel.x, 0.0));\n"
    "        float h = idxAt(c + vec2(0.0, uTexel.y));\n"
    "        if (b != h && d != r) {\n"
    "            if (f.x < 0.5 && f.y < 0.5) {\n"
    "                e = (d == b) ? d : e;\n"
    "            } else if (f.x >= 0.5 && f.y < 0.5) {\n"
    "                e = (b == r) ? r : e;\n"
    "            } else if (f.x < 0.5) {\n"
    "                e = (d == h) ? d : e;\n"
    "            } else {\n"
    "                e = (h == r) ? r : e;\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    vec4 pal = texture2D(uPalette, vec2((e + 0.5) / 256.0, 0.5));\n"
    "    gl_FragColor = vec4(ImageAdjust(pal.rgb), pal.a);\n"
    "}\n";

/* ── FXAA (Fast Approximate Anti-Aliasing) ──────────────────────────────── */
/* Post-process edge-smoothing shader applied during the present pass. */

static const char *s_fxaaFragSrc =
    GLSL_FRAG_HEADER
    GLSL_IMAGE_ADJUST
    "uniform sampler2D uTex;\n"
    "uniform vec2 uTexel;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    vec3 rgbNW = texture2D(uTex, vTexCoord + vec2(-1.0, -1.0) * uTexel).rgb;\n"
    "    vec3 rgbNE = texture2D(uTex, vTexCoord + vec2( 1.0, -1.0) * uTexel).rgb;\n"
    "    vec3 rgbSW = texture2D(uTex, vTexCoord + vec2(-1.0,  1.0) * uTexel).rgb;\n"
    "    vec3 rgbSE = texture2D(uTex, vTexCoord + vec2( 1.0,  1.0) * uTexel).rgb;\n"
    "    vec3 rgbM  = texture2D(uTex, vTexCoord).rgb;\n"
    "    vec3 luma = vec3(0.299, 0.587, 0.114);\n"
    "    float lumaNW = dot(rgbNW, luma);\n"
    "    float lumaNE = dot(rgbNE, luma);\n"
    "    float lumaSW = dot(rgbSW, luma);\n"
    "    float lumaSE = dot(rgbSE, luma);\n"
    "    float lumaM  = dot(rgbM, luma);\n"
    "    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));\n"
    "    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));\n"
    "    if (lumaMax - lumaMin <= 0.08) { gl_FragColor = vec4(ImageAdjust(rgbM), 1.0); return; }\n"
    "    vec2 dir = vec2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)),\n"
    "                    ((lumaNW + lumaSW) - (lumaNE + lumaSE)));\n"
    "    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);\n"
    "    float rcpMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);\n"
    "    dir = clamp(dir * rcpMin, vec2(-8.0), vec2(8.0)) * uTexel;\n"
    "    vec3 rgbA = 0.5 * (\n"
    "        texture2D(uTex, vTexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +\n"
    "        texture2D(uTex, vTexCoord + dir * (2.0 / 3.0 - 0.5)).rgb);\n"
    "    vec3 rgbB = rgbA * 0.5 + 0.25 * (\n"
    "        texture2D(uTex, vTexCoord + dir * -0.5).rgb +\n"
    "        texture2D(uTex, vTexCoord + dir * 0.5).rgb);\n"
    "    float lumaB = dot(rgbB, luma);\n"
    "    if (lumaB < lumaMin || lumaB > lumaMax)\n"
    "        gl_FragColor = vec4(ImageAdjust(rgbA), 1.0);\n"
    "    else\n"
    "        gl_FragColor = vec4(ImageAdjust(rgbB), 1.0);\n"
    "}\n";

static GLuint s_fxaaProgram = 0;
static GLint s_fxaa_uTex = -1;
static GLint s_fxaa_uTexel = -1;
static GLint s_fxaa_aPos = -1;
static GLint s_fxaa_aTexCoord = -1;

/* ── SMAA 1x (Subpixel Morphological Antialiasing) ──────────────────────── */
/* Three-pass post-process (edge detection -> blending weights -> neighbor
   blend) run at the present path where FXAA lives.  Requires a GLES3/GL3
   context for the SMAA_GLSL_3 shader dialect and textureLodOffset; when
   compilation fails the mode falls back to the plain draw (log says so).
   Textures and shader core are the official SMAA reference assets
   (MIT / Unlicense, attribution in SMAA_TEX.H / SMAA_HLSL.H). */
static GLuint s_smaaEdgeProgram = 0;
static GLuint s_smaaWeightProgram = 0;
static GLuint s_smaaBlendProgram = 0;
static GLuint s_smaaEdgeFbo = 0, s_smaaEdgeTex = 0;
static GLuint s_smaaWeightFbo = 0, s_smaaWeightTex = 0;
static GLuint s_smaaAreaTex = 0, s_smaaSearchTex = 0;
static U32 s_smaaW = 0, s_smaaH = 0;
static GLint s_smaaEdge_uTexel = -1, s_smaaEdge_aPos = -1,
             s_smaaEdge_aTexCoord = -1;
static GLint s_smaaWeight_uRTMetrics = -1, s_smaaWeight_aPos = -1,
             s_smaaWeight_aTexCoord = -1;
static GLint s_smaaBlend_uTexel = -1, s_smaaBlend_aPos = -1,
             s_smaaBlend_aTexCoord = -1;
static GLint s_smaaAdj_uBrightness = -1, s_smaaAdj_uContrast = -1,
             s_smaaAdj_uSaturation = -1;

static void GL_SmaaInit(void);
static void GL_SmaaTargetsCreate(U32 w, U32 h);
static void GL_SmaaPresent(S32 bX, S32 bY, S32 bW, S32 bH);

/* ── Frame cost profiler ─────────────────────────────────────────────────── */
/* No on-device profiler exists for the Android build, so the three known
 * CPU-side frame costs are accumulated around their sections and summarized
 * in the log every GL_PROF_FRAMES presented frames. */
#define GL_PROF_FRAMES 300
enum {
    GL_PROF_PRESENT = 0,
    GL_PROF_READBACK = 1,
    GL_PROF_CONVERT = 2,
    GL_PROF_FLUSH = 3,
    GL_PROF_SNAPSHOT = 4,
    GL_PROF_RESTORE = 5,
    GL_PROF_MAPWAIT = 6,
    GL_PROF_BUCKET_COUNT = 7
};
static S32 s_arrCount;           /* defined with the array manager below */
static U32 s_arrEvictions;
static U64 s_profAccumTicks[GL_PROF_BUCKET_COUNT] = {0};
static U32 s_profFrames = 0;
static U32 s_profFlushes = 0;    /* batch flushes (≈ CLUT-program draw calls) */
static U32 s_profTris = 0;       /* triangles submitted to the CLUT program */
static U32 s_profSpriteUps = 0;  /* gameplay sprite texture uploads */
static U32 s_profDepthSkips = 0; /* depth readback consumes skipped (GPU behind) */
static U32 s_profOverlayEmpty = 0; /* presents whose 2D overlay had no content */
static U32 s_profOverlayReuse = 0; /* presents that reused eye 0's overlay scan */
static U32 s_profDirectScene = 0;  /* presents that can read the scene directly */
static U32 s_flushRsnAlpha = 0;  /* throwaway flush-reason attribution */
static U32 s_flushRsnFog = 0;
static U32 s_flushRsnAtlas = 0;
static U32 s_flushRsnZbuf = 0;
static U32 s_flushRsnNzw = 0;

static double GL_ProfMs(U64 ticks, double msPerTick, U32 frames) {
    return (double)ticks * msPerTick / (double)frames;
}

static void GL_ProfFrameDone(void) {
    s_profFrames++;
    if (s_profFrames >= GL_PROF_FRAMES) {
        double msPerTick = 1000.0 / (double)SDL_GetPerformanceFrequency();
        Log_Warn("[GLPerf] frames=%u present=%.2f readback=%.2f convert=%.2f "
                 "flush=%.2f snapshot=%.2f restore=%.2f mapwait=%.2f (ms) "
                 "flushes=%.1f tris=%.0f sprUp=%.1f skips=%u "
                 "ovlEmpty=%.2f ovlReuse=%.2f direct=%.2f "
                 "rsn a=%.1f f=%.1f t=%.1f z=%.1f n=%.1f arr=%u/%u",
                 s_profFrames,
                 GL_ProfMs(s_profAccumTicks[GL_PROF_PRESENT], msPerTick,
                           s_profFrames),
                 GL_ProfMs(s_profAccumTicks[GL_PROF_READBACK], msPerTick,
                           s_profFrames),
                 GL_ProfMs(s_profAccumTicks[GL_PROF_CONVERT], msPerTick,
                           s_profFrames),
                 GL_ProfMs(s_profAccumTicks[GL_PROF_FLUSH], msPerTick,
                           s_profFrames),
                 GL_ProfMs(s_profAccumTicks[GL_PROF_SNAPSHOT], msPerTick,
                           s_profFrames),
                 GL_ProfMs(s_profAccumTicks[GL_PROF_RESTORE], msPerTick,
                           s_profFrames),
                 GL_ProfMs(s_profAccumTicks[GL_PROF_MAPWAIT], msPerTick,
                           s_profFrames),
                 (double)s_profFlushes / (double)s_profFrames,
                 (double)s_profTris / (double)s_profFrames,
                 (double)s_profSpriteUps / (double)s_profFrames,
                 s_profDepthSkips,
                 (double)s_profOverlayEmpty / (double)s_profFrames,
                 (double)s_profOverlayReuse / (double)s_profFrames,
                 (double)s_profDirectScene / (double)s_profFrames,
                 (double)s_flushRsnAlpha / (double)s_profFrames,
                 (double)s_flushRsnFog / (double)s_profFrames,
                 (double)s_flushRsnAtlas / (double)s_profFrames,
                 (double)s_flushRsnZbuf / (double)s_profFrames,
                 (double)s_flushRsnNzw / (double)s_profFrames,
                 s_arrCount, s_arrEvictions);
        s_profFrames = 0;
        for (S32 i = 0; i < GL_PROF_BUCKET_COUNT; i++) {
            s_profAccumTicks[i] = 0;
        }
        s_profFlushes = 0;
        s_profTris = 0;
        s_profSpriteUps = 0;
        s_profDepthSkips = 0;
        s_profOverlayEmpty = 0;
        s_profOverlayReuse = 0;
        s_profDirectScene = 0;
        s_flushRsnAlpha = 0;
        s_flushRsnFog = 0;
        s_flushRsnAtlas = 0;
        s_flushRsnZbuf = 0;
        s_flushRsnNzw = 0;
        s_arrEvictions = 0;
    }
}

/* ── GL state — CLUT shader ─────────────────────────────────────────────── */

static GLuint s_clutProgram = 0;
static GLuint s_clut3Program = 0; /* GLSL-3 variant sampling the layer array */
static GLint s_clut3_uAtlas = -1;
static GLint s_clut3_uCLUT = -1;
static GLint s_clut3_uPalette = -1;
static GLint s_clut3_uAtlasArr = -1;
static GLint s_clut3_uScreenScale = -1;
static GLint s_clut3_uAtlasScale = -1;
static GLint s_clut3_uCLUTBaseRow = -1;
static GLint s_clut3_uScaledFogNear = -1;
static GLint s_clut3_uFogRowScale = -1;
static GLint s_clut3_uBilinear = -1;
static U8 s_batchArray = 0; /* batch samples the layer array (RepMask 0x8000) */
static S32 s_arrayWindow = 0; /* object-pass gate: only bodies/decors bin into
                                 the array; terrain stays on the legacy 2D path
                                 (its snapshot/pool flush paths predate the
                                 array's bind-and-upload flow). */

/* Rewrite a GLSL-100/110 source into a 300 es / 130 source: mechanical
   renames only, plus an `out` color for fragment shaders.  Returns a
   malloc'd string. */
static char *GL_ShaderToV3(const char *src, S32 isVert) {
    if (src == NULL) {
        return NULL;
    }
    size_t cap = strlen(src) * 2 + 2048;
    char *out = (char *)malloc(cap);
    if (out == NULL) {
        return NULL;
    }
    const char *r = src;
    char *w = out;
    *w = '\0';
    while (*r != '\0') {
        if (strncmp(r, "#version 100", 12) == 0) {
            w += sprintf(w, "%s", "#version 300 es\n");
            r += 12;
        } else if (strncmp(r, "#version 110", 12) == 0) {
            w += sprintf(w, "%s", "#version 130\n");
            r += 12;
        } else if (isVert && strncmp(r, "attribute", 9) == 0) {
            w += sprintf(w, "%s", "in");
            r += 9;
        } else if (strncmp(r, "varying", 7) == 0) {
            w += sprintf(w, "%s", isVert ? "out" : "in");
            r += 7;
        } else if (strncmp(r, "texture2D", 9) == 0) {
            w += sprintf(w, "%s", "texture");
            r += 9;
        } else if (!isVert && strncmp(r, "gl_FragColor", 12) == 0) {
            w += sprintf(w, "%s", "lbaFragColor");
            r += 12;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    if (!isVert) {
        /* Declare the out color: after the precision statement (Android
           GLSL 100 header), or right after #version when there is none
           (desktop GLSL 110 header) — without it the renamed gl_FragColor
           has no declaration and the shader fails to compile. */
        const char *anchor = strstr(out, "precision highp float;");
        size_t pos;
        if (anchor != NULL) {
            pos = (size_t)(anchor - out) + strlen("precision highp float;");
        } else {
            const char *ver = strstr(out, "#version");
            const char *verEnd = ver ? strchr(ver, '\n') : NULL;
            if (verEnd == NULL) {
                return out;
            }
            pos = (size_t)(verEnd - out) + 1;
        }
        {
            size_t declLen = strlen("\nout vec4 lbaFragColor;");
            size_t tail = strlen(out + pos) + 1;
            memmove(out + pos + declLen, out + pos, tail);
            memcpy(out + pos, "\nout vec4 lbaFragColor;", declLen);
        }
    }
    return out;
}

static const char *s_arrayInject =
    "uniform sampler2DArray uAtlasArr;\n"
    "vec4 arraySample(vec2 tc, float rmask, float light, float mcolor,\n"
    "                 float chroma, float bilin) {\n"
    "    float layer = rmask - 32768.0;\n"
    "    float gouraudRow = floor((light + 128.0) / 256.0) + mcolor;\n"
    /* tc rides the vertex attribute's 8.8 fixed point (texel*256); fold
       through 65536 first so the division lands in [0,256) texel units. */
    "    float wu = mod(tc.x, 65536.0) / 256.0;\n"
    "    float wv = mod(tc.y, 65536.0) / 256.0;\n"
    "    if (bilin < 0.5) {\n"
    "        float texel = texture(uAtlasArr, vec3((floor(wu) + 0.5) / 256.0,\n"
    "                       (floor(wv) + 0.5) / 256.0, layer)).r * 255.0;\n"
    "        if (chroma > 0.5 && texel < 0.5) discard;\n"
    "        return clutLookup(texel, gouraudRow);\n"
    "    }\n"
    "    float iu = floor(wu);\n"
    "    float iv = floor(wv);\n"
    "    float fu = wu - iu;\n"
    "    float fv = wv - iv;\n"
    "    vec3 p00 = vec3((mod(iu, 256.0) + 0.5) / 256.0,\n"
    "                    (mod(iv, 256.0) + 0.5) / 256.0, layer);\n"
    "    vec3 p10 = vec3((mod(iu + 1.0, 256.0) + 0.5) / 256.0, p00.y, layer);\n"
    "    vec3 p01 = vec3(p00.x, (mod(iv + 1.0, 256.0) + 0.5) / 256.0, layer);\n"
    "    vec3 p11 = vec3(p10.x, p01.y, layer);\n"
    "    float t00 = texture(uAtlasArr, p00).r * 255.0;\n"
    "    float t10 = texture(uAtlasArr, p10).r * 255.0;\n"
    "    float t01 = texture(uAtlasArr, p01).r * 255.0;\n"
    "    float t11 = texture(uAtlasArr, p11).r * 255.0;\n"
    "    if (chroma > 0.5 && t00 < 0.5) discard;\n"
    "    vec4 c00 = clutLookup(t00, gouraudRow);\n"
    "    vec4 c10 = clutLookup(t10, gouraudRow);\n"
    "    vec4 c01 = clutLookup(t01, gouraudRow);\n"
    "    vec4 c11 = clutLookup(t11, gouraudRow);\n"
    "    return mix(mix(c00, c10, fu), mix(c01, c11, fu), fv);\n"
    "}\n";

/* Add the array sampler + branch to a transformed CLUT fragment source.
   Polys stamped with 0x8000|layer in RepMask sample the array; everything
   else falls through the legacy per-mode body untouched. */
static char *GL_InjectArrayBranch(char *src) {
    /* Anchors must exist in the compiled GLSL string, not in C++ comments
       between the string fragments.  The branch lands after the tc/vZsel
       declarations (it samples tc) and wraps the whole mode chain in an
       else, ending where the stipple check begins. */
    const char *mainAnchor = strstr(src, "void main() {");
    const char *vZselAnchor =
        strstr(src, "float vZsel = mix(vZ, vZOverW / vInvW, mPersp);");
    const char *stippleAnchor = strstr(src, "if (mAlpha > 1.5)");
    if (mainAnchor == NULL || vZselAnchor == NULL || stippleAnchor == NULL) {
        Log_Warn("[GL] array branch anchors missing; staying on the 2D path");
        return src;
    }
    size_t cap = strlen(src) + strlen(s_arrayInject) + 512;
    char *out = (char *)malloc(cap);
    if (out == NULL) {
        return src;
    }
    size_t preMain = (size_t)(mainAnchor - src);
    size_t preTrame = (size_t)(stippleAnchor - src);
    size_t afterMc =
        (size_t)(vZselAnchor - src) +
        strlen("float vZsel = mix(vZ, vZOverW / vInvW, mPersp);");
    char *w = out;
    memcpy(w, src, afterMc);
    w += afterMc;
    w += sprintf(w, "%s", "\n    if ((mMode == 0 || mMode == 4) && vRepMask >= 32768.0) {\n"
                          "        if (mMode == 4) {\n"
                          "            float layer = vRepMask - 32768.0;\n"
                          "            float wu4 = mod(tc.x, 65536.0) / 256.0;\n"
                          "            float wv4 = mod(tc.y, 65536.0) / 256.0;\n"
                          "            float texel4 = texture(uAtlasArr, vec3((floor(wu4) + 0.5) / 256.0, (floor(wv4) + 0.5) / 256.0, layer)).r * 255.0;\n"
                          "            if (mChroma > 0.5 && texel4 < 0.5) discard;\n"
                          "            color = texture(uPalette, vec2((texel4 + 0.5) / 256.0, 0.5));\n"
                          "        } else {\n"
                          "            color = arraySample(tc, vRepMask, vLight, mColor, mChroma, float(uBilinear));\n"
                          "        }\n"
                          "    } else {\n");
    memcpy(w, src + afterMc, preTrame - afterMc);
    w += preTrame - afterMc;
    w += sprintf(w, "%s", "    }\n");
    memcpy(w, src + preTrame, strlen(src) - preTrame);
    w += strlen(src) - preTrame;
    *w = '\0';
    free(src);

    /* inject the sampler + arraySample function before main */
    out = (char *)realloc(out, strlen(out) + strlen(s_arrayInject) + 1);
    char *mainPos = strstr(out, "void main() {");
    memmove(mainPos + strlen(s_arrayInject), mainPos,
            strlen(mainPos) + 1);
    memcpy(mainPos, s_arrayInject, strlen(s_arrayInject));
    return out;
}

static GLint s_clut_aPos = -1;
static GLint s_clut_aTexCoord = -1;
static GLint s_clut_aLight = -1;
static GLint s_clut_aZ = -1;
static GLint s_clut_aRepMask = -1;
static GLint s_clut_aMaterial = -1;
static GLint s_clut_uAtlas = -1;
static GLint s_clut_uCLUT = -1;
static GLint s_clut_uPalette = -1;
static GLint s_clut_uScreenScale = -1;
static GLint s_clut_uAtlasScale = -1;
static GLint s_clut_uCLUTBaseRow = -1;
static GLint s_clut_uScaledFogNear = -1;
static GLint s_clut_uFogRowScale = -1;
static GLint s_clut_uBilinear = -1;

/* ── GL state — textures ────────────────────────────────────────────────── */

static GLuint s_atlasTex = 0;  /* 256×256 LUMINANCE */
static GLuint s_clutTex = 0;   /* 256×256 LUMINANCE (full fog table) */
static GLuint s_paletteTex = 0; /* 256×1 BGRA */

/* ── GL state — batch buffer ─────────────────────────────────────────────── */

static const U32 BATCH_MAX_VERTS = 4096;
/* 16 bytes per vertex: XE(2) YE(2) MapU(2) MapV(2) Light(2) ZO(2)
   RepMask(2) Material(2).  The material word carries the shading mode,
   alpha/chroma/perspective bits and the flat colour so polygons that only
   differ in those can share one batch (one draw call) instead of flushing
   per polygon — the dominant driver cost on mobile GPUs. */
static const U32 BATCH_VERT_BYTES = 16;
static U8 s_batchVerts[BATCH_MAX_VERTS * BATCH_VERT_BYTES];
static U32 s_batchVertCount = 0;
static U32 s_batchTriCount = 0;
static GLuint s_batchVBO = 0;
/* Persistent stream ring: each flush fills the next slot instead of
   orphaning the whole buffer — orphan+realloc per flush costs real driver
   time on mobile GPUs once batches reach triple digits. */
#define BATCH_RING_SLOTS 64
static U32 s_batchRingSize = 0;  /* bytes */
static GLsync s_ringWrapFence = 0; /* signals the GPU drained the last ring generation */
static U32 s_batchRingOffset = 0; /* rotating upload offset into the ring */
static U32 s_batchRingBase = 0;   /* ring offset of the batch being flushed */
static U32 s_batchRingOff = 0;   /* next slot's byte offset */
static U32 s_batchRingSlot = 0;  /* current flush's byte offset */
static GLuint s_auxVBO = 0;      /* one-shot draws: shadow/sphere/shade/sprite */

/* MSAA resolve-in-tile: GL_EXT_multisampled_render_to_texture lets the tiler
 * resolve MSAA as part of native tile processing instead of a separate
 * fullscreen blit.  On Adreno (Quest) this eliminates a 2240x1260x4-sample
 * resolve pass per frame — the single largest GPU operation. */
typedef void (*PFN_GLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXT)(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level, GLsizei samples);
static PFN_GLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXT
    p_glFramebufferTexture2DMultisampleEXT = NULL;
static S32 s_msaaEXTAvailable = -1; /* -1 = not checked */
static S32 GL_MSAATextureExtAvailable(void) {
    if (s_msaaEXTAvailable < 0) {
        const char *exts = (const char *)glGetString(GL_EXTENSIONS);
        s_msaaEXTAvailable =
            (exts && strstr(exts, "GL_EXT_multisampled_render_to_texture"))
                ? 1 : 0;
        Log_Info("[GL] EXT_multisampled_render_to_texture: %s",
                 s_msaaEXTAvailable ? "yes" : "no");
    }
    return s_msaaEXTAvailable;
}
static void GL_DestroyMSAAFbo(void);
static void GL_CreateMSAAFbo(U32 w, U32 h, S32 samples);
static void GL_ResolveMSAA(void);
static void GL_LogSurfaceBudget(const char *when);
static void GL_ResolveMSAAColor(void);
static void GL_DestroyHorizon(void);
/* Material of the polygon currently being queued (low byte = mode/alpha/
   chroma/persp flags, high byte = flat colour or shadow level). */
static U16 s_currentMaterial = 0;

/* ── NZW deferred batch ──────────────────────────────────────────────────── */
/* Terrain (NZW=0, depth-writing) and animated polys (NZW=1, no-write)
 * interleave per tile in the exterior render, and a single batch would
 * flush on every alternation — ~300 driver-syncing flushes per frame.
 * The defer pool holds the other NZW state; both pools are flushed at
 * defined points with depth-writing geometry first, which gives the same
 * image because NZW=1 polys never write depth (their color contribution
 * depends only on the depth test, not on submission order relative to
 * other no-write geometry). */
static U8 s_deferVerts[BATCH_MAX_VERTS * BATCH_VERT_BYTES];
static U32 s_deferVertCount = 0;
static U32 s_deferTriCount = 0;
static U32 s_deferTypePoly = 0;
static U16 s_deferMatFlags = 0;
static const void *s_deferAtlas = NULL;
static U8 s_deferTextured = 0;
static U8 s_deferZBuffer = 0;
static U8 s_deferNZW = 0;
static U8 s_deferArray = 0; /* batch class rides the swap like the verts */
static U16 s_deferMaxMask = 0;
static const void *s_deferCLUTGouraud = NULL;
static GLfloat s_deferCLUTBaseRow = 0.0f;
static GLfloat s_deferScaledFogNear = 0.0f;
static GLfloat s_deferFogRowScale = 0.0f;

/* Region of the FBO drawn since GL_SnapshotTerrain().  The incremental
   restore only needs to erase those pixels (last frame's bodies, shadows,
   sprites) back to the snapshot background — a full-screen blit of a
   4x-SSAA framebuffer costs a large fraction of every idle frame. */
static S32 s_fboDirtyX0 = 0, s_fboDirtyY0 = 0;
static S32 s_fboDirtyX1 = -1, s_fboDirtyY1 = -1; /* empty: X1 < X0 */
static S32 s_fboDirtyActive = 0; /* 1 while a snapshot is current */

static void GL_MarkFBORegion(S32 x0, S32 y0, S32 x1, S32 y1) {
    if (!s_fboDirtyActive) {
        return;
    }
    if (s_fboDirtyX1 < s_fboDirtyX0) {
        s_fboDirtyX0 = x0;
        s_fboDirtyY0 = y0;
        s_fboDirtyX1 = x1;
        s_fboDirtyY1 = y1;
        return;
    }
    if (x0 < s_fboDirtyX0) s_fboDirtyX0 = x0;
    if (y0 < s_fboDirtyY0) s_fboDirtyY0 = y0;
    if (x1 > s_fboDirtyX1) s_fboDirtyX1 = x1;
    if (y1 > s_fboDirtyY1) s_fboDirtyY1 = y1;
}
static U16 s_currentMatFlags = 0; /* flags only — the flush trigger */
static U8 s_batchTextured = 0;    /* batch samples the atlas page */

/* ── Cached GL object state ──────────────────────────────────────────────── */
/* The flush path runs once per draw call; on GLES every redundant driver
   call costs real time on slow CPUs.  These track what the context already
   has so repeat flushes skip re-binding and re-specifying.  Anything that
   touches textures/programs/attribs outside the flush path (present quads,
   sprite/shadow/shade draws) invalidates through the sentinels below. */
static GLuint s_curBoundTex[3] = {(GLuint)-1, (GLuint)-1, (GLuint)-1};
static S32 s_curActiveUnit = -1;
static GLuint s_curProgram = (GLuint)-1;
static S32 s_clutAttribsDirty = 1; /* vertex arrays need enable + re-spec */
static GLintptr s_clutAttribsBase = -1; /* ring offset the pointers point at */
/* Last indexed present can be read as the scene alone: empty overlay, no
   present-side post-process, and a content-sized scene FBO. */
static S32 s_directScenePresent = 0;

static void GL_UseProgramCached(GLuint prog) {
    if (s_curProgram != prog) {
        glUseProgram(prog);
        s_curProgram = prog;
    }
}

/* Depth-state cache backing GL_SetDepthTest/GL_SetDepthFunc/GL_SetDepthWrite;
   -1/0 = unknown, re-issued on the next flush. */
static S32 s_depthTestOn = -1;
static GLenum s_depthFuncCur = 0;
static S32 s_depthWriteCur = -1;

static GLuint s_curArrayBuf = 0; /* 0 = untracked; aux-VBO paths rebind raw */

static void GL_BindArrayBuffer(GLuint buf) {
    if (s_curArrayBuf == buf) {
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    s_curArrayBuf = buf;
}

/* Cached blend enable, shared by the flush path and the one-shot draws
   (shadow, sphere, shade, sprite, present): re-issuing identical
   glEnable/glDisable/glBlendFunc per batch is driver overhead.  Anything
   outside this setter must call GL_InvalidateGLState(). */
static S32 s_blendOnCur = -1;
static GLenum s_blendSrcCur = 0;
static GLenum s_blendDstCur = 0;

static void GL_SetBlendFunc(S32 on, GLenum src, GLenum dst) {
    if (s_blendOnCur == on && s_blendSrcCur == src && s_blendDstCur == dst) {
        return;
    }
    if (on != 0) {
        glEnable(GL_BLEND);
        glBlendFunc(src, dst);
    } else {
        glDisable(GL_BLEND);
    }
    s_blendOnCur = on;
    s_blendSrcCur = src;
    s_blendDstCur = dst;
}

static void GL_InvalidateGLState(void) {
    s_curBoundTex[0] = (GLuint)-1;
    s_curBoundTex[1] = (GLuint)-1;
    s_curBoundTex[2] = (GLuint)-1;
    s_curActiveUnit = -1;
    s_curProgram = (GLuint)-1;
    s_clutAttribsDirty = 1;
    s_depthTestOn = -1;
    s_depthFuncCur = 0;
    s_depthWriteCur = -1;
    s_blendOnCur = -1;
    s_blendSrcCur = 0;
    s_blendDstCur = 0;
}

/* Cached depth-state setters: a full frame can submit hundreds of small
   batches, and re-issuing identical glEnable/glDepthFunc/glDepthMask calls
   per batch is measurable driver overhead on mobile GPUs.  Anything outside
   the flush path that touches depth state must call GL_InvalidateGLState()
   so the next flush re-issues it. */
/* LBA2_NO_DEPTHCACHE re-issues every depth-state call unconditionally. */
static S32 s_noDepthCache = -1;

static void GL_SetDepthTest(S32 on) {
    if (s_noDepthCache < 0) {
        s_noDepthCache = getenv("LBA2_NO_DEPTHCACHE") ? 1 : 0;
    }
    if (s_noDepthCache || s_depthTestOn != on) {
        if (on)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        s_depthTestOn = on;
    }
}

static void GL_SetDepthFunc(GLenum func) {
    if (s_noDepthCache < 0) {
        s_noDepthCache = getenv("LBA2_NO_DEPTHCACHE") ? 1 : 0;
    }
    if (s_noDepthCache || s_depthFuncCur != func) {
        glDepthFunc(func);
        s_depthFuncCur = func;
    }
}

static void GL_SetDepthWrite(S32 on) {
    if (s_noDepthCache < 0) {
        s_noDepthCache = getenv("LBA2_NO_DEPTHCACHE") ? 1 : 0;
    }
    if (s_noDepthCache || s_depthWriteCur != on) {
        glDepthMask(on ? GL_TRUE : GL_FALSE);
        s_depthWriteCur = on;
    }
}

static void GL_BindTexUnit(S32 unit, GLuint tex) {
    if (s_curActiveUnit != unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        s_curActiveUnit = unit;
    }
    if (s_curBoundTex[unit] != tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        s_curBoundTex[unit] = tex;
    }
}

static GLfloat s_ussX = -1.0f, s_ussY = -1.0f;         /* uScreenScale */
static GLfloat s_atlasScaleX = -1.0f, s_atlasScaleY = -1.0f;
static GLint s_bilinearCached = -1;
static GLfloat s_clutRowCached = -1.0f;
static GLfloat s_fogNearCached = -1.0f;
static GLfloat s_fogScaleCached = -1.0f;

/* ── Reverse palette lookup (ARGB -> palette index) ──────────────────────── */

/* paletteLUT has 256 entries.  Build a 65536-entry hash table for O(1)
   exact-match reverse lookup, keyed by (R<<16 | G<<8 | B).  On hash
   miss, fall back to a linear nearest-color scan over all 256 entries
   so that minor shader rounding does not collapse pixels to index 0. */
static U8 s_revLutHash[65536];
static S32 s_revLutHashValid = 0;
static const U32 *s_revLutPtr = NULL;

static void GL_BuildReverseLut(void) {
    const U32 *lut = GL_GetPaletteLUT();
    if (!lut) {
        s_revLutHashValid = 0;
        s_revLutPtr = NULL;
        return;
    }

    memset(s_revLutHash, 0xFF, sizeof(s_revLutHash));
    for (U32 i = 0; i < 256; i++) {
        U32 argb = lut[i];
        U32 r = (argb >> 16) & 0xFF;
        U32 g = (argb >> 8) & 0xFF;
        U32 b = argb & 0xFF;
        U32 key = (r << 16) | (g << 8) | b;
        U32 slot = key & 0xFFFF;
        s_revLutHash[slot] = (U8)i;
    }
    s_revLutHashValid = 1;
    s_revLutPtr = lut;
}

static inline U8 GL_ReversePaletteLookup(U32 argb) {
    if (!s_revLutHashValid || s_revLutPtr == NULL) {
        return 0;
    }

    U32 r = (argb >> 16) & 0xFF;
    U32 g = (argb >> 8) & 0xFF;
    U32 b = argb & 0xFF;
    U32 key = (r << 16) | (g << 8) | b;
    U32 slot = key & 0xFFFF;
    U8 idx = s_revLutHash[slot];
    if (idx < 256) {
        return idx;
    }

    const U32 *lut = s_revLutPtr;
    U32 bestDist = 0xFFFFFFFF;
    U8 bestIdx = 0;
    for (U32 i = 0; i < 256; i++) {
        U32 c = lut[i];
        S32 dr = (S32)((c >> 16) & 0xFF) - (S32)r;
        S32 dg = (S32)((c >> 8) & 0xFF) - (S32)g;
        S32 db = (S32)(c & 0xFF) - (S32)b;
        U32 dist = (U32)(dr * dr + dg * dg + db * db);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = (U8)i;
        }
    }
    return bestIdx;
}

/* ── State tracking for texture re-upload ───────────────────────────────── */

static const void *s_boundCLUT = NULL;
static const void *s_batchAtlas = NULL;  /* PtrMap at batch start */
static U16 s_batchMaxMask = 0; /* OR of every RepMask queued for this batch */
static U32 s_currentTypePoly = 0;
static U32 s_currentColorPoly = 0;
static S32 s_bilinearCLUT = 0; /* bilinear CLUT sampling flag for 3D */
static GLfloat s_batchCLUTBaseRow = 0.0f; /* CLUT row offset captured at batch start */
static GLfloat s_batchScaledFogNear = 0.0f; /* Fill_ScaledFogNear captured at batch start */
static GLfloat s_batchFogRowScale = 0.0f; /* Fill_Fog_Factor / 65536 captured at batch start */
static const void *s_batchCLUTGouraud = NULL; /* PtrCLUTGouraud at batch start */
static U8 s_batchZBuffer = 0;   /* Fill_Flag_ZBuffer captured at batch start */
static U8 s_batchNZW = 0;       /* Fill_Flag_NZW captured at batch start */

/* Exchange the active batch with the NZW defer pool (verts + flush state). */
static U8 s_swapTmp[BATCH_MAX_VERTS * BATCH_VERT_BYTES];
static void GL_SwapBatchWithDefer(void) {
    memcpy(s_swapTmp, s_batchVerts, BATCH_MAX_VERTS * BATCH_VERT_BYTES);
    memcpy(s_batchVerts, s_deferVerts, BATCH_MAX_VERTS * BATCH_VERT_BYTES);
    memcpy(s_deferVerts, s_swapTmp, BATCH_MAX_VERTS * BATCH_VERT_BYTES);
    U32 tc = s_batchVertCount;
    s_batchVertCount = s_deferVertCount;
    s_deferVertCount = tc;
    tc = s_batchTriCount;
    s_batchTriCount = s_deferTriCount;
    s_deferTriCount = tc;
    U32 tp = s_currentTypePoly;
    s_currentTypePoly = s_deferTypePoly;
    s_deferTypePoly = tp;
    U16 mf = s_currentMatFlags;
    s_currentMatFlags = s_deferMatFlags;
    s_deferMatFlags = mf;
    const void *ta = s_batchAtlas;
    s_batchAtlas = s_deferAtlas;
    s_deferAtlas = ta;
    U8 tx = s_batchTextured;
    s_batchTextured = s_deferTextured;
    s_deferTextured = tx;
    U8 zb = s_batchZBuffer;
    s_batchZBuffer = s_deferZBuffer;
    s_deferZBuffer = zb;
    U8 nw = s_batchNZW;
    s_batchNZW = s_deferNZW;
    s_deferNZW = nw;
    U8 ar = s_batchArray;
    s_batchArray = s_deferArray;
    s_deferArray = ar;
    U16 mm = s_batchMaxMask;
    s_batchMaxMask = s_deferMaxMask;
    s_deferMaxMask = mm;
    const void *tg = s_batchCLUTGouraud;
    s_batchCLUTGouraud = s_deferCLUTGouraud;
    s_deferCLUTGouraud = tg;
    GLfloat cr = s_batchCLUTBaseRow;
    s_batchCLUTBaseRow = s_deferCLUTBaseRow;
    s_deferCLUTBaseRow = cr;
    GLfloat fn = s_batchScaledFogNear;
    s_batchScaledFogNear = s_deferScaledFogNear;
    s_deferScaledFogNear = fn;
    GLfloat fs = s_batchFogRowScale;
    s_batchFogRowScale = s_deferFogRowScale;
    s_deferFogRowScale = fs;
}

/* ── GL state — FBO for SSAA ─────────────────────────────────────────────── */
static GLuint s_fbo = 0;
static GLuint s_fboColorTex = 0;
static GLuint s_fboDepthRbo = 0;
static U32 s_fboWidth = 0;
static U32 s_fboHeight = 0;
static U32 s_renderQuality = 1; /* 1 = native, 2 = 2x, 4 = 4x */
static S32 s_antiAlias = 0;     /* 0=off, 1=FXAA, 2=MSAA×2, 3=MSAA×4 */
static GLuint s_msaaFbo = 0;
static GLuint s_msaaColorRb = 0;
static GLuint s_msaaDepthRb = 0;
static U32 s_msaaW = 0, s_msaaH = 0;
/* Resolve-in-tile mode (GL_INTILE_MSAA + EXT_multisampled_render_to_texture):
 * a dedicated s_msaaColorTex carries multisample semantics, so the tiler
 * resolves it as part of the render pass instead of doing a separate
 * full-screen blit.  The texture is attached only to the MSAA FBO and a
 * single-sample resolve view FBO; it is deliberately NOT attached to s_fbo.
 * The earlier attempts attached s_fboColorTex to both FBOs, which left the
 * driver with an unresolved texture and produced black frames on Adreno.  A
 * snapshot frame (mono only) copies color+depth into s_fbo explicitly; the
 * stereo present path samples the tiled texture directly.  Depth stays a plain
 * multisample renderbuffer because the depth readback reads it live. */
#define GL_INTILE_MSAA 1
static GLuint s_msaaColorTex = 0;
static GLuint s_msaaResolveFbo = 0;
static S32 s_msaaInTile = 0;

/* Stereo double-wide target.  In stereo the scene FBO is twice the content
 * width, with eye 0 in the left half and eye 1 in the right half.  The
 * per-eye viewport/scissor selects the half; OpenXR reads the whole surface
 * in one blit.  s_fboWidth/s_fboHeight stay the per-eye content size so the
 * existing screen-space projection and HUD paths keep their coordinates. */
static S32 s_stereoDoubleWide = 0;
static U32 s_sceneTargetW = 0;
static U32 s_sceneTargetH = 0;
static S32 s_sceneEyeX = 0;
static void GL_SetStereoTargetMode(S32 doubleWide);
static S32 GL_StereoDoubleWideAllowed(void);

static void GL_SetSceneViewport(void) {
    glViewport(s_sceneEyeX, 0, (GLsizei)s_fboWidth, (GLsizei)s_fboHeight);
}

static GLuint GL_SceneColorTex(void) {
    if (s_msaaInTile && s_msaaColorTex)
        return s_msaaColorTex;
    return s_fboColorTex;
}

/* Single-sample read view of the scene.  In-tile mode exposes the dedicated
 * resolve texture; explicit mode reads s_fbo as before. */
static GLuint GL_SceneReadFbo(void) {
    if (s_msaaInTile && s_msaaResolveFbo)
        return s_msaaResolveFbo;
    return s_fbo;
}

/* Draw target for scene geometry.  Every direct draw (batch flush, shadow,
 * sphere, shade rect, sprite) must bind this, not s_fbo: under MSAA the
 * multisample target owns the live color and depth attachments. */
static GLuint GL_SceneDrawFbo(void) {
    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3))
        return s_msaaFbo;
    return s_fbo;
}
static S32 s_frameActive = 0; /* 1 between GL_BeginFrame and GL_Present */
static U32 s_frameSeq = 0;    /* incremented in GL_BeginFrame */
static U32 s_atlasContentVersion = 1; /* bumped when atlas marked dirty */
/* One GL texture per atlas page (PtrMap), not one shared texture: terrain,
   decor bodies, NPC bodies and the sea each own a texture page and their
   draws interleave within a frame, so a shared texture would need a full
   64 KB re-upload on every switch.  Entries evict LRU; version matches
   s_atlasContentVersion so in-place page mutations (animated water) still
   re-upload. */
#define ATLAS_CACHE_SIZE 128
typedef struct {
    const void *ptr;
    U32 ver;
    GLuint tex;
    U16 w, h; /* used region: only this area is ever sampled/uploaded */
} AtlasTexEntry;
static AtlasTexEntry s_atlasTexCache[ATLAS_CACHE_SIZE];
static S32 s_atlasTexNext = 0;

/* ── GL state — present FBO ─────────────────────────────────────────────── */
/* On Quest with OpenXR there is no SDL window, so FBO 0 (the default
 * framebuffer) has no color attachment and draws into it are lost. This
 * offscreen FBO is the compositing target for GL_Present instead: the 3D
 * FBO texture and the 2D overlay quad are drawn here, and OpenXR_PresentMono
 * copies from it into the runtime swapchain via glCopyTexSubImage2D. */
static GLuint s_presentFBO = 0;
static GLuint s_presentColorTex = 0;

/* ── GL state — stereo eye capture FBOs ─────────────────────────────────── */
/* Native stereo: the engine renders the scene twice per frame (once per
 * eye, see GL_SetStereoEye). Eye 0's composite is captured here so the
 * second eye's normal present can hand both images to OpenXR. Eye 1 is not
 * captured: its composite lands in the present FBO through the regular
 * mono path, and OpenXR_PresentMono blits eye 0's FBO + the present FBO
 * into the two eye halves of the swapchain image. */
static GLuint s_stereoEyeFBO[2] = {0, 0};
static GLuint s_stereoEyeTex[2] = {0, 0};
static U32 s_stereoEyeWidth = 0;
static U32 s_stereoEyeHeight = 0;
static S32 s_stereoEye = -1; /* -1 = mono, 0/1 = capturing that eye */
/* Set when eye 0's composite has been captured for the current frame and
 * not yet consumed by the OpenXR submit; cleared by any mono present so
 * menu/movie frames (no per-eye render) submit the mono layer instead of
 * a stale half. */
static S32 s_stereoPairReady = 0;
/* Set by eye 1's present when the 2D overlay is empty, so the right-eye
 * composite was skipped and OpenXR_PresentMono can read the resolved 3D
 * scene FBO directly.  Cleared by every present and by leaving stereo. */
static S32 s_stereoEye1SceneDirect = 0;

/* Set to 1 by GL_ClearFBO() at the start of terrain rendering, cleared by
   GL_BeginFrame().  Used by GL_RenderTriangleList to route body/shadow
   polygons (Fill_Flag_ZBuffer=0) into the GL batch during terrain rendering
   for correct depth testing. */
static S32 s_terrainRenderedThisFrame = 0;

/* Persistent flag: set to 1 after the first GL_ClearFBO(), remains set until
   the GL renderer is shut down or resized.  Lets GL_RenderTriangleList route
   NZW polygons (animated polys: flowers, water) through the GL batch in both
   full redraws (AFF_ALL_*) and incremental updates (AFF_OBJETS_*), as long
   as the FBO contains valid terrain depth.  Without a persistent flag, NZW
   polygons in the incremental path (where GL_ClearFBO() is not called) would
   fall back to SW/Log, losing correct depth-tested compositing with the FBO. */
static S32 s_fboHasTerrain = 0;

/* ── Async depth readback (double-buffered PBOs) ────────────────────────── */
/* glReadPixels of the depth buffer forces a full pipeline sync — the
   dominant frame cost on desktop GL and brutal on tiled mobile GPUs.
   Instead, issue the read into a pixel-pack PBO and consume the PBO filled
   on the *previous* frame: by map time the GPU has had a full frame to
   finish, so the wait overlaps real work.  The one-frame-old depth feeds
   only sprite occlusion (ZBufBoxOverWrite2) and fully-hidden flags, which
   are insensitive to a single frame of lag. */
static GLuint s_depthPbo[2] = {0, 0};
/* Fence per PBO, armed after each read issue: lets the consume step check
   readiness without stalling (see GL_ReadbackDepthBuffer). */
static GLsync s_depthFence[2] = {0, 0};
static S32 s_depthSkipStreak = 0;
static S32 s_depthPboIndex = 0; /* PBO receiving this frame's read */
static S32 s_depthPboReady = 0; /* PBOs holding a completed read (0..2) */
static U32 s_depthPboW = 0;
static U32 s_depthPboH = 0;
/* Screen-sized depth FBO: the SSAA FBO depth is downscaled here on the GPU
   before readback, cutting the CPU read from ~40 MB to ~2.4 MB per frame. */
static GLuint s_depthSmallFbo = 0;
static GLuint s_depthSmallRbo = 0;
static U32 s_depthSmallW = 0;
static U32 s_depthSmallH = 0;

/* ── Terrain snapshot (object ghost erase on incremental frames) ─────────── */
/* Bodies render GL into the FBO, and incremental (AFF_OBJETS_*) frames do
   not redraw the terrain — so each body's previous-frame pixels would stay
   in the FBO as ghost silhouettes.  Snapshot the FBO right after the
   terrain/decor/sea pass (GL_SnapshotTerrain) and blit it back before the
   object pass on incremental frames (GL_RestoreTerrainSnapshot).  Both
   color AND depth are snapshotted and restored: body batches write depth
   (the Log-layer sprite occlusion reads it), so a color-only restore
   leaves last frame's body depth in the buffer and GEQUAL then eats
   redrawn fragments that shifted marginally away — vanishing body
   triangles on idle frames. */
static GLuint s_snapshotTex = 0;
static GLuint s_snapshotFbo = 0;
static GLuint s_snapshotDepthTex = 0;
static GLuint s_depthRestoreProgram = 0;
static GLint s_depthRestore_uDepthTex = -1;
static GLint s_depthRestore_aPos = -1;
static GLint s_depthRestore_aTexCoord = -1;
static const char *s_depthRestoreVertSrc = NULL;
static const char *s_depthRestoreFragSrc = NULL;
static U32 s_snapshotW = 0;
static U32 s_snapshotH = 0;
static U32 s_snapshotGen = 0; /* GL_ClearFBO generation the snapshot matches */
static U32 s_fboGen = 0;      /* bumped by GL_ClearFBO */

/* Drop the incremental-redraw snapshot.  Stereo full-redraw frames do not
 * call GL_RestoreTerrainSnapshot, and the double-wide target replaces the
 * single-wide scene it was captured from. */
static void GL_DestroySnapshot(void) {
    if (s_snapshotDepthTex) {
        glDeleteTextures(1, &s_snapshotDepthTex);
        s_snapshotDepthTex = 0;
    }
    if (s_snapshotFbo) {
        glDeleteFramebuffers(1, &s_snapshotFbo);
        s_snapshotFbo = 0;
    }
    if (s_snapshotTex) {
        glDeleteTextures(1, &s_snapshotTex);
        s_snapshotTex = 0;
    }
    s_snapshotW = 0;
    s_snapshotH = 0;
    s_snapshotGen = 0;
}

/* ── Fullscreen quad: position (x,y) + texcoord (u,v) ───────────────────── */

/* 2D overlay quad — V is flipped because glTexSubImage2D uploads row 0
   (game Y=0, top of screen) at texture V=0 (bottom in OpenGL). Flipping V
   here maps game Y=0 → screen top. */
static const GLfloat s_quadVerts[] = {
    /* pos       texcoord */
    -1.0f, -1.0f,  0.0f, 1.0f,  /* bottom-left  (flip Y: texcoord 1.0) */
     1.0f, -1.0f,  1.0f, 1.0f,  /* bottom-right */
    -1.0f,  1.0f,  0.0f, 0.0f,  /* top-left      (flip Y: texcoord 0.0) */
     1.0f,  1.0f,  1.0f, 0.0f,  /* top-right */
};

/* FBO quad — V is NOT flipped. The CLUT vertex shader already maps game
   Y=0 → FBO top (clip Y=+1), so V=1.0 at screen top samples the FBO
   correctly without an extra flip. */
static const GLfloat s_fboQuadVerts[] = {
    /* pos       texcoord */
    -1.0f, -1.0f,  0.0f, 0.0f,  /* bottom-left  (V=0.0 = FBO bottom) */
     1.0f, -1.0f,  1.0f, 0.0f,  /* bottom-right */
    -1.0f,  1.0f,  0.0f, 1.0f,  /* top-left      (V=1.0 = FBO top) */
     1.0f,  1.0f,  1.0f, 1.0f,  /* top-right */
};

/* ── Shader helpers ─────────────────────────────────────────────────────── */

static GLuint CompileShader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return 0;
    }
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        log[0] = 0;
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        Log_Error("[GL] shader compile failed (%s): %s",
                  type == GL_VERTEX_SHADER ? "vert" : "frag", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkProgram(GLuint vert, GLuint frag) {
    if (vert == 0 || frag == 0) {
        /* Attaching 0 fails silently per-shader and Mesa links the rest
           (e.g. a vertex-only program that rasterizes nothing) — fail
           instead so the caller's fallback path runs. */
        return 0;
    }
    GLuint prog = glCreateProgram();
    if (prog == 0) {
        return 0;
    }
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

/* ── FBO helpers (SSAA) ──────────────────────────────────────────────────── */

static void GL_DestroyFBO(void) {
    if (s_fbo) {
        glDeleteFramebuffers(1, &s_fbo);
        s_fbo = 0;
    }
    if (s_fboColorTex) {
        glDeleteTextures(1, &s_fboColorTex);
        s_fboColorTex = 0;
    }
    if (s_fboDepthRbo) {
        glDeleteRenderbuffers(1, &s_fboDepthRbo);
        s_fboDepthRbo = 0;
    }
    s_fboWidth = 0;
    s_fboHeight = 0;
    s_sceneTargetW = 0;
    s_sceneTargetH = 0;
}

static void GL_DestroyPresentFBO(void) {
    if (s_presentFBO) {
        glDeleteFramebuffers(1, &s_presentFBO);
        s_presentFBO = 0;
    }
    if (s_presentColorTex) {
        glDeleteTextures(1, &s_presentColorTex);
        s_presentColorTex = 0;
    }
}

static void GL_DestroyStereoEyeFBOs(void) {
    int i;
    for (i = 0; i < 2; i++) {
        if (s_stereoEyeFBO[i]) {
            glDeleteFramebuffers(1, &s_stereoEyeFBO[i]);
            s_stereoEyeFBO[i] = 0;
        }
        if (s_stereoEyeTex[i]) {
            glDeleteTextures(1, &s_stereoEyeTex[i]);
            s_stereoEyeTex[i] = 0;
        }
    }
    s_stereoEyeWidth = 0;
    s_stereoEyeHeight = 0;
}

static void GL_CreateStereoEyeFBOs(U32 width, U32 height) {
    int i;
    GL_DestroyStereoEyeFBOs();
    for (i = 0; i < 2; i++) {
        glGenTextures(1, &s_stereoEyeTex[i]);
        glBindTexture(GL_TEXTURE_2D, s_stereoEyeTex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);

        glGenFramebuffers(1, &s_stereoEyeFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, s_stereoEyeFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, s_stereoEyeTex[i], 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            Log_Error("[GL] Stereo eye FBO %d incomplete (0x%x) %ux%u", i,
                      (unsigned)status, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            GL_DestroyStereoEyeFBOs();
            return;
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    s_stereoEyeWidth = width;
    s_stereoEyeHeight = height;
    Log_Info("[GL] Stereo eye FBOs created %ux%u", width, height);
}

void GL_SetStereoEye(S32 eye) {
    if (eye < -1 || eye > 1)
        eye = -1;

    /* Eye 1 consumes the overlay scan cached by eye 0.  Any other transition
     * starts a fresh scan. */
    if (eye != 1) {
        s_overlayScanValid = 0;
        s_overlayEmptyCached = 0;
        s_overlayEye0Dynamic = 0;
    }

    if (eye == 0) {
        /* Stereo eye passes are full redraws and skip GL_SnapshotTerrain.
         * Drop a snapshot left over from mono before the target switch. */
        GL_DestroySnapshot();
        S32 wantDoubleWide = GL_StereoDoubleWideAllowed();
        if (wantDoubleWide != s_stereoDoubleWide)
            GL_SetStereoTargetMode(wantDoubleWide);
        s_stereoPairReady = 0;
    } else if (eye < 0 && s_stereoDoubleWide &&
               (!DisplayStereo3D || !OpenXR_GetStereoEnabled() ||
                !OpenXR_IsActive())) {
        /* Stereo was switched off (or the session ended): the mono
         * composite expects a single-wide scene, not the left half of a
         * stereo pair. */
        GL_SetStereoTargetMode(0);
    }

    s_stereoEye = eye;
    s_sceneEyeX =
        (eye == 1 && s_stereoDoubleWide) ? (S32)s_fboWidth : 0;
    if (eye != 1)
        s_stereoEye1SceneDirect = 0;
    if (eye < 0) {
        /* Leaving stereo: clear the eye shifts so mono frames project
         * exactly as before. Per-eye shift values are the caller's job —
         * the right convergence depends on the live camera distance. */
        StereoCamShiftX = 0;
        StereoProjShiftX = 0;
    }
}

S32 GL_GetStereoEye(void) {
    return s_stereoEye;
}

S32 GL_StereoOverlayReusable(void) {
    return s_stereoEye == 1 && s_overlayScanValid && !s_overlayEye0Dynamic;
}

S32 GL_TakeStereoPair(void) {
    S32 ready = s_stereoPairReady;
    s_stereoPairReady = 0;
    return ready;
}

U32 GL_GetStereoEyeFBO(S32 eye) {
    if (eye < 0 || eye > 1)
        return 0;
    return (U32)s_stereoEyeFBO[eye];
}

U32 GL_GetStereoPairFBO(void) {
    return s_stereoDoubleWide ? (U32)GL_SceneReadFbo() : 0;
}

U32 GL_GetStereoPairWidth(void) {
    return s_stereoDoubleWide ? s_fboWidth * 2 : 0;
}

U32 GL_GetStereoPairHeight(void) {
    return s_stereoDoubleWide ? s_fboHeight : 0;
}

S32 GL_StereoPairNeedsPresentShader(void) {
    return s_stereoDoubleWide && !GL_ImageAdjustNeutral();
}

S32 GL_BlitStereoPairToCurrent(U32 srcWidth, U32 srcHeight, U32 dstWidth,
                               U32 dstHeight) {
    if (!s_initialized || !s_stereoDoubleWide || s_program == 0 ||
        s_fboVbo == 0 || srcWidth == 0 || srcHeight == 0 || dstWidth == 0 ||
        dstHeight == 0)
        return 0;

    GLuint tex = GL_SceneColorTex();
    if (tex == 0)
        return 0;

    S32 exact = (srcWidth == dstWidth && srcHeight == dstHeight);
    GLenum filter = exact ? GL_NEAREST : GL_LINEAR;

    GL_UseProgramCached(s_program);
    GL_BindTexUnit(0, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glUniform1i(s_uTexLoc, 0);
    GL_UploadImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                         s_adj_uSaturation);
    glUniform2f(s_uTexelLoc, 1.0f / (GLfloat)srcWidth,
                1.0f / (GLfloat)srcHeight);
    glUniform1i(s_uFilterLoc, exact ? 0 : 1);

    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, (GLsizei)dstWidth, (GLsizei)dstHeight);

    GL_BindArrayBuffer(s_fboVbo);
    glEnableVertexAttribArray(s_aPosLoc);
    glVertexAttribPointer(s_aPosLoc, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(s_aTexCoordLoc);
    glVertexAttribPointer(s_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat),
                          (const void *)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(s_aPosLoc);
    glDisableVertexAttribArray(s_aTexCoordLoc);

    GL_BindArrayBuffer(0);
    GL_BindTexUnit(0, 0);
    GL_UseProgramCached(0);
    return 1;
}

S32 GL_StereoEyeSceneDirect(void) {
    return s_stereoEye1SceneDirect;
}

static void GL_CreatePresentFBO(U32 width, U32 height) {
    GL_DestroyPresentFBO();

    glGenTextures(1, &s_presentColorTex);
    glBindTexture(GL_TEXTURE_2D, s_presentColorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &s_presentFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, s_presentFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s_presentColorTex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Log_Error("[GL] Present FBO incomplete (0x%x) %ux%u", (unsigned)status, width, height);
        GL_DestroyPresentFBO();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Log_Info("[GL] Present FBO created %ux%u (fbo=%u tex=%u)", width, height, (unsigned)s_presentFBO, (unsigned)s_presentColorTex);
}

/* Mono/menu presents need the intermediate composite target.  The stereo
 * double-wide path does not, so the surface is created on demand there. */
static void GL_EnsurePresentFBO(void) {
    if (s_presentFBO != 0)
        return;
    U32 w = s_texWidth ? s_texWidth : s_fboWidth;
    U32 h = s_texHeight ? s_texHeight : s_fboHeight;
    if (w == 0 || h == 0)
        return;
    GL_CreatePresentFBO(w, h);
}

static void GL_CreateFBO(U32 width, U32 height) {
    GL_DestroyFBO();

    /* s_fboWidth is the per-eye content width.  In stereo the single
     * allocation is twice as wide and holds both eye viewports. */
    const U32 targetW = s_stereoDoubleWide ? width * 2 : width;
    const U32 targetH = height;

    /* Color texture attachment */
    glGenTextures(1, &s_fboColorTex);
    glBindTexture(GL_TEXTURE_2D, s_fboColorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, targetW, targetH, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Depth renderbuffer attachment */
    glGenRenderbuffers(1, &s_fboDepthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, s_fboDepthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, targetW,
                          targetH);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    /* Framebuffer object */
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s_fboColorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, s_fboDepthRbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        GL_DestroyFBO();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    s_fboWidth = width;
    s_fboHeight = height;
    s_sceneTargetW = targetW;
    s_sceneTargetH = targetH;
    Log_Info("[GL] Scene FBO %ux%u (eye %ux%u, %s)", targetW, targetH, width,
             height, s_stereoDoubleWide ? "double-wide" : "single");
}

/* ── Public API ─────────────────────────────────────────────────────────── */

/* paletteLUT builds 0xAARRGGBB which in little-endian memory is
   [B, G, R, A] — use GL_BGRA to match the byte layout. */
#ifdef GL_BGRA
#define FMT_BGRA GL_BGRA
#else
#define FMT_BGRA 0x80E1 /* GL_BGRA_EXT */
#endif

/* GLES accepts BGRA transfer formats only when the driver exposes
 * GL_EXT_texture_format_BGRA8888; Android GLES contexts are not required
 * to, and a rejected format fails silently here (GL_INVALID_ENUM, texture
 * left incomplete — it samples black). Desktop GL keeps the direct BGRA
 * path; GLES uploads GL_RGBA and the CPU swizzles the BGRA-ordered staging
 * bytes into R,G,B,A order. The swizzle swaps bytes 0 and 2, so it also
 * converts an RGBA readback back to the ARGB word order consumers expect. */
#if defined(__ANDROID__)
#define GL_UPLOAD_FORMAT GL_RGBA
static void GL_SwizzleBGRAtoRGBA(U32 *dst, const U32 *src, U32 pixelCount) {
    U32 i;
    for (i = 0; i < pixelCount; i++) {
        U32 c = src[i];
        dst[i] = (c & 0xFF00FF00u) | ((c >> 16) & 0x000000FFu) |
                 ((c << 16) & 0x00FF0000u);
    }
}
#else
#define GL_UPLOAD_FORMAT FMT_BGRA
#endif

S32 GL_Renderer_Init(U32 resX, U32 resY) {
    if (s_initialized) {
        return TRUE;
    }

    if (!Window_CreateGLContext()) {
        return FALSE;
    }
    if (!Window_GLMakeCurrent()) {
        return FALSE;
    }

    Log_Info("[GL] Initializing renderer (%ux%u)...", resX, resY);
    Log_Info("[GL] GL_VERSION: %s", (const char *)glGetString(GL_VERSION));
    Log_Info("[GL] GL_RENDERER: %s", (const char *)glGetString(GL_RENDERER));

    /* ── Compile present shader ── */
    /* Depth-only restore pass for the MSAA incremental erase: writes the
     * snapshot's terrain depth back under the erased region.  Raw-compiled
     * (no V3 transform) like the present program; gl_FragDepth is core in
     * GLSL 130 and a built-in in GLSL ES 3.00. */
#ifdef __ANDROID__
    s_depthRestoreVertSrc =
        "#version 300 es\nprecision highp float;\n"
        "in vec2 aPos;\nin vec2 aTexCoord;\nout vec2 vTexCoord;\n"
        "void main() { gl_Position = vec4(aPos, 0.0, 1.0); vTexCoord = aTexCoord; }\n";
    s_depthRestoreFragSrc =
        "#version 300 es\nprecision highp float;\n"
        "uniform sampler2D uDepthTex;\nin vec2 vTexCoord;\n"
        "void main() { gl_FragDepth = texture(uDepthTex, vTexCoord).r; }\n";
#else
    s_depthRestoreVertSrc =
        "#version 130\n"
        "in vec2 aPos;\nin vec2 aTexCoord;\nout vec2 vTexCoord;\n"
        "void main() { gl_Position = vec4(aPos, 0.0, 1.0); vTexCoord = aTexCoord; }\n";
    s_depthRestoreFragSrc =
        "#version 130\n"
        "uniform sampler2D uDepthTex;\nin vec2 vTexCoord;\n"
        "void main() { gl_FragDepth = texture(uDepthTex, vTexCoord).r; }\n";
#endif
    GLuint vert = CompileShader(GL_VERTEX_SHADER, s_vertSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, s_fragSrc);
    if (vert == 0 || frag == 0) {
        return FALSE;
    }

    s_program = LinkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (s_program == 0) {
        return FALSE;
    }
    {
        const char *dzVertSrc = s_depthRestoreVertSrc;
        const char *dzFragSrc = s_depthRestoreFragSrc;
        GLuint dzVert = CompileShader(GL_VERTEX_SHADER, dzVertSrc);
        GLuint dzFrag =
            CompileShader(GL_FRAGMENT_SHADER, dzFragSrc);
        if (dzVert != 0 && dzFrag != 0) {
            glBindAttribLocation(dzVert, 0, "aPos");
            glBindAttribLocation(dzVert, 1, "aTexCoord");
            s_depthRestoreProgram = LinkProgram(dzVert, dzFrag);
        }
        glDeleteShader(dzVert);
        glDeleteShader(dzFrag);
        if (s_depthRestoreProgram != 0) {
            GL_UseProgramCached(s_depthRestoreProgram);
            s_depthRestore_uDepthTex =
                glGetUniformLocation(s_depthRestoreProgram, "uDepthTex");
            glUniform1i(s_depthRestore_uDepthTex, 0);
            s_depthRestore_aPos = glGetAttribLocation(s_depthRestoreProgram,
                                                      "aPos");
            s_depthRestore_aTexCoord =
                glGetAttribLocation(s_depthRestoreProgram, "aTexCoord");
            GL_UseProgramCached(0);
        } else {
            Log_Warn("[GL] depth restore program failed; MSAA idle frames "
                     "may show vanishing body fragments");
        }
    }

    s_uTexLoc = glGetUniformLocation(s_program, "uTex");
    s_uTexelLoc = glGetUniformLocation(s_program, "uTexel");
    s_uFilterLoc = glGetUniformLocation(s_program, "uFilter");
    s_adj_uBrightness = glGetUniformLocation(s_program, "uBrightness");
    s_adj_uContrast = glGetUniformLocation(s_program, "uContrast");
    s_adj_uSaturation = glGetUniformLocation(s_program, "uSaturation");
    s_aPosLoc = glGetAttribLocation(s_program, "aPos");
    s_aTexCoordLoc = glGetAttribLocation(s_program, "aTexCoord");

    /* ── Compile indexed-present shader ── */
    GLuint idxVert = CompileShader(GL_VERTEX_SHADER, s_vertSrc);
    GLuint idxFrag = CompileShader(GL_FRAGMENT_SHADER, s_presentIndexedFragSrc);
    if (idxVert == 0 || idxFrag == 0) {
        return FALSE;
    }

    s_presentIndexedProgram = LinkProgram(idxVert, idxFrag);
    glDeleteShader(idxVert);
    glDeleteShader(idxFrag);
    if (s_presentIndexedProgram == 0) {
        return FALSE;
    }

    s_idx_aPos = glGetAttribLocation(s_presentIndexedProgram, "aPos");
    s_idx_aTexCoord = glGetAttribLocation(s_presentIndexedProgram, "aTexCoord");
    s_idx_uIndices = glGetUniformLocation(s_presentIndexedProgram, "uIndices");
    s_idx_uPalette = glGetUniformLocation(s_presentIndexedProgram, "uPalette");
    s_idx_uTexel = glGetUniformLocation(s_presentIndexedProgram, "uTexel");
    s_idx_uFilter = glGetUniformLocation(s_presentIndexedProgram, "uFilter");
    s_idxAdj_uBrightness = glGetUniformLocation(s_presentIndexedProgram, "uBrightness");
    s_idxAdj_uContrast = glGetUniformLocation(s_presentIndexedProgram, "uContrast");
    s_idxAdj_uSaturation = glGetUniformLocation(s_presentIndexedProgram, "uSaturation");

    glGenTextures(1, &s_lutTex);
    glBindTexture(GL_TEXTURE_2D, s_lutTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* ── Compile CLUT shader ── */
    GLuint clutVert = CompileShader(GL_VERTEX_SHADER, s_clutVertSrc);
    GLuint clutFrag = CompileShader(GL_FRAGMENT_SHADER, s_clutFragSrc);
    if (clutVert == 0 || clutFrag == 0) {
        return FALSE;
    }

    /* Same attribute layout for both programs: explicit binding indices so
       one vertex-array spec serves the legacy and array variants. */
    {
        static const char *names[6] = {"aPos",     "aTexCoord", "aLight",
                                       "aZ",       "aRepMask",  "aMaterial"};
        for (S32 i = 0; i < 6; i++) {
            glBindAttribLocation(clutVert, i, names[i]);
        }
    }

    s_clutProgram = LinkProgram(clutVert, clutFrag);
    glDeleteShader(clutVert);
    glDeleteShader(clutFrag);
    if (s_clutProgram == 0) {
        return FALSE;
    }

    s_clut_aPos = glGetAttribLocation(s_clutProgram, "aPos");
    s_clut_aTexCoord = glGetAttribLocation(s_clutProgram, "aTexCoord");
    s_clut_aLight = glGetAttribLocation(s_clutProgram, "aLight");
    s_clut_aZ = glGetAttribLocation(s_clutProgram, "aZ");
    s_clut_aRepMask = glGetAttribLocation(s_clutProgram, "aRepMask");
    s_clut_aMaterial = glGetAttribLocation(s_clutProgram, "aMaterial");
    s_clut_uAtlas = glGetUniformLocation(s_clutProgram, "uAtlas");
    s_clut_uCLUT = glGetUniformLocation(s_clutProgram, "uCLUT");
    s_clut_uPalette = glGetUniformLocation(s_clutProgram, "uPalette");
    s_clut_uScreenScale = glGetUniformLocation(s_clutProgram, "uScreenScale");
    s_clut_uAtlasScale = glGetUniformLocation(s_clutProgram, "uAtlasScale");
    s_clut_uCLUTBaseRow = glGetUniformLocation(s_clutProgram, "uCLUTBaseRow");
    s_clut_uScaledFogNear = glGetUniformLocation(s_clutProgram, "uScaledFogNear");
    s_clut_uFogRowScale = glGetUniformLocation(s_clutProgram, "uFogRowScale");
    s_clut_uBilinear = glGetUniformLocation(s_clutProgram, "uBilinear");

    /* Sampler unit assignments are program state: set them once here
       instead of per draw call. */
    GL_UseProgramCached(s_clutProgram);
    glUniform1i(s_clut_uAtlas, 0);
    glUniform1i(s_clut_uCLUT, 1);
    glUniform1i(s_clut_uPalette, 2);
    GL_UseProgramCached(0);

    /* ── GLSL-3 CLUT variant with the texture-array sampler ──
       Mechanically derived from the legacy source: version bump, es3/130
       in/out and texture() renames, an injected arraySample() used when a
       polygon's RepMask carries the 0x8000|layer array stamp. */
    {
        static const char *attrNames[6] = {"aPos",     "aTexCoord", "aLight",
                                           "aZ",       "aRepMask",  "aMaterial"};
        char *vert3 = GL_ShaderToV3(s_clutVertSrc, 1);
        char *frag3 = GL_ShaderToV3(s_clutFragSrc, 0);
        if (frag3 != NULL) {
            frag3 = GL_InjectArrayBranch(frag3);
        }
        GLuint v3 = 0, f3 = 0;
        if (vert3 != NULL && frag3 != NULL) {
            v3 = CompileShader(GL_VERTEX_SHADER, vert3);
            f3 = CompileShader(GL_FRAGMENT_SHADER, frag3);
            for (S32 i = 0; i < 6; i++) {
                glBindAttribLocation(v3, i, attrNames[i]);
            }
            s_clut3Program = LinkProgram(v3, f3);
        }
        free(vert3);
        free(frag3);
        glDeleteShader(v3);
        glDeleteShader(f3);
        if (s_clut3Program == 0) {
            Log_Warn("[GL] array CLUT program failed; staying on the 2D path");
        } else {
            s_clut3_uAtlas = glGetUniformLocation(s_clut3Program, "uAtlas");
            s_clut3_uCLUT = glGetUniformLocation(s_clut3Program, "uCLUT");
            s_clut3_uPalette = glGetUniformLocation(s_clut3Program, "uPalette");
            s_clut3_uAtlasArr =
                glGetUniformLocation(s_clut3Program, "uAtlasArr");
            s_clut3_uScreenScale =
                glGetUniformLocation(s_clut3Program, "uScreenScale");
            s_clut3_uAtlasScale =
                glGetUniformLocation(s_clut3Program, "uAtlasScale");
            s_clut3_uCLUTBaseRow =
                glGetUniformLocation(s_clut3Program, "uCLUTBaseRow");
            s_clut3_uScaledFogNear =
                glGetUniformLocation(s_clut3Program, "uScaledFogNear");
            s_clut3_uFogRowScale =
                glGetUniformLocation(s_clut3Program, "uFogRowScale");
            s_clut3_uBilinear = glGetUniformLocation(s_clut3Program, "uBilinear");
            GL_UseProgramCached(s_clut3Program);
            glUniform1i(s_clut3_uAtlas, 0);
            glUniform1i(s_clut3_uCLUT, 1);
            glUniform1i(s_clut3_uPalette, 2);
            glUniform1i(s_clut3_uAtlasArr, 3);
            GL_UseProgramCached(0);
        }
    }

    {
        GLuint fv = CompileShader(GL_VERTEX_SHADER, s_vertSrc);
        GLuint ff = CompileShader(GL_FRAGMENT_SHADER, s_fxaaFragSrc);
        if (fv && ff) {
            s_fxaaProgram = LinkProgram(fv, ff);
            s_fxaa_uTex = glGetUniformLocation(s_fxaaProgram, "uTex");
            s_fxaa_uTexel = glGetUniformLocation(s_fxaaProgram, "uTexel");
    s_fxaaAdj_uBrightness = glGetUniformLocation(s_fxaaProgram, "uBrightness");
    s_fxaaAdj_uContrast = glGetUniformLocation(s_fxaaProgram, "uContrast");
    s_fxaaAdj_uSaturation = glGetUniformLocation(s_fxaaProgram, "uSaturation");
            s_fxaa_aPos = glGetAttribLocation(s_fxaaProgram, "aPos");
            s_fxaa_aTexCoord = glGetAttribLocation(s_fxaaProgram, "aTexCoord");
        }
        glDeleteShader(fv);
        glDeleteShader(ff);
    }

    GL_SmaaInit();

    Log_Info("[GL] CLUT shader compiled OK (aPos=%d aTexCoord=%d aLight=%d "
             "aZ=%d aRepMask=%d aMaterial=%d uAtlas=%d uCLUT=%d uPalette=%d "
             "uScreenScale=%d).",
             s_clut_aPos, s_clut_aTexCoord, s_clut_aLight, s_clut_aZ,
             s_clut_aRepMask, s_clut_aMaterial,
             s_clut_uAtlas, s_clut_uCLUT, s_clut_uPalette,
             s_clut_uScreenScale);

    /* ── Create VBOs ── */
    /* Present quad VBO (static) — flipped UVs for 2D overlay */
    glGenBuffers(1, &s_vbo);
    GL_BindArrayBuffer(s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_quadVerts), s_quadVerts,
                 GL_STATIC_DRAW);
    GL_BindArrayBuffer(0);

    /* FBO quad VBO (static) — non-flipped UVs for FBO→screen */
    glGenBuffers(1, &s_fboVbo);
    GL_BindArrayBuffer(s_fboVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_fboQuadVerts), s_fboQuadVerts,
                 GL_STATIC_DRAW);
    GL_BindArrayBuffer(0);

    /* Batch VBO for 3D triangles (dynamic) */
    glGenBuffers(1, &s_batchVBO);
    GL_BindArrayBuffer(s_batchVBO);
    s_batchRingSize = BATCH_MAX_VERTS * BATCH_VERT_BYTES * BATCH_RING_SLOTS;
    glBufferData(GL_ARRAY_BUFFER, s_batchRingSize, NULL, GL_STREAM_DRAW);
    GL_BindArrayBuffer(0);

    /* One-shot VBO for the shadow/sphere/shade/sprite quad draws. */
    glGenBuffers(1, &s_auxVBO);
    GL_BindArrayBuffer(s_auxVBO);
    glBufferData(GL_ARRAY_BUFFER, BATCH_MAX_VERTS * BATCH_VERT_BYTES, NULL,
                 GL_STREAM_DRAW);
    GL_BindArrayBuffer(0);

    /* ── Create textures ── */
    /* Present texture (Log buffer → ARGB) */
    glGenTextures(1, &s_texture);
    glBindTexture(GL_TEXTURE_2D, s_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0, GL_UPLOAD_FORMAT,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Atlas texture (256×256, 8-bit LUMINANCE) */
    glGenTextures(1, &s_atlasTex);
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 256, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* CLUT texture (256×256, 8-bit LUMINANCE — full fog table) */
    glGenTextures(1, &s_clutTex);
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 256, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

     /* Palette texture (256×1, BGRA — paletteLUT) */
     glGenTextures(1, &s_paletteTex);
     glBindTexture(GL_TEXTURE_2D, s_paletteTex);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_UPLOAD_FORMAT,
                   GL_UNSIGNED_BYTE, NULL);
     glBindTexture(GL_TEXTURE_2D, 0);

    /* Surface format problems (e.g. a rejected transfer format) are silent
       otherwise: the texture stays incomplete and just samples black. */
    {
        static const char *texNames[5] = {"surface", "atlas", "clut",
                                          "palette", "post"};
        GLenum texErr;
        for (int ti = 0; ti < 5; ti++) {
            texErr = glGetError();
            if (texErr != GL_NO_ERROR) {
                Log_Warn("[GL] GL error after %s texture setup: 0x%x",
                         texNames[ti], (unsigned)texErr);
            }
        }
    }

    /* GL state */
    p_glFramebufferTexture2DMultisampleEXT =
        (PFN_GLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXT)
            (PFN_GLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXT)
            SDL_GL_GetProcAddress("glFramebufferTexture2DMultisampleEXT");
    glViewport(0, 0, resX, resY);
    glDisable(GL_DEPTH_TEST);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Create FBO — always needed so 3D renders into an offscreen buffer
       that can be composited at the letterbox viewport alongside the
       2D overlay.  Without FBO, GL_Present's letterbox clear erases
       any 3D drawn directly to the default framebuffer. */
    GL_CreateFBO(resX * s_renderQuality, resY * s_renderQuality);

    /* Create present FBO for compositing. On Quest with OpenXR there is no
       SDL window, so the default framebuffer (FBO 0) is not a valid render
       target; this offscreen FBO is what GL_Present draws into, and
       OpenXR_PresentMono copies from it to the swapchain texture. */
    GL_CreatePresentFBO(resX, resY);
    GL_CreateStereoEyeFBOs(resX, resY);

    /* Clear to black so the window is never transparent/garbage before
       the first PresentFrame call.  Depth clear to 0.0 matches GL_GREATER
       convention (closer objects have higher depth values). */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
#if defined(__ANDROID__)
    glClearDepthf(0.0f);
#else
    glClearDepth(0.0);
#endif
    /* glClear(GL_DEPTH_BUFFER_BIT) honours the depth mask, and the batch
       depth-state cache can legitimately leave write disabled after a frame
       ending in a blended batch — a masked clear would leave last frame's
       depth in place (black frame with terrain blinking through it). */
    GL_SetDepthWrite(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Window_GL_SwapWindow();

    s_texWidth = resX;
    s_texHeight = resY;
    s_initialized = 1;
    GL_InvalidateGLState();
    /* Apply the configured anti-alias mode: the menu re-applies on change,
       but at boot nothing else did, so the cfg value was silently dead and
       the composite rendered unsmoothed. */
    GL_SetAntiAlias(DisplayAntiAlias);
    GL_LogSurfaceBudget("init");
    Log_Info("[GL] Renderer initialized OK.");
    return TRUE;
}

void GL_Renderer_Shutdown(void) {
    if (!s_initialized) {
        return;
    }

    if (s_texture) {
        glDeleteTextures(1, &s_texture);
        s_texture = 0;
    }
    if (s_atlasTex) {
        glDeleteTextures(1, &s_atlasTex);
        s_atlasTex = 0;
    }
    for (S32 i = 0; i < ATLAS_CACHE_SIZE; i++) {
        if (s_atlasTexCache[i].tex) {
            glDeleteTextures(1, &s_atlasTexCache[i].tex);
        }
        s_atlasTexCache[i].w = 0;
        s_atlasTexCache[i].h = 0;
        s_atlasTexCache[i].ptr = NULL;
        s_atlasTexCache[i].ver = 0;
        s_atlasTexCache[i].tex = 0;
    }
    s_atlasTexNext = 0;
    GL_DestroySnapshot();
    if (s_depthRestoreProgram) {
        glDeleteProgram(s_depthRestoreProgram);
        s_depthRestoreProgram = 0;
    }
    if (s_clutTex) {
        glDeleteTextures(1, &s_clutTex);
        s_clutTex = 0;
    }
    if (s_paletteTex) {
        glDeleteTextures(1, &s_paletteTex);
        s_paletteTex = 0;
    }
    if (s_vbo) {
        glDeleteBuffers(1, &s_vbo);
        s_vbo = 0;
    }
    if (s_fboVbo) {
        glDeleteBuffers(1, &s_fboVbo);
        s_fboVbo = 0;
    }
    if (s_batchVBO) {
        glDeleteBuffers(1, &s_batchVBO);
        s_batchVBO = 0;
    }
    if (s_auxVBO) {
        glDeleteBuffers(1, &s_auxVBO);
        s_auxVBO = 0;
    }
    s_batchRingSize = 0;
    s_batchRingOff = 0;
    if (s_depthPbo[0]) {
        glDeleteBuffers(2, s_depthPbo);
        s_depthPbo[0] = 0;
        s_depthPbo[1] = 0;
    }
    s_depthPboReady = 0;
    s_depthPboW = 0;
    s_depthPboH = 0;
    if (s_depthSmallFbo) {
        glDeleteFramebuffers(1, &s_depthSmallFbo);
        s_depthSmallFbo = 0;
    }
    if (s_depthSmallRbo) {
        glDeleteRenderbuffers(1, &s_depthSmallRbo);
        s_depthSmallRbo = 0;
    }
    s_depthSmallW = 0;
    s_depthSmallH = 0;
    if (s_program) {
        glDeleteProgram(s_program);
        s_program = 0;
    }
    if (s_presentIndexedProgram) {
        glDeleteProgram(s_presentIndexedProgram);
        s_presentIndexedProgram = 0;
    }
    if (s_logIndexTex) {
        glDeleteTextures(1, &s_logIndexTex);
        s_logIndexTex = 0;
    }
    s_logIndexTexW = 0;
    s_logIndexTexH = 0;
    if (s_lutTex) {
        glDeleteTextures(1, &s_lutTex);
        s_lutTex = 0;
    }
    if (s_clutProgram) {
        glDeleteProgram(s_clutProgram);
        s_clutProgram = 0;
    }
    if (s_fxaaProgram) {
        glDeleteProgram(s_fxaaProgram);
        s_fxaaProgram = 0;
    }
    GL_DestroyMSAAFbo();

    GL_DestroyFBO();
    GL_DestroyPresentFBO();
    GL_DestroyStereoEyeFBOs();
    GL_DestroyHorizon();

    Window_DestroyGLContext();
    s_initialized = 0;
    s_boundCLUT = NULL;
    s_batchAtlas = NULL;
    s_batchVertCount = 0;
    s_batchTriCount = 0;
    s_currentColorPoly = 0;
    s_batchCLUTBaseRow = 0.0f;
    s_batchCLUTGouraud = NULL;
    s_batchZBuffer = 0;
    s_batchNZW = 0;
    s_frameActive = 0;
    s_fboHasTerrain = 0;
    s_stereoDoubleWide = 0;
    s_sceneEyeX = 0;
    s_sceneTargetW = 0;
    s_sceneTargetH = 0;
    GL_InvalidateGLState();
    Log_Info("[GL] Renderer shut down.");
}

void GL_Renderer_Resize(U32 resX, U32 resY) {
    if (!s_initialized) {
        return;
    }

    glViewport(0, 0, resX, resY);

    /* Recreate the present texture at the new size */
    if (s_texture) {
        glBindTexture(GL_TEXTURE_2D, s_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0, GL_UPLOAD_FORMAT,
                     GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    s_texWidth = resX;
    s_texHeight = resY;

    /* Recreate FBO at scaled resolution (always needed) */
    s_fboHasTerrain = 0;
    GL_CreateFBO(resX * s_renderQuality, resY * s_renderQuality);
    if ((s_antiAlias == 2 || s_antiAlias == 3)) {
        GL_CreateMSAAFbo(resX * s_renderQuality, resY * s_renderQuality,
                         (s_antiAlias == 3) ? 4 : 2);
    }

    /* Recreate present FBO only for the mono/fallback path.  Stereo
     * double-wide presents read the scene target directly. */
    if (s_stereoDoubleWide) {
        GL_DestroyPresentFBO();
        GL_DestroyStereoEyeFBOs();
    } else {
        GL_CreatePresentFBO(resX, resY);
        GL_CreateStereoEyeFBOs(resX, resY);
    }
    s_sceneEyeX = 0;
}

void GL_SetRenderQuality(U32 quality) {
    if (quality == 0) quality = 1;
    if (quality > 4) quality = 4;
#ifdef __ANDROID__
    /* Single choke point (boot AND the in-game menu): SSAA multiplies the
     * framebuffer per axis, and quality 4 at 1680x945 is a 25-megapixel
     * render target that stalls the whole frame loop on a mobile GPU. The
     * supersampled resolution ladder provides sharpness far cheaper. */
    if (quality > 1) {
        Log_Info("[GL] Android: RenderQuality %u clamped to 1", quality);
        quality = 1;
    }
#endif
    if (s_renderQuality == quality) return;
    Log_Info("[GL] SetRenderQuality: %u -> %u (tex=%ux%u)",
             s_renderQuality, quality, s_texWidth, s_texHeight);
    s_renderQuality = quality;
    if (!s_initialized) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    s_fboHasTerrain = 0;
    GL_CreateFBO(s_texWidth * quality, s_texHeight * quality);
    if ((s_antiAlias == 2 || s_antiAlias == 3)) {
        GL_CreateMSAAFbo(s_texWidth * quality, s_texHeight * quality,
                         (s_antiAlias == 3) ? 4 : 2);
    }
    Log_Info("[GL] SetRenderQuality: FBO %s (s_fbo=%u).",
             s_fbo ? "OK" : "FAILED", s_fbo);
}

/* ── SMAA 1x (three-pass morphological antialiasing) ────────────────────── */
/* Official SMAA reference implementation (MIT/Unlicense, attribution in
   SMAA_HLSL.H).  Pass 1 finds luma edges, pass 2 searches them and fetches
   blending weights from the Area/Search lookup textures, pass 3 blends the
   scene color along the edges.  Needs a GLES3/GL3 context (SMAA_GLSL_3). */

static const char *s_smaaEdgeVertSrc =
    "#version 300 es\n"
    "#define SMAA_GLSL_3\n"
    "#define SMAA_RT_METRICS vec4(uTexel.x, uTexel.y, 1.0 / uTexel.x, 1.0 / uTexel.y)\n"
    "uniform vec2 uTexel;\n"
    "in vec2 aPos;\n"
    "in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "out vec4 offset[3];\n"
    SMAA_HLSL_SRC
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    SMAAEdgeDetectionVS(aTexCoord, offset);\n"
    "}\n";

static const char *s_smaaEdgeFragSrc =
    "#version 300 es\n"
    "precision highp float;\n"
    "#define SMAA_GLSL_3\n"
    "#define SMAA_RT_METRICS vec4(uTexel.x, uTexel.y, 1.0 / uTexel.x, 1.0 / uTexel.y)\n"
    "uniform sampler2D uTex;\n"
    "uniform vec2 uTexel;\n"
    "in vec2 vTexCoord;\n"
    "in vec4 offset[3];\n"
    "out vec4 fragColor;\n"
    SMAA_HLSL_SRC
    "void main() {\n"
    "    fragColor = vec4(SMAALumaEdgeDetectionPS(vTexCoord, offset, uTex), 0.0, 0.0);\n"
    "}\n";

static const char *s_smaaWeightVertSrc =
    "#version 300 es\n"
    "#define SMAA_GLSL_3\n"
    "#define SMAA_RT_METRICS vec4(uTexel.x, uTexel.y, 1.0 / uTexel.x, 1.0 / uTexel.y)\n"
    "uniform vec2 uTexel;\n"
    "in vec2 aPos;\n"
    "in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "out vec2 pixcoord;\n"
    "out vec4 offset[3];\n"
    SMAA_HLSL_SRC
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    SMAABlendingWeightCalculationVS(aTexCoord, pixcoord, offset);\n"
    "}\n";

static const char *s_smaaWeightFragSrc =
    "#version 300 es\n"
    "precision highp float;\n"
    "#define SMAA_GLSL_3\n"
    "#define SMAA_RT_METRICS vec4(uTexel.x, uTexel.y, 1.0 / uTexel.x, 1.0 / uTexel.y)\n"
    "uniform sampler2D uEdges;\n"
    "uniform sampler2D uArea;\n"
    "uniform sampler2D uSearch;\n"
    "in vec2 vTexCoord;\n"
    "in vec2 pixcoord;\n"
    "in vec4 offset[3];\n"
    "out vec4 fragColor;\n"
    SMAA_HLSL_SRC
    "void main() {\n"
    "    fragColor = SMAABlendingWeightCalculationPS(vTexCoord, pixcoord, offset,\n"
    "                                                uEdges, uArea, uSearch,\n"
    "                                                vec4(0.0));\n"
    "}\n";

static const char *s_smaaBlendVertSrc =
    "#version 300 es\n"
    "#define SMAA_GLSL_3\n"
    "in vec2 aPos;\n"
    "in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "out vec4 offset;\n"
    SMAA_HLSL_SRC
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    SMAANeighborhoodBlendingVS(aTexCoord, offset);\n"
    "}\n";

static const char *s_smaaBlendFragSrc =
    "#version 300 es\n"
    "precision highp float;\n"
    "#define SMAA_GLSL_3\n"
    "uniform sampler2D uTex;\n"
    "uniform sampler2D uBlend;\n"
    "uniform vec2 uTexel;\n"
    "in vec2 vTexCoord;\n"
    "in vec4 offset;\n"
    "out vec4 fragColor;\n"
    GLSL_IMAGE_ADJUST
    SMAA_HLSL_SRC
    "void main() {\n"
    "    vec3 rgb = SMAANeighborhoodBlendingPS(vTexCoord, offset, uTex, uBlend).rgb;\n"
    "    fragColor = vec4(ImageAdjust(rgb), 1.0);\n"
    "}\n";

/* Compile the three SMAA programs and upload the lookup textures.  Called
   once after GL context creation; failure (e.g. a GL 2.1 desktop context
   that cannot compile the SMAA_GLSL_3 dialect) leaves the programs at 0 and
   the SMAA mode falls back to the plain draw. */
static void GL_SmaaInit(void) {
    GLint p;

    if (s_smaaEdgeProgram == 0) {
        GLuint v = CompileShader(GL_VERTEX_SHADER, s_smaaEdgeVertSrc);
        GLuint f = CompileShader(GL_FRAGMENT_SHADER, s_smaaEdgeFragSrc);
        if (v && f) {
            s_smaaEdgeProgram = LinkProgram(v, f);
            p = s_smaaEdgeProgram;
            s_smaaEdge_uTexel = glGetUniformLocation(p, "uTexel");
            s_smaaEdge_aPos = glGetAttribLocation(p, "aPos");
            s_smaaEdge_aTexCoord = glGetAttribLocation(p, "aTexCoord");
        }
        glDeleteShader(v);
        glDeleteShader(f);
    }
    if (s_smaaWeightProgram == 0) {
        GLuint v = CompileShader(GL_VERTEX_SHADER, s_smaaWeightVertSrc);
        GLuint f = CompileShader(GL_FRAGMENT_SHADER, s_smaaWeightFragSrc);
        if (v && f) {
            s_smaaWeightProgram = LinkProgram(v, f);
            p = s_smaaWeightProgram;
            s_smaaWeight_uRTMetrics = glGetUniformLocation(p, "uTexel");
            s_smaaWeight_aPos = glGetAttribLocation(p, "aPos");
            s_smaaWeight_aTexCoord = glGetAttribLocation(p, "aTexCoord");
        }
        glDeleteShader(v);
        glDeleteShader(f);
    }
    if (s_smaaBlendProgram == 0) {
        GLuint v = CompileShader(GL_VERTEX_SHADER, s_smaaBlendVertSrc);
        GLuint f = CompileShader(GL_FRAGMENT_SHADER, s_smaaBlendFragSrc);
        if (v && f) {
            s_smaaBlendProgram = LinkProgram(v, f);
            p = s_smaaBlendProgram;
            s_smaaBlend_uTexel = glGetUniformLocation(p, "uTexel");
            s_smaaBlend_aPos = glGetAttribLocation(p, "aPos");
            s_smaaBlend_aTexCoord = glGetAttribLocation(p, "aTexCoord");
            s_smaaAdj_uBrightness = glGetUniformLocation(p, "uBrightness");
            s_smaaAdj_uContrast = glGetUniformLocation(p, "uContrast");
            s_smaaAdj_uSaturation = glGetUniformLocation(p, "uSaturation");
        }
        glDeleteShader(v);
        glDeleteShader(f);
    }

    if (s_smaaAreaTex == 0) {
        glGenTextures(1, &s_smaaAreaTex);
        glBindTexture(GL_TEXTURE_2D, s_smaaAreaTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, SMAA_AREA_W, SMAA_AREA_H, 0,
                     GL_RG, GL_UNSIGNED_BYTE, SMAA_AreaTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (s_smaaSearchTex == 0) {
        glGenTextures(1, &s_smaaSearchTex);
        glBindTexture(GL_TEXTURE_2D, s_smaaSearchTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, SMAA_SEARCH_W, SMAA_SEARCH_H, 0,
                     GL_RED, GL_UNSIGNED_BYTE, SMAA_SearchTex);
        /* The vertical search wraps across rows: REPEAT is required in T. */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    GL_InvalidateGLState();
}

static void GL_SmaaTargetsDestroy(void) {
    if (s_smaaEdgeFbo) {
        glDeleteFramebuffers(1, &s_smaaEdgeFbo);
        s_smaaEdgeFbo = 0;
    }
    if (s_smaaEdgeTex) {
        glDeleteTextures(1, &s_smaaEdgeTex);
        s_smaaEdgeTex = 0;
    }
    if (s_smaaWeightFbo) {
        glDeleteFramebuffers(1, &s_smaaWeightFbo);
        s_smaaWeightFbo = 0;
    }
    if (s_smaaWeightTex) {
        glDeleteTextures(1, &s_smaaWeightTex);
        s_smaaWeightTex = 0;
    }
}

/* Edge/weight intermediate targets, engine-resolution, recreated on
   resolution changes alongside the other FBOs. */
static void GL_SmaaTargetsCreate(U32 w, U32 h) {
    GL_SmaaTargetsDestroy();
    s_smaaW = w;
    s_smaaH = h;
    if (w == 0 || h == 0)
        return;

    glGenTextures(1, &s_smaaEdgeTex);
    glBindTexture(GL_TEXTURE_2D, s_smaaEdgeTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &s_smaaEdgeFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_smaaEdgeFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s_smaaEdgeTex, 0);

    glGenTextures(1, &s_smaaWeightTex);
    glBindTexture(GL_TEXTURE_2D, s_smaaWeightTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &s_smaaWeightFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_smaaWeightFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s_smaaWeightTex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* Run the three passes over s_fboColorTex and draw the antialiased result
   into the currently bound framebuffer (the composite target the caller
   had bound).  bW/bH is the letterbox viewport of that target, restored
   for the final pass. */
static void GL_SmaaPresent(S32 bX, S32 bY, S32 bW, S32 bH) {
    U32 w = (U32)bW, h = (U32)bH;
    if (s_smaaEdgeFbo == 0 || s_smaaW != (U32)bW || s_smaaH != (U32)bH) {
        GL_SmaaTargetsCreate(w, h);
    }

    /* Pass 1: luma edges of the scene into the edge target. */
    glBindFramebuffer(GL_FRAMEBUFFER, s_smaaEdgeFbo);
    glViewport(0, 0, s_smaaW, s_smaaH);
    glUseProgram(s_smaaEdgeProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_fboColorTex);
    glUniform1i(glGetUniformLocation(s_smaaEdgeProgram, "uTex"), 0);
    glUniform2f(s_smaaEdge_uTexel, 1.0f / (GLfloat)s_smaaW,
                1.0f / (GLfloat)s_smaaH);
    GL_BindArrayBuffer(s_fboVbo);
    glEnableVertexAttribArray(s_smaaEdge_aPos);
    glVertexAttribPointer(s_smaaEdge_aPos, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(s_smaaEdge_aTexCoord);
    glVertexAttribPointer(s_smaaEdge_aTexCoord, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(s_smaaEdge_aPos);
    glDisableVertexAttribArray(s_smaaEdge_aTexCoord);

    /* Pass 2: search the edges and fetch blending weights. */
    glBindFramebuffer(GL_FRAMEBUFFER, s_smaaWeightFbo);
    glUseProgram(s_smaaWeightProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_smaaEdgeTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_smaaAreaTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, s_smaaSearchTex);
    glUniform1i(glGetUniformLocation(s_smaaWeightProgram, "uEdges"), 0);
    glUniform1i(glGetUniformLocation(s_smaaWeightProgram, "uArea"), 1);
    glUniform1i(glGetUniformLocation(s_smaaWeightProgram, "uSearch"), 2);
    glUniform2f(s_smaaWeight_uRTMetrics, 1.0f / (GLfloat)s_smaaW,
                1.0f / (GLfloat)s_smaaH);
    GL_BindArrayBuffer(s_fboVbo);
    glEnableVertexAttribArray(s_smaaWeight_aPos);
    glVertexAttribPointer(s_smaaWeight_aPos, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(s_smaaWeight_aTexCoord);
    glVertexAttribPointer(s_smaaWeight_aTexCoord, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(s_smaaWeight_aPos);
    glDisableVertexAttribArray(s_smaaWeight_aTexCoord);

    /* Pass 3: blend the scene along the edges into the composite target
       (already bound by the caller). */
    GLint smaaDst = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &smaaDst);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)smaaDst);
    glViewport(bX, bY, bW, bH);
    glUseProgram(s_smaaBlendProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_fboColorTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_smaaWeightTex);
    glUniform1i(glGetUniformLocation(s_smaaBlendProgram, "uTex"), 0);
    glUniform1i(glGetUniformLocation(s_smaaBlendProgram, "uBlend"), 1);
    glUniform2f(s_smaaBlend_uTexel, 1.0f / (GLfloat)s_texWidth,
                1.0f / (GLfloat)s_texHeight);
    GL_UploadImageAdjust(s_smaaAdj_uBrightness, s_smaaAdj_uContrast,
                         s_smaaAdj_uSaturation);
    GL_BindArrayBuffer(s_fboVbo);
    glEnableVertexAttribArray(s_smaaBlend_aPos);
    glVertexAttribPointer(s_smaaBlend_aPos, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(s_smaaBlend_aTexCoord);
    glVertexAttribPointer(s_smaaBlend_aTexCoord, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(s_smaaBlend_aPos);
    glDisableVertexAttribArray(s_smaaBlend_aTexCoord);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    GL_BindArrayBuffer(0);
    glUseProgram(0);
    GL_InvalidateGLState();
}

/* ── Antialiasing (MSAA render target + FXAA post-process) ───────────────── */

static void GL_DestroyMSAAFbo(void) {
    if (s_msaaResolveFbo) {
        glDeleteFramebuffers(1, &s_msaaResolveFbo);
        s_msaaResolveFbo = 0;
    }
    if (s_msaaFbo) {
        glDeleteFramebuffers(1, &s_msaaFbo);
        s_msaaFbo = 0;
    }
    if (s_msaaColorTex) {
        glDeleteTextures(1, &s_msaaColorTex);
        s_msaaColorTex = 0;
    }
    if (s_msaaColorRb) {
        glDeleteRenderbuffers(1, &s_msaaColorRb);
        s_msaaColorRb = 0;
    }
    if (s_msaaDepthRb) {
        glDeleteRenderbuffers(1, &s_msaaDepthRb);
        s_msaaDepthRb = 0;
    }
    s_msaaInTile = 0;
    s_msaaW = 0;
    s_msaaH = 0;
}

static void GL_CreateMSAAFbo(U32 w, U32 h, S32 samples) {
    GL_DestroyMSAAFbo();
    const U32 targetW = s_stereoDoubleWide ? w * 2 : w;
    const U32 targetH = h;
    glGenFramebuffers(1, &s_msaaFbo);
    glGenRenderbuffers(1, &s_msaaDepthRb);
    glBindFramebuffer(GL_FRAMEBUFFER, s_msaaFbo);

    S32 inTile = 0;
#if GL_INTILE_MSAA
    if (GL_MSAATextureExtAvailable() &&
        p_glFramebufferTexture2DMultisampleEXT != NULL) {
        /* A dedicated resolve texture, attached only to this FBO.  Adreno
         * resolves it when it is next sampled or blitted; attaching
         * s_fboColorTex here as well left the resolve undefined. */
        glGenTextures(1, &s_msaaColorTex);
        glBindTexture(GL_TEXTURE_2D, s_msaaColorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, targetW, targetH, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
        p_glFramebufferTexture2DMultisampleEXT(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            s_msaaColorTex, 0, samples);
        s_msaaColorRb = 0;
        inTile = 1;
    } else
#endif
    {
        glGenRenderbuffers(1, &s_msaaColorRb);
        glBindRenderbuffer(GL_RENDERBUFFER, s_msaaColorRb);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8,
                                         targetW, targetH);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, s_msaaColorRb);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, s_msaaDepthRb);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                     GL_DEPTH_COMPONENT16, targetW, targetH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, s_msaaDepthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log_Warn("[GL] MSAA FBO incomplete (samples=%d intile=%d)", samples,
                 inTile);
        GL_DestroyMSAAFbo();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (inTile) {
            /* The driver rejected the multisample texture attachment.  Retry
             * with the explicit renderbuffer path and keep it for the
             * session; the extension cannot be trusted here. */
            s_msaaEXTAvailable = 0;
            GL_CreateMSAAFbo(w, h, samples);
        }
        return;
    }

    if (inTile) {
        /* Single-sample view of the same texture: the OpenXR present blits
         * from this FBO with a linear filter, which a multisampled read
         * framebuffer would reject.  It does not copy anything; the tiler
         * resolves the texture when this view is read. */
        glGenFramebuffers(1, &s_msaaResolveFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, s_msaaResolveFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, s_msaaColorTex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE) {
            Log_Warn("[GL] MSAA resolve FBO incomplete (intile); using "
                     "explicit resolve");
            GL_DestroyMSAAFbo();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            s_msaaEXTAvailable = 0;
            GL_CreateMSAAFbo(w, h, samples);
            return;
        }

        /* Driver smoke test.  Some Adreno drivers accept the multisample
         * texture attachment and then return stale/black data when the
         * resolve view is read.  Clear the multisample target to a known
         * colour and read one pixel through the single-sample view; if the
         * resolve did not happen, use the explicit path for this session
         * instead of showing black frames. */
        GLfloat oldClear[4];
        GLboolean oldMask[4];
        GLboolean scissorOn = glIsEnabled(GL_SCISSOR_TEST);
        GLint oldScissor[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClear);
        glGetBooleanv(GL_COLOR_WRITEMASK, oldMask);
        glGetIntegerv(GL_SCISSOR_BOX, oldScissor);
        if (scissorOn)
            glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindFramebuffer(GL_FRAMEBUFFER, s_msaaFbo);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, s_msaaResolveFbo);
        U8 probe[4] = {0, 0, 0, 0};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, probe);
        /* Do not leave the probe colour behind: a menu frame can present the
         * scene target before the next frame clears it. */
        glBindFramebuffer(GL_FRAMEBUFFER, s_msaaFbo);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(oldClear[0], oldClear[1], oldClear[2], oldClear[3]);
        glColorMask(oldMask[0], oldMask[1], oldMask[2], oldMask[3]);
        glScissor(oldScissor[0], oldScissor[1], oldScissor[2], oldScissor[3]);
        if (scissorOn)
            glEnable(GL_SCISSOR_TEST);
        if (probe[0] < 250 || probe[1] > 5 || probe[2] > 5) {
            Log_Warn("[GL] resolve-in-tile probe read %u,%u,%u; using "
                     "explicit resolve", probe[0], probe[1], probe[2]);
            GL_DestroyMSAAFbo();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            s_msaaEXTAvailable = 0;
            GL_CreateMSAAFbo(w, h, samples);
            return;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    s_msaaInTile = inTile;
    s_msaaW = targetW;
    s_msaaH = targetH;
    Log_Info("[GL] MSAA FBO created %ux%u samples=%d (%s)", targetW, targetH,
             samples, s_msaaInTile ? "resolve-in-tile" : "explicit resolve");
}

/* A double-wide target bypasses the per-eye scene composite.  MSAA (not
 * FXAA/SMAA) is required because those post-processes need a per-eye
 * single-sample composite.  Brightness/contrast/saturation do not force the
 * fallback: the final pair-to-swapchain pass applies them to the whole
 * composite.  The environment escape hatch lets a device run compare the old
 * path without a rebuild. */
static S32 GL_StereoDoubleWideAllowed(void) {
    const char *reason = NULL;
    if (getenv("LBA2_NO_STEREO_DW") != NULL)
        reason = "LBA2_NO_STEREO_DW";
    else if (!DisplayStereo3D)
        reason = "DisplayStereo3D=0";
    else if (!OpenXR_IsActive())
        reason = "OpenXR inactive";
    else if (!OpenXR_GetStereoEnabled())
        reason = "stereo disabled";
    else if (s_antiAlias != 2 && s_antiAlias != 3)
        reason = "AA not MSAA2/4";

    if (reason != NULL) {
        static const char *s_lastReason = NULL;
        if (reason != s_lastReason) {
            Log_Info("[GL] Double-wide unavailable: %s (stereo=%d openxr=%d "
                     "enabled=%d aa=%d bright=%d contrast=%d sat=%d)",
                     reason, DisplayStereo3D, OpenXR_IsActive() ? 1 : 0,
                     OpenXR_GetStereoEnabled() ? 1 : 0, s_antiAlias,
                     DisplayBrightness, DisplayContrast, DisplaySaturation);
            s_lastReason = reason;
        }
        return 0;
    }
    return 1;
}

static void GL_SetStereoTargetMode(S32 doubleWide) {
    if (!s_initialized || !Window_GLMakeCurrent())
        return;
    doubleWide = doubleWide ? 1 : 0;
    if (doubleWide == s_stereoDoubleWide)
        return;

    U32 w = s_fboWidth;
    U32 h = s_fboHeight;
    if (w == 0 || h == 0) {
        w = s_texWidth * s_renderQuality;
        h = s_texHeight * s_renderQuality;
    }
    if (w == 0 || h == 0)
        return;

    Log_Info("[GL] Stereo scene target: %s",
             doubleWide ? "double-wide" : "per-eye");

    /* Full-redraw stereo cannot consume the incremental snapshot or the
     * horizon capture, and it does not use the mono present target.  Drop
     * them before allocating the double-wide scene so the transition has no
     * extra full-resolution peak. */
    if (doubleWide) {
        GL_DestroySnapshot();
        GL_DestroyHorizon();
        GL_DestroyPresentFBO();
    }

    s_stereoDoubleWide = doubleWide;
    GL_CreateFBO(w, h);
    if (doubleWide && s_fbo == 0) {
        Log_Warn("[GL] Double-wide scene FBO failed; using per-eye targets");
        s_stereoDoubleWide = 0;
        GL_CreateFBO(w, h);
    }

    if (s_antiAlias == 2 || s_antiAlias == 3) {
        GL_CreateMSAAFbo(w, h, (s_antiAlias == 3) ? 4 : 2);
    } else {
        GL_DestroyMSAAFbo();
    }

    if (s_stereoDoubleWide) {
        GL_DestroyStereoEyeFBOs();
    } else {
        GL_CreateStereoEyeFBOs(w, h);
    }

    s_frameActive = 0;
    s_fboHasTerrain = 0;
    s_sceneEyeX = 0;
    s_stereoPairReady = 0;
    s_stereoEye1SceneDirect = 0;
    GL_InvalidateGLState();
    GL_LogSurfaceBudget(s_stereoDoubleWide ? "stereo-double"
                                          : "stereo-per-eye");
}

/* Blit (resolve) from the MSAA FBO to the regular s_fbo.  Snapshot frames
   need both attachments in a single-sample FBO; the stereo present path never
   calls this and reads the tiled texture directly. */
static void GL_ResolveMSAA(void) {
    if (s_msaaFbo == 0 || s_msaaW == 0)
        return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_msaaFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_fbo);
    glBlitFramebuffer(0, 0, s_msaaW, s_msaaH, 0, 0, s_msaaW, s_msaaH,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

/* Color-only variant for the present paths.  In explicit mode the composite
   samples s_fbo's color texture, so the color half must be resolved here; the
   depth half is refreshed separately by consumers that read it.  In-tile mode
   has no color blit: the composite samples s_msaaColorTex and the driver
   resolves it as part of the read. */
static void GL_ResolveMSAAColor(void) {
    if (s_msaaInTile)
        return;
    if (s_msaaFbo == 0 || s_msaaW == 0)
        return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_msaaFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_fbo);
    glBlitFramebuffer(0, 0, s_msaaW, s_msaaH, 0, 0, s_msaaW, s_msaaH,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void GL_SetAntiAlias(S32 mode) {
    if (mode < 0) mode = 0;
    if (mode > 3) mode = 3;
#ifdef __ANDROID__
    /* 4xMSAA quadruples the tile bandwidth on mobile GPUs and stalled the
     * whole frame loop; the AA menu offers SMAA in its place and a cfg
     * value of 3 clamps to MSAA×2 (same reasoning as the Android
     * RenderQuality clamp). */
    if (mode == 3) {
        Log_Info("[GL] Android: AntiAlias 3 (MSAA×4) clamped to 2");
        mode = 2;
    }
#endif
    if (s_antiAlias == mode) return;
    Log_Info("[GL] SetAntiAlias: %d -> %d", s_antiAlias, mode);
    s_antiAlias = mode;
    if (!s_initialized || !Window_GLMakeCurrent())
        return;

    /* A target-mode change recreates the scene FBOs itself.  This also
     * covers leaving MSAA for FXAA/SMAA, where the double-wide scene cannot
     * be used because the post-process needs a per-eye composite. */
    {
        S32 wantDoubleWide = GL_StereoDoubleWideAllowed();
        if (wantDoubleWide != s_stereoDoubleWide) {
            GL_SetStereoTargetMode(wantDoubleWide);
            return;
        }
    }

    if ((s_antiAlias == 2 || s_antiAlias == 3) && s_fbo) {
        GLint samples = (s_antiAlias == 3) ? 4 : 2;
        GL_CreateMSAAFbo(s_texWidth * s_renderQuality,
                         s_texHeight * s_renderQuality, samples);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, s_fboColorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, s_fboDepthRbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GL_DestroyMSAAFbo();
        GL_InvalidateGLState();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   Phase 3 — CLUT-based Gouraud shading
   ═══════════════════════════════════════════════════════════════════════════ */

/* ── Texture uploads ────────────────────────────────────────────────────── */


static void GL_UploadPalette(void) {
    /* Uploaded once per frame per LUT pointer, not per polygon — same
       reasoning as GL_UploadCLUT above. */
    static U32 s_palUpSeq = 0;
    static const U32 *s_palUpPtr = NULL;
    const U32 *lut = GL_GetPaletteLUT();
    if (lut == NULL) {
        return;
    }
    if (s_palUpSeq == s_frameSeq && s_palUpPtr == lut) {
        return;
    }

    /* Copy palette and set alpha=0 for palette index 0 (transparent colour
       in the Log buffer — sprites/UI use colour 0 as chroma key). */
    U32 paletteCopy[256];
    memcpy(paletteCopy, lut, sizeof(paletteCopy));
    paletteCopy[0] &= 0x00FFFFFF; /* alpha → 0 */

    GL_BindTexUnit(0, s_paletteTex);
#if defined(__ANDROID__)
    {
        U32 paletteGL[256];
        GL_SwizzleBGRAtoRGBA(paletteGL, paletteCopy, 256);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_UPLOAD_FORMAT,
                        GL_UNSIGNED_BYTE, paletteGL);
    }
#else
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_UPLOAD_FORMAT,
                    GL_UNSIGNED_BYTE, paletteCopy);
#endif
    GL_BindTexUnit(0, 0);

    /* Invalidate cached reverse lookup so it gets rebuilt on next readback. */
    s_revLutHashValid = 0;
    s_palUpSeq = s_frameSeq;
    s_palUpPtr = lut;
}

static void GL_UploadCLUT(void) {
    /* The fog table is uploaded once per frame per pointer, not per
       polygon.  GL_RenderTriangleList runs for every terrain tile, and a
       64 KB glTexSubImage2D per polygon meant ~500 MB of redundant uploads
       per pass (hundreds of ms on mobile GPUs).  The game only modifies
       the fog table in-place between scenes, so per-frame is sufficient. */
    static U32 s_clutUpSeq = 0;
    static const U8 *s_clutUpPtr = NULL;
    if (PtrCLUTFog == NULL) {
        return;
    }
    if (s_clutUpSeq == s_frameSeq && s_clutUpPtr == PtrCLUTFog) {
        return;
    }

    /* The table only actually changes between scenes (faded in gradually);
       skip the 64 KB transfer on frames where no byte moved. */
    static U8 s_clutShadow[256 * 256];
    if (s_clutUpPtr == PtrCLUTFog &&
        memcmp(s_clutShadow, PtrCLUTFog, sizeof(s_clutShadow)) == 0) {
        s_clutUpSeq = s_frameSeq;
        return;
    }

    /* Upload the full 64 KB fog table (256×256) once. SetCLUT selects a
       16-row block within it; the gouraud value's high byte indexes into
       the correct rows directly via the shader. */
    GL_BindTexUnit(0, s_clutTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, PtrCLUTFog);
    GL_BindTexUnit(0, 0);

    memcpy(s_clutShadow, PtrCLUTFog, sizeof(s_clutShadow));
    s_boundCLUT = PtrCLUTFog;
    s_clutUpSeq = s_frameSeq;
    s_clutUpPtr = PtrCLUTFog;
}

static void GL_SetAtlasTexParams(GLuint tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 256, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* Upload a (w,h) sub-rect of a texture page.  The page lives in memory at a
   256-byte row stride, so the upload must read its rows 256 bytes apart —
   GL_UNPACK_ROW_LENGTH is GLES 3.0+ (the project's floor is GLES 2.0), and
   without it the only correct source is the full 256x256 page, which this
   falls back to by forcing w/h. */
static void GL_UploadAtlasRect(GLuint tex, const void *map, U32 *w, U32 *h) {
    GL_BindTexUnit(0, tex);
#ifndef GL_UNPACK_ROW_LENGTH
    *w = 256;
    *h = 256;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, map);
#else
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 256);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)*w, (GLsizei)*h,
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, map);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
}

/* Make the texture for texture page `map` current on the GL_TEXTURE0 unit:
   reuse the cached texture when the page content is up to date, (re)upload
   into it otherwise, evicting the oldest entry when the pointer is new.
   NULL falls back to the legacy shared s_atlasTex (never sampled). */
static GLuint GL_BindAtlasFor(const void *map, U16 mask) {
    if (map == NULL) {
        GL_BindTexUnit(0, s_atlasTex);
        return s_atlasTex;
    }

    /* Only the region the RepMask can sample is ever read: a (w,h)-sized
       page serves (w,h)-wrapped UVs, so uploading w*h bytes instead of the
       full 64 KB page cuts upload bandwidth by up to 64x on the per-offset
       uploads inside the flush path. */
    U32 w = (U32)(mask & 0xFF) + 1;
    U32 h = (U32)(mask >> 8) + 1;

    for (S32 i = 0; i < ATLAS_CACHE_SIZE; i++) {
        if (s_atlasTexCache[i].ptr == map) {
            AtlasTexEntry *e = &s_atlasTexCache[i];
            if (w > e->w || h > e->h) {
                /* Region grew (offset reused with a larger mask): upload it
                   in full, covering both the old and the new area. */
                w = (w > e->w) ? w : e->w;
                h = (h > e->h) ? h : e->h;
                GL_UploadAtlasRect(e->tex, map, &w, &h);
                e->w = (U16)w;
                e->h = (U16)h;
                e->ver = s_atlasContentVersion;
            } else if (e->ver != s_atlasContentVersion) {
                U32 uw = e->w, uh = e->h;
                GL_UploadAtlasRect(e->tex, map, &uw, &uh);
                e->w = (U16)uw;
                e->h = (U16)uh;
                e->ver = s_atlasContentVersion;
            }
            GL_BindTexUnit(0, e->tex);
            return e->tex;
        }
    }

    AtlasTexEntry *e = &s_atlasTexCache[s_atlasTexNext];
    s_atlasTexNext = (s_atlasTexNext + 1) % ATLAS_CACHE_SIZE;
    if (e->tex == 0) {
        glGenTextures(1, &e->tex);
        GL_SetAtlasTexParams(e->tex);
        GL_InvalidateGLState();
    }
    GL_UploadAtlasRect(e->tex, map, &w, &h);
    e->ptr = map;
    e->w = (U16)w;
    e->h = (U16)h;
    e->ver = s_atlasContentVersion;
    GL_BindTexUnit(0, e->tex);
    return e->tex;
}

/* ── Texture array for full-page polys (the body-pass flush win) ───────────
   Every textured poly whose RepMask is 0xFFFF addresses a full 256×256 page
   — bodies, decors, sky/sea.  Those polys carry 0x8000|layer in RepMask and
   the flush samples GL_TEXTURE_2D_ARRAY unit 3 through a GLSL-3 variant of
   the CLUT program, so consecutive bodies with different pages stop
   splitting batches: the ~200 per-frame texture-page flushes on Adreno
   collapse to the true state changes.  Sub-rect polys (terrain tiles) keep
   RepMask masks and the legacy 2D path. */
/* 512 layers = 32 MB (16 MB on the 256-layer Android budget): must exceed
   the distinct pages a scene touches per frame, or LRU eviction steals
   layers that already-queued vertices reference (white body patches). */
#ifdef __ANDROID__
#define GL_ARR_LAYERS 256
#else
#define GL_ARR_LAYERS 512
#endif
typedef struct {
    const void *map;
    U16 w; /* region size: (mask&0xFF)+1, power of two */
    U16 h; /* region size: (mask>>8)+1, power of two */
    U32 ver;
    S32 layer;
    U32 lastUse;
} GL_ArrSlot;
static GL_ArrSlot s_arrSlots[GL_ARR_LAYERS];
static U32 s_arrUseCounter = 0;
static GLuint s_arrayTex = 0;
static U8 s_arrStage[256 * 256];

/* The array layer holds the (w,h) region TILED to 256×256, so the shader's
   mod-256 UV wrap reproduces the SW rasterizer's RepMask wrap: UVs pushed
   outside the region land on the next tile copy instead of the neighbouring
   texture block's bytes (which read as garbage silhouettes).  Slot identity
   is (map, w, h) — the same keying the 2D atlas cache proves stable — and
   the copy only reads rows the SW renderer reads too, so it never overruns
   the bank. */
static GL_ArrSlot *GL_ArraySlotFor(const void *map, U32 w, U32 h) {
    GL_ArrSlot *slot = NULL;
    GL_ArrSlot *lru = NULL;
    S32 i;

    s_arrUseCounter++;
    for (i = 0; i < s_arrCount; i++) {
        if (s_arrSlots[i].map == map && s_arrSlots[i].w == w &&
            s_arrSlots[i].h == h) {
            slot = &s_arrSlots[i];
            break;
        }
        if (lru == NULL || s_arrSlots[i].lastUse < lru->lastUse) {
            lru = &s_arrSlots[i];
        }
    }
    if (slot == NULL) {
        if (s_arrCount < GL_ARR_LAYERS) {
            slot = &s_arrSlots[s_arrCount++];
        } else {
            slot = lru;
        }
        slot->map = map;
        slot->w = (U16)w;
        slot->h = (U16)h;
        slot->ver = 0; /* force upload */
        slot->layer = (S32)(slot - s_arrSlots);
    }
    slot->lastUse = s_arrUseCounter;
    if (s_arrCount >= GL_ARR_LAYERS) {
        s_arrEvictions++; /* layer stolen from a map (may be in flight) */
    }
    return slot;
}

static void GL_ArrayEnsureTexture(void) {
    if (s_arrayTex != 0) {
        return;
    }
    glGenTextures(1, &s_arrayTex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, s_arrayTex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, 256, 256, GL_ARR_LAYERS, 0,
                 GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

/* Upload stale layers as tiled regions, then bind the array on unit 3.
   Called from FlushActiveBatch on array batches. */
static void GL_ArrayBind(void) {
    S32 i;
    GL_ArrayEnsureTexture();
    for (i = 0; i < s_arrCount; i++) {
        if (s_arrSlots[i].ver != s_atlasContentVersion &&
            s_arrSlots[i].map != NULL) {
            U32 w = s_arrSlots[i].w;
            U32 h = s_arrSlots[i].h;
            U32 x, y;
            const U8 *base = (const U8 *)s_arrSlots[i].map;
            for (y = 0; y < 256; y++) {
                const U8 *srow = base + (U32)(y % h) * 256;
                U8 *drow = s_arrStage + y * 256;
                for (x = 0; x < 256; x += w) {
                    memcpy(drow + x, srow, w);
                }
            }
            glBindTexture(GL_TEXTURE_2D_ARRAY, s_arrayTex);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, s_arrSlots[i].layer,
                            256, 256, 1, GL_RED, GL_UNSIGNED_BYTE, s_arrStage);
            s_arrSlots[i].ver = s_atlasContentVersion;
        }
    }
    /* GL_BindTexUnit binds the GL_TEXTURE_2D target; the array sampler
       reads the GL_TEXTURE_2D_ARRAY target on the unit, so bind the array
       texture directly under its own target. */
    if (s_curActiveUnit != 3) {
        glActiveTexture(GL_TEXTURE3);
        s_curActiveUnit = 3;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, s_arrayTex);
}

void GL_ArrayWindowBegin(void) {
    s_arrayWindow = 1;
}

void GL_ArrayWindowEnd(void) {
    s_arrayWindow = 0;
}

/* ── Batch management ───────────────────────────────────────────────────── */

static void FlushActiveBatch(void) {

    if (s_batchVertCount == 0) {
        return;
    }

    U64 profT0 = SDL_GetPerformanceCounter();

    GL_BatchFlushCount++;
    s_profFlushes++;
    s_profTris += s_batchVertCount / 3;

    /* Bind FBO on the first flush of each frame.  The FBO clear is now
       done by GL_ClearFBO() at the start of terrain rendering, so that
       frames with no terrain (e.g. Twinsen standing still) preserve the
       previous frame's 3D content in the FBO. */
    if (!s_frameActive) {
        s_frameActive = 1;
        if (s_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    /* Upload batch vertices.  Default is the classic orphan + fill: the
       storage is re-specified per flush so the driver hands out fresh
       backing while earlier draws may still be pending.  LBA2_RING=1 opts
       into a rotating glBufferSubData ring instead (BATCH_RING_SLOTS
       batches deep, allocated at init) to avoid re-specifying storage
       hundreds of times per frame on drivers that charge for it — opt-in
       until its rendering is verified on the target driver. */
    GL_BindArrayBuffer(s_batchVBO);
    {
        const U32 bytes = s_batchVertCount * BATCH_VERT_BYTES;
        static S32 s_ring = -1;
        if (s_ring < 0) {
            /* Opt-in: on Mesa/radeonsi, ring submission corrupted frames
               even with the wrap fence (see LBA2_RING).  The orphan path
               stays the default until a driver-safe design exists. */
            s_ring = getenv("LBA2_RING") ? 1 : 0;
        }
        if (s_ring) {
            if (s_batchRingOffset + bytes > s_batchRingSize) {
                /* The ring is full: reusing the storage while earlier draws
                   still read it corrupts their vertex data (blinking
                   terrain).  Fence the point every previous wrap ended at
                   and wait for the GPU to drain the whole ring before
                   re-filling it — in practice the GPU reached this fence
                   several frames ago, so the wait never stalls. */
                if (s_ringWrapFence != 0) {
                    /* Covers every draw submitted up to the previous wrap:
                       the slots being reused were written before it, so the
                       GPU has long consumed them. */
                    glClientWaitSync(s_ringWrapFence, 0, UINT64_MAX);
                    glDeleteSync(s_ringWrapFence);
                }
                s_ringWrapFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,
                                              0);
                s_batchRingOffset = 0;
            }
            glBufferSubData(GL_ARRAY_BUFFER, s_batchRingOffset, bytes,
                            s_batchVerts);
            s_batchRingBase = s_batchRingOffset;
            s_batchRingOffset = (s_batchRingOffset + bytes + 255) & ~(U32)255;
        } else {
            /* Attrib pointers reference the buffer object, not its storage:
               they survive this orphan and stay valid at offset 0, so the
               12-call attrib re-spec only runs when something actually
               invalidates them. */
            glBufferData(GL_ARRAY_BUFFER, bytes, s_batchVerts,
                         GL_STREAM_DRAW);
            s_batchRingBase = 0;
        }
    }

    /* Use CLUT shader (array variant for full-page body batches) */
    GLuint prog = (s_batchArray && s_clut3Program != 0) ? s_clut3Program
                                                        : s_clutProgram;
    GL_UseProgramCached(prog);

    if (s_batchArray && s_clut3Program != 0) {
        /* Array variant: per-batch uniforms set directly — sharing the
           legacy cache banks would need per-program keys for ~20 batches
           per frame. */
        glUniform2f(s_clut3_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                    2.0f / (GLfloat)s_texHeight);
        glUniform2f(s_clut3_uAtlasScale, 256.0f, 256.0f);
        glUniform1f(s_clut3_uCLUTBaseRow, s_batchCLUTBaseRow);
        glUniform1f(s_clut3_uScaledFogNear, s_batchScaledFogNear);
        glUniform1f(s_clut3_uFogRowScale, s_batchFogRowScale);
        glUniform1i(s_clut3_uBilinear, s_bilinearCLUT);
        GL_ArrayBind();
    } else {
    /* Set screen-scale uniform: maps pixel coords → clip space */
    {
        GLfloat sx = 2.0f / (GLfloat)s_texWidth;
        GLfloat sy = 2.0f / (GLfloat)s_texHeight;
        if (s_ussX != sx || s_ussY != sy) {
            glUniform2f(s_clut_uScreenScale, sx, sy);
            s_ussX = sx;
            s_ussY = sy;
        }
    }
    }

    /* Shading mode, alpha/chroma/perspective bits and flat colour now ride
       per vertex in aMaterial, so polygons that differ only in those share
       one draw call.  Blending stays a per-draw state: TRANS batches blend,
       opaque/TRAME ones do not — the flush trigger keeps them apart. */
    if (!(s_batchArray && s_clut3Program != 0)) {
        if (s_bilinearCached != s_bilinearCLUT) {
            glUniform1i(s_clut_uBilinear, s_bilinearCLUT);
            s_bilinearCached = s_bilinearCLUT;
        }
    }
    GL_SetBlendFunc((s_currentMatFlags & 8) != 0, GL_SRC_ALPHA,
                    GL_ONE_MINUS_SRC_ALPHA); /* alphaMode 1 (TRANS) */

    /* Batch-start state that the shader still reads as uniforms. */
    if (!(s_batchArray && s_clut3Program != 0)) {
        if (s_clutRowCached != s_batchCLUTBaseRow) {
            glUniform1f(s_clut_uCLUTBaseRow, s_batchCLUTBaseRow);
            s_clutRowCached = s_batchCLUTBaseRow;
        }
        if (s_fogNearCached != s_batchScaledFogNear) {
            glUniform1f(s_clut_uScaledFogNear, s_batchScaledFogNear);
            s_fogNearCached = s_batchScaledFogNear;
        }
        if (s_fogScaleCached != s_batchFogRowScale) {
            glUniform1f(s_clut_uFogRowScale, s_batchFogRowScale);
            s_fogScaleCached = s_batchFogRowScale;
        }
    }

    /* Bind textures to units 0/1/2 (skipped when already bound).  Sampler
       unit assignments are program state, set once at init.  Textured
       batches sample the page the batch was recorded with; flat/gouraud
       batches never sample it. */
    if (s_batchArray && s_clut3Program != 0) {
        /* The array program samples uAtlasArr on unit 3 (bound with the
           stale-layer sync above); unit 0 stays a valid dummy. */
        GL_BindTexUnit(0, s_atlasTex);
    } else if (s_batchTextured && s_batchAtlas != NULL) {
        GL_BindTexUnit(0, GL_BindAtlasFor(s_batchAtlas, s_batchMaxMask));
        s_batchMaxMask = 0; /* consumed: next batch starts with a clean mask */
    } else {
        GL_BindTexUnit(0, s_atlasTex);
    }
    if (!(s_batchArray && s_clut3Program != 0)) {
        if (s_atlasScaleX != 256.0f || s_atlasScaleY != 256.0f) {
            glUniform2f(s_clut_uAtlasScale, 256.0f, 256.0f);
            s_atlasScaleX = 256.0f;
            s_atlasScaleY = 256.0f;
        }
    }

    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    /* Set up vertex attributes — 16 bytes per vertex: XE YE U V Light Z
       RepMask Material.  Re-specified whenever the ring slot moves (every
       flush) or an outside path dirtied the attribs. */
    if (s_clutAttribsDirty ||
        s_clutAttribsBase != (GLintptr)s_batchRingBase) {
        const GLvoid *ringBase = (const GLvoid *)(uintptr_t) s_batchRingBase;
        glEnableVertexAttribArray(s_clut_aPos);
        glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 0));
        glEnableVertexAttribArray(s_clut_aTexCoord);
        glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE,
                              16, (const void *)((const U8 *)ringBase + 4));
        glEnableVertexAttribArray(s_clut_aLight);
        glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 8));
        glEnableVertexAttribArray(s_clut_aZ);
        glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 10));
        glEnableVertexAttribArray(s_clut_aRepMask);
        glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE,
                              16, (const void *)((const U8 *)ringBase + 12));
        glEnableVertexAttribArray(s_clut_aMaterial);
        glVertexAttribPointer(s_clut_aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE,
                              16, (const void *)((const U8 *)ringBase + 14));
        s_clutAttribsDirty = 0;
        s_clutAttribsBase = (GLintptr)s_batchRingBase;
    }

    /* Depth testing: enable for ALL valid types (0–24).

       The SW rasterizer occludes non-OBJ_ZBUFFER objects (NPCs, sprites)
       behind terrain via ZBufBoxOverWrite2(): a post-process that restores
       closer terrain pixels over the already-rendered object.  The GL
       equivalent is a depth test during rendering: the depth buffer still
       holds terrain Z values from the prior AffGrilleExt() pass.

       GetZO3/GetZO4 always compute Pt_ZO (regardless of
       Fill_Flag_ZBuffer), so all body polygons have valid Z values for
       depth comparison.  This includes FLAT/SOLID types 0–7 which are
       common in NPC body models and decoration objects.

       Depth-write is gated separately:
       - Types 16–24 always write (they are inherently Z-buffered).
       - Types 0–15 write only when Fill_Flag_ZBuffer was TRUE at batch
         start (terrain overlays/base rendered under FILL_POLY_FOG_ZBUFFER).
       - Types 0–15 without Z-buffered fillers (NPC path) Z-test only —
         matches SW ZBufBoxOverWrite2 which reads terrain Z but never
         writes object Z into the buffer.
        - Fill_Flag_NZW suppresses write for animated polys and transparent
          objects that should read but not write depth.
        - Alpha blending suppresses write (TRANS/TRAME translucency).

        NZW batches use GL_ALWAYS only for the coplanar animated-polys
        overlays (type 24, sea) — they sit on the terrain surface and
        GEQUAL at far distances z-fights due to 16-bit depth precision.
        Body polygons under FILL_POLY_FOG_NZW keep GEQUAL so they are
        correctly occluded by closer terrain. */
    if (s_currentTypePoly <= POLY_TEXTURE_Z_FOG) {
        GL_SetDepthTest(1);
        GL_SetDepthFunc(
            (s_batchNZW && s_currentTypePoly == POLY_TEXTURE_Z_FOG) ? GL_ALWAYS
                                                                    : GL_GEQUAL);

        if (s_batchZBuffer || s_currentTypePoly >= POLY_TEXTURE_Z) {
            /* Z-buffered path: write depth unless blending.  NZW opaque
               batches (bodies) ALSO write: the FBO depth then contains
               model geometry, which the Log-layer sprite occlusion
               (PtrZBuffer readback) needs — sprites must hide behind
               nearer models, not just terrain.  Blended batches (TRANS/
               TRAME) keep write-off so translucency never clips. */
            static S32 s_noBodyDWrite = -1;
            if (s_noBodyDWrite < 0) {
                s_noBodyDWrite = getenv("LBA2_NO_BODY_DWRITE") ? 1 : 0;
            }
            GL_SetDepthWrite(!((s_currentMatFlags & 8) != 0 ||
                               (s_batchNZW && s_noBodyDWrite)));
        } else {
            /* Types 0–15 without Z-buffered fillers (NPC/sprite path):
               Z-test only — matches SW ZBufBoxOverWrite2 which reads
               terrain Z but never writes object Z into the buffer.
               Types 0–7 (FLAT/SOLID) still need Z-test so they are
               occluded by closer terrain (e.g. NPCs behind walls). */
            GL_SetDepthWrite(0);
        }
    } else {
        GL_SetDepthTest(0);
        GL_SetDepthWrite(0);
    }

    /* Draw all batched triangles.  Re-bind the render FBO: intermediate
       texture/state changes between GL_ClearFBO and this draw can reset
       the FBO binding on some drivers. */
    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3))
        glBindFramebuffer(GL_FRAMEBUFFER, s_msaaFbo);
    else if (s_fbo)
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glDrawArrays(GL_TRIANGLES, 0, s_batchVertCount);

    /* Restore depth state lazily: the caches hold the batch's values until
       GL_InvalidateGLState() or the next flush's setter changes them — the
       per-flush unconditional glDepthMask/glDisable pair was driver-call
       overhead with no consumer in between. */

    /* Reset batch counters */
    s_batchVertCount = 0;
    s_batchTriCount = 0;

    s_profAccumTicks[GL_PROF_FLUSH] += SDL_GetPerformanceCounter() - profT0;
}


void GL_FlushBatch(void) {
    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* Two-pool NZW batching (see GL_RenderTriangleList): the active pool
       and the NZW defer pool hold interleaved terrain/animated geometry.
       Draw depth-writing (NZW=0) geometry before no-write (NZW=1): the
       no-write polys' colors depend only on the depth test, so their
       position after all depth-writing geometry is equivalent to the
       submission order — but it lets each pool stay open across the
       per-tile alternation instead of flushing hundreds of times. */
    if (s_deferVertCount > 0) {
        if (s_deferNZW == 0) {
            /* Defer holds the depth-writing pool: draw it first. */
            GL_SwapBatchWithDefer();
            FlushActiveBatch();
            GL_SwapBatchWithDefer();
            if (s_batchVertCount > 0) {
                FlushActiveBatch();
            }
        } else {
            /* Active is depth-writing: draw it first, then the defer. */
            if (s_batchVertCount > 0) {
                FlushActiveBatch();
            }
            GL_SwapBatchWithDefer();
            FlushActiveBatch();
            GL_SwapBatchWithDefer();
        }
    } else if (s_batchVertCount > 0) {
        FlushActiveBatch();
    }
}

/* Draw only the depth-writing (NZW=0) pools, leaving any no-write pool
   queued.  GL_SnapshotTerrain uses this so the snapshot holds terrain and
   decor only: the no-write polys queued alongside them (animated polys
   drawn at the end of the terrain pass) redraw every frame and must not be
   baked into the background the incremental restore paints. */
static void GL_FlushBatchTerrain(void) {
    if (s_deferVertCount > 0 && s_deferNZW == 0) {
        GL_SwapBatchWithDefer();
        FlushActiveBatch();
        GL_SwapBatchWithDefer();
    }
    if (s_batchVertCount > 0 && s_batchNZW == 0) {
        FlushActiveBatch();
    }
}
/* Draw the projected ground-shadow polygon directly into the FBO, replacing
   the SW path (DrawShadow → DrawLineShade → per-scanline ShadeBoxBlk into
   Log + DrawRecover occlusion).  The caller walks the same Bezier-rounded
   outline the SW span filler fills (BuildOmbreOutline), with per-vertex
   camera depth from the same midpoint arithmetic over ShadowZO.  The fill
   is depth-tested GEQUAL against the FBO depth (terrain/hills clip it
   exactly where ZBufBoxOverWrite2 used to clear the Log pixels) and
   multiplies the scene color by (15 - level) / 15 via GL_DST_COLOR/GL_ZERO
   blending — the linear counterpart of the SW fade-CLUT row at
   (15 - ShadowLevel).  Unlike the SW path this works on the true FBO
   colors, so the result does not depend on the FBO→palette reverse lookup
   staying faithful. */
void GL_DrawShadowPoly(const S16 *xs, const S16 *ys, const U16 *az, S32 n,
                       U32 level) {
    Struc_Point pts[64];
    U16 tris[3 * 64];
    static U8 polyVerts[3 * 64 * BATCH_VERT_BYTES];
    S32 ntri;
    S32 nv;
    S32 i;
    /* Material: polyMode 6 (shadow multiply); the vertex colour byte is
       the shadow level — the shader derives (15 - level) / 15 from it. */
    const U16 shadowMat = (U16)(6 | ((U16)(level & 15) << 8));

    if (!s_initialized || n < 3) {
        return;
    }
    if (n > 64) {
        n = 64;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    for (i = 0; i < n; i++) {
        memset(&pts[i], 0, sizeof(Struc_Point));
        pts[i].Pt_XE = xs[i];
        pts[i].Pt_YE = ys[i];
    }

    /* Occlusion reference depth.  The SW path never tests the shadow against
       the local ground plane: DrawRecover clears shadow pixels only where
       the Z-buffer is nearer than the cylinder around the actor (center
       depth minus the body half-diagonal), so the ground the shadow lies on
       can never win.  A strict per-pixel test against the FBO depth does the
       opposite: the polygon interpolates ZO linearly in screen space while
       the ground depth is hyperbolic, so mid-span fragments read as behind
       the terrain and the shadow punches a hole at its own centre — a
       stable crescent when idle, blinking while moving.  Bias every vertex
       toward the camera by the shadow's full near-far ZO span, the GL
       counterpart of DrawRecover's near-edge reference; genuine occluders
       (walls, hills between camera and shadow) stay much nearer and keep
       clipping it. */
    S32 zmin = 0xFFFF;
    S32 zmax = 0;
    for (i = 0; i < n; i++) {
        if (az[i] < zmin) {
            zmin = az[i];
        }
        if (az[i] > zmax) {
            zmax = az[i];
        }
    }
    S32 bias = (zmax - zmin) + 16; /* + margin: depth16 is (131070 - aZ) / 2 */

    /* Pending batch first: it carries its own state. */
    GL_FlushBatch();

    if (!s_frameActive) {
        s_frameActive = 1;
        if (s_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    GL_UseProgramCached(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    s_ussX = 2.0f / (GLfloat)s_texWidth;
    s_ussY = 2.0f / (GLfloat)s_texHeight;

    /* Bind the three sampler textures (mode 6 ignores them, but the
       sampler units must reference valid textures). */
    GL_BindTexUnit(0, s_atlasTex);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    /* Depth-test against the terrain, never write. */
    GL_SetDepthTest(1);
    GL_SetDepthFunc(GL_GEQUAL);
    GL_SetDepthWrite(0);

    /* Multiply blending: result = shadow gray * scene color. */
    GL_SetBlendFunc(1, GL_DST_COLOR, GL_ZERO);

    /* Ear-clip the outline: the shadow quad is planar in front of the
       camera, but corners ride terrain altitude, so a skewed projection
       can go concave — a fan from vertex 0 would spill outside the
       outline (same reason the batch polys triangulate). */
    {
        S32 mx0 = xs[0], mx1 = mx0, my0 = ys[0], my1 = my0;
        for (i = 1; i < n; i++) {
            if (xs[i] < mx0) mx0 = xs[i];
            if (xs[i] > mx1) mx1 = xs[i];
            if (ys[i] < my0) my0 = ys[i];
            if (ys[i] > my1) my1 = ys[i];
        }
        GL_MarkFBORegion(mx0, my0, mx1, my1);
    }
    ntri = GL_TriangulatePolygon(pts, n, tris);

    if (ntri > 0) {
        nv = ntri;
        for (i = 0; i < nv; i++) {
            U8 *v = polyVerts + i * BATCH_VERT_BYTES;
            S32 c = tris[i];
            S16 x = xs[c];
            S16 y = ys[c];
            U16 z = ((S32)az[c] > bias) ? (U16)(az[c] - bias) : 0;
            memcpy(v + 0, &x, 2);
            memcpy(v + 2, &y, 2);
            memset(v + 4, 0, 6); /* UV, light = 0 (unused) */
            memcpy(v + 10, &z, 2);
            memset(v + 12, 0, 2); /* RepMask = 0 */
            memcpy(v + 14, &shadowMat, 2);
        }

        GL_BindArrayBuffer(s_batchVBO);
        /* Orphan + upload in one call: see GL_FlushBatch. */
        glBufferData(GL_ARRAY_BUFFER, nv * BATCH_VERT_BYTES, polyVerts,
                     GL_STREAM_DRAW);

        glEnableVertexAttribArray(s_clut_aPos);
        glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 16,
                              (const void *)0);
        glEnableVertexAttribArray(s_clut_aTexCoord);
        glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE,
                              16, (const void *)4);
        glEnableVertexAttribArray(s_clut_aLight);
        glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)8);
        glEnableVertexAttribArray(s_clut_aZ);
        glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)10);
        glEnableVertexAttribArray(s_clut_aRepMask);
        glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE,
                              16, (const void *)12);
        glEnableVertexAttribArray(s_clut_aMaterial);
        glVertexAttribPointer(s_clut_aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE,
                              16, (const void *)14);

        glDrawArrays(GL_TRIANGLES, 0, nv);
        s_profTris += (U32)nv / 3;
    }

    GL_SetDepthWrite(1);
    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Vertex arrays, program and texture bindings stay as they are — the
       next flush (or present, which invalidates) reuses or re-specifies
       them through the caches. */
    GL_BindArrayBuffer(0);
}

/* ── Body sphere disc (GL Fill_Sphere replacement) ───────────────────────── */

/* Whether sphere discs belong in the FBO: mirrors GL_RenderTriangleList's
   body-poly acceptance.  When the surrounding body polys fall back to the
   SW rasterizer (non-Z-buffered fills, e.g. interior cubes) their pixels
   land in the Log overlay, which composites OVER the FBO — a sphere sent
   to the FBO there would be painted over by the body that surrounds it
   (hair, eyes, buttons vanish).  Spheres must take the same route as the
   polys they decorate. */
S32 GL_SphereTargetsFBO(void) {
    if (!s_initialized) {
        return 0;
    }
    if (!Fill_Flag_ZBuffer) {
        return 0;
    }
    if (Fill_Flag_NZW && !s_fboHasTerrain) {
        return 0;
    }
    return 1;
}

/* LBA bodies model round details (eyes, buttons, nostrils) as shaded
   spheres.  The SW Fill_Sphere paints them into the Log buffer, where the
   painter-ordered body polys drawn after them cover the hidden ones.  In
   GL the body polys render into the FBO, so a Log-drawn sphere would
   composite on top of everything with no occlusion — black buttons floated
   over the body whenever their z-test against the one-frame-old PtrZBuffer
   let them through.  This draws the disc straight into the FBO with the
   same depth-test semantics as the SW filler (at-or-nearer passes) and the
   batch polys' depth-write rule (opaque writes, transparent does not). */
void GL_DrawSphere(S32 cx, S32 cy, S32 radius, S32 colorIndex, U32 zo,
                   S32 zWrite, S32 transparent) {
    if (!s_initialized || radius <= 0) {
        return;
    }
    /* Ring vertices are S16 screen coords; an oversized radius (near-plane
       projection blowup) wraps them into arbitrary on-screen triangles. */
    if (radius > 8192 || cx + radius > 32767 || cx - radius < -32768 ||
        cy + radius > 32767 || cy - radius < -32768) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* Pending batch first: it carries its own state. */
    GL_FlushBatch();

    if (!s_frameActive) {
        s_frameActive = 1;
        if (s_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    GL_UseProgramCached(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    s_ussX = 2.0f / (GLfloat)s_texWidth;
    s_ussY = 2.0f / (GLfloat)s_texHeight;
    /* Material: polyMode 1 (flat palette colour), TRANS blends. */
    const U16 sphereMat =
        (U16)(1 | (transparent ? (1 << 3) : 0) | ((U16)(colorIndex & 0xFF) << 8));

    GL_BindTexUnit(0, s_atlasTex);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    GL_SetDepthTest(1);
    GL_SetDepthFunc(GL_GEQUAL);
    GL_SetDepthWrite(zWrite && !transparent);

    if (transparent) {
        GL_SetBlendFunc(1, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    GL_MarkFBORegion(cx - radius, cy - radius, cx + radius, cy + radius);

    /* Disc as a triangle fan: centre + 17 ring vertices, 16-byte layout
       matching the batch (x, y, uv=0, light=0, zo, repmask=0, material). */
    const int SEGMENTS = 16;
    U8 verts[(SEGMENTS + 2) * BATCH_VERT_BYTES];
    S16 cx16 = (S16)cx, cy16 = (S16)cy;
    U16 zo16 = (U16)zo;
    for (int i = 0; i < SEGMENTS + 2; i++) {
        U8 *v = verts + i * BATCH_VERT_BYTES;
        int k = (i <= 1) ? 0 : (i - 1);
        double a = 2.0 * 3.14159265358979323846 * k / SEGMENTS;
        S16 x = cx16, y = cy16;
        if (i >= 1) {
            x = (S16)(cx16 + radius * cos(a) + 0.5);
            y = (S16)(cy16 + radius * sin(a) + 0.5);
        }
        memcpy(v + 0, &x, 2);
        memcpy(v + 2, &y, 2);
        memset(v + 4, 0, 4);  /* uv = 0 */
        memset(v + 8, 0, 2);  /* light = 0 */
        memcpy(v + 10, &zo16, 2);
        memset(v + 12, 0, 2); /* repmask = 0 */
        memcpy(v + 14, &sphereMat, 2);
    }

    GL_BindArrayBuffer(s_batchVBO);
    /* Orphan + upload in one call: see GL_FlushBatch. */
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 16,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)12);
    glEnableVertexAttribArray(s_clut_aMaterial);
    glVertexAttribPointer(s_clut_aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)14);

    glDrawArrays(GL_TRIANGLE_FAN, 0, SEGMENTS + 2);
    s_profTris += SEGMENTS;

    GL_SetDepthWrite(1);
    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

/* Body-model line entities (necklace chains, whiskers) draw through the
   immediate aux path like spheres: depth-tested against the FBO (which holds
   the body polys' depth) and depth-written, so a necklace seen from behind is
   hidden by the body polys drawn nearer, and one seen from the front stays
   visible over them.  Without this the lines rasterize into the Log overlay,
   which composites over the FBO with no occlusion — chest decals x-ray
   through the body whenever the camera looks at its back. */
void GL_DrawLineFBO(S32 x0, S32 y0, S32 x1, S32 y1, S32 colorIndex,
                    U32 zo0, U32 zo1) {
    if (!s_initialized) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    if (x0 < -32768 || x0 > 32767 || y0 < -32768 || y0 > 32767 ||
        x1 < -32768 || x1 > 32767 || y1 < -32768 || y1 > 32767) {
        return;
    }

    /* Pending batch first: it carries its own state. */
    GL_FlushBatch();

    if (!s_frameActive) {
        s_frameActive = 1;
        if (s_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    GL_UseProgramCached(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    s_ussX = 2.0f / (GLfloat)s_texWidth;
    s_ussY = 2.0f / (GLfloat)s_texHeight;
    /* Material: polyMode 1 (flat palette colour), opaque. */
    const U16 lineMat = (U16)(1 | ((U16)(colorIndex & 0xFF) << 8));

    GL_BindTexUnit(0, s_atlasTex);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    GL_SetDepthTest(1);
    GL_SetDepthFunc(GL_GEQUAL);
    GL_SetDepthWrite(1);

    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    S32 xmin = x0 < x1 ? x0 : x1;
    S32 xmax = x0 > x1 ? x0 : x1;
    S32 ymin = y0 < y1 ? y0 : y1;
    S32 ymax = y0 > y1 ? y0 : y1;
    GL_MarkFBORegion(xmin, ymin, xmax + 1, ymax + 1);

    U8 verts[2 * BATCH_VERT_BYTES];
    const S16 px[2] = {(S16)x0, (S16)x1};
    const S16 py[2] = {(S16)y0, (S16)y1};
    const U16 pz[2] = {(U16)zo0, (U16)zo1};
    for (int i = 0; i < 2; i++) {
        U8 *v = verts + i * BATCH_VERT_BYTES;
        memcpy(v + 0, &px[i], 2);
        memcpy(v + 2, &py[i], 2);
        memset(v + 4, 0, 4);  /* uv = 0 */
        memset(v + 8, 0, 2);  /* light = 0 */
        memcpy(v + 10, &pz[i], 2);
        memset(v + 12, 0, 2); /* repmask = 0 */
        memcpy(v + 14, &lineMat, 2);
    }

    GL_BindArrayBuffer(s_batchVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 16,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)12);
    glEnableVertexAttribArray(s_clut_aMaterial);
    glVertexAttribPointer(s_clut_aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)14);

    glDrawArrays(GL_LINES, 0, 2);

    GL_SetDepthWrite(1);
    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

/* ── Screen-space shade rect (GL ShadeBoxBlk replacement) ────────────────── */

/* The SW ShadeBoxBlk darkens the Log pixels in place through the fog
   window's fade rows — that works because in SW the Log holds the scene.
   In GL the Log overlay is empty where only 2D drew, so darkening it
   produces a solid band instead of a translucent one.  This multiplies
   the FBO rect's colors by (15 - level) / 15 instead — the linear
   counterpart of the fade row — with the depth test off so the band
   shades everything composited behind it. */
void GL_ShadeBox(S32 x0, S32 y0, S32 x1, S32 y1, S32 level) {
    if (!s_initialized) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    GL_FlushBatch();

    if (!s_frameActive) {
        s_frameActive = 1;
        if (s_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    GL_UseProgramCached(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    s_ussX = 2.0f / (GLfloat)s_texWidth;
    s_ussY = 2.0f / (GLfloat)s_texHeight;
    /* Material: polyMode 6 (multiply by (15 - level) / 15). */
    const U16 shadeMat = (U16)(6 | ((U16)(level & 15) << 8));

    GL_BindTexUnit(0, s_atlasTex);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    /* Unconditional: the band shades everything behind it. */
    GL_SetDepthTest(0);
    GL_SetDepthWrite(0);

    GL_SetBlendFunc(1, GL_DST_COLOR, GL_ZERO);

    GL_MarkFBORegion(x0, y0, x1, y1);

    U8 verts[6 * BATCH_VERT_BYTES];
    S16 coords[4][2] = {
        {(S16)x0, (S16)y0}, {(S16)x1, (S16)y0}, {(S16)x1, (S16)y1}, {(S16)x0, (S16)y1}};
    static const S8 order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) {
        U8 *v = verts + i * BATCH_VERT_BYTES;
        memcpy(v + 0, &coords[order[i]][0], 2);
        memcpy(v + 2, &coords[order[i]][1], 2);
        memset(v + 4, 0, 4);  /* uv = 0 */
        memset(v + 8, 0, 2);  /* light = 0 */
        memset(v + 10, 0, 2); /* zo = 0 */
        memset(v + 12, 0, 2); /* repmask = 0 */
        memcpy(v + 14, &shadeMat, 2);
    }

    GL_BindArrayBuffer(s_batchVBO);
    /* Orphan + upload in one call: see GL_FlushBatch. */
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 16,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)12);
    glEnableVertexAttribArray(s_clut_aMaterial);
    glVertexAttribPointer(s_clut_aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)14);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    GL_SetDepthWrite(1);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

static void GL_AddTriangle(Struc_Point *p0, Struc_Point *p1,
                           Struc_Point *p2) {
    /* Flush if batch is full */
    if (s_batchVertCount + 3 > BATCH_MAX_VERTS) {
        GL_FlushBatch();
    }


    /* Copy one 16-byte vertex per corner into the batch buffer:
       Pt_XE(2) + Pt_YE(2) + Pt_MapU(2) + Pt_MapV(2) +
       Pt_Light(2) + Pt_ZO(2) + Pt_RepMask(2) + Material(2)
       Note: Pt_W (S32, offset 12 in Struc_Point) is skipped;
       Pt_RepMask (offset 16) lands at vertex offset 12 and the current
       polygon material at offset 14. */
    U8 *dst = s_batchVerts + s_batchVertCount * BATCH_VERT_BYTES;
    memcpy(dst + 0, p0, 12);   /* XE, YE, MapU, MapV, Light, ZO */
    memcpy(dst + 12, (const U8 *)p0 + 16, 2); /* Pt_RepMask (skip Pt_W) */
    memcpy(dst + 14, &s_currentMaterial, 2);
    memcpy(dst + 16, p1, 12);
    memcpy(dst + 16 + 12, (const U8 *)p1 + 16, 2);
    memcpy(dst + 16 + 14, &s_currentMaterial, 2);
    memcpy(dst + 32, p2, 12);
    memcpy(dst + 32 + 12, (const U8 *)p2 + 16, 2);
    memcpy(dst + 32 + 14, &s_currentMaterial, 2);

    /* Track where the batch will draw (screen-space coords) for the
       region-bounded incremental restore. */
    {
        S32 x0 = p0->Pt_XE, x1 = x0;
        S32 y0 = p0->Pt_YE, y1 = y0;
        if (p1->Pt_XE < x0) x0 = p1->Pt_XE;
        if (p1->Pt_XE > x1) x1 = p1->Pt_XE;
        if (p1->Pt_YE < y0) y0 = p1->Pt_YE;
        if (p1->Pt_YE > y1) y1 = p1->Pt_YE;
        if (p2->Pt_XE < x0) x0 = p2->Pt_XE;
        if (p2->Pt_XE > x1) x1 = p2->Pt_XE;
        if (p2->Pt_YE < y0) y0 = p2->Pt_YE;
        if (p2->Pt_YE > y1) y1 = p2->Pt_YE;
        GL_MarkFBORegion(x0, y0, x1, y1);
    }

    s_batchVertCount += 3;
    s_batchTriCount++;
}

/* ── FBO clear (called at start of terrain rendering) ─────────────────────── */

/* Clear the FBO color and depth buffers at the start of terrain rendering.
   This is called from AffGrilleExt() instead of GL_FlushBatch() so that
   frames with no terrain (e.g. Twinsen standing still) preserve the
   previous frame's 3D content in the FBO. */
void GL_ClearFBO(void) {
    if (!s_initialized) {
        return;
    }

    if (!Window_GLMakeCurrent()) {
        return;
    }

    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3)) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_msaaFbo);
        GL_SetSceneViewport();
    } else if (s_fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        GL_SetSceneViewport();
    } else {
        glViewport(0, 0, s_texWidth, s_texHeight);
    }

#if defined(__ANDROID__)
    glClearDepthf(0.0f);
#else
    glClearDepth(0.0);
#endif
    /* glClear(GL_DEPTH_BUFFER_BIT) honours the depth mask, and the batch
       depth-state cache can legitimately leave write disabled after a frame
       ending in a blended batch — a masked clear would leave last frame's
       depth in place (black frame with terrain blinking through it). */
    GL_SetDepthWrite(1);
    if (s_stereoDoubleWide) {
        /* glClear ignores the viewport but honours the scissor.  Clear only
         * the current eye half, or eye 1 would erase eye 0's scene. */
        glEnable(GL_SCISSOR_TEST);
        glScissor(s_sceneEyeX, 0, (GLsizei)s_fboWidth, (GLsizei)s_fboHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    } else {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    s_terrainRenderedThisFrame = 1;
    s_fboHasTerrain = 1;
    s_fboGen++;
    s_fboDirtyActive = 1;
    s_fboDirtyX0 = 0;
    s_fboDirtyY0 = 0;
    s_fboDirtyX1 = -1; /* empty: everything drawn after this is tracked */
}

/* Blit the snapshot back over the FBO color on incremental frames so last
   frame's body/shadow pixels are erased before the object pass redraws
   them at their new positions.  No-op until the first snapshot exists and
   whenever the FBO has been cleared since (full redraw refreshes it).

   Only the region drawn since the snapshot needs erasing - bodies, their
   shadows, sphere discs, sprite quads and shade bands register their
   screen rects as they queue, and everything else in the FBO still matches
   the snapshot.  The tracked region is exact, so nothing can trail.  A
   frame whose object pass drew nothing skips the blit entirely - a
   full-screen blit of a 4x-SSAA framebuffer is a large slice of an idle
   frame's GPU bandwidth. */
void GL_SnapshotTerrain(void) {
    if (!s_initialized || !s_fbo) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* The terrain/decor batches are still queued vertices at this point —
       submit them first, or the snapshot captures a cleared framebuffer
       and every incremental-frame restore paints black over the scene.
       No-write pools stay queued: they draw after the snapshot and are
       tracked in the dirty region like the bodies. */
    GL_FlushBatchTerrain();

    /* With MSAA active, resolve color+depth first: the incremental restore
       needs the snapshot's DEPTH as well — body batches write depth, and a
       color-only restore leaves last frame's body depth in the MSAA buffer,
       where GEQUAL eats redrawn fragments that shifted marginally away
       (vanishing body triangles while idle).  Runs on full-redraw frames
       only, so the extra depth resolve is amortized. */

    const U32 w = s_fboWidth;
    const U32 h = s_fboHeight;
    if (s_snapshotTex == 0 || s_snapshotW != w || s_snapshotH != h) {
        if (s_snapshotTex == 0) {
            glGenTextures(1, &s_snapshotTex);
        }
        glBindTexture(GL_TEXTURE_2D, s_snapshotTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_UPLOAD_FORMAT, w, h, 0,
                     GL_UPLOAD_FORMAT, GL_UNSIGNED_BYTE, NULL);
        if (s_snapshotFbo == 0) {
            glGenFramebuffers(1, &s_snapshotFbo);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, s_snapshotFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, s_snapshotTex, 0);
        if (s_snapshotDepthTex == 0) {
            glGenTextures(1, &s_snapshotDepthTex);
        }
        glBindTexture(GL_TEXTURE_2D, s_snapshotDepthTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, w, h, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, s_snapshotDepthTex, 0);
        s_snapshotW = w;
        s_snapshotH = h;
        GL_LogSurfaceBudget("snapshot");
    }

    /* Color + depth: the depth must be restored too, otherwise a body that
       moved away would leave stale nearer depth that eats later fragments
       until the next full redraw.  From the MSAA FBO only color is legal
       (depth blits require equal sample counts) — and enough there: the
       MSAA restore erases color only, and nothing in the object pass
       writes MSAA depth to go stale. */
    U64 profT0 = SDL_GetPerformanceCounter();
    S32 msaaActive = (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3)) ? 1 : 0;
    if (msaaActive) {
        GL_ResolveMSAA();
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_snapshotFbo);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                      GL_NEAREST);
    /* Mid-frame invariant: subsequent direct draws (s_frameActive still set)
       must keep drawing into the scene target, not the default fb. */
    glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
    s_profAccumTicks[GL_PROF_SNAPSHOT] += SDL_GetPerformanceCounter() - profT0;

    s_snapshotGen = s_fboGen;
    s_fboDirtyActive = 1;
    s_fboDirtyX0 = 0;
    s_fboDirtyY0 = 0;
    s_fboDirtyX1 = -1; /* empty: everything drawn after this is tracked */
}

/* Blit the snapshot back over the FBO color on incremental frames so last
   frame's body/shadow pixels are erased before the object pass redraws
   them at their new positions.  No-op until the first snapshot exists and
   whenever the FBO has been cleared since (full redraw refreshes it).

   Only the region drawn since the snapshot needs erasing - bodies, their
   shadows, sphere discs, sprite quads and shade bands register their
   screen rects as they queue, and everything else in the FBO still matches
   the snapshot.  The tracked region is exact, so nothing can trail.  A
   frame whose object pass drew nothing skips the blit entirely - a
   full-screen blit of a 4x-SSAA framebuffer is a large slice of an idle
   frame's GPU bandwidth. */
void GL_RestoreTerrainSnapshot(void) {
    
    if (!s_initialized || !s_fbo || s_snapshotFbo == 0) {
        return;
    }
    if (s_snapshotGen != s_fboGen) {
        return;
    }
    if (!s_fboDirtyActive || s_fboDirtyX1 < s_fboDirtyX0) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* Screen-space rect -> FBO pixels (SSAA), clamped. */
    S32 qx0 = s_fboDirtyX0 * (S32)s_renderQuality;
    S32 qy0 = s_fboDirtyY0 * (S32)s_renderQuality;
    S32 qx1 = (s_fboDirtyX1 + 1) * (S32)s_renderQuality;
    S32 qy1 = (s_fboDirtyY1 + 1) * (S32)s_renderQuality;
    if (qx0 < 0) qx0 = 0;
    if (qy0 < 0) qy0 = 0;
    if (qx1 > (S32)s_fboWidth) qx1 = (S32)s_fboWidth;
    if (qy1 > (S32)s_fboHeight) qy1 = (S32)s_fboHeight;
    if (qx1 <= qx0 || qy1 <= qy0) {
        s_fboDirtyX1 = -1; /* nothing on-screen to erase */
        return;
    }

    /* Screen-space Y grows downward, glBlitFramebuffer and the quad's clip
       space grow upward — mirror the band or the erase lands on the
       vertically flipped region (small NPC-sized regions would restore the
       wrong band and leave ghosts). */
    S32 glY0 = (S32)s_fboHeight - qy1;
    S32 glY1 = (S32)s_fboHeight - qy0;
    GLfloat fw = (GLfloat)s_fboWidth;
    GLfloat fh = (GLfloat)s_fboHeight;

    U64 profT0 = SDL_GetPerformanceCounter();
    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3)) {
        /* The scene renders into the multisampled FBO, and a single-sample
           source cannot blit into it — erase through a textured quad
           instead.  Color only: body/shadow/animated batches never write
           depth, so the MSAA depth buffer still holds the terrain pass the
           snapshot was taken from. */
        GLfloat rverts[4][4] = {
            {2.0f * (GLfloat)qx0 / fw - 1.0f, 2.0f * (GLfloat)glY0 / fh - 1.0f,
             (GLfloat)qx0 / fw, (GLfloat)glY0 / fh},
            {2.0f * (GLfloat)qx1 / fw - 1.0f, 2.0f * (GLfloat)glY0 / fh - 1.0f,
             (GLfloat)qx1 / fw, (GLfloat)glY0 / fh},
            {2.0f * (GLfloat)qx0 / fw - 1.0f, 2.0f * (GLfloat)glY1 / fh - 1.0f,
             (GLfloat)qx0 / fw, (GLfloat)glY1 / fh},
            {2.0f * (GLfloat)qx1 / fw - 1.0f, 2.0f * (GLfloat)glY1 / fh - 1.0f,
             (GLfloat)qx1 / fw, (GLfloat)glY1 / fh}};
        glBindFramebuffer(GL_FRAMEBUFFER, s_msaaFbo);
        GL_SetSceneViewport();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(s_program);
        glUniform1i(s_uTexLoc, 0);
        glUniform1i(s_uFilterLoc, 1);
        GL_BindTexUnit(0, s_snapshotTex);
        GL_BindArrayBuffer(s_auxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(rverts), rverts, GL_STREAM_DRAW);
        glEnableVertexAttribArray(s_aPosLoc);
        glVertexAttribPointer(s_aPosLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), (const void *)0);
        glEnableVertexAttribArray(s_aTexCoordLoc);
        glVertexAttribPointer(s_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (const void *)(2 * sizeof(GLfloat)));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(s_aPosLoc);
        glDisableVertexAttribArray(s_aTexCoordLoc);
        GL_BindArrayBuffer(0);
        glUseProgram(0);
        /* The quad used its own program/attribs/texture — the next batch
           flush must re-specify everything. */
        GL_InvalidateGLState();
        if (s_depthRestoreProgram != 0) {
            /* Second pass: write the snapshot's terrain depth back under
               the same region.  Body batches write depth, so without this
               the MSAA depth keeps last frame's body depth and GEQUAL eats
               redrawn fragments that shifted marginally away (vanishing
               body triangles on idle frames).  Depth blits into a
               multisampled draw framebuffer are illegal, so this goes
               through a fragDepth quad. */
            glBindFramebuffer(GL_FRAMEBUFFER, s_msaaFbo);
            GL_SetSceneViewport();
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_ALWAYS);
            glDepthMask(GL_TRUE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glUseProgram(s_depthRestoreProgram);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s_snapshotDepthTex);
            GL_BindArrayBuffer(s_auxVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(rverts), rverts,
                         GL_STREAM_DRAW);
            glEnableVertexAttribArray(s_depthRestore_aPos);
            glVertexAttribPointer(s_depthRestore_aPos, 2, GL_FLOAT, GL_FALSE,
                                  4 * sizeof(GLfloat), (const void *)0);
            glEnableVertexAttribArray(s_depthRestore_aTexCoord);
            glVertexAttribPointer(s_depthRestore_aTexCoord, 2, GL_FLOAT,
                                  GL_FALSE, 4 * sizeof(GLfloat),
                                  (const void *)(2 * sizeof(GLfloat)));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray(s_depthRestore_aPos);
            glDisableVertexAttribArray(s_depthRestore_aTexCoord);
            GL_BindArrayBuffer(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            /* Coherent caches: the object pass's first flush re-issues its
               own depth state through the setters below. */
            GL_SetDepthFunc(GL_GEQUAL);
            GL_InvalidateGLState();
        }
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_snapshotFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_fbo);
        glBlitFramebuffer(qx0, glY0, qx1, glY1, qx0, glY0, qx1, glY1,
                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                          GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    }
    s_profAccumTicks[GL_PROF_RESTORE] += SDL_GetPerformanceCounter() - profT0;

    s_fboDirtyX0 = 0;
    s_fboDirtyY0 = 0;
    s_fboDirtyX1 = -1; /* empty again until the object pass draws */
}

/* ── Stereo horizon share (eye 1 reuses eye 0's horizon) ─────────────────── */

/* The neighbor-cube horizon (DrawHorizon2ZBuf) is drawn per eye in stereo,
   but its stereo disparity is sub-pixel: the backdrop sits a cube or more
   away (>= 32k units) and the eye separation is ~30 units, which projects
   to ~0.5 px at that depth.  Eye 0 captures the horizon after drawing it;
   eye 1 restores the capture instead of re-projecting ~34k grid vertices
   (the measured 4 ms).  The capture is valid within one frame only:
   GL_BeginFrame clears it, so menu/mono frames (no eye 0 capture) draw
   the horizon normally. */
static GLuint s_horizonTex = 0;
static GLuint s_horizonFbo = 0;
static GLuint s_horizonDepthRbo = 0;
static U32 s_horizonW = 0;
static U32 s_horizonH = 0;
static S32 s_horizonValid = 0;

/* Backdrop capture reuse: the horizon capture depends only on the camera
 * pose, and while the pose (and the streamed-in cube set — see
 * HorizonBackdropStable) is unchanged, the previous capture is still exact.
 * The common head-mounted case is a still rig with the head looking around,
 * which made the eye-0 capture a per-frame resolve-and-blit for nothing. */
static S32 s_hznCapValid = 0;
static S32 s_hznCapX = 0, s_hznCapY = 0, s_hznCapZ = 0;
static S32 s_hznCapAlpha = 0, s_hznCapBeta = 0, s_hznCapGamma = 0;
static U32 s_hznCapW = 0, s_hznCapH = 0;

/* Capture region in FBO pixels, GL bottom-left origin.  Eye 1 restores
   exactly this rect; a two-screen-pixel margin covers the sub-pixel stereo
   disparity at the backdrop's depth. */
static S32 s_hznCapRectX0 = 0, s_hznCapRectY0 = 0;
static S32 s_hznCapRectX1 = 0, s_hznCapRectY1 = 0;
static S32 s_hznCapRectValid = 0;

/* Latest capture cost split and footprint, read by GL_GetHorizonPerf for
   the GrillePerf diagnostic.  Zeroed on a cache-hit capture. */
static U64 s_hznPerfResolveTicks = 0;
static U64 s_hznPerfBlitTicks = 0;
static U32 s_hznPerfRectPct = 0;

/* Free the horizon capture surface.  Stereo draws the horizon per eye, so the
 * capture cannot be consumed there; mono/fallback frames allocate it lazily
 * in GL_CaptureHorizon. */
static void GL_DestroyHorizon(void) {
    if (s_horizonFbo) {
        glDeleteFramebuffers(1, &s_horizonFbo);
        s_horizonFbo = 0;
    }
    if (s_horizonDepthRbo) {
        glDeleteRenderbuffers(1, &s_horizonDepthRbo);
        s_horizonDepthRbo = 0;
    }
    if (s_horizonTex) {
        glDeleteTextures(1, &s_horizonTex);
        s_horizonTex = 0;
    }
    s_horizonW = 0;
    s_horizonH = 0;
    s_horizonValid = 0;
    s_hznCapValid = 0;
    s_hznCapRectValid = 0;
    s_hznPerfResolveTicks = 0;
    s_hznPerfBlitTicks = 0;
    s_hznPerfRectPct = 0;
}

/* Convert the tracked draw region (screen coords, top-left origin) to a
   clamped FBO rect.  Every draw in the horizon pass registers its screen
   footprint (batch polygons and sprite quads alike), so the rect bounds
   the backdrop exactly.  Returns FALSE when nothing was drawn. */
static S32 GL_HorizonDirtyRect(S32 *rx0, S32 *ry0, S32 *rx1, S32 *ry1) {
    S32 qx0, qy0, qx1, qy1;
    const S32 margin = 2 * (S32)s_renderQuality;

    if (!s_fboDirtyActive || s_fboDirtyX1 < s_fboDirtyX0 ||
        s_fboWidth == 0 || s_fboHeight == 0) {
        return FALSE;
    }
    qx0 = s_fboDirtyX0 * (S32)s_renderQuality - margin;
    qy0 = s_fboDirtyY0 * (S32)s_renderQuality - margin;
    qx1 = (s_fboDirtyX1 + 1) * (S32)s_renderQuality + margin;
    qy1 = (s_fboDirtyY1 + 1) * (S32)s_renderQuality + margin;
    if (qx0 < 0) qx0 = 0;
    if (qy0 < 0) qy0 = 0;
    if (qx1 > (S32)s_fboWidth) qx1 = (S32)s_fboWidth;
    if (qy1 > (S32)s_fboHeight) qy1 = (S32)s_fboHeight;
    if (qx1 <= qx0 || qy1 <= qy0) {
        return FALSE;
    }
    /* Screen-space Y grows downward, GL blit/quad space grows upward. */
    *rx0 = qx0;
    *ry0 = (S32)s_fboHeight - qy1;
    *rx1 = qx1;
    *ry1 = (S32)s_fboHeight - qy0;
    return TRUE;
}

static U32 GL_HorizonRectPct(S32 x0, S32 y0, S32 x1, S32 y1) {
    U64 area, total;

    if (x1 <= x0 || y1 <= y0 || s_fboWidth == 0 || s_fboHeight == 0) {
        return 0;
    }
    area = (U64)(x1 - x0) * (U64)(y1 - y0);
    total = (U64)s_fboWidth * (U64)s_fboHeight;
    return (U32)(area * 100 / total);
}

void GL_GetHorizonPerf(U32 *resolveUs, U32 *blitUs, U32 *rectPct) {
    double usPerTick = 1000000.0 / (double)SDL_GetPerformanceFrequency();

    if (resolveUs) {
        *resolveUs = (U32)((double)s_hznPerfResolveTicks * usPerTick + 0.5);
    }
    if (blitUs) {
        *blitUs = (U32)((double)s_hznPerfBlitTicks * usPerTick + 0.5);
    }
    if (rectPct) {
        *rectPct = s_hznPerfRectPct;
    }
}

void GL_CaptureHorizon(S32 backdropStable) {
    if (!s_initialized || !s_fbo) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* Eye 1 never restored the capture: GL_BeginFrame clears the valid flag
     * before the second eye starts, and each eye renders its own horizon.
     * Skipping the capture in double-wide stereo also avoids allocating the
     * full-resolution horizon surface.  Flush here anyway: the viewport and
     * target must select this eye's half before the next eye starts. */
    if (s_stereoDoubleWide && s_stereoEye >= 0) {
        GL_FlushBatch();
        s_hznCapValid = 0;
        s_hznCapRectValid = 0;
        s_horizonValid = 0;
        s_hznPerfResolveTicks = 0;
        s_hznPerfBlitTicks = 0;
        s_hznPerfRectPct = 0;
        return;
    }

    GL_FlushBatch();

    if (s_hznCapValid && s_hznCapRectValid && backdropStable &&
        s_hznCapW == s_fboWidth && s_hznCapH == s_fboHeight &&
        s_hznCapX == CameraX && s_hznCapY == CameraY && s_hznCapZ == CameraZ &&
        s_hznCapAlpha == CameraAlpha && s_hznCapBeta == CameraBeta &&
        s_hznCapGamma == CameraGamma) {
        /* Identical pose, identical backdrop: the previous capture is
         * exact.  The per-frame valid flag still needs raising so this
         * frame's second eye restores from the texture. */
        s_hznPerfResolveTicks = 0;
        s_hznPerfBlitTicks = 0;
        s_hznPerfRectPct = 0;
        s_horizonValid = 1;
        return;
    }

    /* Capture the horizon's own footprint in the steady state.  A missing
       cube sea placeholder can cover the whole frame, so those transition
       frames keep the full-surface path.  An empty footprint means the
       horizon drew nothing: eye 1 then draws its (equally empty) horizon
       itself instead of restoring a stale capture. */
    S32 rx0 = 0, ry0 = 0;
    S32 rx1 = (S32)s_fboWidth, ry1 = (S32)s_fboHeight;
    if (backdropStable && !GL_HorizonDirtyRect(&rx0, &ry0, &rx1, &ry1)) {
        s_hznCapValid = 0;
        s_hznCapRectValid = 0;
        s_horizonValid = 0;
        s_hznPerfResolveTicks = 0;
        s_hznPerfBlitTicks = 0;
        s_hznPerfRectPct = 0;
        return;
    }

    /* A placeholder capture must not seed the pose cache: when the cube
       streams in, the same pose has to recapture real terrain. */
    s_hznCapValid = backdropStable ? 1 : 0;
    s_hznCapX = CameraX;
    s_hznCapY = CameraY;
    s_hznCapZ = CameraZ;
    s_hznCapAlpha = CameraAlpha;
    s_hznCapBeta = CameraBeta;
    s_hznCapGamma = CameraGamma;
    s_hznCapW = s_fboWidth;
    s_hznCapH = s_fboHeight;
    s_hznCapRectX0 = rx0;
    s_hznCapRectY0 = ry0;
    s_hznCapRectX1 = rx1;
    s_hznCapRectY1 = ry1;
    s_hznCapRectValid = 1;
    s_hznPerfRectPct = GL_HorizonRectPct(rx0, ry0, rx1, ry1);

    const U32 w = s_fboWidth;
    const U32 h = s_fboHeight;
    if (s_horizonTex == 0 || s_horizonW != w || s_horizonH != h) {
        if (s_horizonTex == 0) {
            glGenTextures(1, &s_horizonTex);
        }
        glBindTexture(GL_TEXTURE_2D, s_horizonTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        /* GL_RGBA internally, not GL_UPLOAD_FORMAT: this is a blit
           destination, and matching the RGBA8 scene target keeps the
           resolve blit format-compatible on every driver. */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        if (s_horizonFbo == 0) {
            glGenFramebuffers(1, &s_horizonFbo);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, s_horizonFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, s_horizonTex, 0);
        if (s_horizonDepthRbo == 0) {
            glGenRenderbuffers(1, &s_horizonDepthRbo);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, s_horizonDepthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, s_horizonDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        s_horizonW = w;
        s_horizonH = h;
        GL_LogSurfaceBudget("horizon");
    }

    /* Color + depth: the current-cube terrain drawn after the restore must
       depth-test against the backdrop exactly as if it had drawn it.
       Under MSAA the restore reads only color (see below), so the blit
       drops the depth half.  The color read comes straight from the
       multisampled FBO in both resolve modes, so the blit resolves just this
       rect instead of resolving the whole surface first.  In-tile mode uses
       the same read; the tiler resolves the texture as part of it. */
    {
        const S32 msaaActive =
            (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3)) ? 1 : 0;
        const GLuint sceneFbo = msaaActive ? s_msaaFbo : s_fbo;
        s_hznPerfResolveTicks = 0;

        U64 blitT0 = SDL_GetPerformanceCounter();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_horizonFbo);
        glBlitFramebuffer(s_sceneEyeX + rx0, ry0, s_sceneEyeX + rx1, ry1,
                          rx0, ry0, rx1, ry1,
                          msaaActive ? GL_COLOR_BUFFER_BIT
                                     : (GL_COLOR_BUFFER_BIT |
                                        GL_DEPTH_BUFFER_BIT),
                          GL_NEAREST);
        /* Mid-frame invariant: keep drawing into the scene FBO.  In-tile
           mode the scene FBO is the multisample target, not s_fbo. */
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        GL_SetSceneViewport();
        s_hznPerfBlitTicks = SDL_GetPerformanceCounter() - blitT0;
    }
    s_horizonValid = 1;
}

void GL_RestoreHorizon(void) {
    if (!s_initialized || !s_fbo || !s_horizonValid || !s_hznCapRectValid ||
        s_horizonFbo == 0 || s_horizonW == 0 || s_horizonH == 0) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    const S32 rx0 = s_hznCapRectX0, ry0 = s_hznCapRectY0;
    const S32 rx1 = s_hznCapRectX1, ry1 = s_hznCapRectY1;
    if (rx1 <= rx0 || ry1 <= ry0) {
        return;
    }
    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3)) {
        /* Cannot blit single-sample depth into the MSAA target; draw the
           color as a quad (same as the terrain-snapshot MSAA restore).  The
           backdrop depth is not restored, but the current-cube terrain is
           drawn after this and near-always passes; the per-pixel depth test
           against the backdrop matters only where near hills occlude it,
           which the terrain's own depth writes rebuild anyway. */
        GLfloat fw = (GLfloat)s_fboWidth;
        GLfloat fh = (GLfloat)s_fboHeight;
        GLfloat tw = (GLfloat)s_horizonW;
        GLfloat th = (GLfloat)s_horizonH;
        GLfloat rverts[4][4] = {
            {2.0f * (GLfloat)rx0 / fw - 1.0f, 2.0f * (GLfloat)ry0 / fh - 1.0f,
             (GLfloat)rx0 / tw, (GLfloat)ry0 / th},
            {2.0f * (GLfloat)rx1 / fw - 1.0f, 2.0f * (GLfloat)ry0 / fh - 1.0f,
             (GLfloat)rx1 / tw, (GLfloat)ry0 / th},
            {2.0f * (GLfloat)rx0 / fw - 1.0f, 2.0f * (GLfloat)ry1 / fh - 1.0f,
             (GLfloat)rx0 / tw, (GLfloat)ry1 / th},
            {2.0f * (GLfloat)rx1 / fw - 1.0f, 2.0f * (GLfloat)ry1 / fh - 1.0f,
             (GLfloat)rx1 / tw, (GLfloat)ry1 / th}};
        glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
        GL_SetSceneViewport();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(s_program);
        glUniform1i(s_uTexLoc, 0);
        glUniform1i(s_uFilterLoc, 1);
        GL_BindTexUnit(0, s_horizonTex);
        GL_BindArrayBuffer(s_auxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(rverts), rverts, GL_STREAM_DRAW);
        glEnableVertexAttribArray(s_aPosLoc);
        glVertexAttribPointer(s_aPosLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), (const void *)0);
        glEnableVertexAttribArray(s_aTexCoordLoc);
        glVertexAttribPointer(s_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (const void *)(2 * sizeof(GLfloat)));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableVertexAttribArray(s_aPosLoc);
        glDisableVertexAttribArray(s_aTexCoordLoc);
        GL_BindArrayBuffer(0);
        glUseProgram(0);
        GL_InvalidateGLState();
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_horizonFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GL_SceneDrawFbo());
        glBlitFramebuffer(rx0, ry0, rx1, ry1, s_sceneEyeX + rx0, ry0,
                          s_sceneEyeX + rx1, ry1,
                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                          GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
        GL_SetSceneViewport();
    }
}

/* Approximate GPU footprint of the render targets the engine allocates. */
static U64 GL_TargetBytes(U32 w, U32 h, U32 bpp) {
    return (U64)w * (U64)h * (U64)bpp;
}

static void GL_LogSurfaceBudget(const char *when) {
    U64 fbo = 0, msaa = 0, eyes = 0, present = 0, snapshot = 0;
    U64 horizon = 0, overlay = 0;
    if (s_fboColorTex)
        fbo += GL_TargetBytes(s_sceneTargetW, s_sceneTargetH, 4);
    if (s_fboDepthRbo)
        fbo += GL_TargetBytes(s_sceneTargetW, s_sceneTargetH, 4);
    if (s_msaaColorTex) msaa += GL_TargetBytes(s_msaaW, s_msaaH, 4);
    if (s_msaaColorRb) msaa += GL_TargetBytes(s_msaaW, s_msaaH, 4);
    if (s_msaaDepthRb) msaa += GL_TargetBytes(s_msaaW, s_msaaH, 4);
    for (int i = 0; i < 2; i++) {
        if (s_stereoEyeTex[i])
            eyes += GL_TargetBytes(s_stereoEyeWidth, s_stereoEyeHeight, 4);
    }
    if (s_presentColorTex)
        present += GL_TargetBytes(s_texWidth, s_texHeight, 4);
    if (s_snapshotTex)
        snapshot += GL_TargetBytes(s_snapshotW, s_snapshotH, 4);
    if (s_snapshotDepthTex)
        snapshot += GL_TargetBytes(s_snapshotW, s_snapshotH, 4);
    if (s_horizonTex)
        horizon += GL_TargetBytes(s_horizonW, s_horizonH, 4);
    if (s_horizonDepthRbo)
        horizon += GL_TargetBytes(s_horizonW, s_horizonH, 4);
    if (s_logIndexTex)
        overlay += GL_TargetBytes(s_logIndexTexW, s_logIndexTexH, 1);
    U64 total = fbo + msaa + eyes + present + snapshot + horizon + overlay;
    const double mib = 1.0 / (1024.0 * 1024.0);
    Log_Info("[Perf] surfaces(%s): fbo=%.1f msaa=%.1f eyes=%.1f present=%.1f "
             "snapshot=%.1f horizon=%.1f overlay=%.1f total=%.1f MiB",
             when, (double)fbo * mib, (double)msaa * mib, (double)eyes * mib,
             (double)present * mib, (double)snapshot * mib,
             (double)horizon * mib, (double)overlay * mib,
             (double)total * mib);
}

S32 GL_HorizonValid(void) {
    return s_horizonValid;
}

/* ── Begin frame ─────────────────────────────────────────────────────────── */

void GL_BeginFrame(void) {
    if (!s_initialized) {
        return;
    }

    /* Read the texture filter toggle once per frame so GL_FlushBatch
       (which may fire mid-frame when the batch fills) uses the
       correct bilinear/nearest mode for all batches. */
    s_bilinearCLUT = DisplayTexFilter;

    s_frameSeq++;
    GL_BatchFlushCount = 0;
    s_terrainRenderedThisFrame = 0;
    s_horizonValid = 0; /* horizon capture is per-frame (eye 0 only) */
}

/* ── Depth readback (GL → SW Z-buffer) ──────────────────────────────────── */

/* Read back the FBO depth buffer into the SW Z-buffer (PtrZBuffer) so that
   ZBufBoxOverWrite2() can occlude shadows/objects behind terrain in GL mode.
   The vertex shader outputs Z = 1.0 - aZ/65535.0; after the viewport
   transform this becomes depth = (2.0 - aZ/65535) / 2.0.  Stored as
   GL_DEPTH_COMPONENT16: gl_depth_16 = (131070 - aZ) / 2.
   Inverting: aZ = 131070 - 2 * gl_depth_16.
   When the depth buffer is cleared (0.0), aZ = 131070 — larger than any
   valid aZ, so ZBufBoxOverWrite2 always keeps shadows there (correct). */
extern "C" {
extern U16 *PtrZBuffer;
extern void *Screen;
extern void *Log;
extern U32 ModeDesiredX;
extern U32 ModeDesiredY;
}

/* Convert a freshly-read FBO depth buffer (GL_UNSIGNED_SHORT rows,
 * bottom-up, screen-sized) into the SW aZ format in PtrZBuffer, top-down.
 * The GPU downscales the SSAA depth before this runs, so src is always
 * screen-sized. */
static void GL_ConvertDepthToZBuffer(const U16 *src, U32 fboW, U32 fboH) {
    const U32 scrW = ModeDesiredX;
    const U32 scrH = ModeDesiredY;

    /* 1:1 path: invert depth, flip Y into PtrZBuffer.  The vertex shader
       outputs Z = 1.0 - aZ/65535.0; after the viewport transform depth =
       (2.0 - aZ/65535) / 2, stored as gl_depth_16 = (131070 - aZ) / 2.
       Inverting: aZ = 131070 - 2 * gl_depth_16.
       ZBufBoxOverWrite2 compares aZ values, so the inverse mapping is
       required. */
    U64 profT0 = SDL_GetPerformanceCounter();
    for (U32 sy = 0; sy < scrH; sy++) {
        const U16 *srcRow = src + (scrH - 1 - sy) * scrW;
        U16 *dstRow = PtrZBuffer + sy * scrW;
        for (U32 sx = 0; sx < scrW; sx++) {
            U32 gl_depth_16 = srcRow[sx];
            dstRow[sx] = (U16)(131070u - 2u * gl_depth_16);
        }
    }
    U64 profT1 = SDL_GetPerformanceCounter();
    s_profAccumTicks[2] += profT1 - profT0;
}

/* ── Async depth readback (double-buffered PBOs) ────────────────────────── */
/* glReadPixels of the depth buffer forces a full pipeline sync — the
   dominant frame cost on desktop GL and brutal on tlined mobile GPUs.
   Instead, issue the read into a pixel-pack PBO and consume the PBO filled
   on the *previous* frame: by map time the GPU has had a full frame to
   finish, so the wait overlaps real work.  The one-frame-old depth feeds
   only sprite occlusion (ZBufBoxOverWrite2) and fully-hidden flags, which
   are insensitive to a single frame of lag. */
void GL_ReadbackDepthBuffer(void) {
    /* NOTE: runs after the object pass on every exterior frame; on
       incremental frames the FBO depth persists (restore only touches
       color), so there is no per-frame terrain-rendered guard here. */
    if (!s_initialized || !s_fbo || PtrZBuffer == NULL || !s_fboHasTerrain) {
        return;
    }

    /* Stereo: eye 1's depth differs from eye 0's by the tiny camera shift,
       and the consumers (sprite occlusion, fully-hidden flags) already
       tolerate a frame of lag — read once per frame, not once per eye. */
    if (s_stereoEye == 1) {
        return;
    }

    const U32 fboW = s_fboWidth;
    const U32 fboH = s_fboHeight;
    const U32 scrW = ModeDesiredX;
    const U32 scrH = ModeDesiredY;
    const U32 bytes = scrW * scrH * sizeof(U16);

    /* At native render scale (quality 1) the FBO depth is already
       screen-sized: read from it directly instead of routing through the
       downscale FBO — the same-size blit is a full-screen depth copy whose
       only product is a buffer identical to the source. */
    const S32 depthNative = (fboW == scrW && fboH == scrH);

    U64 profT0 = SDL_GetPerformanceCounter();

    /* Flush any pending batch so the FBO contains the latest geometry
       (terrain + bodies + shadows) before we read back its depth. */
    GL_FlushBatch();

    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3)) {
        if (depthNative) {
            /* The PBO read below consumes a single-sample buffer, and at
               quality 1 that buffer is s_fbo itself — refresh only its
               depth (the color half of GL_ResolveMSAA is re-resolved by
               GL_Present and would be a full-surface copy wasted here). */
            glBindFramebuffer(GL_READ_FRAMEBUFFER, s_msaaFbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_fbo);
            glBlitFramebuffer(0, 0, fboW, fboH, 0, 0, fboW, fboH,
                              GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }
        /* Non-native scales: the screen-sized blit further down resolves
           the depth straight out of the MSAA buffer, so a resolved copy in
           s_fbo would never be read. */
    }

    if (!depthNative) {
        /* GPU downscale: blit the SSAA depth into a screen-sized FBO so the
           CPU readback moves 2.4 MB instead of ~40 MB per frame. */
        if (s_depthSmallFbo == 0 || s_depthSmallW != scrW ||
            s_depthSmallH != scrH) {
            if (s_depthSmallFbo == 0) {
                glGenFramebuffers(1, &s_depthSmallFbo);
                glGenRenderbuffers(1, &s_depthSmallRbo);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, s_depthSmallFbo);
            glBindRenderbuffer(GL_RENDERBUFFER, s_depthSmallRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                  scrW, scrH);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, s_depthSmallRbo);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            s_depthSmallW = scrW;
            s_depthSmallH = scrH;
        }

    /* With MSAA, the scene depth is in the MSAA depth renderbuffer:
       glBlitFramebuffer auto-resolves multisample to single-sample. */
    GLuint depthSrcFbo = (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3)) ? s_msaaFbo : s_fbo;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, depthSrcFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_depthSmallFbo);
    glBlitFramebuffer(0, 0, fboW, fboH, 0, 0, scrW, scrH,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    /* Pack buffers track the screen-sized read; recreating them after a
       resize invalidates the queued read, so force the bootstrap path. */
    if (s_depthPbo[0] == 0 || s_depthPboW != scrW || s_depthPboH != scrH) {
        S32 i;
        for (i = 0; i < 2; i++) {
            if (s_depthFence[i] != 0) {
                glDeleteSync(s_depthFence[i]);
                s_depthFence[i] = 0;
            }
        }
        s_depthSkipStreak = 0;
        if (s_depthPbo[0] == 0) {
            glGenBuffers(2, s_depthPbo);
        }
        for (i = 0; i < 2; i++) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, s_depthPbo[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, bytes, NULL, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        s_depthPboW = scrW;
        s_depthPboH = scrH;
        s_depthPboReady = 0;
    }

    /* Async double-buffered PBO readback: consume the read issued on the
       previous frame (the GPU has had a full frame to finish it), then
       issue this frame's read. */
    const U32 readIdx = s_depthPboIndex ^ 1;
    if (s_depthPboReady > 0) {
        U64 profMap0 = SDL_GetPerformanceCounter();
        U64 convertBefore = s_profAccumTicks[GL_PROF_CONVERT];
        /* Non-blocking readiness check first. Mapping a PBO whose read is
           still in flight stalls the CPU until the GPU drains everything
           queued before it — during traversal that is the whole scene
           render, so GPU-side jitter surfaced here as traversal stutter.
           If the read is not done, keep the previous PtrZBuffer (it is
           already one frame old by design) and retry next frame; a safety
           valve bounds the staleness so a persistently backed-up GPU
           cannot starve the occlusion data forever. */
        if (s_depthFence[readIdx] != 0 &&
            glClientWaitSync(s_depthFence[readIdx], 0, 0) ==
                GL_TIMEOUT_EXPIRED) {
            s_depthSkipStreak++;
            s_profDepthSkips++;
            if (s_depthSkipStreak < 5) {
                return;
            }
            glFlush();
            glClientWaitSync(s_depthFence[readIdx], 0, 1000000000ULL);
        }
        s_depthSkipStreak = 0;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, s_depthPbo[readIdx]);
        const U16 *src = (const U16 *)glMapBufferRange(GL_PIXEL_PACK_BUFFER,
                                                       0, bytes,
                                                       GL_MAP_READ_BIT);
        if (src != NULL) {
            GL_ConvertDepthToZBuffer(src, scrW, scrH);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        } else {
            memset(PtrZBuffer, 0xFF,
                   (size_t)scrW * (size_t)scrH * sizeof(U16));
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        if (s_depthFence[readIdx] != 0) {
            glDeleteSync(s_depthFence[readIdx]);
            s_depthFence[readIdx] = 0;
        }
        s_profAccumTicks[GL_PROF_MAPWAIT] +=
            (SDL_GetPerformanceCounter() - profMap0) -
            (s_profAccumTicks[GL_PROF_CONVERT] - convertBefore);
    } else {
        /* Bootstrap: one synchronous read so the first frame is correct. */
        U16 *tmp = (U16 *)malloc(bytes);
        if (tmp != NULL) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER,
                              depthNative ? s_fbo : s_depthSmallFbo);
            glReadPixels(0, 0, scrW, scrH, GL_DEPTH_COMPONENT,
                         GL_UNSIGNED_SHORT, tmp);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            GL_ConvertDepthToZBuffer(tmp, scrW, scrH);
            free(tmp);
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      depthNative ? s_fbo : s_depthSmallFbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, s_depthPbo[s_depthPboIndex]);
    glReadPixels(0, 0, scrW, scrH, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
                 NULL);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    s_depthPboIndex ^= 1;
    if (s_depthPboReady < 2) {
        s_depthPboReady++;
    }

    U64 profT1 = SDL_GetPerformanceCounter();
    s_profAccumTicks[1] += profT1 - profT0;

    /* Keep the FBO bound so subsequent GL_FlushBatch() calls (e.g. from
       body polygons that don't go through the SW path) continue drawing
       into the FBO rather than the default framebuffer. */
}

/* ── Color readback (FBO → SW Screen buffer as palette indices) ────────── */

/* Read back the FBO color buffer and convert the ARGB pixels to 8-bit
   palette indices, storing the result in the Screen buffer.  This is used
   by AffGrilleExt() to copy terrain into the Log buffer so that
   ShadeBoxBlk (shadow darkening) has real pixels to operate on.

   Uses a cached 65536-entry hash table for O(1) ARGB→palette-index lookup.
   Handles SSAA downscaling (FBO may be larger than Screen). */
/* ── Gameplay sprite quads (GL AffGraph/ScaleSprite replacement) ─────────── */

#define SPRITE_CACHE_SIZE 24
typedef struct {
    const void *ptr;
    S32 w;
    S32 h;
    GLuint tex;
} SpriteTexEntry;
static SpriteTexEntry s_spriteTexCache[SPRITE_CACHE_SIZE];
static S32 s_spriteTexNext = 0;

static GLuint GL_BindSpriteTexture(const void *pixels, S32 w, S32 h) {
    for (S32 i = 0; i < SPRITE_CACHE_SIZE; i++) {
        if (s_spriteTexCache[i].ptr == pixels && s_spriteTexCache[i].w == w &&
            s_spriteTexCache[i].h == h) {
            GL_BindTexUnit(0, s_spriteTexCache[i].tex);
            return s_spriteTexCache[i].tex;
        }
    }

    SpriteTexEntry *e = &s_spriteTexCache[s_spriteTexNext];
    s_spriteTexNext = (s_spriteTexNext + 1) % SPRITE_CACHE_SIZE;
    if (e->tex == 0) {
        glGenTextures(1, &e->tex);
    }
    GL_BindTexUnit(0, e->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, pixels);
    s_profSpriteUps++;
    e->ptr = pixels;
    e->w = w;
    e->h = h;
    return e->tex;
}

/* Draw one gameplay sprite (eyes, hands, extras) as a chroma-keyed,
   depth-tested quad straight into the FBO.  Replaces the SW blit into Log,
   whose pixels always composited over the FBO and could therefore never be
   occluded by GL-drawn models.  (x0,y0)-(x1,y1) is the on-screen rect,
   (u0,v0)-(u1,v1) the sub-rect of the w×h sprite image to sample, az the
   anchor depth in aZ units (GET_ZO(CameraZr - Z0)).  When transparent, the
   quad blends at 50% like ScaleSpriteTransp; otherwise texel 0 is
   discarded (colour 0 = transparent). */
void GL_DrawSpriteQuad(S32 x0, S32 y0, S32 x1, S32 y1,
                       S32 u0, S32 v0, S32 u1, S32 v1,
                       const void *pixels, S32 w, S32 h,
                       U16 az, S32 transparent) {
    if (!s_initialized || pixels == NULL || w <= 0 || h <= 0 || x1 <= x0 ||
        y1 <= y0) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    GL_FlushBatch();

    if (!s_frameActive) {
        s_frameActive = 1;
        if (s_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    GL_UseProgramCached(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    s_ussX = 2.0f / (GLfloat)s_texWidth;
    s_ussY = 2.0f / (GLfloat)s_texHeight;
    /* Material: polyMode 4 (texel is the palette index).  Opaque sprites
       chroma-key texel 0; transparent ones blend at 50% and keep texel 0. */
    const U16 spriteMat = (U16)(4 | (transparent ? (1 << 3) : 0) |
                                (transparent ? 0 : (1 << 5)));
    {
        GLfloat sx = (GLfloat)w, sy = (GLfloat)h;
        glUniform2f(s_clut_uAtlasScale, sx, sy);
        s_atlasScaleX = sx;
        s_atlasScaleY = sy;
    }

    GL_BindTexUnit(0, GL_BindSpriteTexture(pixels, w, h));
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    GL_SetDepthTest(1);
    GL_SetDepthFunc(GL_GEQUAL);
    /* Opaque sprites write depth: they draw through the immediate aux path
       while bodies batch into the NZW defer pool drained at frame end, so
       the submission order between an overlay and the body polygons around
       it depends on the frame path (full redraw vs incremental) and on the
       camera.  Depth — not submission order — must decide occlusion, or a
       chest decal seen from behind (owner body nearer to the camera, drawn
       after it) stays visible through the body.  Transparent sprites keep
       write-off so their blend never clips geometry behind them. */
    GL_SetDepthWrite(transparent ? 0 : 1);

    if (transparent) {
        GL_SetBlendFunc(1, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    GL_MarkFBORegion(x0, y0, x1, y1);

    static U8 quadVerts[6 * BATCH_VERT_BYTES];
    static const S8 order[6] = {0, 1, 2, 0, 2, 3};
    const S16 px[4] = {(S16)x0, (S16)x1, (S16)x1, (S16)x0};
    const S16 py[4] = {(S16)y0, (S16)y0, (S16)y1, (S16)y1};
    const U16 pu[4] = {(U16)u0, (U16)u1, (U16)u1, (U16)u0};
    const U16 pv[4] = {(U16)v0, (U16)v0, (U16)v1, (U16)v1};
    for (S32 i = 0; i < 6; i++) {
        U8 *v = quadVerts + i * BATCH_VERT_BYTES;
        S32 c = order[i];
        S16 x = px[c];
        S16 y = py[c];
        U16 u = pu[c];
        U16 vv = pv[c];
        memcpy(v + 0, &x, 2);
        memcpy(v + 2, &y, 2);
        memcpy(v + 4, &u, 2);
        memcpy(v + 6, &vv, 2);
        memset(v + 8, 0, 2); /* light = 0 */
        memcpy(v + 10, &az, 2);
        memset(v + 12, 0, 2); /* RepMask = 0 */
        memcpy(v + 14, &spriteMat, 2);
    }

    GL_BindArrayBuffer(s_batchVBO);
    /* Orphan + upload in one call: see GL_FlushBatch. */
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts,
                 GL_STREAM_DRAW);

    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 16,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)12);
    glEnableVertexAttribArray(s_clut_aMaterial);
    glVertexAttribPointer(s_clut_aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)14);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    s_profTris += 2;

    GL_SetDepthWrite(1);
    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

void GL_ReadbackColorBuffer(void) {
    if (!s_initialized || !s_fbo || Screen == NULL || !s_fboHasTerrain || !s_terrainRenderedThisFrame) {
        return;
    }

    /* FBO should still be bound from GL_ReadbackDepthBuffer().  Bind the
       scene's single-sample read view explicitly for safety in case the
       caller skips the depth readback. */
    glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneReadFbo());

    const U32 fboW = s_fboWidth;
    const U32 fboH = s_fboHeight;
    const U32 scrW = ModeDesiredX;
    const U32 scrH = ModeDesiredY;

    /* Read FBO color into a temporary buffer */
    const U32 totalPixels = fboW * fboH;
    static U32 *s_fboColorBuf = NULL;
    static U32 s_fboColorBufSize = 0;
    if (s_fboColorBufSize < totalPixels) {
        free(s_fboColorBuf);
        s_fboColorBuf = (U32 *)malloc(totalPixels * sizeof(U32));
        s_fboColorBufSize = s_fboColorBuf ? totalPixels : 0;
    }
    if (s_fboColorBuf == NULL) {
        return;
    }

    /* Read as BGRA: paletteLUT is 0xAARRGGBB = [B,G,R,A] in LE memory,
        and GL_BGRA reads bytes in B,G,R,A order → U32 = ARGB. GLES has no
        BGRA without the extension, so read RGBA and swizzle back to the
        same ARGB word order. */
#if defined(__ANDROID__)
    glReadPixels(0, 0, fboW, fboH, GL_RGBA, GL_UNSIGNED_BYTE, s_fboColorBuf);
    GL_SwizzleBGRAtoRGBA(s_fboColorBuf, s_fboColorBuf, (U32)(fboW * fboH));
#else
    glReadPixels(0, 0, fboW, fboH, FMT_BGRA, GL_UNSIGNED_BYTE, s_fboColorBuf);
#endif

    /* Build reverse palette lookup on demand. */
    if (!s_revLutHashValid) {
        GL_BuildReverseLut();
    }

    /* Convert FBO pixels to palette indices and write to Screen.
       glReadPixels returns rows bottom-to-top (Y=0 at bottom), but the
       Screen buffer stores rows top-to-bottom (Y=0 at top).  Flip Y by
       mapping FBO row (fboH - 1 - sy) to Screen row sy. */
    U8 *dst = (U8 *)Screen;

    if (fboW == scrW && fboH == scrH) {
        /* 1:1 mapping with Y-flip */
        for (U32 sy = 0; sy < scrH; sy++) {
            U8 *dstRow = dst + sy * scrW;
            const U32 *srcRow = s_fboColorBuf + (fboH - 1 - sy) * fboW;
            for (U32 sx = 0; sx < scrW; sx++) {
                dstRow[sx] = GL_ReversePaletteLookup(srcRow[sx]);
            }
        }
    } else {
        /* SSAA path: pick the center pixel of each screen-pixel block,
           with Y-flip applied to the FBO source row. */
        const U32 scaleX = fboW / scrW;
        const U32 scaleY = fboH / scrH;
        for (U32 sy = 0; sy < scrH; sy++) {
            U8 *dstRow = dst + sy * scrW;
            for (U32 sx = 0; sx < scrW; sx++) {
                U32 fx = sx * scaleX + scaleX / 2;
                U32 fy = (fboH - 1 - sy * scaleY - scaleY / 2);
                dstRow[sx] = GL_ReversePaletteLookup(s_fboColorBuf[fy * fboW + fx]);
            }
        }
    }

    /* Restore the scene draw target for subsequent direct draws. */
    glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
}

/* Composite the 3D FBO color with the Log overlay the way GL_Present draws
   them (Log over FBO, palette index 0 transparent) and write an RGB PNG.
   Screenshot tools that capture the Log buffer alone see only the 2D
   overlay in GL mode — the 3D scene lives in the FBO. */
void GL_SavePresentPNG(const char *path) {
    if (!s_initialized || !s_fbo) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    GL_FlushBatch();

    const U32 fboW = s_fboWidth;
    const U32 fboH = s_fboHeight;
    const U32 scrW = ModeDesiredX;
    const U32 scrH = ModeDesiredY;

    U32 *fboBuf = (U32 *)malloc((size_t)fboW * fboH * sizeof(U32));
    /* Composite holds 0xAARRGGBB words, same layout as paletteLUT. */
    U32 *comp = (U32 *)malloc((size_t)scrW * scrH * sizeof(U32));
    U8 *rgb = (U8 *)malloc((size_t)scrW * scrH * 3);
    if (fboBuf == NULL || comp == NULL || rgb == NULL) {
        free(fboBuf);
        free(comp);
        free(rgb);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneReadFbo());
#if defined(__ANDROID__)
    glReadPixels(0, 0, fboW, fboH, GL_RGBA, GL_UNSIGNED_BYTE, fboBuf);
    GL_SwizzleBGRAtoRGBA(fboBuf, fboBuf, (U32)(fboW * fboH));
#else
    glReadPixels(0, 0, fboW, fboH, FMT_BGRA, GL_UNSIGNED_BYTE, fboBuf);
#endif


    /* Downsample (SSAA center pick) with the Y-flip glReadPixels implies,
       giving screen-space top-down ARGB like GL_Present's FBO draw. */
    const U32 scaleX = fboW / scrW;
    const U32 scaleY = fboH / scrH;
    for (U32 sy = 0; sy < scrH; sy++) {
        const U32 fy = fboH - 1 - sy * scaleY - scaleY / 2;
        const U32 *srcRow = fboBuf + fy * fboW;
        U32 *dstRow = comp + sy * scrW;
        if (scaleX == 1) {
            memcpy(dstRow, srcRow, scrW * sizeof(U32));
        } else {
            for (U32 sx = 0; sx < scrW; sx++) {
                dstRow[sx] = srcRow[sx * scaleX + scaleX / 2];
            }
        }
    }

    /* Log overlay on top: palette index 0 is transparent (alpha forced 0,
       same rule PresentFrame applies before the GPU composite). */
    const U32 *lut = GL_GetPaletteLUT();
    const U8 *logIdx = (const U8 *)Log;
    for (U32 i = 0; i < scrW * scrH; i++) {
        U32 idx = logIdx[i];
        if (idx != 0 && lut != NULL) {
            comp[i] = lut[idx];
        }
        U32 c = comp[i];
        rgb[i * 3 + 0] = (U8)(c >> 16);
        rgb[i * 3 + 1] = (U8)(c >> 8);
        rgb[i * 3 + 2] = (U8)(c);
    }

    SavePNGRGB(path, rgb, (S32)scrW, (S32)scrH);

    /* The capture temporarily bound the read view; restore the draw target. */
    glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());

    free(fboBuf);
    free(comp);
    free(rgb);
}

/* ── Present (extended with 3D flush) ───────────────────────────────────── */

/* Black out just the letterbox bars around the content rectangle. The
   content itself is overwritten by the FBO/overlay draws, so clearing it
   too is wasted bandwidth at headset-class resolutions. */
static void GL_ClearLetterboxBars(int vpX, int vpY, int vpW, int vpH,
                                  int winW, int winH) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (vpX == 0 && vpY == 0 && vpW == winW && vpH == winH) {
        return; /* no bars: the content draw covers everything */
    }
    glEnable(GL_SCISSOR_TEST);
    if (vpY > 0) {
        glScissor(0, 0, winW, vpY); /* bottom bar (GL origin is bottom-left) */
        glClear(GL_COLOR_BUFFER_BIT);
    }
    if (vpY + vpH < winH) {
        glScissor(0, vpY + vpH, winW, winH - vpY - vpH); /* top bar */
        glClear(GL_COLOR_BUFFER_BIT);
    }
    if (vpX > 0) {
        glScissor(0, vpY, vpX, vpH); /* left bar */
        glClear(GL_COLOR_BUFFER_BIT);
    }
    if (vpX + vpW < winW) {
        glScissor(vpX + vpW, vpY, winW - vpX - vpW, vpH); /* right bar */
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);
}

/* Draw the ARGB 2D overlay into the currently bound target/viewport. */
static void GL_DrawArgbOverlay(const void *argbPixels, U32 pitch, U32 width,
                               U32 height, S32 filterLinear) {
    /* Upload the ARGB framebuffer to the GL texture (2D overlay) */
    glBindTexture(GL_TEXTURE_2D, s_texture);

    /* Set filter mode (bilinear vs nearest; Scale2x samples exact texels) */
    GLenum filter = (filterLinear == 1) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    /* GL_UNPACK_ROW_LENGTH is GLES 3.0+; the project's minimum is GLES 2.0,
       so guard the pixel-store call (row length falls back to width when
       unset, which is what we want anyway because pitch == width*4 here). */
    /* Stereo eye 1 reuses eye 0's overlay texture — the Log content is
       identical for both eyes, so skip the CPU swizzle and the upload. */
    if (s_stereoEye != 1) {
        const void *uploadPixels = argbPixels;
#if defined(__ANDROID__)
        static U32 *s_uploadBuf = NULL;
        static U32 s_uploadBufPixels = 0;
        U32 needed = width * height;
        if (s_uploadBufPixels < needed) {
            free(s_uploadBuf);
            s_uploadBuf = (U32 *)malloc(needed * sizeof(U32));
            s_uploadBufPixels = s_uploadBuf ? needed : 0;
        }
        if (s_uploadBuf != NULL) {
            GL_SwizzleBGRAtoRGBA(s_uploadBuf, (const U32 *)argbPixels, needed);
            uploadPixels = s_uploadBuf;
        }
#endif
#ifdef GL_UNPACK_ROW_LENGTH
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 4);
#endif
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_UPLOAD_FORMAT,
                        GL_UNSIGNED_BYTE, uploadPixels);
#ifdef GL_UNPACK_ROW_LENGTH
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    }

    /* Enable alpha blending so palette index 0 (alpha=0) is transparent,
       letting the 3D scene (or black background) show through underneath. */
    GL_SetBlendFunc(1, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Draw fullscreen quad for 2D overlay */
    glUseProgram(s_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_texture);
    glUniform1i(s_uTexLoc, 0);
    if (s_overlayImageAdjust) {
        GL_UploadImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                             s_adj_uSaturation);
    } else {
        GL_UploadNeutralImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                                    s_adj_uSaturation);
    }
    glUniform2f(s_uTexelLoc, 1.0f / (GLfloat)width, 1.0f / (GLfloat)height);
    glUniform1i(s_uFilterLoc, filterLinear);

    GL_BindArrayBuffer(s_vbo);
    glEnableVertexAttribArray(s_aPosLoc);
    glVertexAttribPointer(s_aPosLoc, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(s_aTexCoordLoc);
    glVertexAttribPointer(s_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat),
                          (const void *)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(s_aPosLoc);
    glDisableVertexAttribArray(s_aTexCoordLoc);
    GL_BindArrayBuffer(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear) {
    if (!s_initialized || argbPixels == NULL) {
        Log_Info("GL_Present: early return (initialized=%d, pixels=%p)", s_initialized, argbPixels);
        return;
    }

    if (!Window_GLMakeCurrent()) {
        Log_Info("GL_Present: Window_GLMakeCurrent failed");
        return;
    }

    U64 profT0 = SDL_GetPerformanceCounter();

    /* A fresh present supersedes the previous eye's direct-scene decision. */
    s_stereoEye1SceneDirect = 0;

    /* Flush any remaining batched 3D geometry from the current frame.
       This draws into the FBO if SSAA is active, or the default
       framebuffer otherwise. */
    GL_FlushBatch();

    /* Double-wide stereo: both eyes are already in the scene target, so
       there is no per-eye scene composite.  Draw this eye's HUD straight
       into its half, then resolve once after the second eye. */
    if (s_stereoDoubleWide && s_stereoEye >= 0) {
        s_frameActive = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
        glDisable(GL_DEPTH_TEST);
        glViewport(s_sceneEyeX, 0, (GLsizei)s_fboWidth,
                   (GLsizei)s_fboHeight);
        {
            S32 savedAdjust = s_overlayImageAdjust;
            s_overlayImageAdjust = 0;
            GL_DrawArgbOverlay(argbPixels, pitch, width, height, filterLinear);
            s_overlayImageAdjust = savedAdjust;
        }

        if (s_stereoEye == 0) {
            s_stereoPairReady = 0;
        } else {
            GL_ResolveMSAAColor();
            s_stereoPairReady = 1;
        }

        U64 profT1 = SDL_GetPerformanceCounter();
        s_profAccumTicks[0] += profT1 - profT0;
        GL_ProfFrameDone();
        s_directScenePresent = 0;
        GL_InvalidateGLState();
        if (s_stereoEye == 1) {
            Window_GL_SwapWindow();
        }
        return;
    }

    if (OpenXR_IsActive())
        GL_EnsurePresentFBO();

    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3))
        GL_ResolveMSAAColor();

    /* Reset frame flag so GL_FlushBatch will rebind FBO next frame. */
    s_frameActive = 0;

    /* Switch back to the default framebuffer for compositing. On Quest with
       OpenXR there is no valid default framebuffer (FBO 0 has no color
       attachment), so use the present FBO instead so OpenXR_PresentMono can
       copy from it. On desktop, FBO 0 is the window backbuffer and
       SDL_GL_SwapWindow presents it, so we must draw there. */
    static S32 s_presentLogCount = 0;
    if (s_presentLogCount < 3) {
        Log_Info("GL_Present: openxr=%d presentFBO=%u stereoEye=%d", OpenXR_IsActive() ? 1 : 0, (unsigned)s_presentFBO, s_stereoEye);
        s_presentLogCount++;
    }
    if (s_stereoEye >= 0 && s_stereoEyeFBO[s_stereoEye]) {
        /* Stereo capture: composite this eye offscreen. Eye 0's present is
         * suppressed here (no swap, no clock step); eye 1's present falls
         * through to the swap below and its OpenXR_PresentMono submits the
         * stereo pair for the frame. */
        glBindFramebuffer(GL_FRAMEBUFFER, s_stereoEyeFBO[s_stereoEye]);
        if (s_stereoEye == 0)
            s_stereoPairReady = 1;
    } else {
        s_stereoPairReady = 0; /* mono frame: no fresh pair behind it */
        if (OpenXR_IsActive() && s_presentFBO) {
            glBindFramebuffer(GL_FRAMEBUFFER, s_presentFBO);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    /* Restore GL state for compositing (depth test may still be on from
       the batch flush above). */
    glDisable(GL_DEPTH_TEST);

    /* Compute letterbox viewport to maintain the game's aspect ratio,
       matching the SW renderer's SDL_LOGICAL_PRESENTATION_LETTERBOX. */
    SDL_Window *win = Window_GetSDLWindow();
    int winW = (int)width, winH = (int)height;
    if (win) {
        SDL_GetWindowSize(win, &winW, &winH);
    }
    /* On Quest with OpenXR, the window size is the headset resolution (e.g.
       4128x2208), but the present FBO is at the engine's render resolution
       (e.g. 1280x720). Use the content size for the letterbox calculation so
       the viewport fits the present FBO, not the headset display. */
    if (OpenXR_IsActive() && s_presentFBO) {
        winW = (int)width;
        winH = (int)height;
    }
    /* Compute the largest centred rectangle that preserves aspect ratio. */
    int vpX, vpY, vpW, vpH;
    {
        double windowAspect = (double)winW / (double)winH;
        double contentAspect = (double)width / (double)height;
        if (windowAspect > contentAspect) {
            /* Window wider than content → pillarbox */
            vpH = winH;
            vpW = (int)((double)winH * contentAspect + 0.5);
            vpW = (vpW + 1) & ~1;
            if (vpW > winW) vpW = winW;
            vpX = (winW - vpW) / 2;
            vpY = 0;
        } else {
            /* Window taller than content → letterbox */
            vpW = winW;
            vpH = (int)((double)winW / contentAspect + 0.5);
            vpH = (vpH + 1) & ~1;
            if (vpH > winH) vpH = winH;
            vpX = 0;
            vpY = (winH - vpH) / 2;
        }
    }
    /* glClear is unaffected by the viewport, so the bar clear needs no
       full-target viewport switch; go straight to the content rect. */
    GL_ClearLetterboxBars(vpX, vpY, vpW, vpH, winW, winH);
    glViewport(vpX, vpY, vpW, vpH);

    /* Draw the FBO color texture (3D scene) first, then the 2D overlay on top */
    if (s_fbo) {
        GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GL_SceneColorTex());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_BindArrayBuffer(s_fboVbo);
        if (s_antiAlias == 1 && s_fxaaProgram) {
            /* FXAA post-process: edge-detecting AA on the scene texture. */
            glUseProgram(s_fxaaProgram);
            glUniform1i(s_fxaa_uTex, 0);
            glUniform2f(s_fxaa_uTexel, 1.0f / (GLfloat)s_texWidth,
                        1.0f / (GLfloat)s_texHeight);
            GL_UploadImageAdjust(s_fxaaAdj_uBrightness, s_fxaaAdj_uContrast,
                                 s_fxaaAdj_uSaturation);
            glEnableVertexAttribArray(s_fxaa_aPos);
            glVertexAttribPointer(s_fxaa_aPos, 2, GL_FLOAT, GL_FALSE,
                                  4 * sizeof(GLfloat), (const void *)0);
            glEnableVertexAttribArray(s_fxaa_aTexCoord);
            glVertexAttribPointer(s_fxaa_aTexCoord, 2, GL_FLOAT, GL_FALSE,
                                  4 * sizeof(GLfloat),
                                  (const void *)(2 * sizeof(GLfloat)));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray(s_fxaa_aPos);
            glDisableVertexAttribArray(s_fxaa_aTexCoord);
        } else if (s_antiAlias == 4 && s_smaaBlendProgram) {
            /* SMAA post-process: three-pass morphological AA on the scene
               texture; draws the blended result into the current target. */
            GL_SmaaPresent(vpX, vpY, vpW, vpH);
        } else {
        glUseProgram(s_program);
        glUniform1i(s_uTexLoc, 0);
        /* The hi-res FBO is always drawn plainly; the Scale2x mode below is
         * for the 640x480 overlay only.  Pin it here or a stale Scale2x from
         * the previous frame's overlay draw would smear the FBO blit. */
        GL_UploadImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                             s_adj_uSaturation);
        glUniform1i(s_uFilterLoc, 1);
        glEnableVertexAttribArray(s_aPosLoc);
        glVertexAttribPointer(s_aPosLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), (const void *)0);
        glEnableVertexAttribArray(s_aTexCoordLoc);
        glVertexAttribPointer(s_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (const void *)(2 * sizeof(GLfloat)));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        GL_BindArrayBuffer(0);
        glUseProgram(0);

        glDisableVertexAttribArray(s_aPosLoc);
        glDisableVertexAttribArray(s_aTexCoordLoc);
        GL_BindArrayBuffer(0);
        glUseProgram(0);
    }

    GL_DrawArgbOverlay(argbPixels, pitch, width, height, filterLinear);

    /* Eye 0 stops here (its content is captured, not presented). Eye 1 and
     * every mono path continue to the swap, which on the Quest routes into
     * OpenXR_PresentMono — the frame's single submit point. */
    U64 profT1 = SDL_GetPerformanceCounter();
    s_profAccumTicks[0] += profT1 - profT0;
    GL_ProfFrameDone();
    s_directScenePresent = 0;
    /* The present quads rebind programs, textures and attrib locations
       0/1 for their own VBOs — the next frame's flushes must re-specify. */
    GL_InvalidateGLState();
    if (s_stereoEye != 0) {
        Window_GL_SwapWindow();
    }
}

/* ── Indexed present (GPU palette expansion) ─────────────────────────────── */

/* Draw the indexed 2D overlay into the currently bound target/viewport.
   The index texture and its band uploads are maintained by the caller. */
static void GL_DrawIndexedOverlay(U32 width, U32 height,
                                  const U32 *paletteU32, S32 filterLinear) {
    /* Refresh the 256x1 palette texture only when the palette changed. */
    {
        static U32 s_lutCache[256];
        static S32 s_lutCacheValid = 0;
        if (!s_lutCacheValid ||
            memcmp(s_lutCache, paletteU32, 256 * sizeof(U32)) != 0) {
            U8 rgba[256 * 4];
            for (int i = 0; i < 256; i++) {
                U32 c = paletteU32[i];
                rgba[i * 4 + 0] = (U8)(c >> 16); /* R */
                rgba[i * 4 + 1] = (U8)(c >> 8);  /* G */
                rgba[i * 4 + 2] = (U8)(c);       /* B */
                rgba[i * 4 + 3] = (U8)(c >> 24); /* A (0 for index 0) */
            }
            glBindTexture(GL_TEXTURE_2D, s_lutTex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA,
                            GL_UNSIGNED_BYTE, rgba);
            memcpy(s_lutCache, paletteU32, 256 * sizeof(U32));
            s_lutCacheValid = 1;
        }
    }

    GL_SetBlendFunc(1, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(s_presentIndexedProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
    glUniform1i(s_idx_uIndices, 0);
    if (s_overlayImageAdjust) {
        GL_UploadImageAdjust(s_idxAdj_uBrightness, s_idxAdj_uContrast,
                             s_idxAdj_uSaturation);
    } else {
        GL_UploadNeutralImageAdjust(s_idxAdj_uBrightness, s_idxAdj_uContrast,
                                    s_idxAdj_uSaturation);
    }
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_lutTex);
    glUniform1i(s_idx_uPalette, 1);
    glUniform2f(s_idx_uTexel, 1.0f / (GLfloat)width, 1.0f / (GLfloat)height);
    glUniform1i(s_idx_uFilter, filterLinear);

    GL_BindArrayBuffer(s_vbo);
    glEnableVertexAttribArray(s_idx_aPos);
    glVertexAttribPointer(s_idx_aPos, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(s_idx_aTexCoord);
    glVertexAttribPointer(s_idx_aTexCoord, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat),
                          (const void *)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(s_idx_aPos);
    glDisableVertexAttribArray(s_idx_aTexCoord);
    GL_BindArrayBuffer(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GL_PresentIndexed(const U8 *indices, U32 pitch, U32 width, U32 height,
                       const U32 *paletteU32, S32 filterLinear) {
    if (!s_initialized || indices == NULL || paletteU32 == NULL) {
        return;
    }

    if (!Window_GLMakeCurrent()) {
        return;
    }

    U64 profT0 = SDL_GetPerformanceCounter();

    /* A fresh present supersedes the previous eye's direct-scene decision. */
    s_stereoEye1SceneDirect = 0;

    /* Same frame plumbing as GL_Present: flush 3D, pick the composite
     * target, letterbox, draw the 3D FBO content, then the 2D overlay. */
    S32 useDoubleWide = (s_stereoDoubleWide && s_stereoEye >= 0);
    GL_FlushBatch();

    if (!useDoubleWide && OpenXR_IsActive())
        GL_EnsurePresentFBO();

    if (!useDoubleWide && s_msaaFbo &&
        (s_antiAlias == 2 || s_antiAlias == 3))
        GL_ResolveMSAAColor();

    s_frameActive = 0;

    int winW = (int)width, winH = (int)height;
    int vpX = 0, vpY = 0, vpW = (int)width, vpH = (int)height;
    if (useDoubleWide) {
        /* The scene for both eyes is already in the double-wide target.
         * Bind it directly and draw this eye's HUD into its half. */
        glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());
        glDisable(GL_DEPTH_TEST);
        glViewport(s_sceneEyeX, 0, (GLsizei)s_fboWidth,
                   (GLsizei)s_fboHeight);
    } else {
        if (s_stereoEye >= 0 && s_stereoEyeFBO[s_stereoEye]) {
            glBindFramebuffer(GL_FRAMEBUFFER, s_stereoEyeFBO[s_stereoEye]);
            if (s_stereoEye == 0)
                s_stereoPairReady = 1;
        } else {
            s_stereoPairReady = 0;
            if (OpenXR_IsActive() && s_presentFBO) {
                glBindFramebuffer(GL_FRAMEBUFFER, s_presentFBO);
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }

        glDisable(GL_DEPTH_TEST);

        SDL_Window *win = Window_GetSDLWindow();
        if (win) {
            SDL_GetWindowSize(win, &winW, &winH);
        }
        if (OpenXR_IsActive() && s_presentFBO) {
            winW = (int)width;
            winH = (int)height;
        }

        {
            double windowAspect = (double)winW / (double)winH;
            double contentAspect = (double)width / (double)height;
            if (windowAspect > contentAspect) {
                vpH = winH;
                vpW = (int)((double)winH * contentAspect + 0.5);
                vpW = (vpW + 1) & ~1;
                if (vpW > winW) vpW = winW;
                vpX = (winW - vpW) / 2;
                vpY = 0;
            } else {
                vpW = winW;
                vpH = (int)((double)winW / contentAspect + 0.5);
                vpH = (vpH + 1) & ~1;
                if (vpH > winH) vpH = winH;
                vpX = 0;
                vpY = (winH - vpH) / 2;
            }
        }

        /* glClear is unaffected by the viewport, so the bar clear needs no
           full-target viewport switch; go straight to the content rect. */
        GL_ClearLetterboxBars(vpX, vpY, vpW, vpH, winW, winH);
        glViewport(vpX, vpY, vpW, vpH);
    }

    /* Diff the Log overlay against the copy the texture currently holds:
       upload only the rows that changed, and skip the overlay draw entirely
       when the overlay is all zero (palette index 0 composites transparent,
       so an empty overlay draws exactly nothing — see the shader's palette
       entry 0).  A full 900 KB upload + full-screen memcmp every frame was
       a measurable slice of the present cost on slow CPUs. */
    S32 overlayEmpty = 0;
    /* Eye 1 can reuse eye 0's texture only when neither pass wrote moving
     * 2D content.  Moving boxes cover world sprites, dialogue and counters,
     * whose projection differs between the eyes; static boxes are the
     * screen-space HUD, which is identical for the pair. */
    if (s_stereoEye == 1 && s_overlayScanValid && !s_overlayEye0Dynamic &&
        !BoxHadMovingWrites()) {
        overlayEmpty = s_overlayEmptyCached;
        s_profOverlayReuse++;
    } else {
        static U8 *s_prevUpload = NULL;  /* mirror of s_logIndexTex content */
        static U8 *s_rowNonZero = NULL;  /* per row: 1 = has any nonzero */
        static U32 s_prevCap = 0;
        static S32 s_prevH = 0;
        U32 needed = (U32)pitch * height;
        S32 fullUpload = (s_logIndexTex == 0 || s_logIndexTexW != width ||
                          s_logIndexTexH != height);
        if (s_prevCap < needed || s_prevH < (S32)height) {
            free(s_prevUpload);
            free(s_rowNonZero);
            s_prevUpload = (U8 *)calloc(1, needed);
            s_rowNonZero = (U8 *)calloc(1, height);
            s_prevCap = s_prevUpload ? needed : 0;
            s_prevH = s_rowNonZero ? (S32)height : 0;
            fullUpload = 1; /* mirror restarted: reseed the texture */
        }
        if (s_prevUpload != NULL && s_rowNonZero != NULL) {
            /* The overlay upload used to follow the scene composite, which
               left unit 0 active.  It now runs first, so select unit 0
               explicitly before binding the index texture. */
            glActiveTexture(GL_TEXTURE0);
            if (fullUpload) {
                if (s_logIndexTex == 0) {
                    glGenTextures(1, &s_logIndexTex);
                }
                glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
                             GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                s_logIndexTexW = width;
                s_logIndexTexH = height;
#ifdef GL_UNPACK_ROW_LENGTH
                glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(pitch));
#endif
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)width,
                                (GLsizei)height, GL_LUMINANCE,
                                GL_UNSIGNED_BYTE, indices);
#ifdef GL_UNPACK_ROW_LENGTH
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
                memcpy(s_prevUpload, indices, needed);
                S32 hasContent = 0;
                for (U32 y = 0; y < height; y++) {
                    const U8 *row = indices + (U32)y * pitch;
                    const U8 *p = row;
                    const U8 *end = row + pitch;
                    U8 nz = 0;
                    while (p < end) {
                        if (*p++ != 0) {
                            nz = 1;
                            break;
                        }
                    }
                    s_rowNonZero[y] = nz;
                    if (nz) {
                        hasContent = 1;
                    }
                }
                overlayEmpty = !hasContent;
            } else {
                /* Coalesce: keep a band open across small gaps so scattered
                   changes (animating extras) don't degenerate into one
                   2-row upload call per changed row — a short run of
                   unchanged rows inside a band is cheaper to re-upload than
                   to split into another driver call. */
                const U32 BAND_GAP = 16;
                S32 hasContent = 0;
                S32 band0 = -1;
                S32 gap = 0;
                for (U32 y = 0; y < height; y++) {
                    const U8 *row = indices + (U32)y * pitch;
                    U8 *prev = s_prevUpload + (U32)y * pitch;
                    S32 changed = (memcmp(row, prev, pitch) != 0);
                    if (changed) {
                        const U8 *p = row;
                        const U8 *end = row + pitch;
                        U8 nz = 0;
                        while (p < end) {
                            if (*p++ != 0) {
                                nz = 1;
                                break;
                            }
                        }
                        s_rowNonZero[y] = nz;
                        memcpy(prev, row, pitch);
                        if (band0 < 0) {
                            band0 = (S32)y;
                        }
                        gap = 0;
                    } else if (band0 >= 0 && ++gap > (S32)BAND_GAP) {
                        /* band [band0..y-gap] closed: upload it */
#ifdef GL_UNPACK_ROW_LENGTH
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(pitch));
#endif
                        glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, band0,
                                        (GLsizei)width,
                                        (GLsizei)(y - gap - band0 + 1),
                                        GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                        indices + (U32)band0 * pitch);
#ifdef GL_UNPACK_ROW_LENGTH
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
                        band0 = -1;
                        gap = 0;
                    }
                    if (s_rowNonZero[y]) {
                        hasContent = 1;
                    }
                }
                if (band0 >= 0) {
#ifdef GL_UNPACK_ROW_LENGTH
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(pitch));
#endif
                    glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, band0,
                                    (GLsizei)width, (GLsizei)(height - band0),
                                    GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                    indices + (U32)band0 * pitch);
#ifdef GL_UNPACK_ROW_LENGTH
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
                }
                overlayEmpty = !hasContent;
            }
        }
        if (s_stereoEye == 0) {
            s_overlayScanValid = (s_logIndexTex != 0 || overlayEmpty);
            s_overlayEmptyCached = overlayEmpty;
            s_overlayEye0Dynamic = BoxHadMovingWrites();
        }
    }

    if (useDoubleWide) {
        if (overlayEmpty) {
            s_profOverlayEmpty++;
        } else {
            S32 savedAdjust = s_overlayImageAdjust;
            s_overlayImageAdjust = 0;
            GL_DrawIndexedOverlay(width, height, paletteU32, filterLinear);
            s_overlayImageAdjust = savedAdjust;
        }

        if (s_stereoEye == 0) {
            s_stereoPairReady = 0;
        } else {
            /* The overlay is part of the scene target now, so this single
             * resolve covers both eyes' 3D and HUD content. */
            GL_ResolveMSAAColor();
            s_stereoPairReady = 1;
        }
        s_profDirectScene++;

        U64 profT1 = SDL_GetPerformanceCounter();
        s_profAccumTicks[0] += profT1 - profT0;
        GL_ProfFrameDone();
        s_directScenePresent = 0;
        GL_InvalidateGLState();
        if (s_stereoEye == 1) {
            Window_GL_SwapWindow();
        }
        return;
    }

    /* An empty overlay composites to exactly the 3D scene.  On OpenXR the
     * swapchain blit can read that scene directly for mono frames and for
     * stereo eye 1 (the last eye rendered), so skip the intermediate
     * full-screen composite there.  Eye 0 cannot skip: its scene FBO is
     * overwritten by the second eye before the submit.  Keep the composite
     * when a post-process or a non-neutral display transform would change
     * the pixels. */
    /* The direct read assumes the scene texture is exactly the content size:
       OpenXR blits ModeDesiredX x ModeDesiredY texels.  An SSAA FBO is
       larger and must go through the downscaling composite. */
    S32 sceneFboMatches = (s_fboWidth == width && s_fboHeight == height);
    /* A direct scene read is only equivalent to the composite when the
       overlay adds nothing, the content fills the target, the scene is
       content-sized, and no post-process or display transform is active. */
    S32 directSceneSafe = overlayEmpty && s_fbo && !s_stereoDoubleWide &&
                          sceneFboMatches && vpX == 0 && vpY == 0 &&
                          vpW == winW && vpH == winH &&
                          s_antiAlias != 1 && s_antiAlias != 4 &&
                          GL_ImageAdjustNeutral();
    S32 sceneOnlyPresent = 0;
    if (directSceneSafe && OpenXR_IsActive() &&
        (s_stereoEye < 0 || s_stereoEye == 1)) {
        sceneOnlyPresent = 1;
        if (s_stereoEye == 1) {
            s_stereoEye1SceneDirect = 1;
        }
    }
    if (overlayEmpty) s_profOverlayEmpty++;
    if (directSceneSafe) s_profDirectScene++;

    if (s_fbo && !sceneOnlyPresent) {
        GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GL_SceneColorTex());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_BindArrayBuffer(s_fboVbo);
        if (s_antiAlias == 1 && s_fxaaProgram) {
            /* FXAA post-process: edge-detecting AA on the scene texture. */
            glUseProgram(s_fxaaProgram);
            glUniform1i(s_fxaa_uTex, 0);
            glUniform2f(s_fxaa_uTexel, 1.0f / (GLfloat)s_texWidth,
                        1.0f / (GLfloat)s_texHeight);
            GL_UploadImageAdjust(s_fxaaAdj_uBrightness, s_fxaaAdj_uContrast,
                                 s_fxaaAdj_uSaturation);
            glEnableVertexAttribArray(s_fxaa_aPos);
            glVertexAttribPointer(s_fxaa_aPos, 2, GL_FLOAT, GL_FALSE,
                                  4 * sizeof(GLfloat), (const void *)0);
            glEnableVertexAttribArray(s_fxaa_aTexCoord);
            glVertexAttribPointer(s_fxaa_aTexCoord, 2, GL_FLOAT, GL_FALSE,
                                  4 * sizeof(GLfloat),
                                  (const void *)(2 * sizeof(GLfloat)));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray(s_fxaa_aPos);
            glDisableVertexAttribArray(s_fxaa_aTexCoord);
        } else if (s_antiAlias == 4 && s_smaaBlendProgram) {
            /* SMAA post-process: three-pass morphological AA on the scene
               texture; draws the blended result into the current target. */
            GL_SmaaPresent(vpX, vpY, vpW, vpH);
        } else {
        glUseProgram(s_program);
        glUniform1i(s_uTexLoc, 0);
        /* The hi-res FBO is always drawn plainly; the Scale2x mode below is
         * for the 640x480 overlay only.  Pin it here or a stale Scale2x from
         * the previous frame's overlay draw would smear the FBO blit. */
        GL_UploadImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                             s_adj_uSaturation);
        glUniform1i(s_uFilterLoc, 1);
        glEnableVertexAttribArray(s_aPosLoc);
        glVertexAttribPointer(s_aPosLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat), (const void *)0);
        glEnableVertexAttribArray(s_aTexCoordLoc);
        glVertexAttribPointer(s_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (const void *)(2 * sizeof(GLfloat)));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        GL_BindArrayBuffer(0);
        glUseProgram(0);

        glDisableVertexAttribArray(s_aPosLoc);
        glDisableVertexAttribArray(s_aTexCoordLoc);
        GL_BindArrayBuffer(0);
        glUseProgram(0);
    }

    {
        /* Optional one-shot dump of the raw Log overlay for comparison. */
        static S32 s_logDumped = 0;
        if (!s_logDumped && getenv("LBA2_DUMP_LOG")) {
            U8 *rgb2 = (U8 *)malloc((size_t)width * height * 3);
            if (rgb2) {
                const U8 *src = (const U8 *)indices;
                for (U32 i = 0; i < width * height; i++) {
                    U32 c = paletteU32[src[i]];
                    rgb2[i * 3 + 0] = (U8)(c >> 16);
                    rgb2[i * 3 + 1] = (U8)(c >> 8);
                    rgb2[i * 3 + 2] = (U8)(c);
                }
                SavePNGRGB("/tmp/gl_log.png", rgb2, (S32)width, (S32)height);
                free(rgb2);
                s_logDumped = 1;
            }
        }
    }

    /* PresentMono's mono fast path reads the scene directly when this is 1;
       advertise it only under the same conditions as sceneOnlyPresent, so a
       post-processed or display-adjusted composite is not bypassed. */
    s_directScenePresent = directSceneSafe;
    if (!overlayEmpty) {
        GL_DrawIndexedOverlay(width, height, paletteU32, filterLinear);
    }

    U64 profT1 = SDL_GetPerformanceCounter();
    s_profAccumTicks[0] += profT1 - profT0;
    GL_ProfFrameDone();
    /* The present quads rebind programs, textures and attrib locations
       0/1 for their own VBOs — the next frame's flushes must re-specify. */
    GL_InvalidateGLState();

    if (s_stereoEye != 0) {
        Window_GL_SwapWindow();
    }
}

/* ── Accessors for OpenXR present ────────────────────────────────────────── */
/* OpenXR_PresentMono needs to read the present FBO on Quest where there is
   no SDL window. These accessors expose the handles without exposing the
   full GL renderer internals. */
U32 GL_GetPresentFBO(void) {
    return (U32)s_presentFBO;
}

/* Quest present fast path: when the last indexed present was a plain
 * scene (empty overlay, no post-process, content-sized FBO), PresentMono
 * can blit the 3D FBO straight into the swapchain and skip the
 * intermediate fullscreen composite. */
S32 GL_PresentSceneDirect(void) {
    /* The mono fast path reads a single content-sized scene.  A double-wide
     * target is a stereo pair, not a mono frame, even if stereo was just
     * switched off. */
    return s_stereoDoubleWide ? 0 : s_directScenePresent;
}

U32 GL_Get3DColorTex(void) {
    return (U32)GL_SceneColorTex();
}

/* Direct present reads this FBO.  In-tile mode the single-sample resolve
 * view exposes s_msaaColorTex with the same linear-filter blit contract as
 * the explicit s_fbo path; a multisampled read framebuffer would reject the
 * scaling blit. */
U32 GL_Get3DFBO(void) {
    return (U32)GL_SceneReadFbo();
}

U32 GL_GetPresentColorTex(void) {
    return (U32)s_presentColorTex;
}

/* ── Triangle list rendering ────────────────────────────────────────────── */

S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    if (!s_initialized) {
        GL_LastRejectReason = 1;
        return 0;
    }

    /* Log every textured call (once per type/color change triggers flush) */

    /* ── Non-textured polygon types (0–7) ── */
    if (typePoly >= 0 && typePoly <= 7) {
        /* Types 0/1 (SOLID/FLAT), 2/3 (TRANS/TRAME),
           4/5 (GOURAUD/DITHER), 6/7 (GOURAUD_TABLE/DITHER_TABLE)
           are all handled in GL. */
    } else if (typePoly < POLY_TEXTURE || typePoly > POLY_TEXTURE_Z_FOG) {
        /* Unknown type — fall back to SW. */
        GL_LastRejectReason = 2;
        return 0;
    }

    /* Body polygons (non-Z-buffered, non-OBJ_ZBUFFER) must fall back to
       the SW rasterizer (Log buffer) rather than the FBO.  The FBO is
       cleared only by GL_ClearFBO() at the start of terrain rendering.
       Body polygons rendered to the FBO outside terrain passes accumulate
       across frames (e.g. the comportement menu's spinning Twinsen models),
       producing ghosting.  Routing them to Log keeps them in the same
       compositing layer as shadows and the 2D overlay.

       ShadeBoxBlk (shadow darkening) operates on Log pixels via CLUT
       lookup.  When body polygons go through the GL batch into the FBO,
       Log is empty and ShadeBoxBlk darkens index-0 → opaque black.
       Keeping bodies in Log ensures ShadeBoxBlk has real pixels to darken
       (producing correct transparent shadows) and bodies remain visible
       on top of the terrain in the final GL_Present composite. */
    if (!Fill_Flag_ZBuffer) {
        GL_LastRejectReason = 3;
        return 0;
    }

    /* Vertices the SW projection could not place (behind the near plane, or
       beyond S16 screen coords) carry the (-32768, -32768) sentinel.  The GPU
       would treat that as an ordinary clip-space position — a triangle flung
       across the whole screen that also poisons the depth buffer for the
       terrain drawn after it — so hand these polygons to the SW filler, which
       clips them against the screen rect the way the original engine did. */
    for (S32 i = 0; i < nbPoints; i++) {
        if (ptrPoints[i].Pt_XE == -32768 && ptrPoints[i].Pt_YE == -32768) {
            GL_LastRejectReason = 7;
            return 0;
        }
    }

    /* NZW polygons (animated polys: flowers, water) go through the GL
       batch when the FBO contains valid terrain depth (s_fboHasTerrain).
       This routes them into the FBO with depth testing against terrain,
       rather than the SW rasterizer / Log buffer.  In the FBO they are
       properly occluded by closer terrain; in GL_Present the Log overlay
       (objects, shadows) composites on top, so objects always appear in
       front of animated polys.  Falls back to SW when the FBO has no
       terrain (menu/modal loops, before the first GL_ClearFBO). */
    if (Fill_Flag_NZW && !s_fboHasTerrain) {
        GL_LastRejectReason = 4;
        return 0;
    }

    /* Need at least 3 points for a triangle */
    if (nbPoints < 3) {
        GL_LastRejectReason = 5;
        return 0;
    }

    /* 2D screen-space effects (menu plasma/fire bars) must render into
       the Log buffer (2D overlay), not the FBO (3D scene).  They use
       POLY_TEXTURE (type 8) with RepMask 0x3F3F — same polygon type
       as terrain but in a different compositing layer.  Let the SW
       rasterizer handle them so they land in the 2D overlay on top of
       the menu text. */
    if (typePoly == POLY_TEXTURE && (RepMask & 0xFFFF) == 0x3F3F) {
        GL_LastRejectReason = 6;
        return 0;
    }

    /* Pack the polygon material: shading mode, alpha/chroma/perspective
       bits in the low byte, flat colour (or shadow level) in the high
       byte.  All of it rides per vertex, so polygons that differ only in
       colour — or in flat-vs-textured mode — can share one draw call.
       The flush trigger keeps only true per-draw state: blend-relevant
       alpha bits, atlas page, Z-buffer flags — and the CLUT row for the
       non-textured modes that still read it from the uniform.
       CLUT-textured polygons (mode 0) carry their CLUT base row in the
       high byte instead: SetCLUT() changes PtrCLUTGouraud per terrain
       tile with the fog distance, and a flush per tile fragmented the
       terrain pass into ~100 draw calls per frame (one per tile row
       block).  The high byte is free there — the flat colour is only
       meaningful for types below POLY_TEXTURE. */
    U16 matFlags;
    if (typePoly < POLY_TEXTURE) {
        if (typePoly <= 1) {
            matFlags = 1; /* SOLID / FLAT */
        } else if (typePoly == 2) {
            matFlags = 1 | (1 << 3); /* TRANS → flat + blend */
        } else if (typePoly == 3) {
            matFlags = 1 | (2 << 3); /* TRAME → flat + stipple */
        } else if (typePoly <= 5) {
            matFlags = 2; /* GOURAUD / DITHER (dithering not in GL) */
        } else {
            matFlags = 3; /* GOURAUD_TABLE / DITHER_TABLE */
        }
    } else if (typePoly == POLY_TEXTURE ||
               typePoly == POLY_TEXTURE_INCRUST ||
               typePoly == POLY_TEXTURE_Z ||
               typePoly == POLY_TEXTURE_Z_INCRUST) {
        /* The non-flat texture types write the texel as a palette index
           directly (Filler_Texture / TextureChromaKey / TextureZ /
           TextureZChromaKey: *log = texel) — no CLUT lookup. */
        matFlags = 4;
    } else if (typePoly == POLY_TEXTURE_Z_FOG) {
        /* Type 24 (POLY_TEXTURE_Z_FOG, e.g. sea/sky): SW
           Filler_TextureZFogSmoothZBuf / ...NZW select the CLUT row from
           the per-pixel Z distance to the fog near plane, ignoring
           Pt_Light.  polyMode 5 replicates that in the fragment shader. */
        matFlags = 5;
    } else {
        matFlags = 0; /* textured (CLUT-based) */
    }
    if (typePoly >= POLY_TEXTURE) {
        /* Chroma key: only INCRUST types (12-15, 20-23) discard palette
           index 0 — the INCRUST bit is bit 2 of the type number. */
        matFlags = (U16)(matFlags | ((typePoly & 4) ? (1 << 5) : 0));
        /* Z-textured types (16-24) are perspective-corrected in the SW
           rasterizer (Filler_TextureZ / FogSmooth divide MapU, MapV, ZBuf
           by W = 1/Z); types 8-15 (Filler_Texture / Gouraud) are affine. */
        if (typePoly >= POLY_TEXTURE_Z && typePoly <= POLY_TEXTURE_Z_FOG) {
            matFlags = (U16)(matFlags | (1 << 6));
        }
    }

    /* High byte: flat colour for non-textured modes, CLUT base row for
       CLUT-textured ones.  SetCLUT() stores PtrCLUTGouraud at
       PtrCLUTFog + ((line << 8) & 0xF000), so the row is 0..240 — see
       SetCLUT in LIB386/pol_work/POLY.CPP. */
    U8 highByte = (U8)((typePoly < POLY_TEXTURE ? colorPoly : 0) & 0xFF);
    /* CLUT-textured polys (mode 0) carry their CLUT base row in the high
       byte and gouraud-table ones (mode 3) in the RepMask vertex field —
       SetCLUT() changes PtrCLUTGouraud per terrain tile with the fog
       distance, so keeping it out of the flush trigger is what lets tiles
       share draw calls.  SetCLUT() stores PtrCLUTGouraud at
       PtrCLUTFog + ((line << 8) & 0xF000), so the row is 0..240 — see
       SetCLUT in LIB386/pol_work/POLY.CPP. */
    if ((matFlags & 7) == 0 && PtrCLUTFog != NULL && PtrCLUTGouraud != NULL) {
        highByte = (U8)(((const U8 *)PtrCLUTGouraud -
                         (const U8 *)PtrCLUTFog) >> 8);
    }
    U16 material = (U16)(matFlags | ((U16)highByte << 8));

    /* Textured polys inside the decor/object windows sample the texture
       array; terrain tiles, sky/sea and anything outside those windows
       stay on the legacy 2D path.  Modes 0 and 4 branch to the array
       sampler in the injected fragment code; alpha and fog-smooth keep
       per-draw state the array branch doesn't reproduce. */
    static S32 s_noArray = -1;
    if (s_noArray < 0) {
        s_noArray = getenv("LBA2_NO_ARRAY") ? 1 : 0;
    }
    U8 arrNow;
    if (typePoly >= POLY_TEXTURE) {
        arrNow = (U8)(!s_noArray && s_arrayWindow && s_clut3Program != 0 &&
                      ((matFlags & 7) == 0 || (matFlags & 7) == 4) &&
                      !(matFlags & 0x18));
    } else {
        /* Non-sampling polys inherit the open batch's class: bodies mix
           textured and flat polys, and flipping the class per flat poly
           would flush the batch to pieces.  Their RepMask stays the CLUT
           row (< 32768), so the array branch never samples them. */
        arrNow = s_batchArray;
    }

    if (s_batchVertCount > 0) {
        /* Only the alpha mode rides real per-draw GL state (TRANS batches
           enable blending), and the fog-smooth mode pins the depth func to
           ALWAYS when NZW (sea/sky over terrain) — everything else
           (mode/chroma/perspective) branches per fragment in the shader. */
        S32 trigger = ((s_currentMatFlags & 0x18) != (matFlags & 0x18) ||
                       ((matFlags & 7) == 5) !=
                           ((s_currentMatFlags & 7) == 5) ||
                       (typePoly >= POLY_TEXTURE && !arrNow &&
                        s_batchAtlas != (const void *)PtrMap) ||
                       s_batchArray != arrNow ||
                       s_batchZBuffer != Fill_Flag_ZBuffer ||
                       s_batchNZW != Fill_Flag_NZW);
        if (trigger) {
            /* NZW alternation (terrain <-> animated per tile) is the dominant
               flush driver: route to the defer pool instead of flushing.
               Both pools draw depth-tested opaque geometry, so flushing
               NZW=0 before NZW=1 at frame end gives the same image. */
            S32 poolable = !(matFlags & 8) && ((matFlags & 7) != 5) &&
                           !(s_currentMatFlags & 8) &&
                           ((s_currentMatFlags & 7) != 5);
            if (poolable && s_batchNZW != Fill_Flag_NZW) {
                if (s_deferVertCount > 0 &&
                    s_deferNZW == (U8)Fill_Flag_NZW) {
                    GL_SwapBatchWithDefer();
                } else if (s_deferVertCount == 0) {
                    GL_SwapBatchWithDefer();
                } else {
                    GL_FlushBatch();
                }
            } else {
                GL_FlushBatch();
            }
        }
    } else if (s_deferVertCount > 0 && s_deferNZW == (U8)Fill_Flag_NZW &&
               !(matFlags & 8) && ((matFlags & 7) != 5)) {
        GL_SwapBatchWithDefer();
    }

    /* Update state tracking (captured at batch start for flush) */
    s_currentTypePoly = (U32)typePoly;
    s_currentColorPoly = (U32)colorPoly;
    s_currentMatFlags = matFlags;
    s_currentMaterial = material;
    /* Sticky: a batch may mix textured and flat polys (only the alpha mode
       forces a flush), so the flush must bind an atlas page as long as ANY
       poly in the batch samples it — the flat ones never do. */
    if (typePoly >= POLY_TEXTURE) {
        s_batchTextured = 1;
        s_batchAtlas = (const void *)PtrMap;
    }
    if (typePoly >= POLY_TEXTURE) {
        s_batchArray = arrNow;
    }
    s_batchCLUTGouraud = (const void *)PtrCLUTGouraud;
    s_batchZBuffer = Fill_Flag_ZBuffer;
    s_batchNZW = Fill_Flag_NZW;

    /* Capture CLUT base row from current PtrCLUTGouraud.
       SetCLUT() changes PtrCLUTGouraud per terrain tile with the fog
       distance, so keeping it out of the flush trigger is what lets tiles
       share draw calls. */
    s_batchCLUTBaseRow = 0.0f;
    if (PtrCLUTFog != NULL && PtrCLUTGouraud != NULL) {
        s_batchCLUTBaseRow =
            (GLfloat)(((const U8 *)PtrCLUTGouraud - (const U8 *)PtrCLUTFog) /
                      256);
    }

    /* Capture fog distance constants at batch start (mode 5, type 24).
       SetFog() runs once per scene load, so these are stable across the
       whole batch; capturing here keeps flush-time consistent. */
    s_batchScaledFogNear = (GLfloat)Fill_ScaledFogNear;
    s_batchFogRowScale = (GLfloat)((double)Fill_Fog_Factor / 65536.0);

    /* Upload textures if the underlying data has changed.
       CLUT re-uploads every frame because the game modifies the fog table
       in-place between scenes (same PtrCLUTFog pointer, different data). */
    GL_UploadCLUT();
    if (typePoly >= POLY_TEXTURE && AtlasTextureDirty) {
        /* Animated polys (water, menu plasma) mark the atlas dirty mid
           frame.  Bump the content version so the next flush of every page
           re-uploads exactly once; GL_BindAtlasFor (flush time) does the
           upload.  Uploading per polygon cost another ~40 MB per terrain
           pass on mobile GPUs. */
        s_atlasContentVersion++;
        AtlasTextureDirty = 0;
    }
    GL_UploadPalette(); /* re-upload every frame (cheap: 1 KB) */

    /* Flat-textured types (9/13/17/21) shade at a fixed CLUT row selected
       by the polygon's color argument: the SW preps build Fill_Color.Ptr =
       PtrCLUTGouraud + (color << 8) and the flat fillers ignore Pt_Light.
       Stamp the row selector into Pt_Light so the CLUT shader's
       gouraudRow = floor((light+128)/256) + baseRow lands on the same
       fogTable[16*fog + color] row the SW fillers read. */
    if (typePoly == POLY_TEXTURE_FLAT || typePoly == POLY_TEXTURE_INCRUST_FLAT ||
        typePoly == POLY_TEXTURE_Z_FLAT || typePoly == POLY_TEXTURE_Z_INCRUST_FLAT) {
        U16 rowLight = (U16)((colorPoly & 0xF) << 8);
        for (S32 i = 0; i < nbPoints; i++) {
            ptrPoints[i].Pt_Light = rowLight;
        }
    }

    /* Stamp per-polygon RepMask onto each vertex (SW rasterizer uses the
       global RepMask; GL needs it per-vertex for batching).  Gouraud-table
       polys (types 6/7) never texture, so their RepMask vertex field is
       free — stamp the CLUT base row there instead.  Like the mode-0 high
       byte above, this lets SetCLUT() changes ride per vertex instead of
       forcing a flush per terrain tile. */
    if ((matFlags & 7) == 3) {
        S32 row = (PtrCLUTFog != NULL && PtrCLUTGouraud != NULL)
                      ? (S32)(((const U8 *)PtrCLUTGouraud -
                               (const U8 *)PtrCLUTFog) >> 8)
                      : 0;
        for (S32 i = 0; i < nbPoints; i++) {
            ptrPoints[i].Pt_RepMask = (U16)row;
        }
    } else {
        U16 rm;
        if (arrNow && typePoly >= POLY_TEXTURE) {
            rm = (U16)(0x8000u | (U16)GL_ArraySlotFor(
                                     PtrMap, (U32)(RepMask & 0xFF) + 1,
                                     (U32)((RepMask >> 8) & 0xFF) + 1)
                                     ->layer);
        } else {
            rm = (U16)(RepMask & 0xFFFF);
            s_batchMaxMask |= (U16)(RepMask & 0xFFFF);
        }
        for (S32 i = 0; i < nbPoints; i++) {
            ptrPoints[i].Pt_RepMask = rm;
        }
    }

    /* Triangulate with interior diagonals only — a fan from vertex 0 would
       spill this polygon's shade outside the outline whenever a reflex
       vertex lands on the apex (see GL_TriangulatePolygon). */
    U16 tris[3 * 64];
    S32 ntri = GL_TriangulatePolygon(ptrPoints, nbPoints, tris);
    for (S32 i = 0; i < ntri; i += 3) {
        GL_AddTriangle(&ptrPoints[tris[i]], &ptrPoints[tris[i + 1]],
                       &ptrPoints[tris[i + 2]]);
    }

    return 1; /* handled */
}

#else /* !USE_GPURENDERER */

/* Stubs when GL renderer is compiled out */
S32 GL_Renderer_Init(U32 resX, U32 resY) {
    (void)resX; (void)resY;
    return FALSE;
}
void GL_Renderer_Shutdown(void) {}
void GL_Renderer_Resize(U32 resX, U32 resY) {
    (void)resX; (void)resY;
}
void GL_BeginFrame(void) {}
void GL_ClearFBO(void) {}
void GL_ReadbackDepthBuffer(void) {}
void GL_ReadbackColorBuffer(void) {}
void GL_SnapshotTerrain(void) {}
void GL_SavePresentPNG(const char *path) {
    (void)path;
}
void GL_RestoreTerrainSnapshot(void) {}
void GL_DrawShadowPoly(const S16 *xs, const S16 *ys, const U16 *az, S32 n,
                       U32 level) {
    (void)xs;
    (void)ys;
    (void)az;
    (void)n;
    (void)level;
}
void GL_ShadeBox(S32 x0, S32 y0, S32 x1, S32 y1, S32 level) {
    (void)x0; (void)y0; (void)x1; (void)y1; (void)level;
}
void GL_SetRenderQuality(U32 quality) { (void)quality; }
void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear) {
    (void)argbPixels; (void)pitch; (void)width; (void)height;
    (void)filterLinear;
}
void GL_PresentIndexed(const U8 *indices, U32 pitch, U32 width, U32 height,
                       const U32 *paletteU32, S32 filterLinear) {
    (void)indices; (void)pitch; (void)width; (void)height;
    (void)paletteU32; (void)filterLinear;
}
S32 GL_SphereTargetsFBO(void) { return 0; }
void GL_ArrayWindowBegin(void) {}
void GL_ArrayWindowEnd(void) {}
S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    (void)typePoly; (void)colorPoly; (void)nbPoints; (void)ptrPoints;
    return 0;
}

#endif /* USE_GPURENDERER */
