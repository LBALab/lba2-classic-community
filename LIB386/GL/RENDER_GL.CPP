#include <GL/RENDER_GL.H>

#include <3D/CAMERA.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/WINDOW.H>
#include <SYSTEM/OPENXR.H>

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

/* ── External palette getter (defined in SDL.CPP) ───────────────────────── */
extern "C" const U32 *GL_GetPaletteLUT(void);

/* ── External CLUT / texture globals (from POLY.CPP) ────────────────────── */
extern "C" {
extern PTR_U8 PtrCLUTFog;
extern PTR_U8 PtrCLUTGouraud;
extern PTR_U8 PtrMap;
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
   terrain, 5=too few points, 6=menu plasma mask; 0=accepted). */
extern "C" S32 GL_LastRejectReason;
S32 GL_LastRejectReason = 0;

/* ── External texture filter toggle (from GLOBAL.CPP) ────────────────────── */
extern "C" S32 DisplayTexFilter;

/* Temporary perf instrumentation: draw calls flushed since GL_BeginFrame */
extern "C" S32 GL_BatchFlushCount;
S32 GL_BatchFlushCount = 0;

/* ── Shader sources (GL 2.1 / GLES 2.0 compatible) ─────────────────────── */

#if defined(__ANDROID__)
#define GLSL_VERT_HEADER "#version 100\n"
#define GLSL_FRAG_HEADER "#version 100\nprecision mediump float;\n"
#else
#define GLSL_VERT_HEADER "#version 110\n"
#define GLSL_FRAG_HEADER "#version 110\n"
#endif

/* ── Present shader (2D overlay quad) ───────────────────────────────────── */

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
    "uniform sampler2D uTex;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(uTex, vTexCoord);\n"
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
    "}\n";

static const char *s_clutFragSrc =
    GLSL_FRAG_HEADER
    "uniform sampler2D uAtlas;\n"   /* 256×256 LUMINANCE (or sprite page) */
    "uniform vec2 uAtlasScale;\n"   /* texture dims for texel-space UVs */
    "uniform sampler2D uCLUT;\n"    /* 256×256 LUMINANCE (fog table) */
    "uniform sampler2D uPalette;\n" /* 256×1 BGRA (paletteLUT) */
    "uniform int uPolyMode;\n"      /* 0=texture 1=flat 2=gouraud 3=gouraudTable 4=textureNoCLUT 5=fogSmooth 6=shadowGray */
    "uniform float uFlatColor;\n"   /* palette index (mode 1) / CLUT column (mode 3) */
    "uniform float uCLUTBaseRow;\n" /* CLUT row offset for gouraud table (mode 3) */
    "uniform float uScaledFogNear;\n" /* Fill_ScaledFogNear (fog-smooth distance, mode 5) */
    "uniform float uFogRowScale;\n"   /* Fill_Fog_Factor / 65536 (fog-smooth distance, mode 5) */
    "uniform int uAlphaMode;\n"     /* 0=opaque 1=TRANS(semi) 2=TRAME(stipple) */
    "uniform int uBilinear;\n"      /* 0=nearest 1=bilinear CLUT sampling */
    "uniform int uChromaKey;\n"     /* 0=no chroma key 1=discard index 0 */
    "uniform int uPerspCorrect;\n"  /* 1=perspective-correct UV/Z (types 16-24) */
    "varying vec2 vTexCoord;\n"
    "varying vec2 vTexCoordOverW;\n"
    "varying float vInvW;\n"
    "varying float vLight;\n"
    "varying float vRepMask;\n"
    "varying float vZ;\n"
    "varying float vZOverW;\n"
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
    "                   float gouraudRow) {\n"
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
    "    if (uChromaKey != 0 && t00 < 0.5) discard;\n"
    "    vec4 c00 = clutLookup(t00, gouraudRow);\n"
    "    vec4 c10 = clutLookup(t10, gouraudRow);\n"
    "    vec4 c01 = clutLookup(t01, gouraudRow);\n"
    "    vec4 c11 = clutLookup(t11, gouraudRow);\n"
    "    return mix(mix(c00, c10, fu), mix(c01, c11, fu), fv);\n"
    "}\n"
    "void main() {\n"
    "    vec4 color;\n"
    "    float alphaOverride = 0.0;\n"
    /* Select perspective-corrected or affine texture/Z varyings.
       uPerspCorrect is 1 for Z-textured types (16-24), where the SW
       rasterizer divides MapU/MapV/ZBuf by W = 1/Z; types 8-15
       (SW POLYTEXT/POLYGTEX) interpolate affinely. */
    "    vec2 tc = mix(vTexCoord, vTexCoordOverW / vInvW, float(uPerspCorrect));\n"
    "    float vZsel = mix(vZ, vZOverW / vInvW, float(uPerspCorrect));\n"
     "    if (uPolyMode == 0) {\n"
     /* Textured: atlas → CLUT → palette */
     "        tc = tc / 256.0;\n"
     "        float widthMask = mod(vRepMask, 256.0);\n"
     "        float heightMask = floor(vRepMask / 256.0);\n"
     /* SW init: Fill_CurGouraudMin = (Pt_Light << 8) + 0x8000.
        The +128 (0x80) rounding bias shifts the CLUT row by 1 in some
        cases — same bias applied in polyMode 2. */
     "        float gouraudRow = floor((vLight + 128.0) / 256.0) + uCLUTBaseRow;\n"
    "        if (uBilinear != 0) {\n"
    "            color = bilinearCLUT(tc, widthMask, heightMask, gouraudRow);\n"
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
    "            if (uChromaKey != 0 && texel < 0.5) discard;\n"
    "            color = clutLookup(texel, gouraudRow);\n"
    "        }\n"
    "    } else if (uPolyMode == 1) {\n"
    /* Flat solid: single palette color */
    "        float palIdx = uFlatColor;\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    } else if (uPolyMode == 2) {\n"
    /* Gouraud (types 4/5): palette = (color + (light+128)/256).
       SW init: Fill_CurGouraudMin = (Pt_Light << 8) + 0x8000 + (color << 16).
       The +0x8000 rounding bias means the palette index is
       (color + (Pt_Light + 128) / 256), not (color + Pt_Light / 256). */
    "        float palIdx = mod(uFlatColor + floor((vLight + 128.0) / 256.0), 256.0);\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    } else if (uPolyMode == 4) {\n"
    /* Texture without CLUT (types 8 and 12):
       SW Filler_Texture / Filler_TextureChromaKey writes texel as palette
       index directly (*line = texel).  No CLUT lookup — the atlas texel
       IS the palette index.  Chroma key controlled by uChromaKey. */
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
    "        if (uChromaKey != 0 && palIdx < 0.5) discard;\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    } else if (uPolyMode == 5) {\n"
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
    "    } else if (uPolyMode == 6) {\n"
    /* Ground shadow (SW ShadeBoxBlk replacement): constant gray source
       combined with GL_DST_COLOR/GL_ZERO blending multiplies the scene
       color by uFlatColor = (15 - ShadowLevel) / 15 — the linear
       counterpart of the SW fade-CLUT row (15 - level)/15, which maps
       each palette entry to its darker ramp value. */
    "        color = vec4(uFlatColor, uFlatColor, uFlatColor, 1.0);\n"
    "    } else {\n"
    /* Gouraud table (types 6/7): CLUT lookup, color = column.
       Same +128 rounding bias as polyMode 0. */
    "        float gouraudRow = floor((vLight + 128.0) / 256.0) + uCLUTBaseRow;\n"
    "        vec2 clutUV = vec2((uFlatColor + 0.5) / 256.0,\n"
    "                           (gouraudRow + 0.5) / 256.0);\n"
    "        float palIdx = texture2D(uCLUT, clutUV).r * 255.0;\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    }\n"
    /* TRAME stipple: discard every other fragment (50% coverage) */
    "    if (uAlphaMode == 2) {\n"
    "        if (mod(floor(gl_FragCoord.x) + floor(gl_FragCoord.y), 2.0) < 1.0)\n"
    "            discard;\n"
    "    }\n"
    "    if (uAlphaMode > 0) {\n"
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

