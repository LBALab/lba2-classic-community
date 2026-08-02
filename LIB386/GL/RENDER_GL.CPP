#include <GL/RENDER_GL.H>

#include <SYSTEM/LOG.H>
#include <SYSTEM/WINDOW.H>

#ifdef USE_GPURENDERER

/* ── GL headers ─────────────────────────────────────────────────────────── */
#if defined(__ANDROID__)
#include <GLES2/gl2.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL_opengl.h>
#endif

#include <SDL3/SDL.h>
#include <string.h>
#include <stdlib.h>

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

/* ── External texture filter toggle (from GLOBAL.CPP) ────────────────────── */
extern "C" S32 DisplayTexFilter;

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
    "uniform sampler2D uAtlas;\n"   /* 256×256 LUMINANCE */
    "uniform sampler2D uCLUT;\n"    /* 256×256 LUMINANCE (fog table) */
    "uniform sampler2D uPalette;\n" /* 256×1 BGRA (paletteLUT) */
    "uniform int uPolyMode;\n"      /* 0=texture 1=flat 2=gouraud 3=gouraudTable 4=textureNoCLUT 5=fogSmooth */
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
    "        vec2 atlasUV = vec2((floor(wrappedU) + 0.5) / 256.0,\n"
    "                            (floor(wrappedV) + 0.5) / 256.0);\n"
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

/* ── GL state — batch buffer ────────────────────────────────────────────── */

static const U32 BATCH_MAX_VERTS = 4096;
static U8 s_batchVerts[BATCH_MAX_VERTS * 14]; /* 14 bytes per vertex */
static U32 s_batchVertCount = 0;
static U32 s_batchTriCount = 0;
static GLuint s_batchVBO = 0;

/* ── State tracking for texture re-upload ───────────────────────────────── */

static const void *s_boundAtlas = NULL;
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0, FMT_BGRA,
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, FMT_BGRA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* GL state */
    glViewport(0, 0, resX, resY);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    /* Create FBO — always needed so 3D renders into an offscreen buffer
       that can be composited at the letterbox viewport alongside the
       2D overlay.  Without FBO, GL_Present's letterbox clear erases
       any 3D drawn directly to the default framebuffer. */
    GL_CreateFBO(resX * s_renderQuality, resY * s_renderQuality);

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
    if (s_program) {
        glDeleteProgram(s_program);
        s_program = 0;
    }
    if (s_clutProgram) {
        glDeleteProgram(s_clutProgram);
        s_clutProgram = 0;
    }

    GL_DestroyFBO();

    Window_DestroyGLContext();
    s_initialized = 0;
    s_boundAtlas = NULL;
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0, FMT_BGRA,
                     GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    s_texWidth = resX;
    s_texHeight = resY;

    /* Recreate FBO at scaled resolution (always needed) */
    s_fboHasTerrain = 0;
    GL_CreateFBO(resX * s_renderQuality, resY * s_renderQuality);
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
    const U32 *lut = GL_GetPaletteLUT();
    if (lut == NULL) {
        return;
    }

    /* Copy palette and set alpha=0 for palette index 0 (transparent colour
       in the Log buffer — sprites/UI use colour 0 as chroma key). */
    U32 paletteCopy[256];
    memcpy(paletteCopy, lut, sizeof(paletteCopy));
    paletteCopy[0] &= 0x00FFFFFF; /* alpha → 0 */

    glBindTexture(GL_TEXTURE_2D, s_paletteTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, FMT_BGRA,
                    GL_UNSIGNED_BYTE, paletteCopy);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void GL_UploadCLUT(void) {
    if (PtrCLUTFog == NULL) {
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
}

static void GL_UploadAtlas(void) {
    static S32 s_atlasLogCount = 0;
    if (PtrMap == NULL) {
        return;
    }

    /* Force re-upload when in-place texture mutations have occurred
       (same pointer, different pixel data — e.g. terrain animated polys
       or menu plasma fire animation). */
    if (AtlasTextureDirty) {
        s_boundAtlas = NULL;
        AtlasTextureDirty = 0;
    }

    /* Upload the current 256×256 atlas page. */
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, PtrMap);
    glBindTexture(GL_TEXTURE_2D, 0);

    s_boundAtlas = PtrMap;
}

/* ── Batch management ───────────────────────────────────────────────────── */