static const char *s_presentIndexedFragSrc =
    GLSL_FRAG_HEADER
    "uniform sampler2D uIndices;\n"
    "uniform sampler2D uPalette;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    float v = texture2D(uIndices, vTexCoord).r;\n"
    /* NEAREST filtering makes v exactly i/255; rebuild the texel index and
     * step to the palette texel centre the same way the CLUT shader does. */
    "    float i = floor(v * 255.0 + 0.5);\n"
    "    gl_FragColor = texture2D(uPalette, vec2((i + 0.5) / 256.0, 0.5));\n"
    "}\n";

/* ── Frame cost profiler ─────────────────────────────────────────────────── */
/* No on-device profiler exists for the Android build, so the three known
 * CPU-side frame costs are accumulated around their sections and summarized
 * in the log every GL_PROF_FRAMES presented frames. */
#define GL_PROF_FRAMES 300
static U64 s_profAccumTicks[3] = {0, 0, 0}; /* 0=present 1=depth readback 2=depth convert */
static U32 s_profFrames = 0;

static void GL_ProfFrameDone(void) {
    s_profFrames++;
    if (s_profFrames >= GL_PROF_FRAMES) {
        double msPerTick = 1000.0 / (double)SDL_GetPerformanceFrequency();
        Log_Warn("[GLPerf] frames=%u present=%.1fms readback=%.1fms "
                 "convert=%.1fms",
                 s_profFrames,
                 (double)s_profAccumTicks[0] * msPerTick / (double)s_profFrames,
                 (double)s_profAccumTicks[1] * msPerTick / (double)s_profFrames,
                 (double)s_profAccumTicks[2] * msPerTick / (double)s_profFrames);
        s_profFrames = 0;
        s_profAccumTicks[0] = 0;
        s_profAccumTicks[1] = 0;
        s_profAccumTicks[2] = 0;
    }
}

/* ── GL state — CLUT shader ─────────────────────────────────────────────── */

static GLuint s_clutProgram = 0;
static GLint s_clut_aPos = -1;
static GLint s_clut_aTexCoord = -1;
static GLint s_clut_aLight = -1;
static GLint s_clut_aZ = -1;
static GLint s_clut_aRepMask = -1;
static GLint s_clut_uAtlas = -1;
static GLint s_clut_uCLUT = -1;
static GLint s_clut_uPalette = -1;
static GLint s_clut_uScreenScale = -1;
static GLint s_clut_uPolyMode = -1;
static GLint s_clut_uAtlasScale = -1;
static GLint s_clut_uFlatColor = -1;
static GLint s_clut_uCLUTBaseRow = -1;
static GLint s_clut_uScaledFogNear = -1;
static GLint s_clut_uFogRowScale = -1;
static GLint s_clut_uAlphaMode = -1;
static GLint s_clut_uBilinear = -1;
static GLint s_clut_uChromaKey = -1;
static GLint s_clut_uPerspCorrect = -1;

/* ── GL state — textures ────────────────────────────────────────────────── */

static GLuint s_atlasTex = 0;  /* 256×256 LUMINANCE */
static GLuint s_clutTex = 0;   /* 256×256 LUMINANCE (full fog table) */
static GLuint s_paletteTex = 0; /* 256×1 BGRA */

/* ── GL state — batch buffer ─────────────────────────────────────────────── */

static const U32 BATCH_MAX_VERTS = 4096;
static U8 s_batchVerts[BATCH_MAX_VERTS * 14]; /* 14 bytes per vertex */
static U32 s_batchVertCount = 0;
static U32 s_batchTriCount = 0;
static GLuint s_batchVBO = 0;

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
static U32 s_currentTypePoly = 0;
static U32 s_currentColorPoly = 0;
static S32 s_bilinearCLUT = 0; /* bilinear CLUT sampling flag for 3D */
static GLfloat s_batchCLUTBaseRow = 0.0f; /* CLUT row offset captured at batch start */
static GLfloat s_batchScaledFogNear = 0.0f; /* Fill_ScaledFogNear captured at batch start */
static GLfloat s_batchFogRowScale = 0.0f; /* Fill_Fog_Factor / 65536 captured at batch start */
static const void *s_batchCLUTGouraud = NULL; /* PtrCLUTGouraud at batch start */
static U8 s_batchZBuffer = 0;   /* Fill_Flag_ZBuffer captured at batch start */
static U8 s_batchNZW = 0;       /* Fill_Flag_NZW captured at batch start */

/* ── GL state — FBO for SSAA ─────────────────────────────────────────────── */
static GLuint s_fbo = 0;
static GLuint s_fboColorTex = 0;
static GLuint s_fboDepthRbo = 0;
static U32 s_fboWidth = 0;
static U32 s_fboHeight = 0;
static U32 s_renderQuality = 1; /* 1 = native, 2 = 2x, 4 = 4x */
static S32 s_frameActive = 0; /* 1 between GL_BeginFrame and GL_Present */
static U32 s_frameSeq = 0;    /* incremented in GL_BeginFrame */
static U32 s_atlasContentVersion = 1; /* bumped when atlas marked dirty */
/* One GL texture per atlas page (PtrMap), not one shared texture: terrain,
   decor bodies, NPC bodies and the sea each own a texture page and their
   draws interleave within a frame, so a shared texture would need a full
   64 KB re-upload on every switch.  Entries evict LRU; version matches
   s_atlasContentVersion so in-place page mutations (animated water) still
   re-upload. */
#define ATLAS_CACHE_SIZE 8
typedef struct {
    const void *ptr;
    U32 ver;
    GLuint tex;
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
   object pass on incremental frames (GL_RestoreTerrainSnapshot).  Depth is
   never written by body batches (NZW → depthMask off), so restoring color
   alone is sufficient. */
static GLuint s_snapshotTex = 0;
static GLuint s_snapshotFbo = 0;
static GLuint s_snapshotDepthRbo = 0;
static U32 s_snapshotW = 0;
static U32 s_snapshotH = 0;
static U32 s_snapshotGen = 0; /* GL_ClearFBO generation the snapshot matches */
static U32 s_fboGen = 0;      /* bumped by GL_ClearFBO */

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
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkProgram(GLuint vert, GLuint frag) {
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
    s_stereoEye = eye;
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

static void GL_CreateFBO(U32 width, U32 height) {
    GL_DestroyFBO();

    /* Color texture attachment */
    glGenTextures(1, &s_fboColorTex);
    glBindTexture(GL_TEXTURE_2D, s_fboColorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Depth renderbuffer attachment */
    glGenRenderbuffers(1, &s_fboDepthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, s_fboDepthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
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

    s_uTexLoc = glGetUniformLocation(s_program, "uTex");
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
    s_clut_uPerspCorrect = glGetUniformLocation(s_clutProgram, "uPerspCorrect");
    s_clut_uAtlas = glGetUniformLocation(s_clutProgram, "uAtlas");
    s_clut_uCLUT = glGetUniformLocation(s_clutProgram, "uCLUT");
    s_clut_uPalette = glGetUniformLocation(s_clutProgram, "uPalette");
    s_clut_uScreenScale = glGetUniformLocation(s_clutProgram, "uScreenScale");
    s_clut_uPolyMode = glGetUniformLocation(s_clutProgram, "uPolyMode");
    s_clut_uAtlasScale = glGetUniformLocation(s_clutProgram, "uAtlasScale");
    s_clut_uFlatColor = glGetUniformLocation(s_clutProgram, "uFlatColor");
    s_clut_uCLUTBaseRow = glGetUniformLocation(s_clutProgram, "uCLUTBaseRow");
    s_clut_uScaledFogNear = glGetUniformLocation(s_clutProgram, "uScaledFogNear");
    s_clut_uFogRowScale = glGetUniformLocation(s_clutProgram, "uFogRowScale");
    s_clut_uAlphaMode = glGetUniformLocation(s_clutProgram, "uAlphaMode");
    s_clut_uBilinear = glGetUniformLocation(s_clutProgram, "uBilinear");
    s_clut_uChromaKey = glGetUniformLocation(s_clutProgram, "uChromaKey");

    Log_Info("[GL] CLUT shader compiled OK (aPos=%d aTexCoord=%d aLight=%d "
             "aZ=%d aRepMask=%d uAtlas=%d uCLUT=%d uPalette=%d uScreenScale=%d).",
             s_clut_aPos, s_clut_aTexCoord, s_clut_aLight, s_clut_aZ,
             s_clut_aRepMask,
             s_clut_uAtlas, s_clut_uCLUT, s_clut_uPalette,
             s_clut_uScreenScale);

    /* ── Create VBOs ── */
    /* Present quad VBO (static) — flipped UVs for 2D overlay */
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_quadVerts), s_quadVerts,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* FBO quad VBO (static) — non-flipped UVs for FBO→screen */
    glGenBuffers(1, &s_fboVbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_fboVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_fboQuadVerts), s_fboQuadVerts,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Batch VBO for 3D triangles (dynamic) */
    glGenBuffers(1, &s_batchVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_batchVBO);
    glBufferData(GL_ARRAY_BUFFER, BATCH_MAX_VERTS * 14, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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
        GLenum texErr = glGetError();
        if (texErr != GL_NO_ERROR) {
            Log_Warn("[GL] GL error after texture setup: 0x%x", (unsigned)texErr);
        }
    }

    /* GL state */
    glViewport(0, 0, resX, resY);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Window_GL_SwapWindow();

    s_texWidth = resX;
    s_texHeight = resY;
    s_initialized = 1;
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
        s_atlasTexCache[i].ptr = NULL;
        s_atlasTexCache[i].ver = 0;
        s_atlasTexCache[i].tex = 0;
    }
    s_atlasTexNext = 0;
    if (s_snapshotDepthRbo) {
        glDeleteRenderbuffers(1, &s_snapshotDepthRbo);
        s_snapshotDepthRbo = 0;
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

    GL_DestroyFBO();
    GL_DestroyPresentFBO();
    GL_DestroyStereoEyeFBOs();

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

    /* Recreate present FBO at the new size */
    GL_CreatePresentFBO(resX, resY);
    GL_CreateStereoEyeFBOs(resX, resY);
}

void GL_SetRenderQuality(U32 quality) {
    if (quality == 0) quality = 1;
    if (quality > 4) quality = 4;
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
    Log_Info("[GL] SetRenderQuality: FBO %s (s_fbo=%u).",
             s_fbo ? "OK" : "FAILED", s_fbo);
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

    glBindTexture(GL_TEXTURE_2D, s_paletteTex);
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
    glBindTexture(GL_TEXTURE_2D, 0);

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

    /* Upload the full 64 KB fog table (256×256) once. SetCLUT selects a
       16-row block within it; the gouraud value's high byte indexes into
       the correct rows directly via the shader. */
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, PtrCLUTFog);
    glBindTexture(GL_TEXTURE_2D, 0);

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

/* Make the texture for texture page `map` current on the GL_TEXTURE0 unit:
   reuse the cached texture when the page content is up to date, (re)upload
   into it otherwise, evicting the oldest entry when the pointer is new.
   NULL falls back to the legacy shared s_atlasTex (never sampled). */
static GLuint GL_BindAtlasFor(const void *map) {
    if (map == NULL) {
        glBindTexture(GL_TEXTURE_2D, s_atlasTex);
        return s_atlasTex;
    }

    for (S32 i = 0; i < ATLAS_CACHE_SIZE; i++) {
        if (s_atlasTexCache[i].ptr == map) {
            if (s_atlasTexCache[i].ver != s_atlasContentVersion) {
                glBindTexture(GL_TEXTURE_2D, s_atlasTexCache[i].tex);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_LUMINANCE,
                                GL_UNSIGNED_BYTE, map);
                s_atlasTexCache[i].ver = s_atlasContentVersion;
            }
            glBindTexture(GL_TEXTURE_2D, s_atlasTexCache[i].tex);
            return s_atlasTexCache[i].tex;
        }
    }

    AtlasTexEntry *e = &s_atlasTexCache[s_atlasTexNext];
    s_atlasTexNext = (s_atlasTexNext + 1) % ATLAS_CACHE_SIZE;
    if (e->tex == 0) {
        glGenTextures(1, &e->tex);
        GL_SetAtlasTexParams(e->tex);
    }
    e->ptr = map;
    e->ver = s_atlasContentVersion;
    glBindTexture(GL_TEXTURE_2D, e->tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, map);
    return e->tex;
}

/* ── Batch management ───────────────────────────────────────────────────── */

void GL_FlushBatch(void) {
    if (!Window_GLMakeCurrent()) {
        return;
    }

    if (s_batchVertCount == 0) {
        return;
    }

    GL_BatchFlushCount++;

    /* Bind FBO on the first flush of each frame.  The FBO clear is now
       done by GL_ClearFBO() at the start of terrain rendering, so that
       frames with no terrain (e.g. Twinsen standing still) preserve the
       previous frame's 3D content in the FBO. */
    if (!s_frameActive) {
        s_frameActive = 1;
        if (s_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
            glViewport(0, 0, s_fboWidth, s_fboHeight);
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    /* Upload batch vertices to VBO */
    glBindBuffer(GL_ARRAY_BUFFER, s_batchVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, s_batchVertCount * 14, s_batchVerts);

    /* Use CLUT shader */
    glUseProgram(s_clutProgram);

    /* Set screen-scale uniform: maps pixel coords → clip space */
    GLfloat screenScale[2];
    screenScale[0] = 2.0f / (GLfloat)s_texWidth;
    screenScale[1] = 2.0f / (GLfloat)s_texHeight;
    glUniform2f(s_clut_uScreenScale, screenScale[0], screenScale[1]);

    /* Set polygon mode uniform and color.
       Mode 0 = textured (CLUT), 1 = flat, 2 = gouraud, 3 = gouraud table,
       4 = texture without CLUT (type 8: texel IS palette index).
       Alpha mode: 0 = opaque, 1 = TRANS (semi), 2 = TRAME (stipple). */
    GLint polyMode;
    GLint alphaMode = 0;
    GLfloat flatColor = 0.0f;
    if (s_currentTypePoly < POLY_TEXTURE) {
        if (s_currentTypePoly <= 1) {
            polyMode = 1; /* SOLID / FLAT */
            flatColor = (GLfloat)(s_currentColorPoly & 0xFF);
        } else if (s_currentTypePoly == 2) {
            polyMode = 1; /* TRANS → flat color */
            alphaMode = 1;
            flatColor = (GLfloat)(s_currentColorPoly & 0xFF);
        } else if (s_currentTypePoly == 3) {
            polyMode = 1; /* TRAME → flat color */
            alphaMode = 2;
            flatColor = (GLfloat)(s_currentColorPoly & 0xFF);
        } else if (s_currentTypePoly <= 5) {
            polyMode = 2; /* GOURAUD / DITHER (dithering not in GL) */
            flatColor = (GLfloat)(s_currentColorPoly & 0xFF);
        } else {
            polyMode = 3; /* GOURAUD_TABLE / DITHER_TABLE */
            flatColor = (GLfloat)(s_currentColorPoly & 0xFF);
        }
    } else if (s_currentTypePoly == POLY_TEXTURE ||
               s_currentTypePoly == POLY_TEXTURE_INCRUST ||
               s_currentTypePoly == POLY_TEXTURE_Z ||
               s_currentTypePoly == POLY_TEXTURE_Z_INCRUST) {
        /* The non-flat texture types write the texel as a palette index
           directly (Filler_Texture / TextureChromaKey / TextureZ /
           TextureZChromaKey: *log = texel) — no CLUT lookup.  Chroma key
           (discard index 0) controlled by uChromaKey. */
        polyMode = 4;
    } else if (s_currentTypePoly == POLY_TEXTURE_Z_FOG) {
        /* Type 24 (POLY_TEXTURE_Z_FOG, e.g. sea/sky): SW
           Filler_TextureZFogSmoothZBuf / ...NZW select the CLUT row from
           the per-pixel Z distance to the fog near plane, ignoring
           Pt_Light.  polyMode 5 replicates that in the fragment shader. */
        polyMode = 5;
    } else {
        polyMode = 0; /* textured (CLUT-based) */
    }
    glUniform1i(s_clut_uPolyMode, polyMode);
    glUniform1f(s_clut_uFlatColor, flatColor);
    glUniform1i(s_clut_uAlphaMode, alphaMode);
    glUniform1i(s_clut_uBilinear, s_bilinearCLUT);

    /* Z-textured types (16-24) are perspective-corrected in the SW
       rasterizer (Filler_TextureZ / FogSmooth divide MapU, MapV, ZBuf
       by W = 1/Z); types 8-15 (Filler_Texture / Gouraud) are affine.
       Type 25 (POLY_FLAG_ZBUFFER) is a Z-marker polygon, not textured. */
    GLint perspCorrect =
        (s_currentTypePoly >= POLY_TEXTURE_Z &&
         s_currentTypePoly <= POLY_TEXTURE_Z_FOG) ? 1 : 0;
    glUniform1i(s_clut_uPerspCorrect, perspCorrect);

    /* Chroma key: only INCRUST types (12-15, 20-23) discard palette index 0.
       Non-INCRUST types (8-11, 16-19, 24) write all texels unconditionally.
       The INCRUST bit is bit 2 of the type number (0b100 = 4). */
    GLint chromaKey = 0;
    if (s_currentTypePoly >= POLY_TEXTURE &&
        s_currentTypePoly <= POLY_TEXTURE_Z_FOG) {
        chromaKey = (s_currentTypePoly & 4) ? 1 : 0;
    }
    glUniform1i(s_clut_uChromaKey, chromaKey);

    /* Enable/disable blending.
       TRANS/TRAME types (alphaMode 1/2) need blending for transparency.
       INCRUST textured types use palette index 0 as chroma key — handled
       by discard in the fragment shader (uChromaKey), not blending,
       so Z-write remains enabled.  Non-INCRUST types (8-11, 16-19, 24)
       write all texels including index 0, matching the SW rasterizer. */
    GLint useBlend = (alphaMode != 0);
    if (useBlend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    /* CLUT base row offset: captured at batch start from PtrCLUTGouraud
       at the time the batch was begun. This avoids the race where SetCLUT()
       changes PtrCLUTGouraud between Fill_Poly calls within the same frame. */
    glUniform1f(s_clut_uCLUTBaseRow, s_batchCLUTBaseRow);
    glUniform1f(s_clut_uScaledFogNear, s_batchScaledFogNear);
    glUniform1f(s_clut_uFogRowScale, s_batchFogRowScale);

    /* Bind textures to units 0/1/2.  Textured batches sample the page the
       batch was recorded with; flat/gouraud batches never sample it. */
    glActiveTexture(GL_TEXTURE0);
    if (s_currentTypePoly >= POLY_TEXTURE && s_batchAtlas != NULL) {
        GL_BindAtlasFor(s_batchAtlas);
    } else {
        glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    }
    glUniform1i(s_clut_uAtlas, 0);
    glUniform2f(s_clut_uAtlasScale, 256.0f, 256.0f);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glUniform1i(s_clut_uCLUT, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, s_paletteTex);
    glUniform1i(s_clut_uPalette, 2);

    glActiveTexture(GL_TEXTURE0); /* restore default */

    /* Set up vertex attributes (14 bytes per vertex) */
    /* aPos: 2 × GL_SHORT, stride=14, offset=0 */
    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 14,
                          (const void *)0);

    /* aTexCoord: 2 × GL_UNSIGNED_SHORT, stride=14, offset=4 */
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)4);

    /* aLight: 1 × GL_UNSIGNED_SHORT, stride=14, offset=8 */
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)8);

    /* aZ: 1 × GL_UNSIGNED_SHORT, stride=14, offset=10 */
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)10);

    /* aRepMask: 1 × GL_UNSIGNED_SHORT, stride=14, offset=12 */
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)12);

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
        glEnable(GL_DEPTH_TEST);
        glDepthFunc((s_batchNZW && s_currentTypePoly == POLY_TEXTURE_Z_FOG)
                        ? GL_ALWAYS
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
            glDepthMask((useBlend || (s_batchNZW && s_noBodyDWrite))
                            ? GL_FALSE
                            : GL_TRUE);
        } else {
            /* Types 0–15 without Z-buffered fillers (NPC/sprite path):
               Z-test only — matches SW ZBufBoxOverWrite2 which reads
               terrain Z but never writes object Z into the buffer.
               Types 0–7 (FLAT/SOLID) still need Z-test so they are
               occluded by closer terrain (e.g. NPCs behind walls). */
            glDepthMask(GL_FALSE);
        }
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    /* Draw all batched triangles */
    glDrawArrays(GL_TRIANGLES, 0, s_batchVertCount);

    /* Restore depth mask so subsequent Z-buffer batches (types 16–24)
       can write to the depth buffer. */
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    /* Clean up */
    glDisableVertexAttribArray(s_clut_aPos);
    glDisableVertexAttribArray(s_clut_aTexCoord);
    glDisableVertexAttribArray(s_clut_aLight);
    glDisableVertexAttribArray(s_clut_aZ);
    glDisableVertexAttribArray(s_clut_aRepMask);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    /* Reset batch counters */
    s_batchVertCount = 0;
    s_batchTriCount = 0;
}