void GL_FlushBatch(void) {
    if (!Window_GLMakeCurrent()) {
        return;
    }

    if (s_batchVertCount == 0) {
        return;
    }

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
               s_currentTypePoly == POLY_TEXTURE_INCRUST) {
        /* Type 8 (POLY_TEXTURE) and 12 (POLY_TEXTURE_INCRUST):
           SW Filler_Texture / Filler_TextureChromaKey writes texel as
           palette index directly (*line = texel) — no CLUT lookup.
           Chroma key (discard index 0) controlled by uChromaKey. */
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

    /* Bind textures to units 0/1/2 */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glUniform1i(s_clut_uAtlas, 0);

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

        NZW batches (animated polys) use GL_ALWAYS: they are surface
        decorations coplanar with the terrain, so GEQUAL at far
        distances z-fights due to16-bit depth precision.  Objects
        (types 0–15, non-NZW) keep GEQUAL so they are correctly
        occluded by closer terrain. */
    if (s_currentTypePoly <= POLY_TEXTURE_Z_FOG) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(s_batchNZW ? GL_ALWAYS : GL_GEQUAL);

        if (s_batchZBuffer || s_currentTypePoly >= POLY_TEXTURE_Z) {
            /* Z-buffered path: write depth unless NZW or blending.
               Includes terrain base types 0/6/7 (via s_batchZBuffer)
               and transparent/translucent types 16–24. */
            glDepthMask((!s_batchNZW && !useBlend) ? GL_TRUE : GL_FALSE);
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
extern U32 ModeDesiredX;
extern U32 ModeDesiredY;
}

void GL_ReadbackDepthBuffer(void) {
    if (!s_initialized || !s_fbo || PtrZBuffer == NULL) {
        return;
    }

    /* Flush any pending terrain batch so the FBO contains the latest
       geometry before we read back its depth buffer. */
    GL_FlushBatch();

    /* FBO is now bound (GL_FlushBatch binds it on first use per frame,
       or it was already bound).  No need to rebind here. */

    const U32 fboW = s_fboWidth;
    const U32 fboH = s_fboHeight;
    const U32 scrW = ModeDesiredX;
    const U32 scrH = ModeDesiredY;

    /* glReadPixels returns rows bottom-to-top (Y=0 at bottom), but
       PtrZBuffer stores rows top-to-bottom (Y=0 at top).  Flip Y when
       writing each row so screen coordinates match. */
    if (fboW == scrW && fboH == scrH) {
        /* 1:1 path: read into temp buffer, invert depth, flip Y into
           PtrZBuffer.  The vertex shader outputs
           Z = 1.0 - aZ/65535.0; after the viewport transform
           depth = (2.0 - aZ/65535) / 2, stored as
           gl_depth_16 = (131070 - aZ) / 2.
           Inverting: aZ = 131070 - 2 * gl_depth_16.
           ZBufBoxOverWrite2 compares aZ values, so the inverse
           mapping is required. */
        static U16 *s_depthFlipBuf = NULL;
        static U32 s_depthFlipBufSize = 0;
        U32 totalPixels = scrW * scrH;
        if (s_depthFlipBufSize < totalPixels) {
            free(s_depthFlipBuf);
            s_depthFlipBuf = (U16 *)malloc(totalPixels * sizeof(U16));
            s_depthFlipBufSize = s_depthFlipBuf ? totalPixels : 0;
        }
        if (s_depthFlipBuf == NULL) {
            return;
        }
        glReadPixels(0, 0, scrW, scrH, GL_DEPTH_COMPONENT,
                     GL_UNSIGNED_SHORT, s_depthFlipBuf);
        for (U32 sy = 0; sy < scrH; sy++) {
            const U16 *srcRow = s_depthFlipBuf + (scrH - 1 - sy) * scrW;
            U16 *dstRow = PtrZBuffer + sy * scrW;
            for (U32 sx = 0; sx < scrW; sx++) {
                U32 gl_depth_16 = srcRow[sx];
                dstRow[sx] = (U16)(131070u - 2u * gl_depth_16);
            }
        }
    } else {
        /* SSAA path: read full FBO depth, downsample by picking the
           closest terrain (minimum aZ) per screen pixel block. */
        const U32 totalPixels = fboW * fboH;
        static U16 *s_fboDepthBuf = NULL;
        static U32 s_fboDepthBufSize = 0;
        if (s_fboDepthBufSize < totalPixels) {
            free(s_fboDepthBuf);
            s_fboDepthBuf = (U16 *)malloc(totalPixels * sizeof(U16));
            s_fboDepthBufSize = s_fboDepthBuf ? totalPixels : 0;
        }
        if (s_fboDepthBuf == NULL) {
            return;
        }

        glReadPixels(0, 0, fboW, fboH, GL_DEPTH_COMPONENT,
                     GL_UNSIGNED_SHORT, s_fboDepthBuf);

        const U32 scaleX = fboW / scrW;
        const U32 scaleY = fboH / scrH;

        for (U32 sy = 0; sy < scrH; sy++) {
            U16 *dstRow = PtrZBuffer + sy * scrW;
            for (U32 sx = 0; sx < scrW; sx++) {
                U16 minAZ = 0x7FFF; /* start with smallest possible aZ */
                const U32 fx0 = sx * scaleX;
                const U32 fy0 = (fboH - 1 - sy * scaleY) - scaleY + 1;
                for (U32 fy = fy0; fy < fy0 + scaleY; fy++) {
                    const U16 *srcRow = s_fboDepthBuf + fy * fboW;
                    for (U32 fx = fx0; fx < fx0 + scaleX; fx++) {
                        U32 gl_depth_16 = srcRow[fx];
                        U16 az = (U16)(131070u - 2u * gl_depth_16);
                        if (az < minAZ) {
                            minAZ = az;
                        }
                    }
                }
                dstRow[sx] = minAZ;
            }
        }
    }

    /* Keep the FBO bound so subsequent GL_FlushBatch() calls (e.g. from
       body polygons that don't go through the SW path) continue drawing
       into the FBO rather than the default framebuffer. */
}

/* ── Color readback (FBO → SW Screen buffer as palette indices) ────────── */

/* Read back the FBO color buffer and convert the ARGB pixels to 8-bit
   palette indices, storing the result in the Screen buffer.  This is used
   by AffGrilleExt() to copy terrain into the Log buffer so that
   ShadeBoxBlk (shadow darkening) has real pixels to operate on.

   The FBO color texture stores ARGB pixels produced by the CLUT shader,
   which maps palette indices through paletteLUT.  To reverse this, we
   build a sorted (ARGB → index) lookup table from paletteLUT and binary
   search each pixel.  Handles SSAA downscaling (FBO may be larger than
   Screen). */

void GL_ReadbackColorBuffer(void) {
    if (!s_initialized || !s_fbo || Screen == NULL) {
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
       and GL_BGRA reads bytes in B,G,R,A order → U32 = ARGB. */
    glReadPixels(0, 0, fboW, fboH, FMT_BGRA, GL_UNSIGNED_BYTE, s_fboColorBuf);

    /* Build reverse palette lookup: ARGB → palette index.
       paletteLUT has 256 unique entries.  Sort by ARGB for binary search. */
    const U32 *lut = GL_GetPaletteLUT();
    if (lut == NULL) {
        return;
    }

    struct RevEntry {
        U32 argb;
        U8 idx;
    };
    static RevEntry s_revLut[256];
    for (U32 i = 0; i < 256; i++) {
        s_revLut[i].argb = lut[i];
        s_revLut[i].idx = (U8)i;
    }
    /* Insertion sort (256 elements — trivial) */
    for (U32 i = 1; i < 256; i++) {
        RevEntry key = s_revLut[i];
        S32 j = (S32)i - 1;
        while (j >= 0 && s_revLut[j].argb > key.argb) {
            s_revLut[j + 1] = s_revLut[j];
            j--;
        }
        s_revLut[j + 1] = key;
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
                U32 argb = srcRow[sx];
                S32 lo = 0, hi = 255;
                while (lo <= hi) {
                    S32 mid = (lo + hi) >> 1;
                    U32 midArgb = s_revLut[mid].argb;
                    if (midArgb == argb) {
                        dstRow[sx] = s_revLut[mid].idx;
                        goto next_pixel;
                    } else if (midArgb < argb) {
                        lo = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                /* No exact match — nearest by linear scan */
                {
                    U32 bestDist = 0xFFFFFFFF;
                    U8 bestIdx = 0;
                    for (U32 j = 0; j < 256; j++) {
                        U32 c = lut[j];
                        S32 dr = (S32)((c >> 16) & 0xFF) - (S32)((argb >> 16) & 0xFF);
                        S32 dg = (S32)((c >> 8) & 0xFF) - (S32)((argb >> 8) & 0xFF);
                        S32 db = (S32)(c & 0xFF) - (S32)(argb & 0xFF);
                        U32 dist = (U32)(dr * dr + dg * dg + db * db);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = (U8)j;
                        }
                    }
                    dstRow[sx] = bestIdx;
                }
            next_pixel:;
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
                U32 argb = s_fboColorBuf[fy * fboW + fx];
                S32 lo = 0, hi = 255;
                while (lo <= hi) {
                    S32 mid = (lo + hi) >> 1;
                    U32 midArgb = s_revLut[mid].argb;
                    if (midArgb == argb) {
                        dstRow[sx] = s_revLut[mid].idx;
                        goto next_ssaa_pixel;
                    } else if (midArgb < argb) {
                        lo = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                /* Fallback: nearest by linear scan */
                {
                    U32 bestDist = 0xFFFFFFFF;
                    U8 bestIdx = 0;
                    for (U32 j = 0; j < 256; j++) {
                        U32 c = lut[j];
                        S32 dr = (S32)((c >> 16) & 0xFF) - (S32)((argb >> 16) & 0xFF);
                        S32 dg = (S32)((c >> 8) & 0xFF) - (S32)((argb >> 8) & 0xFF);
                        S32 db = (S32)(c & 0xFF) - (S32)(argb & 0xFF);
                        U32 dist = (U32)(dr * dr + dg * dg + db * db);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = (U8)j;
                        }
                    }
                    dstRow[sx] = bestIdx;
                }
            next_ssaa_pixel:;
            }
        }
    }

    /* Keep FBO bound for subsequent GL_FlushBatch() calls */
}

/* ── Present (extended with 3D flush) ───────────────────────────────────── */

void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear) {
    if (!s_initialized || argbPixels == NULL) {
        return;
    }

    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* Flush any remaining batched 3D geometry from the current frame.
       This draws into the FBO if SSAA is active, or the default
       framebuffer otherwise. */
    GL_FlushBatch();

    /* Reset frame flag so GL_FlushBatch will rebind FBO next frame. */
    s_frameActive = 0;

    /* Switch back to the default framebuffer for compositing */
    if (s_fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    /* Clear the default framebuffer to black (for letterbox bars). */
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

    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, FMT_BGRA,
                    GL_UNSIGNED_BYTE, argbPixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

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

    Window_GL_SwapWindow();
}

/* ── Triangle list rendering ────────────────────────────────────────────── */

S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    if (!s_initialized) {
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
        return 0;
    }

    /* Need at least 3 points for a triangle */
    if (nbPoints < 3) {
        return 0;
    }

    /* 2D screen-space effects (menu plasma/fire bars) must render into
       the Log buffer (2D overlay), not the FBO (3D scene).  They use
       POLY_TEXTURE (type 8) with RepMask 0x3F3F — same polygon type
       as terrain but in a different compositing layer.  Let the SW
       rasterizer handle them so they land in the 2D overlay on top of
       the menu text. */
    if (typePoly == POLY_TEXTURE && (RepMask & 0xFFFF) == 0x3F3F) {
        return 0;
    }

    /* Detect batch-flush triggers: type, color, atlas page, CLUT fog row,
       or Z-buffer state changed.  SetCLUT(fog) is called per terrain tile
       with different fog distances — each tile needs its own CLUT base row
       for correct brightness.  Without flushing on CLUT change, tiles with
       different fog distances share the wrong CLUT row, causing brightness
       flickering as the camera moves. */
    if (s_batchVertCount > 0) {
        if (s_currentTypePoly != (U32)typePoly ||
            s_currentColorPoly != (U32)colorPoly ||
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
    if (typePoly >= POLY_TEXTURE &&
        (s_boundAtlas != PtrMap || AtlasTextureDirty)) {
        GL_UploadAtlas();
    }
    GL_UploadPalette(); /* re-upload every frame (cheap: 1 KB) */

    /* Stamp per-polygon RepMask onto each vertex (SW rasterizer uses the
       global RepMask; GL needs it per-vertex for batching). */
    for (S32 i = 0; i < nbPoints; i++) {
        ptrPoints[i].Pt_RepMask = (U16)(RepMask & 0xFFFF);
    }

    /* Convert polygon to triangles (triangle fan from first vertex) */
    for (S32 i = 1; i < nbPoints - 1; i++) {
        GL_AddTriangle(&ptrPoints[0], &ptrPoints[i], &ptrPoints[i + 1]);
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
void GL_SetRenderQuality(U32 quality) { (void)quality; }
void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear) {
    (void)argbPixels; (void)pitch; (void)width; (void)height;
    (void)filterLinear;
}
S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    (void)typePoly; (void)colorPoly; (void)nbPoints; (void)ptrPoints;
    return 0;
}

#endif /* USE_GPURENDERER */