/* ── Ground shadow quad (GL ShadeBoxBlk replacement) ────────────────────── */

/* Draw the projected ground-shadow quad directly into the FBO, replacing
   the SW path (DrawShadow → DrawLineShade → per-scanline ShadeBoxBlk into
   Log + DrawRecover occlusion).  The quad is depth-tested GEQUAL against
   the FBO depth (terrain/hills clip it exactly where ZBufBoxOverWrite2
   used to clear the Log pixels) and multiplies the scene color by
   (15 - level) / 15 via GL_DST_COLOR/GL_ZERO blending — the linear
   counterpart of the SW fade-CLUT row at (15 - ShadowLevel).  Unlike the
   SW path this works on the true FBO colors, so the result does not
   depend on the FBO→palette reverse lookup staying faithful. */
void GL_DrawShadowQuad(const S16 *xs, const S16 *ys, const U16 *az, U32 level) {
    if (!s_initialized) {
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
            glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
            glViewport(0, 0, s_fboWidth, s_fboHeight);
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    glUseProgram(s_clutProgram);

    GLfloat screenScale[2];
    screenScale[0] = 2.0f / (GLfloat)s_texWidth;
    screenScale[1] = 2.0f / (GLfloat)s_texHeight;
    glUniform2f(s_clut_uScreenScale, screenScale[0], screenScale[1]);
    glUniform1i(s_clut_uPolyMode, 6);
    glUniform1f(s_clut_uFlatColor, (GLfloat)(15 - (level & 15)) / 15.0f);
    glUniform1i(s_clut_uAlphaMode, 0);
    glUniform1i(s_clut_uChromaKey, 0);

    /* Bind the three sampler textures (mode 6 ignores them, but the
       sampler units must reference valid textures). */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, s_paletteTex);
    glActiveTexture(GL_TEXTURE0);

    /* Depth-test against the terrain, never write. */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);

    /* Multiply blending: result = shadow gray * scene color. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ZERO);

    /* Two triangles, fan order (0,1,2) (0,2,3). */
    static U8 quadVerts[6 * 14];
    static const S8 order[6] = {0, 1, 2, 0, 2, 3};
    for (S32 i = 0; i < 6; i++) {
        U8 *v = quadVerts + i * 14;
        S32 c = order[i];
        S16 x = xs[c];
        S16 y = ys[c];
        memcpy(v + 0, &x, 2);
        memcpy(v + 2, &y, 2);
        memset(v + 4, 0, 6); /* UV, light = 0 (unused) */
        memcpy(v + 10, &az[c], 2);
        memset(v + 12, 0, 2); /* RepMask = 0 */
    }

    glBindBuffer(GL_ARRAY_BUFFER, s_batchVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVerts), quadVerts);

    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 14,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)12);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glDisableVertexAttribArray(s_clut_aPos);
    glDisableVertexAttribArray(s_clut_aTexCoord);
    glDisableVertexAttribArray(s_clut_aLight);
    glDisableVertexAttribArray(s_clut_aZ);
    glDisableVertexAttribArray(s_clut_aRepMask);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

/* ── Body sphere disc (GL Fill_Sphere replacement) ───────────────────────── */

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
            glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
            glViewport(0, 0, s_fboWidth, s_fboHeight);
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    glUseProgram(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    glUniform1i(s_clut_uPolyMode, 1); /* flat palette color */
    glUniform1f(s_clut_uFlatColor, (GLfloat)(colorIndex & 0xFF));
    glUniform1i(s_clut_uAlphaMode, transparent ? 1 : 0);
    glUniform1i(s_clut_uChromaKey, 0);
    glUniform1f(s_clut_uCLUTBaseRow, 0.0f);
    glUniform1f(s_clut_uScaledFogNear, s_batchScaledFogNear);
    glUniform1f(s_clut_uFogRowScale, s_batchFogRowScale);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, s_paletteTex);
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDepthMask((zWrite && !transparent) ? GL_TRUE : GL_FALSE);

    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    /* Disc as a triangle fan: centre + 17 ring vertices, 14-byte layout
       matching the batch (x, y, uv=0, light=0, zo, repmask=0). */
    const int SEGMENTS = 16;
    U8 verts[(SEGMENTS + 2) * 14];
    S16 cx16 = (S16)cx, cy16 = (S16)cy;
    U16 zo16 = (U16)zo;
    for (int i = 0; i < SEGMENTS + 2; i++) {
        U8 *v = verts + i * 14;
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
    }

    glBindBuffer(GL_ARRAY_BUFFER, s_batchVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 14,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)12);

    glDrawArrays(GL_TRIANGLE_FAN, 0, SEGMENTS + 2);

    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glDisableVertexAttribArray(s_clut_aPos);
    glDisableVertexAttribArray(s_clut_aTexCoord);
    glDisableVertexAttribArray(s_clut_aLight);
    glDisableVertexAttribArray(s_clut_aZ);
    glDisableVertexAttribArray(s_clut_aRepMask);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
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
            glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
            glViewport(0, 0, s_fboWidth, s_fboHeight);
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    glUseProgram(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    glUniform1i(s_clut_uPolyMode, 6); /* multiply by flat color */
    glUniform1f(s_clut_uFlatColor, (GLfloat)(15 - (level & 15)) / 15.0f);
    glUniform1i(s_clut_uAlphaMode, 0);
    glUniform1i(s_clut_uChromaKey, 0);
    glUniform1f(s_clut_uCLUTBaseRow, 0.0f);
    glUniform1f(s_clut_uScaledFogNear, s_batchScaledFogNear);
    glUniform1f(s_clut_uFogRowScale, s_batchFogRowScale);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, s_paletteTex);
    glActiveTexture(GL_TEXTURE0);

    /* Unconditional: the band shades everything behind it. */
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ZERO);

    U8 verts[6 * 14];
    S16 coords[4][2] = {
        {(S16)x0, (S16)y0}, {(S16)x1, (S16)y0}, {(S16)x1, (S16)y1}, {(S16)x0, (S16)y1}};
    static const S8 order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) {
        U8 *v = verts + i * 14;
        memcpy(v + 0, &coords[order[i]][0], 2);
        memcpy(v + 2, &coords[order[i]][1], 2);
        memset(v + 4, 0, 4);  /* uv = 0 */
        memset(v + 8, 0, 2);  /* light = 0 */
        memset(v + 10, 0, 2); /* zo = 0 */
        memset(v + 12, 0, 2); /* repmask = 0 */
    }

    glBindBuffer(GL_ARRAY_BUFFER, s_batchVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 14,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)12);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glDisableVertexAttribArray(s_clut_aPos);
    glDisableVertexAttribArray(s_clut_aTexCoord);
    glDisableVertexAttribArray(s_clut_aLight);
    glDisableVertexAttribArray(s_clut_aZ);
    glDisableVertexAttribArray(s_clut_aRepMask);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

static void GL_AddTriangle(Struc_Point *p0, Struc_Point *p1,
                           Struc_Point *p2) {
    /* Flush if batch is full */
    if (s_batchVertCount + 3 > BATCH_MAX_VERTS) {
        GL_FlushBatch();
    }

    /* Copy 14 bytes per vertex into the batch buffer:
       Pt_XE(2) + Pt_YE(2) + Pt_MapU(2) + Pt_MapV(2) +
       Pt_Light(2) + Pt_ZO(2) + Pt_RepMask(2)
       Note: Pt_W (S32, offset 12 in Struc_Point) is skipped;
       Pt_RepMask (offset 16) is placed at vertex offset 12. */
    U8 *dst = s_batchVerts + s_batchVertCount * 14;
    memcpy(dst + 0, p0, 12);   /* XE, YE, MapU, MapV, Light, ZO */
    memcpy(dst + 12, (const U8 *)p0 + 16, 2); /* Pt_RepMask (skip Pt_W) */
    memcpy(dst + 14, p1, 12);
    memcpy(dst + 14 + 12, (const U8 *)p1 + 16, 2);
    memcpy(dst + 28, p2, 12);
    memcpy(dst + 28 + 12, (const U8 *)p2 + 16, 2);

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

    if (s_fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        glViewport(0, 0, s_fboWidth, s_fboHeight);
    } else {
        glViewport(0, 0, s_texWidth, s_texHeight);
    }

#if defined(__ANDROID__)
    glClearDepthf(0.0f);
#else
    glClearDepth(0.0);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    s_terrainRenderedThisFrame = 1;
    s_fboHasTerrain = 1;
    s_fboGen++;
}

/* ── Terrain snapshot capture/restore ───────────────────────────────────── */

/* Copy the current FBO color (terrain + decor + sea, no bodies/shadows yet)
   into the snapshot texture.  Called once per full redraw, after the last
   FBO-baked content and before bodies/shadows are drawn. */
void GL_SnapshotTerrain(void) {
    if (!s_initialized || !s_fbo) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* The terrain/decor batches are still queued vertices at this point —
       submit them first, or the snapshot captures a cleared framebuffer
       and every incremental-frame restore paints black over the scene. */
    GL_FlushBatch();

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
        if (s_snapshotDepthRbo == 0) {
            glGenRenderbuffers(1, &s_snapshotDepthRbo);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, s_snapshotDepthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, s_snapshotDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        s_snapshotW = w;
        s_snapshotH = h;
    }

    /* Color + depth: the depth must be restored too, otherwise a body that
       moved away would leave stale nearer depth that eats later fragments
       until the next full redraw. */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_snapshotFbo);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    /* Mid-frame invariant: subsequent GL_FlushBatch() calls (s_frameActive
       still set) must keep drawing into the FBO, not the default fb. */
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);

    s_snapshotGen = s_fboGen;
}

/* Blit the snapshot back over the FBO color on incremental frames so last
   frame's body/shadow pixels are erased before the object pass redraws
   them at their new positions.  No-op until the first snapshot exists and
   whenever the FBO has been cleared since (full redraw refreshes it). */
void GL_RestoreTerrainSnapshot(void) {
    if (!s_initialized || !s_fbo || s_snapshotFbo == 0) {
        return;
    }
    if (s_snapshotGen != s_fboGen) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }

    {
        static S32 s_rsLog = 0;
        if (s_rsLog < 30) {
            Log_Info("RestoreLog: blit gen=%u %ux%u", s_snapshotGen,
                     s_snapshotW, s_snapshotH);
            s_rsLog++;
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_snapshotFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_fbo);
    glBlitFramebuffer(0, 0, s_snapshotW, s_snapshotH,
                      0, 0, s_fboWidth, s_fboHeight,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
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

    U64 profT0 = SDL_GetPerformanceCounter();

    /* Flush any pending batch so the FBO contains the latest geometry
       (terrain + bodies + shadows) before we read back its depth. */
    GL_FlushBatch();

    const U32 fboW = s_fboWidth;
    const U32 fboH = s_fboHeight;
    const U32 scrW = ModeDesiredX;
    const U32 scrH = ModeDesiredY;
    const U32 bytes = scrW * scrH * sizeof(U16);

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

    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_depthSmallFbo);
    glBlitFramebuffer(0, 0, fboW, fboH, 0, 0, scrW, scrH,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    /* Async double-buffered PBO readback: consume the read issued on the
       previous frame (the GPU has had a full frame to finish it), then
       issue this frame's read. */
    const U32 readIdx = s_depthPboIndex ^ 1;
    if (s_depthPboReady > 0) {
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
    } else {
        /* Bootstrap: one synchronous read so the first frame is correct. */
        U16 *tmp = (U16 *)malloc(bytes);
        if (tmp != NULL) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, s_depthSmallFbo);
            glReadPixels(0, 0, scrW, scrH, GL_DEPTH_COMPONENT,
                         GL_UNSIGNED_SHORT, tmp);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            GL_ConvertDepthToZBuffer(tmp, scrW, scrH);
            free(tmp);
        }
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, s_depthPbo[s_depthPboIndex]);
    glReadPixels(0, 0, scrW, scrH, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
                 NULL);
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
            glBindTexture(GL_TEXTURE_2D, s_spriteTexCache[i].tex);
            return s_spriteTexCache[i].tex;
        }
    }

    SpriteTexEntry *e = &s_spriteTexCache[s_spriteTexNext];
    s_spriteTexNext = (s_spriteTexNext + 1) % SPRITE_CACHE_SIZE;
    if (e->tex == 0) {
        glGenTextures(1, &e->tex);
        glBindTexture(GL_TEXTURE_2D, e->tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, e->tex);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, pixels);
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
            glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
            glViewport(0, 0, s_fboWidth, s_fboHeight);
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    /* Temporary occlusion diagnostic: quad az vs stored depth at centre. */
    {
        static S32 s_dbgCount = 0;
        if (s_dbgCount < 60 && s_fbo) {
            U16 gl16 = 0;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
            glReadPixels((x0 + x1) / 2, (y0 + y1) / 2, 1, 1,
                         GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, &gl16);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            U16 storedAz = (U16)(131070u - 2u * gl16);
            Log_Info("SpriteDbg: frame=%u az=%u storedAz=%u rect=(%d,%d)-(%d,%d)",
                     s_frameSeq, az, storedAz, x0, y0, x1, y1);
            s_dbgCount++;
        }
    }

    /* Temporary occlusion diagnostic: quad az vs stored depth at centre. */
    {
        static S32 s_dbgCount = 0;
        if (s_dbgCount < 60 && s_fbo) {
            U16 gl16 = 0;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
            glReadPixels((x0 + x1) / 2, (y0 + y1) / 2, 1, 1,
                         GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, &gl16);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            U16 storedAz = (U16)(131070u - 2u * gl16);
            Log_Info("SpriteDbg: frame=%u az=%u storedAz=%u rect=(%d,%d)-(%d,%d)",
                     s_frameSeq, az, storedAz, x0, y0, x1, y1);
            s_dbgCount++;
        }
    }

    glUseProgram(s_clutProgram);

    glUniform2f(s_clut_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    glUniform1i(s_clut_uPolyMode, 4);
    glUniform1i(s_clut_uAlphaMode, transparent ? 1 : 0);
    glUniform1i(s_clut_uChromaKey, transparent ? 0 : 1);
    glUniform2f(s_clut_uAtlasScale, (GLfloat)w, (GLfloat)h);

    glActiveTexture(GL_TEXTURE0);
    GL_BindSpriteTexture(pixels, w, h);
    glUniform1i(s_clut_uAtlas, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, s_paletteTex);
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDepthMask(GL_FALSE);

    if (transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    static U8 quadVerts[6 * 14];
    static const S8 order[6] = {0, 1, 2, 0, 2, 3};
    const S16 px[4] = {(S16)x0, (S16)x1, (S16)x1, (S16)x0};
    const S16 py[4] = {(S16)y0, (S16)y0, (S16)y1, (S16)y1};
    const U16 pu[4] = {(U16)u0, (U16)u1, (U16)u1, (U16)u0};
    const U16 pv[4] = {(U16)v0, (U16)v0, (U16)v1, (U16)v1};
    for (S32 i = 0; i < 6; i++) {
        U8 *v = quadVerts + i * 14;
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
    }

    glBindBuffer(GL_ARRAY_BUFFER, s_batchVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVerts), quadVerts);

    glEnableVertexAttribArray(s_clut_aPos);
    glVertexAttribPointer(s_clut_aPos, 2, GL_SHORT, GL_FALSE, 14,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut_aTexCoord);
    glVertexAttribPointer(s_clut_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut_aLight);
    glVertexAttribPointer(s_clut_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut_aZ);
    glVertexAttribPointer(s_clut_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut_aRepMask);
    glVertexAttribPointer(s_clut_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 14,
                          (const void *)12);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glDisableVertexAttribArray(s_clut_aPos);
    glDisableVertexAttribArray(s_clut_aTexCoord);
    glDisableVertexAttribArray(s_clut_aLight);
    glDisableVertexAttribArray(s_clut_aZ);
    glDisableVertexAttribArray(s_clut_aRepMask);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void GL_ReadbackColorBuffer(void) {
    if (!s_initialized || !s_fbo || Screen == NULL || !s_fboHasTerrain || !s_terrainRenderedThisFrame) {
        return;
    }

    /* FBO should still be bound from GL_ReadbackDepthBuffer().  Bind
       explicitly for safety in case the caller skips the depth readback. */
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);

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

    /* Keep FBO bound for subsequent GL_FlushBatch() calls */
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

    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
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

    free(fboBuf);
    free(comp);
    free(rgb);
}

/* ── Present (extended with 3D flush) ───────────────────────────────────── */

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

    /* Flush any remaining batched 3D geometry from the current frame.
       This draws into the FBO if SSAA is active, or the default
       framebuffer otherwise. */
    GL_FlushBatch();

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
    /* Clear the render target to black (for letterbox bars). On Quest the
       present FBO is the target; on desktop it is the default framebuffer. */
    glViewport(0, 0, winW, winH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

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
    glViewport(vpX, vpY, vpW, vpH);

    /* Draw the FBO color texture (3D scene) first, then the 2D overlay on top */
    if (s_fbo) {
        glDisable(GL_BLEND);
        glUseProgram(s_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_fboColorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(s_uTexLoc, 0);

        glBindBuffer(GL_ARRAY_BUFFER, s_fboVbo);
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
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
    }

    /* Upload the ARGB framebuffer to the GL texture (2D overlay) */
    glBindTexture(GL_TEXTURE_2D, s_texture);

    /* Set filter mode (bilinear vs nearest) */
    GLenum filter = filterLinear ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    /* GL_UNPACK_ROW_LENGTH is GLES 3.0+; the project's minimum is GLES 2.0,
       so guard the pixel-store call (row length falls back to width when
       unset, which is what we want anyway because pitch == width*4 here). */
    const void *uploadPixels = argbPixels;
#if defined(__ANDROID__)
    {
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

    /* Enable alpha blending so palette index 0 (alpha=0) is transparent,
       letting the 3D scene (or black background) show through underneath. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Draw fullscreen quad for 2D overlay */
    glUseProgram(s_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_texture);
    glUniform1i(s_uTexLoc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
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
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);

    /* Eye 0 stops here (its content is captured, not presented). Eye 1 and
     * every mono path continue to the swap, which on the Quest routes into
     * OpenXR_PresentMono — the frame's single submit point. */
    U64 profT1 = SDL_GetPerformanceCounter();
    s_profAccumTicks[0] += profT1 - profT0;
    GL_ProfFrameDone();
    if (s_stereoEye != 0) {
        Window_GL_SwapWindow();
    }
}

/* ── Indexed present (GPU palette expansion) ─────────────────────────────── */

void GL_PresentIndexed(const U8 *indices, U32 pitch, U32 width, U32 height,
                       const U32 *paletteU32, S32 filterLinear) {
    if (!s_initialized || indices == NULL || paletteU32 == NULL) {
        return;
    }

    if (!Window_GLMakeCurrent()) {
        return;
    }

    U64 profT0 = SDL_GetPerformanceCounter();

    /* Same frame plumbing as GL_Present: flush 3D, pick the composite
     * target, letterbox, draw the 3D FBO content, then the 2D overlay. */
    GL_FlushBatch();
    s_frameActive = 0;

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
    int winW = (int)width, winH = (int)height;
    if (win) {
        SDL_GetWindowSize(win, &winW, &winH);
    }
    if (OpenXR_IsActive() && s_presentFBO) {
        winW = (int)width;
        winH = (int)height;
    }
    glViewport(0, 0, winW, winH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    int vpX, vpY, vpW, vpH;
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
    glViewport(vpX, vpY, vpW, vpH);

    if (s_fbo) {
        glDisable(GL_BLEND);
        glUseProgram(s_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_fboColorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(s_uTexLoc, 0);

        glBindBuffer(GL_ARRAY_BUFFER, s_fboVbo);
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
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
    }

    /* Upload the raw palette indices (1 byte per pixel instead of 4). */
    if (s_logIndexTex == 0 || s_logIndexTexW != width ||
        s_logIndexTexH != height) {
        if (s_logIndexTex == 0) {
            glGenTextures(1, &s_logIndexTex);
        }
        glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        s_logIndexTexW = width;
        s_logIndexTexH = height;
    } else {
        glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
    }

#ifdef GL_UNPACK_ROW_LENGTH
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(pitch));
#endif
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, indices);
#ifdef GL_UNPACK_ROW_LENGTH
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(s_presentIndexedProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
    glUniform1i(s_idx_uIndices, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_lutTex);
    glUniform1i(s_idx_uPalette, 1);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
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
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);

    U64 profT1 = SDL_GetPerformanceCounter();
    s_profAccumTicks[0] += profT1 - profT0;
    GL_ProfFrameDone();

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

    /* Detect batch-flush triggers: type, color, atlas page, CLUT fog row,
       or Z-buffer state changed.  Color only reaches GL as the flush-time
       uFlatColor uniform for non-textured types, so those must flush when
       it changes.  Textured types never sample the color (UVs are absolute
       into the atlas), and terrain tiles vary the color per tile (texture
       bank in the high nibble) — flushing on it would emit one tiny draw
       call per tile, costing hundreds of ms on mobile GPUs. */
    if (s_batchVertCount > 0) {
        if (s_currentTypePoly != (U32)typePoly ||
            (typePoly < POLY_TEXTURE &&
             s_currentColorPoly != (U32)colorPoly) ||
            (typePoly >= POLY_TEXTURE && s_batchAtlas != (const void *)PtrMap) ||
            s_batchCLUTGouraud != (const void *)PtrCLUTGouraud ||
            s_batchZBuffer != Fill_Flag_ZBuffer ||
            s_batchNZW != Fill_Flag_NZW) {
            GL_FlushBatch();
        }
    }

    /* Update state tracking (captured at batch start for flush) */
    s_currentTypePoly = (U32)typePoly;
    s_currentColorPoly = (U32)colorPoly;
    if (typePoly >= POLY_TEXTURE) {
        s_batchAtlas = (const void *)PtrMap;
    }
    s_batchCLUTGouraud = (const void *)PtrCLUTGouraud;
    s_batchZBuffer = Fill_Flag_ZBuffer;
    s_batchNZW = Fill_Flag_NZW;

    /* Capture CLUT base row from current PtrCLUTGouraud.
       SetCLUT(fog) is called per terrain tile with different fog distances.
       The batch flushes when PtrCLUTGouraud changes, so all polygons in
       the batch share the same fog distance and CLUT base row. */
    s_batchCLUTBaseRow = 0.0f;
    if (PtrCLUTFog != NULL && PtrCLUTGouraud != NULL) {
        s_batchCLUTBaseRow = (GLfloat)(((const U8 *)PtrCLUTGouraud -
                                        (const U8 *)PtrCLUTFog) / 256);
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
       global RepMask; GL needs it per-vertex for batching). */
    for (S32 i = 0; i < nbPoints; i++) {
        ptrPoints[i].Pt_RepMask = (U16)(RepMask & 0xFFFF);
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
void GL_DrawShadowQuad(const S16 *xs, const S16 *ys, const U16 *az, U32 level) {
    (void)xs; (void)ys; (void)az; (void)level;
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
S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    (void)typePoly; (void)colorPoly; (void)nbPoints; (void)ptrPoints;
    return 0;
}

#endif /* USE_GPURENDERER */
