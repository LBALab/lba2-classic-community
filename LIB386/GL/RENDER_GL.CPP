#include <GL/RENDER_GL.H>

#include <3D/CAMERA.H>
#include <3D/PROJ.H>
#include <3D/DATAMAT.H>
#include <3D/LIGHT.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/WINDOW.H>
#include <SYSTEM/OPENXR.H>
#include <SVGA/DIRTYBOX.H>
#include <SVGA/XBR.H>

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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <FILEIO/SAVEPNG.H>

/* ── External palette getter (defined in SDL.CPP) ───────────────────────── */
extern "C" const U32 *GL_GetPaletteLUT(void);
extern "C" U32 GL_GetPaletteGeneration(void);
extern "C" U32 CLUTGeneration;

/* ── External CLUT / texture globals (from POLY.CPP) ────────────────────── */
extern "C" {
extern PTR_U8 PtrCLUTFog;
extern PTR_U8 PtrCLUTGouraud;
extern PTR_U8 PtrMap;
extern S32 DisplayBrightness;
extern S32 DisplayContrast;
extern S32 DisplaySaturation;
extern S32 CubeMode; /* CUBE_INTERIEUR = 0, from SOURCES/COMMON.H */
extern S32 RepMask;
extern U8 Fill_Flag_ZBuffer;
extern U8 Fill_Flag_NZW;
extern U32 Fill_Fog_Factor;
extern U32 Fill_ScaledFogNear;
}

/* ── External dirty flag (from VAR_EXT.CPP) ─────────────────────────────── */
extern "C" S32 AtlasTextureDirty;
extern "C" U8 *AtlasTextureDirtyPtrs[];
extern "C" S32 AtlasTextureDirtyCount;
extern "C" S32 AtlasTextureDirtyAll;

/* Temporary perf instrumentation: why the last poly fell back to SW
   (1=renderer down, 2=type range, 3=no zbuffer flag, 4=NZW without
   terrain, 5=too few points, 6=menu plasma mask, 7=unprojectable vertex;
   0=accepted). */
extern "C" S32 GL_LastRejectReason;
S32 GL_LastRejectReason = 0;

/* ── External texture filter toggle (from GLOBAL.CPP) ────────────────────── */
extern "C" S32 DisplayTexFilter;
extern "C" S32 Obj_GLWorld;
extern "C" S32 DisplayPerspectiveTextures;
extern "C" S32 DisplayAntiAlias;
extern "C" S32 DisplayDither;
extern "C" S32 DisplaySharpen;
extern "C" S32 DisplayStereo3D;

/* Present-side sharpening strength: 0 = off, 1 = low, 2 = high. */
static float GL_SharpenAmount(void) {
    if (DisplaySharpen == 2)
        return 0.7f;
    if (DisplaySharpen == 1)
        return 0.35f;
    return 0.0f;
}

/* Temporary perf instrumentation: draw calls flushed since GL_BeginFrame */
extern "C" S32 GL_BatchFlushCount;
S32 GL_BatchFlushCount = 0;

/* Texture units the binding cache tracks, and the unit the indexed layer
   takes for its depth map.  Units 0-2 are the atlas/CLUT/palette pair every
   CLUT program samples, units 3-5 belong to the texture arrays the batch,
   decor and terrain programs use, and the layer takes one of its own so its
   2D depth texture never lands on a unit bound to another target. */
#define GL_TEX_UNIT_CACHE 8
#define GL_LAYER_DEPTH_UNIT 6
#define GL_LAYER_MASK_UNIT 7

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
#define GLSL_IMAGE_ADJUST                                                     \
    "uniform float uBrightness;\n"                                            \
    "uniform float uContrast;\n"                                              \
    "uniform float uSaturation;\n"                                            \
    "vec3 ImageAdjust(vec3 c) {\n"                                            \
    "    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, uSaturation);\n" \
    "    c = (c - 0.5) * uContrast + 0.5 + uBrightness;\n"                    \
    "    return c;\n"                                                         \
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
    GLSL_FRAG_HEADER GLSL_IMAGE_ADJUST
    "uniform sampler2D uTex;\n"
    "uniform vec2 uTexel;\n"
    "uniform float uSharpen;\n" /* 0 = off, unsharp strength */
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    /* The 2D overlay is a screen-space layer: Scale2x and xBR belong to the
       3D texture sampler, so the overlay itself samples plainly. */
    "    vec4 result = texture2D(uTex, vTexCoord);\n"
    /* Unsharp mask on the composited scene.  Four cross taps at one source
       texel keep it cheap; the overlay is drawn by a different program, so
       text never gets halos. */
    "    vec3 sharpened = result.rgb;\n"
    "    if (uSharpen > 0.001) {\n"
    "        vec3 sn = texture2D(uTex, vTexCoord + vec2(0.0, -uTexel.y)).rgb;\n"
    "        vec3 ss = texture2D(uTex, vTexCoord + vec2(0.0, uTexel.y)).rgb;\n"
    "        vec3 sw = texture2D(uTex, vTexCoord + vec2(-uTexel.x, 0.0)).rgb;\n"
    "        vec3 se = texture2D(uTex, vTexCoord + vec2(uTexel.x, 0.0)).rgb;\n"
    "        vec3 sblur = (sn + ss + sw + se) * 0.25;\n"
    "        sharpened = result.rgb + (result.rgb - sblur) * uSharpen;\n"
    "    }\n"
    "    gl_FragColor = vec4(ImageAdjust(sharpened), result.a);\n"
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
    "uniform float uEye;\n"
    "uniform vec2 uScreenScale;\n" /* 2.0/resX, 2.0/resY */
    "uniform float uSinglePass;\n"
    "uniform vec3 uEyeWorld;\n"
    "uniform vec2 uEyeProj;\n"
    "uniform vec3 uBaseWorld;\n" /* camera delta common to both eyes */
    "uniform vec2 uBaseProj;\n"
    "uniform vec2 uEyeFocal;\n"
    "uniform float uEyeZScale;\n"
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
    "varying float vW;\n"
    "varying float vLight;\n"
    "varying float vRepMask;\n"
    "varying float vZ;\n"
    "varying float vZOverW;\n"
    "varying float vMaterial;\n"
    "void main() {\n"
    "    float cx = aPos.x * uScreenScale.x - 1.0;\n"
    "    float cy = 1.0 - aPos.y * uScreenScale.y;\n"
    "    float eye = (uSinglePass > 0.5) ? uEye : 0.0;\n"
    /* Screen-space effects (aZ == 0, e.g. the shade rect) carry no 3D
       anchor: their second half is the same pixels, not a parallax-shifted
       version, so only the half remap uses the eye index. */
    "    float parallax = (uSinglePass > 0.5 && aZ > 0.5) ? eye : 0.0;\n"
    /* The hover rig's delta applies to both eyes; a screen-space effect
       (aZ == 0) has no 3D anchor and must not take it either. */
    "    float base = (uSinglePass > 0.5 && aZ > 0.5) ? 1.0 : 0.0;\n"
    "    float az = aZ;\n"
    "    if (uSinglePass > 0.5) {\n"
    "        /* Eye 1's camera is translated along Z as well as X/Y (head\n"
    "           lean); its depth must move with it or objects depth-test\n"
    "           against terrain drawn with the shifted eye. */\n"
    "        az = clamp(aZ + (parallax * uEyeWorld.z + base * uBaseWorld.z) *\n"
    "                          uEyeZScale,\n"
    "                     0.0, 65535.0);\n"
    "        float z0 = max(az, 1.0) / max(uEyeZScale, 0.000001);\n"
    "        float sx =\n"
    "            parallax * (-uEyeWorld.x * uEyeFocal.x / z0 + uEyeProj.x) +\n"
    "            base * (-uBaseWorld.x * uEyeFocal.x / z0 + uBaseProj.x);\n"
    "        float sy = parallax *\n"
    "                   (-uEyeWorld.y * uEyeFocal.x * uEyeFocal.y / z0 +\n"
    "                    uEyeProj.y) +\n"
    "                   base * (-uBaseWorld.y * uEyeFocal.x * uEyeFocal.y /\n"
    "                               z0 +\n"
    "                           uBaseProj.y);\n"
    "        cx = (aPos.x + sx) * uScreenScale.x - 1.0;\n"
    "        cy = 1.0 - (aPos.y + sy) * uScreenScale.y;\n"
    "        cx = (eye > 0.5) ? (cx * 0.5 + 0.5) : (cx * 0.5 - 0.5);\n"
    "    }\n"
    "    gl_Position = vec4(cx, cy, 1.0 - az / 65535.0, 1.0);\n"
    "    float invW = 1.0 / max(az, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    vTexCoordOverW = aTexCoord * invW;\n"
    "    vInvW = invW;\n"
    "    vW = 1.0;\n"
    "    vLight = aLight;\n"
    "    vRepMask = aRepMask;\n"
    "    vZ = az;\n"
    "    vZOverW = az * invW;\n"
    "    vMaterial = aMaterial;\n"
    "}\n";

/* Indexed layer vertex stage: the CLUT vertex program carries the atlas UV
   in the 8.8 fixed-point attribute pair, whose 16 bits cap a quad at 256
   texels — far below a scene-sized layer.  This variant takes normalized
   coordinates and scales them into the same 8.8 texel space in floating
   point, so the shared fragment program is unchanged.

   The quad itself never moves: both eyes cover the same screen rectangle and
   the per-pixel depth warp happens in the fragment stage, where each pixel
   can carry the offset its own depth earns. */
static const char *s_layerVertSrc =
    GLSL_VERT_HEADER
    "attribute vec2 aPos;\n" /* screen pixels */
    "attribute vec2 aUV;\n"  /* normalized 0..1 */
    "attribute float aMaterial;\n"
    "uniform float uEye;\n"
    "uniform vec2 uScreenScale;\n"
    "uniform float uSinglePass;\n"
    "uniform vec2 uAtlasScale;\n"
    "varying vec2 vTexCoord;\n"
    "varying vec2 vTexCoordOverW;\n"
    "varying float vInvW;\n"
    "varying float vW;\n"
    "varying float vLight;\n"
    "varying float vRepMask;\n"
    "varying float vZ;\n"
    "varying float vZOverW;\n"
    "varying float vMaterial;\n"
    "void main() {\n"
    "    float cx = aPos.x * uScreenScale.x - 1.0;\n"
    "    float cy = 1.0 - aPos.y * uScreenScale.y;\n"
    "    if (uSinglePass > 0.5) {\n"
    "        float eye = (uEye > 0.5) ? 1.0 : 0.0;\n"
    "        cx = (eye > 0.5) ? (cx * 0.5 + 0.5) : (cx * 0.5 - 0.5);\n"
    "    }\n"
    "    gl_Position = vec4(cx, cy, 1.0, 1.0);\n"
    "    vec2 tc = aUV * uAtlasScale * 256.0;\n"
    "    vTexCoord = tc;\n"
    "    vTexCoordOverW = tc;\n"
    "    vInvW = 1.0;\n"
    "    vW = 1.0;\n"
    "    vLight = 0.0;\n"
    "    vRepMask = 0.0;\n"
    "    vZ = 0.0;\n"
    "    vZOverW = 0.0;\n"
    "    vMaterial = aMaterial;\n"
    "}\n";

/* Terrain shader: the same fragment program as the batch, with the CPU
   projection supplied per vertex.  Positions are cube-local world
   coordinates, exactly the frame SetPosCamera() shifts the camera into,
   so the rotated camera and the vertices stay small enough for float
   precision.  The CPU's LongProjectPoint3D math is reproduced here:
   factor = FRatioX / (CameraZr - z), screen = centre + delta * factor,
   NDC y is flipped once for GL.  aFogPos is the tile's top-left corner,
   which selects the whole tile's fog row just like DrawFeuillePolyZBuf. */
static const char *s_terrainVertSrc =
    GLSL_VERT_HEADER
    "attribute vec3 aPos;\n"
    "attribute vec2 aTexCoord;\n" /* 8.8 fixed-point atlas texels */
    "attribute float aLight;\n"
    "attribute float aRepMask;\n"
    "attribute float aMaterial;\n"
    "attribute vec3 aFogPos;\n"
    "uniform float uEye;\n"
    "uniform mat3 uRot;\n"    /* MatriceWorld, transposed for GL */
    "uniform vec3 uCamera;\n" /* CameraXr/Yr/Zr (cube-local frame) */
    "uniform vec2 uFocal;\n"  /* FRatioX, FRatioY */
    "uniform vec2 uCentre;\n" /* XCentre, YCentre */
    "uniform float uProjShiftX;\n"
    "uniform float uSinglePass;\n"
    "uniform vec3 uEyeWorld;\n"
    "uniform vec2 uEyeProj;\n"
    "uniform vec3 uBaseWorld;\n" /* camera delta common to both eyes */
    "uniform vec2 uBaseProj;\n"
    "uniform vec2 uScreenScale;\n"    /* 2.0/resX, 2.0/resY */
    "uniform float uZBufferFactor;\n" /* Fill_ZBuffer_Factor / 65536 */
    "uniform float uStartZFog;\n"
    "uniform float uFogStepScale;\n" /* 15 / (ClipZFar - StartZFog) */
    "uniform float uPalLevel;\n"
    "varying vec2 vTexCoord;\n"
    "varying vec2 vTexCoordOverW;\n"
    "varying float vInvW;\n"
    "varying float vW;\n"
    "varying float vLight;\n"
    "varying float vRepMask;\n"
    "varying float vZ;\n"
    "varying float vZOverW;\n"
    "varying float vMaterial;\n"
    "void main() {\n"
    "    vec3 p = uRot * aPos;\n"
    "    float eye = (uSinglePass > 0.5) ? uEye : 0.0;\n"
    "    vec3 camDelta = eye * uEyeWorld + uBaseWorld;\n"
    "    vec2 projDelta = eye * uEyeProj + uBaseProj;\n"
    "    float depth = uCamera.z + camDelta.z - p.z;\n"
    "    float denom = depth;\n"
    "    if (abs(denom) < 0.0001) {\n"
    "        denom = (denom < 0.0) ? -0.0001 : 0.0001;\n"
    "    }\n"
    "    float factor = uFocal.x / denom;\n"
    "    float xp = uCentre.x + uProjShiftX + projDelta.x +\n"
    "               (p.x - uCamera.x - camDelta.x) * factor;\n"
    "    float yp = uCentre.y +\n"
    "               (p.y - uCamera.y - camDelta.y) * factor * uFocal.y;\n"
    "    float cx = xp * uScreenScale.x - 1.0;\n"
    "    if (uSinglePass > 0.5) {\n"
    "        cx = (eye > 0.5) ? (cx * 0.5 + 0.5) : (cx * 0.5 - 0.5);\n"
    "    }\n"
    "    float az = clamp(floor(max(depth, 0.0) * uZBufferFactor), 0.0, 65535.0);\n"
    /* Clip w is the camera-space depth: positive in front, negative
       behind, zero on the eye.  Keeping it linear in the world position
       (no near-plane offset) makes the homogeneous edge the rasterizer
       clips at w = 0 the true perspective edge; the fragment shader
       discards the sub-NearClip sliver.  Premultiplying the screen-linear
       varyings by w and dividing by the interpolated w in the fragment
       shader undoes GL's perspective correction. */
    "    float w = depth;\n"
    "    gl_Position = vec4(cx * w,\n"
    "                       (1.0 - yp * uScreenScale.y) * w,\n"
    "                       (1.0 - az / 65535.0) * w, w);\n"
    "    vTexCoord = aTexCoord * w;\n"
    /* Clip w is the camera depth, so the hardware already interpolates a
       plain varying perspective-correctly.  Premultiplying it by invW
       would make the fragment's divide weight the samples by 1/w^2
       instead of 1/w: the texture foreshortens too hard and swims as the
       camera moves.  Keep the affine variant premultiplied by w; hand the
       perspective variant over undivided. */
    "    vTexCoordOverW = aTexCoord;\n"
    "    vInvW = 1.0;\n"
    "    vW = w;\n"
    "    vLight = aLight;\n"
    "    vZ = depth * w;\n"
    "    vZOverW = depth;\n"
    /* One fog sample per tile, from its top-left corner.  SetCLUT()
       masks the row to 0xF0, so fold the same mask in before packing
       the value into the material high byte / RepMask row. */
    "    float fogDepth = uCamera.z - (uRot * aFogPos).z;\n"
    "    float fogLevel = 0.0;\n"
    "    if (uFogStepScale > 0.0) {\n"
    "        fogLevel = clamp(floor((fogDepth - uStartZFog) * uFogStepScale),\n"
    "                         0.0, 15.0);\n"
    "    } else if (fogDepth > uStartZFog) {\n"
    /* Degenerate fog range: BoundRegleTrois(0,15,range<=0,delta)
       returns 15 for any positive delta and 0 otherwise. */
    "        fogLevel = 15.0;\n"
    "    }\n"
    "    float fogRow = mod(floor((uPalLevel + 16.0 * fogLevel) / 16.0), 16.0) * 16.0;\n"
    "    float mFlags = mod(aMaterial, 256.0);\n"
    "    int mMode = int(mod(mFlags, 8.0) + 0.5);\n"
    "    float material = aMaterial;\n"
    "    float repMask = aRepMask;\n"
    "    if (mMode == 0) {\n"
    "        material = aMaterial + fogRow * 256.0;\n"
    "    } else if (mMode == 3) {\n"
    "        repMask = fogRow;\n"
    "    }\n"
    "    vMaterial = material;\n"
    "    vRepMask = repMask;\n"
    "}\n";

/* Static decor shader: the decor body's positions/normals are already
   rotated by the decor's beta (translation arrives per draw), so this only
   applies MatriceWorld, the camera-space light and the CPU projection.
   Flat shading needs the face-normal light byte; textured-flat polygons use
   the same byte as a CLUT row selector (the terrain type-9/13 rule). */
static const char *s_decorVertSrc =
    GLSL_VERT_HEADER
    "attribute vec3 aPos;\n"
    "attribute vec3 aNormal;\n"
    "attribute vec2 aTexCoord;\n" /* 8.8 fixed-point atlas texels */
    "attribute float aMaterial;\n"
    "attribute float aFlat;\n"  /* 0 none, 1 flat solid, 2 flat texture */
    "attribute float aLayer;\n" /* pinned texture-array layer */
    "uniform float uEye;\n"
    "uniform mat3 uRot;\n"
    "uniform vec3 uCamera;\n"
    "uniform vec3 uDecorPos;\n"
    "uniform vec2 uFocal;\n"
    "uniform vec2 uCentre;\n"
    "uniform float uProjShiftX;\n"
    "uniform float uSinglePass;\n"
    "uniform vec3 uEyeWorld;\n"
    "uniform vec2 uEyeProj;\n"
    "uniform vec3 uBaseWorld;\n" /* camera delta common to both eyes */
    "uniform vec2 uBaseProj;\n"
    "uniform vec2 uScreenScale;\n"
    "uniform float uZBufferFactor;\n"
    "uniform float uNearClip;\n"
    "uniform float uFogRow;\n"
    "uniform vec3 uLight;\n"
    "uniform float uLightFactor;\n"
    "varying vec2 vTexCoord;\n"
    "varying vec2 vTexCoordOverW;\n"
    "varying float vInvW;\n"
    "varying float vW;\n"
    "varying float vLight;\n"
    "varying float vRepMask;\n"
    "varying float vZ;\n"
    "varying float vZOverW;\n"
    "varying float vMaterial;\n"
    "void main() {\n"
    "    vec3 p = uRot * (aPos + uDecorPos);\n"
    "    float eye = (uSinglePass > 0.5) ? uEye : 0.0;\n"
    "    vec3 camDelta = eye * uEyeWorld + uBaseWorld;\n"
    "    vec2 projDelta = eye * uEyeProj + uBaseProj;\n"
    "    float depth = uCamera.z + camDelta.z - p.z;\n"
    "    float denom = depth;\n"
    "    if (abs(denom) < 0.0001) {\n"
    "        denom = (denom < 0.0) ? -0.0001 : 0.0001;\n"
    "    }\n"
    "    float factor = uFocal.x / denom;\n"
    "    float xp = uCentre.x + uProjShiftX + projDelta.x +\n"
    "               (p.x - uCamera.x - camDelta.x) * factor;\n"
    "    float yp = uCentre.y +\n"
    "               (p.y - uCamera.y - camDelta.y) * factor * uFocal.y;\n"
    "    float cx = xp * uScreenScale.x - 1.0;\n"
    "    if (uSinglePass > 0.5) {\n"
    "        cx = (eye > 0.5) ? (cx * 0.5 + 0.5) : (cx * 0.5 - 0.5);\n"
    "    }\n"
    "    float az = clamp(floor(max(depth, 0.0) * uZBufferFactor), 0.0, 65535.0);\n"
    "    float w = depth;\n"
    "    gl_Position = vec4(cx * w,\n"
    "                       (1.0 - yp * uScreenScale.y) * w,\n"
    "                       (1.0 - az / 65535.0) * w, w);\n"
    "    vTexCoord = aTexCoord * w;\n"
    /* Clip w is the camera depth, so the hardware already interpolates a
       plain varying perspective-correctly.  Premultiplying it by invW
       would make the fragment's divide weight the samples by 1/w^2
       instead of 1/w: the texture foreshortens too hard and swims as the
       camera moves.  Keep the affine variant premultiplied by w; hand the
       perspective variant over undivided. */
    "    vTexCoordOverW = aTexCoord;\n"
    "    vInvW = 1.0;\n"
    "    vW = w;\n"
    "    vZ = depth * w;\n"
    "    vZOverW = depth;\n"
    "    float light = max(dot(uRot * aNormal, uLight) * uLightFactor, 0.0);\n"
    /* LightList stores lrint(dot); the filler's >>8 and the CLUT row
       (light+128)/256 both assume that rounded integer, so round once here
       instead of feeding the raw dot into the flat byte / row math. */
    "    light = floor(light + 0.5);\n"
    "    vLight = light;\n"
    "    float low = mod(aMaterial, 256.0);\n"
    "    float raw = floor(aMaterial / 256.0);\n"
    "    int mMode = int(mod(low, 8.0) + 0.5);\n"
    "    float material = aMaterial;\n"
    "    float rep = 0.0;\n"
    "    if (aFlat > 1.5) {\n"
    /* Textured flat (9/13): the SW handler passes the light byte itself as
       the CLUT row selector; the body colour is not part of that row. */
    "        float byte = floor(light / 256.0);\n"
    "        vLight = mod(byte, 16.0) * 256.0;\n"
    "        material = mod(low, 128.0) + uFogRow * 256.0;\n"
    "        rep = aLayer;\n"
    "    } else if (aFlat > 0.5) {\n"
    "        float byte = floor(light / 256.0);\n"
    "        float col = mod(raw + byte, 256.0);\n"
    "        material = low + col * 256.0;\n"
    "    } else if (mMode == 0 || mMode == 4) {\n"
    "        material = aMaterial + uFogRow * 256.0;\n"
    "        rep = aLayer;\n"
    "    } else if (mMode == 3) {\n"
    "        rep = uFogRow;\n"
    "    }\n"
    "    vMaterial = material;\n"
    "    vRepMask = rep;\n"
    "}\n";

static const char *s_clutFragSrc =
    GLSL_FRAG_HEADER
    "uniform sampler2D uAtlas;\n"     /* 256×256 LUMINANCE (or sprite page) */
    "uniform vec2 uAtlasScale;\n"     /* texture dims for texel-space UVs */
    "uniform sampler2D uCLUT;\n"      /* 256×256 RGBA (baked CLUT+palette) */
    "uniform sampler2D uPalette;\n"   /* 256×1 BGRA (paletteLUT) */
    "uniform float uCLUTBaseRow;\n"   /* legacy row offset (now per vertex) */
    "uniform float uScaledFogNear;\n" /* Fill_ScaledFogNear (fog-smooth distance, mode 5) */
    "uniform float uFogRowScale;\n"   /* Fill_Fog_Factor / 65536 (fog-smooth distance, mode 5) */
    "uniform int uBilinear;\n"        /* 0=nearest 1=bilinear 2=Scale2x 3=xBR 4=xBR Lite */
    "uniform float uDither;\n"        /* 1 = ordered dither between CLUT rows */
    /* Terrain far-plane discard.  The static terrain VBO path projects
       cube-local world positions in the vertex shader, where the CPU
       path's per-vertex ClipZFar cull cannot run; the terrain vertex
       shader leaves the true camera-space depth in vZ so this fragment
       test reproduces the far cut.  Zero for every other program. */
    "uniform float uTerrainFarClip;\n"
    "uniform float uTerrainPersp;\n"
    "uniform float uTerrainFar;\n"
    "uniform float uNearClip;\n"
    /* Indexed-layer per-pixel stereo warp (see GL_DrawIndexedLayer).  Only the
       layer program declares these; LBA_LAYER_NO_WRAP, which GL_MakeLayerFragSrc
       injects for that program alone, keeps them out of every other one. */
    "#ifdef LBA_LAYER_NO_WRAP\n"
    "uniform sampler2D uLayerDepth;\n" /* 8-bit depth, one per layer pixel */
    "uniform float uLayerWarp;\n"      /* 0 = draw flat, 1 = warp per eye */
    "uniform float uLayerAzMin;\n"     /* depth of byte 0, aZ units */
    "uniform float uLayerAzSpan;\n"    /* depth of byte 255 minus byte 0 */
    "uniform float uLayerAzShift;\n"   /* this eye's depth delta, aZ units */
    "uniform vec2 uLayerShiftPx;\n"    /* this eye's fixed screen offset, px */
    "uniform vec2 uLayerShiftPerAz;\n" /* this eye's 1/depth offset, px*aZ */
    /* Coverage mask: an 8-bit tile of the region around the drawn rect, read
       at the SAME shifted texel the colour is read at, so the silhouette
       moves with the pixels it belongs to. */
    "uniform sampler2D uLayerMask;\n"
    "uniform float uLayerMaskOn;\n"
    "uniform vec4 uLayerMaskRect;\n" /* tile origin x, y, 1/w, 1/h */
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "varying vec2 vTexCoordOverW;\n"
    "varying float vInvW;\n"
    "varying float vW;\n"
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
    /* Look up a palette index through the baked CLUT, returning the final
       RGBA color.  `palIdx` is the raw atlas texel (palette index) and
       `gouraudRow` the CLUT row selected by the gouraud intensity.  The
       texture already contains palette-resolved colors, so one fetch
       replaces the old CLUT→palette pair. */
    "vec4 clutLookup(float palIdx, float gouraudRow) {\n"
    "    vec2 cuv = vec2((palIdx + 0.5) / 256.0,\n"
    "                     (gouraudRow + 0.5) / 256.0);\n"
    "    return texture2D(uCLUT, cuv);\n"
    "}\n"
    /* 4x4 ordered dither (Bayer).  The software gouraud fillers add the low
       byte of the interpolated shade to the row before quantising; this is
       the per-pixel equivalent, so a gradient between two CLUT rows keeps the
       palette average instead of snapping to one of them.  Recursive form of
       the Bayer matrix: M(2n) = [[4M(n), 4M(n)+2], [4M(n)+3, 4M(n)+1]]. */
    "float dither2x2(vec2 p) {\n"
    "    float x = mod(p.x, 2.0);\n"
    "    float y = mod(p.y, 2.0);\n"
    "    if (x < 1.0)\n"
    "        return (y < 1.0) ? 0.0 : 3.0;\n"
    "    return (y < 1.0) ? 2.0 : 1.0;\n"
    "}\n"
    "float dither4x4(vec2 p) {\n"
    "    return 4.0 * dither2x2(mod(p, 2.0)) +\n"
    "           dither2x2(mod(floor(p * 0.5), 2.0));\n"
    "}\n"
    /* ── xBR-lv2 sampling (texture filter mode 3) ──────────────────────────
       Hyllian's xBR-lv2 edge rules (MIT; the CPU twin in LIB386/SVGA/XBR.CPP
       documents the provenance and keeps the rules host-tested), run on the
       atlas neighbourhood of the texel being sampled.  Edge weights come
       from the unlit palette colors; the blend applies those weights to the
       CLUT-lit neighbours, so gouraud lighting stays a per-texel lookup and
       only the smoothing is new.  CORNER_C variant, rgbw luminance,
       small-details path. */
    "const vec3 xbrRgbw = vec3(14.352, 28.176, 5.472);\n"
    "const vec4 xbrDelta = vec4(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0);\n"
    "const vec4 xbrDeltaL = vec4(0.5 / 3.0, 1.0 / 3.0, 0.5 / 3.0, 1.0 / 3.0);\n"
    "const vec4 xbrDeltaU = vec4(1.0 / 3.0, 0.5 / 3.0, 1.0 / 3.0, 0.5 / 3.0);\n"
    "const vec4 xbrAo = vec4( 1.0, -1.0, -1.0,  1.0);\n"
    "const vec4 xbrBo = vec4( 1.0,  1.0, -1.0, -1.0);\n"
    "const vec4 xbrCo = vec4( 1.5,  0.5, -0.5,  0.5);\n"
    "const vec4 xbrAx = vec4( 1.0, -1.0, -1.0,  1.0);\n"
    "const vec4 xbrBx = vec4( 0.5,  2.0, -0.5, -2.0);\n"
    "const vec4 xbrCx = vec4( 1.0,  1.0, -0.5,  0.0);\n"
    "const vec4 xbrAy = vec4( 1.0, -1.0, -1.0,  1.0);\n"
    "const vec4 xbrBy = vec4( 2.0,  0.5, -2.0, -0.5);\n"
    "const vec4 xbrCy = vec4( 2.0,  0.0, -1.0,  0.5);\n"
    "const vec4 xbrCi = vec4(0.25, 0.25, 0.25, 0.25);\n"
    "float xbrIndex(vec2 p, vec2 whm) {\n"
    /* A full-image layer has no wrap window: taps outside it must clamp at
       the image edge, not jump to the opposite side. */
    "#ifdef LBA_LAYER_NO_WRAP\n"
    "    return texture2D(uAtlas, vec2((p.x + 0.5) / uAtlasScale.x,\n"
    "                                  (p.y + 0.5) / uAtlasScale.y)).r * 255.0;\n"
    "#else\n"
    "    return texture2D(uAtlas, vec2((mod(p.x, whm.x) + 0.5) / uAtlasScale.x,\n"
    "                                  (mod(p.y, whm.y) + 0.5) / uAtlasScale.y)).r * 255.0;\n"
    "#endif\n"
    "}\n"
    "vec3 xbrPal(float idx) {\n"
    "    return texture2D(uPalette, vec2((idx + 0.5) / 256.0, 0.5)).rgb;\n"
    "}\n"
    "vec3 xbrLit(float idx, float row, float noClut) {\n"
    "    if (noClut > 0.5) return xbrPal(idx);\n"
    "    return clutLookup(idx, row).rgb;\n"
    "}\n" /* Chroma-keyed texels are transparent in the software blit, not black:
       a filtered tap that lands on one must keep the centre texel instead of
       dragging palette[0] into the edge.  Otherwise every sprite/tile
       boundary darkens. */
    "float spriteChromaIdx(float idx, float centre, float chroma) {\n"
    "    return (chroma > 0.5 && idx < 0.5) ? centre : idx;\n"
    "}\n"
    /* Scale2x on the atlas neighbourhood (texture filter mode 2): the
       documented 2x edge rules, evaluated per fragment — the sample
       position inside the source texel picks the output sub-quadrant, and
       the rule replaces the centre texel with the matching neighbour only
       when the two neighbours guarding that corner compare equal.  The
       comparisons run on raw atlas indices (exact integers), so unlike
       xBR this needs no color math — and exactly one lit lookup. */
    "vec4 clutScale2x(vec2 tc, float widthMask, float heightMask,\n"
    "                  float gouraudRow, float chroma) {\n"
    "    vec2 whm = vec2(widthMask + 1.0, heightMask + 1.0);\n"
    "    float u = mod(tc.x, whm.x);\n"
    "    float v = mod(tc.y, whm.y);\n"
    "    vec2 pC = vec2(floor(u), floor(v));\n"
    "    vec2 fp = vec2(u - pC.x, v - pC.y);\n"
    "    float e = xbrIndex(pC, whm);\n"
    "    if (chroma > 0.5 && e < 0.5) discard;\n"
    "    float b = xbrIndex(pC + vec2( 0.0, -1.0), whm);\n"
    "    float d = xbrIndex(pC + vec2(-1.0,  0.0), whm);\n"
    "    float f = xbrIndex(pC + vec2( 1.0,  0.0), whm);\n"
    "    float h = xbrIndex(pC + vec2( 0.0,  1.0), whm);\n"
    "    float r = e;\n"
    "    if (b != h && d != f) {\n"
    "        if (fp.x < 0.5 && fp.y < 0.5) {\n"
    "            r = (d == b) ? d : e;\n"
    "        } else if (fp.x >= 0.5 && fp.y < 0.5) {\n"
    "            r = (b == f) ? f : e;\n"
    "        } else if (fp.x < 0.5) {\n"
    "            r = (d == h) ? d : e;\n"
    "        } else {\n"
    "            r = (h == f) ? f : e;\n"
    "        }\n"
    "    }\n"
    "    return vec4(xbrLit(r, gouraudRow, 0.0), 1.0);\n"
    "}\n"
    /* xBR Lite (texture filter mode 4): a five-tap cardinal approximation of
       xBR.  For each sub-quadrant, if the two neighbours guarding that corner
       compare equal (the Scale2x guard), blend the centre toward that
       neighbour with a weight that ramps from zero at the texel centre to one
       at the outer corner.  The result keeps xBR's soft diagonal edge without
       the twelve corner taps, the palette-color distance chain or the LV2
       quadrant search. */
    "vec4 clutXbrLite(vec2 tc, float widthMask, float heightMask,\n"
    "                  float gouraudRow, float chroma, float noClut) {\n"
    "    vec2 whm = vec2(widthMask + 1.0, heightMask + 1.0);\n"
    "    float u = mod(tc.x, whm.x);\n"
    "    float v = mod(tc.y, whm.y);\n"
    "    vec2 pC = vec2(floor(u), floor(v));\n"
    "    vec2 fp = vec2(u - pC.x, v - pC.y);\n"
    "    float e = xbrIndex(pC, whm);\n"
    "    if (chroma > 0.5 && e < 0.5) discard;\n"
    "    float b = xbrIndex(pC + vec2( 0.0, -1.0), whm);\n"
    "    float d = xbrIndex(pC + vec2(-1.0,  0.0), whm);\n"
    "    float f = xbrIndex(pC + vec2( 1.0,  0.0), whm);\n"
    "    float h = xbrIndex(pC + vec2( 0.0,  1.0), whm);\n"
    "    if (chroma > 0.5) {\n"
    "        b = spriteChromaIdx(b, e, chroma);\n"
    "        d = spriteChromaIdx(d, e, chroma);\n"
    "        f = spriteChromaIdx(f, e, chroma);\n"
    "        h = spriteChromaIdx(h, e, chroma);\n"
    "    }\n"
    "    vec3 cE = xbrLit(e, gouraudRow, noClut);\n"
    "    if (b == e && d == e && f == e && h == e)\n"
    "        return vec4(cE, 1.0);\n"
    "    vec3 cD = xbrLit(d, gouraudRow, noClut);\n"
    "    vec3 cF = xbrLit(f, gouraudRow, noClut);\n"
    "    vec3 c = cE;\n"
    "    float wl = max(1.0 - 2.0 * fp.x, 0.0);\n"
    "    float wr = max(2.0 * fp.x - 1.0, 0.0);\n"
    "    float wt = max(1.0 - 2.0 * fp.y, 0.0);\n"
    "    float wb = max(2.0 * fp.y - 1.0, 0.0);\n"
    "    if (d == b) c = mix(c, cD, wl * wt);\n"
    "    if (b == f) c = mix(c, cF, wr * wt);\n"
    "    if (d == h) c = mix(c, cD, wl * wb);\n"
    "    if (h == f) c = mix(c, cF, wr * wb);\n"
    "    return vec4(c, 1.0);\n"
    "}\n"
    "float xbrCDF(vec3 c1, vec3 c2) {\n"
    "    vec3 df = abs(c1 - c2);\n"
    "    return df.r + df.g + df.b;\n"
    "}\n"
    "vec4 xbrEq(vec4 A, vec4 B) {\n"
    "    return step(abs(A - B), vec4(15.0));\n"
    "}\n"
    "vec4 xbrNeq(vec4 A, vec4 B) {\n"
    "    return vec4(1.0) - xbrEq(A, B);\n"
    "}\n"
    "vec4 xbrDiff(vec4 A, vec4 B) {\n"
    "    return vec4(notEqual(A, B));\n"
    "}\n"
    "vec4 clutXbr(vec2 tc, float widthMask, float heightMask,\n"
    "              float gouraudRow, float chroma, float noClut) {\n"
    "    vec2 whm = vec2(widthMask + 1.0, heightMask + 1.0);\n"
    "    float u = mod(tc.x, whm.x);\n"
    "    float v = mod(tc.y, whm.y);\n"
    "    float iu = floor(u);\n"
    "    float iv = floor(v);\n"
    "    vec2 fp = vec2(u - iu, v - iv);\n"
    "    vec2 pC = vec2(iu, iv);\n"
    /*    A1 B1 C1                                  */
    /* A0  A  B  C C4                              */
    /* D0  D  E  F F4                              */
    /* G0  G  H  I I4                              */
    /*    G5 H5 I5                                 */
    "    float eIdx = xbrIndex(pC, whm);\n"
    "    if (chroma > 0.5 && eIdx < 0.5) discard;\n"
    "    float a1 = xbrIndex(pC + vec2(-1.0, -2.0), whm);\n"
    "    float b1 = xbrIndex(pC + vec2( 0.0, -2.0), whm);\n"
    "    float c1 = xbrIndex(pC + vec2( 1.0, -2.0), whm);\n"
    "    float a  = xbrIndex(pC + vec2(-1.0, -1.0), whm);\n"
    "    float b  = xbrIndex(pC + vec2( 0.0, -1.0), whm);\n"
    "    float c  = xbrIndex(pC + vec2( 1.0, -1.0), whm);\n"
    "    float d  = xbrIndex(pC + vec2(-1.0,  0.0), whm);\n"
    "    float f  = xbrIndex(pC + vec2( 1.0,  0.0), whm);\n"
    "    float g  = xbrIndex(pC + vec2(-1.0,  1.0), whm);\n"
    "    float h  = xbrIndex(pC + vec2( 0.0,  1.0), whm);\n"
    "    float i  = xbrIndex(pC + vec2( 1.0,  1.0), whm);\n"
    "    float g5 = xbrIndex(pC + vec2(-1.0,  2.0), whm);\n"
    "    float h5 = xbrIndex(pC + vec2( 0.0,  2.0), whm);\n"
    "    float i5 = xbrIndex(pC + vec2( 1.0,  2.0), whm);\n"
    /* Chroma-keyed sprite taps are holes, not black pixels. */
    "    if (chroma > 0.5) {\n"
    "        a1 = spriteChromaIdx(a1, eIdx, chroma);\n"
    "        b1 = spriteChromaIdx(b1, eIdx, chroma);\n"
    "        c1 = spriteChromaIdx(c1, eIdx, chroma);\n"
    "        a = spriteChromaIdx(a, eIdx, chroma);\n"
    "        b = spriteChromaIdx(b, eIdx, chroma);\n"
    "        c = spriteChromaIdx(c, eIdx, chroma);\n"
    "        d = spriteChromaIdx(d, eIdx, chroma);\n"
    "        f = spriteChromaIdx(f, eIdx, chroma);\n"
    "        g = spriteChromaIdx(g, eIdx, chroma);\n"
    "        h = spriteChromaIdx(h, eIdx, chroma);\n"
    "        i = spriteChromaIdx(i, eIdx, chroma);\n"
    "        g5 = spriteChromaIdx(g5, eIdx, chroma);\n"
    "        h5 = spriteChromaIdx(h5, eIdx, chroma);\n"
    "        i5 = spriteChromaIdx(i5, eIdx, chroma);\n"
    "    }\n"
    "    float a0 = xbrIndex(pC + vec2(-2.0, -1.0), whm);\n"
    "    float d0 = xbrIndex(pC + vec2(-2.0,  0.0), whm);\n"
    "    float g0 = xbrIndex(pC + vec2(-2.0,  1.0), whm);\n"
    "    float c4 = xbrIndex(pC + vec2( 2.0, -1.0), whm);\n"
    "    float f4 = xbrIndex(pC + vec2( 2.0,  0.0), whm);\n"
    "    float i4 = xbrIndex(pC + vec2( 2.0,  1.0), whm);\n"
    "    if (chroma > 0.5) {\n"
    "        a0 = spriteChromaIdx(a0, eIdx, chroma);\n"
    "        d0 = spriteChromaIdx(d0, eIdx, chroma);\n"
    "        g0 = spriteChromaIdx(g0, eIdx, chroma);\n"
    "        c4 = spriteChromaIdx(c4, eIdx, chroma);\n"
    "        f4 = spriteChromaIdx(f4, eIdx, chroma);\n"
    "        i4 = spriteChromaIdx(i4, eIdx, chroma);\n"
    "    }\n"
    /* Packed luminances; vec4 lanes: x=NW, y=NE, z=SW, w=SE. */
    "    vec4 lb = vec4(dot(xbrPal(b), xbrRgbw), dot(xbrPal(d), xbrRgbw),\n"
    "                   dot(xbrPal(h), xbrRgbw), dot(xbrPal(f), xbrRgbw));\n"
    "    vec4 lc = vec4(dot(xbrPal(c), xbrRgbw), dot(xbrPal(a), xbrRgbw),\n"
    "                   dot(xbrPal(g), xbrRgbw), dot(xbrPal(i), xbrRgbw));\n"
    "    vec4 ld = lb.yzwx;\n"
    "    vec4 le = vec4(dot(xbrPal(eIdx), xbrRgbw));\n"
    "    vec4 lf = lb.wxyz;\n"
    "    vec4 lg = lc.zwxy;\n"
    "    vec4 lh = lb.zwxy;\n"
    "    vec4 li = lc.wxyz;\n"
    "    vec4 li4 = vec4(dot(xbrPal(i4), xbrRgbw), dot(xbrPal(c1), xbrRgbw),\n"
    "                    dot(xbrPal(a0), xbrRgbw), dot(xbrPal(g5), xbrRgbw));\n"
    "    vec4 li5 = vec4(dot(xbrPal(i5), xbrRgbw), dot(xbrPal(c4), xbrRgbw),\n"
    "                    dot(xbrPal(a1), xbrRgbw), dot(xbrPal(g0), xbrRgbw));\n"
    "    vec4 lh5 = vec4(dot(xbrPal(h5), xbrRgbw), dot(xbrPal(f4), xbrRgbw),\n"
    "                    dot(xbrPal(b1), xbrRgbw), dot(xbrPal(d0), xbrRgbw));\n"
    "    vec4 lf4 = lh5.yzwx;\n"
    "    vec4 fx   = xbrAo * fp.y + xbrBo * fp.x;\n"
    "    vec4 fx_l = xbrAx * fp.y + xbrBx * fp.x;\n"
    "    vec4 fx_u = xbrAy * fp.y + xbrBy * fp.x;\n"
    "    vec4 irlv0 = xbrDiff(le, lf) * xbrDiff(le, lh);\n"
    "    vec4 irlv1 = irlv0 * ( xbrNeq(lf, lb) * xbrNeq(lf, lc)\n"
    "                         + xbrNeq(lh, ld) * xbrNeq(lh, lg)\n"
    "                         + xbrEq(le, li) * (xbrNeq(lf, lf4) * xbrNeq(lf, li4)\n"
    "                                          + xbrNeq(lh, lh5) * xbrNeq(lh, li5))\n"
    "                         + xbrEq(le, lg) + xbrEq(le, lc) );\n"
    "    vec4 irlv2l = xbrDiff(le, lg) * xbrDiff(ld, lg);\n"
    "    vec4 irlv2u = xbrDiff(le, lc) * xbrDiff(lb, lc);\n"
    "    vec4 fx45i = clamp((fx   + xbrDelta  - xbrCo - xbrCi) / (2.0 * xbrDelta),  0.0, 1.0);\n"
    "    vec4 fx45  = clamp((fx   + xbrDelta  - xbrCo)         / (2.0 * xbrDelta),  0.0, 1.0);\n"
    "    vec4 fx30  = clamp((fx_l + xbrDeltaL - xbrCx)         / (2.0 * xbrDeltaL), 0.0, 1.0);\n"
    "    vec4 fx60  = clamp((fx_u + xbrDeltaU - xbrCy)         / (2.0 * xbrDeltaU), 0.0, 1.0);\n"
    "    vec4 wd1 = abs(le - lc) + abs(le - lg) + abs(li - lh5) + abs(li - lf4)\n"
    "             + 4.0 * abs(lh - lf);\n"
    "    vec4 wd2 = abs(lh - ld) + abs(lh - li5) + abs(lf - li4) + abs(lf - lb)\n"
    "             + 4.0 * abs(le - li);\n"
    "    vec4 edri = step(wd1, wd2) * irlv0;\n"
    "    vec4 edr  = step(wd1 + vec4(0.1), wd2) * step(vec4(0.5), irlv1);\n"
    "    vec4 edr_l = step(2.0 * abs(lf - lg), abs(lh - lc)) * irlv2l * edr;\n"
    "    vec4 edr_u = step(abs(lf - lg), 2.0 * abs(lh - lc)) * irlv2u * edr;\n"
    "    fx45  = edr   * fx45;\n"
    "    fx30  = edr_l * fx30;\n"
    "    fx60  = edr_u * fx60;\n"
    "    fx45i = edri  * fx45i;\n"
    "    vec4 px = step(abs(le - lf), abs(le - lh));\n"
    "    vec4 maximos = max(max(fx30, fx60), max(fx45, fx45i));\n"
    /* Blend the lit colours, not the palette colors the edges were weighed
       on — lighting stays a per-texel CLUT lookup. */
    "    vec3 cE = xbrLit(eIdx, gouraudRow, noClut);\n"
    "    vec3 cB = xbrLit(b, gouraudRow, noClut);\n"
    "    vec3 cD = xbrLit(d, gouraudRow, noClut);\n"
    "    vec3 cF = xbrLit(f, gouraudRow, noClut);\n"
    "    vec3 cH = xbrLit(h, gouraudRow, noClut);\n"
    "    vec3 res1 = cE;\n"
    "    res1 = mix(res1, mix(cH, cF, px.x), maximos.x);\n"
    "    res1 = mix(res1, mix(cB, cD, px.z), maximos.z);\n"
    "    vec3 res2 = cE;\n"
    "    res2 = mix(res2, mix(cF, cB, px.y), maximos.y);\n"
    "    res2 = mix(res2, mix(cD, cH, px.w), maximos.w);\n"
    "    return vec4(mix(res1, res2, step(xbrCDF(cE, res1), xbrCDF(cE, res2))), 1.0);\n"
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
    "    /* iu/iv are already in range; only the +1 neighbours wrap. */\n"
    "    float iu1 = iu + 1.0;\n"
    "    if (iu1 > widthMask) iu1 = 0.0;\n"
    "    float iv1 = iv + 1.0;\n"
    "    if (iv1 > heightMask) iv1 = 0.0;\n"
    "    vec2 p00 = vec2(iu, iv);\n"
    "    vec2 p10 = vec2(iu1, iv);\n"
    "    vec2 p01 = vec2(iu, iv1);\n"
    "    vec2 p11 = vec2(iu1, iv1);\n"
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
    /* Palette-resolved sprite filters: the sprite texture stores palette
       indices, so hardware filtering would blend indices.  Sample the
       indices, then resolve through uPalette. */
    "vec4 spriteScale2x(vec2 tc, float wm, float hm, float chroma) {\n"
    "    vec2 whm = vec2(wm + 1.0, hm + 1.0);\n"
    "    vec2 pC = floor(mod(tc, whm));\n"
    "    vec2 fp = fract(mod(tc, whm));\n"
    "    float e = xbrIndex(pC, whm);\n"
    "    if (chroma > 0.5 && e < 0.5) discard;\n"
    "    float b = spriteChromaIdx(xbrIndex(pC + vec2(0.0, -1.0), whm), e, chroma);\n"
    "    float d = spriteChromaIdx(xbrIndex(pC + vec2(-1.0, 0.0), whm), e, chroma);\n"
    "    float f = spriteChromaIdx(xbrIndex(pC + vec2(1.0, 0.0), whm), e, chroma);\n"
    "    float h = spriteChromaIdx(xbrIndex(pC + vec2(0.0, 1.0), whm), e, chroma);\n"
    "    float r = e;\n"
    "    if (b != h && d != f) {\n"
    "        if (fp.x < 0.5 && fp.y < 0.5) r = (d == b) ? d : e;\n"
    "        else if (fp.x >= 0.5 && fp.y < 0.5) r = (b == f) ? f : e;\n"
    "        else if (fp.x < 0.5) r = (d == h) ? d : e;\n"
    "        else r = (h == f) ? f : e;\n"
    "    }\n"
    "    return texture2D(uPalette, vec2((r + 0.5) / 256.0, 0.5));\n"
    "}\n"
    "vec4 spriteBilinear(vec2 tc, float wm, float hm, float chroma) {\n"
    "    vec2 whm = vec2(wm + 1.0, hm + 1.0);\n"
    "    vec2 uvi = floor(mod(tc, whm));\n"
    "    vec2 f = fract(mod(tc, whm));\n"
    "    float i0 = uvi.x, i1 = mod(i0 + 1.0, whm.x);\n"
    "    float j0 = uvi.y, j1 = mod(j0 + 1.0, whm.y);\n"
    "    float t00 = xbrIndex(vec2(i0, j0), whm);\n"
    "    if (chroma > 0.5 && t00 < 0.5) discard;\n"
    "    vec4 c00 = texture2D(uPalette, vec2((t00 + 0.5) / 256.0, 0.5));\n"
    "    float t10 = spriteChromaIdx(xbrIndex(vec2(i1, j0), whm), t00, chroma);\n"
    "    float t01 = spriteChromaIdx(xbrIndex(vec2(i0, j1), whm), t00, chroma);\n"
    "    float t11 = spriteChromaIdx(xbrIndex(vec2(i1, j1), whm), t00, chroma);\n"
    "    vec4 c10 = texture2D(uPalette, vec2((t10 + 0.5) / 256.0, 0.5));\n"
    "    vec4 c01 = texture2D(uPalette, vec2((t01 + 0.5) / 256.0, 0.5));\n"
    "    vec4 c11 = texture2D(uPalette, vec2((t11 + 0.5) / 256.0, 0.5));\n"
    "    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);\n"
    "}\n"
    "void main() {\n"
    "    float terrainInvW =\n"
    "        (uTerrainFarClip > 0.5 && vW > 0.0001) ? 1.0 / vW : 0.0;\n"
    "    if (uTerrainFarClip > 0.5) {\n"
    "        float dpt = vZ * terrainInvW;\n"
    "        if (dpt < uNearClip || dpt >= uTerrainFar) discard;\n"
    "    }\n"
    "    vec4 color;\n"
    "    float alphaOverride = 0.0;\n"
    /* Per-vertex material word, flat across the polygon. */
    "    float mFlags = mod(vMaterial, 256.0);\n"
    "    int mMode = int(mod(mFlags, 8.0) + 0.5);\n"
    /* alphaMode is bits 3-4 of the word: the mod-32 residue still holds
       the mode's low bits, so the /8 quotient is only integral when the
       alpha bits are actually set (e.g. mode 4 leaves residue 4 = 0.5).
       Round with a small epsilon, not 0.5, or mode 4 reads as alpha 1
       and the transparent mode (residue 12) rounds up to stipple 2. */
    "    float mAlpha = floor(mod(mFlags, 32.0) / 8.0 + 0.1);\n"
    "    float mChroma = floor(mod(mFlags, 64.0) / 32.0 + 0.5);\n"
    "    float mPersp = floor(mod(mFlags, 128.0) / 64.0 + 0.5);\n"
    "    float mColor = floor(vMaterial / 256.0);\n"
    /* Ordered-dither phase: a centred fraction of one CLUT row for the
       gouraud paths, and the raw 0..15 cell for the 16-row fog steps. */
    "    float ditherIndex = dither4x4(gl_FragCoord.xy);\n"
    "    float ditherPhase = ((ditherIndex + 0.5) / 16.0 - 0.5) * uDither;\n"
    /* Select perspective-corrected or affine texture/Z varyings.
       The persp bit is set for Z-textured types (16-24), where the SW
       rasterizer divides MapU/MapV/ZBuf by W = 1/Z; types 8-15
       (SW POLYTEXT/POLYGTEX) interpolate affinely. */
    "    vec2 tc = mix(vTexCoord, vTexCoordOverW / vInvW, mPersp);\n"
    "    float vZsel = mix(vZ, vZOverW / vInvW, mPersp);\n"
    "    if (uTerrainFarClip > 0.5) {\n"
    /* The static decor path can opt into its camera-space perspective
       varyings; the terrain path converts GL's perspective-correct
       interpolation back to the SW rasterizer's screen-linear mapping. */
    "        if (uTerrainPersp > 0.5) {\n"
    "            tc = vTexCoordOverW / vInvW;\n"
    "            vZsel = vZOverW / vInvW;\n"
    "        } else {\n"
    "            tc = vTexCoord * terrainInvW;\n"
    "            vZsel = vZ * terrainInvW;\n"
    "        }\n"
    "    }\n"
    /* Per-pixel stereo warp for the isometric brick layer.  The layer's own
       depth map says how far the thing painted at this pixel is, so the pixel
       takes the shift the polygon stage would give a point at that depth:
       fixed screen offset plus a 1/depth term.  Sampling the depth at the
       pixel's own (unshifted) texel is the point; sampling it at the shifted
       one would feed the offset back into itself. */
    "#ifdef LBA_LAYER_NO_WRAP\n"
    "    vec2 layerTexel = tc / 256.0;\n"
    "    if (uLayerWarp > 0.5) {\n"
    "        float dn = texture2D(uLayerDepth, layerTexel / uAtlasScale).r;\n"
    "        float az =\n"
    "            max(uLayerAzMin + dn * uLayerAzSpan + uLayerAzShift, 1.0);\n"
    "        tc =\n"
    "            (layerTexel - (uLayerShiftPx + uLayerShiftPerAz / az)) * 256.0;\n"
    "    }\n"
    /* The coverage mask is read at the same shifted texel the colour is, so
       the outline travels with the pixels it belongs to.  Reading it where
       the pixel is instead leaves the repainted wall behind by the eye offset
       in the near eye, and the model stands in front of it. */
    "    if (uLayerMaskOn > 0.5) {\n"
    "        vec2 muv = (tc / 256.0 - uLayerMaskRect.xy) * uLayerMaskRect.zw;\n"
    "        if (texture2D(uLayerMask, muv).r < 0.5) discard;\n"
    "    }\n"
    "#endif\n"
    "    if (mMode == 0) {\n"
    /* Textured: atlas → CLUT → palette */
    "        tc = tc / 256.0;\n"
    /* RepMask carries the wrap window in a byte pair, which caps a quad at
       256 texels.  The layer path is wider than that and must not wrap
       inside its own image, so it takes the window from the texture size. */
    "#ifdef LBA_LAYER_NO_WRAP\n"
    "        float widthMask = uAtlasScale.x - 1.0;\n"
    "        float heightMask = uAtlasScale.y - 1.0;\n"
    "#else\n"
    "        float widthMask = mod(vRepMask, 256.0);\n"
    "        float heightMask = floor(vRepMask / 256.0);\n"
    "#endif\n"
    /* SW init: Fill_CurGouraudMin = (Pt_Light << 8) + 0x8000.
        The +128 (0x80) rounding bias shifts the CLUT row by 1 in some
        cases — same bias applied in polyMode 2.  The CLUT base row rides
        in the material's high byte (see GL_RenderTriangleList): SetCLUT()
        changes it per terrain tile, and carrying it per vertex lets
        tiles share one draw call. */
    "        float gouraudRow =\n"
    "            floor((vLight + ditherPhase * 256.0 + 128.0) / 256.0) + mColor;\n"
    "        if (uBilinear == 4) {\n"
    "            color = clutXbrLite(tc, widthMask, heightMask, gouraudRow,\n"
    "                                mChroma, 0.0);\n"
    "        } else if (uBilinear == 3) {\n"
    "            color = clutXbr(tc, widthMask, heightMask, gouraudRow,\n"
    "                            mChroma, 0.0);\n"
    "        } else if (uBilinear == 2) {\n"
    "            color = clutScale2x(tc, widthMask, heightMask, gouraudRow,\n"
    "                                mChroma);\n"
    "        } else if (uBilinear != 0) {\n"
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
    "        float palIdx = mod(mColor +\n"
    "            floor((vLight + ditherPhase * 256.0 + 128.0) / 256.0), 256.0);\n"
    "        color = texture2D(uPalette,\n"
    "                 vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "    } else if (mMode == 4) {\n"
    /* Texture without CLUT (types 8 and 12):
       SW Filler_Texture / Filler_TextureChromaKey writes texel as palette
       index directly (*line = texel).  No CLUT lookup — the atlas texel
       IS the palette index.  Chroma key controlled by mChroma. */
    "        tc = tc / 256.0;\n"
    /* RepMask carries the wrap window in a byte pair, which caps a quad at
       256 texels.  The layer path is wider than that and must not wrap
       inside its own image, so it takes the window from the texture size. */
    "#ifdef LBA_LAYER_NO_WRAP\n"
    "        float widthMask = uAtlasScale.x - 1.0;\n"
    "        float heightMask = uAtlasScale.y - 1.0;\n"
    "#else\n"
    "        float widthMask = mod(vRepMask, 256.0);\n"
    "        float heightMask = floor(vRepMask / 256.0);\n"
    "#endif\n"
    "        float wrappedU = mod(tc.x, widthMask + 1.0);\n"
    "        float wrappedV = mod(tc.y, heightMask + 1.0);\n"
    "        if (uBilinear == 4) {\n"
    "            color = clutXbrLite(tc, widthMask, heightMask, 0.0, mChroma, 1.0);\n"
    "        } else if (uBilinear == 3) {\n"
    "            color = clutXbr(tc, widthMask, heightMask, 0.0, mChroma, 1.0);\n"
    "        } else if (uBilinear == 2) {\n"
    "            color = spriteScale2x(tc, widthMask, heightMask, mChroma);\n"
    "        } else if (uBilinear != 0) {\n"
    "            color = spriteBilinear(tc, widthMask, heightMask, mChroma);\n"
    "        } else {\n"
    "            vec2 atlasUV = vec2((floor(wrappedU) + 0.5) / uAtlasScale.x,\n"
    "                                (floor(wrappedV) + 0.5) / uAtlasScale.y);\n"
    "            float palIdx = texture2D(uAtlas, atlasUV).r * 255.0;\n"
    "            if (mChroma > 0.5 && palIdx < 0.5) discard;\n"
    "            color = texture2D(uPalette,\n"
    "                     vec2((palIdx + 0.5) / 256.0, 0.5));\n"
    "        }\n"
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
    /* RepMask carries the wrap window in a byte pair, which caps a quad at
       256 texels.  The layer path is wider than that and must not wrap
       inside its own image, so it takes the window from the texture size. */
    "#ifdef LBA_LAYER_NO_WRAP\n"
    "        float widthMask = uAtlasScale.x - 1.0;\n"
    "        float heightMask = uAtlasScale.y - 1.0;\n"
    "#else\n"
    "        float widthMask = mod(vRepMask, 256.0);\n"
    "        float heightMask = floor(vRepMask / 256.0);\n"
    "#endif\n"
    "        float wrappedU = mod(tc.x, widthMask + 1.0);\n"
    "        float wrappedV = mod(tc.y, heightMask + 1.0);\n"
    "        vec2 atlasUV = vec2((floor(wrappedU) + 0.5) / 256.0,\n"
    "                            (floor(wrappedV) + 0.5) / 256.0);\n"
    "        float texel = texture2D(uAtlas, atlasUV).r * 255.0;\n"
    /* Chroma key: type 24 is non-INCRUST (bit 2 clear), so index 0 is
       written unconditionally — no discard here. */
    "        float fogDist = max(vZsel - uScaledFogNear, 0.0);\n"
    "        float fogRow16 = min(floor(fogDist * uFogRowScale), 255.0);\n"
    /* The fog row is quantised to 16-row steps (SW masks the low nibble).
       Dither the discarded fraction into the next step, ordered, so the
       horizon keeps a smooth ramp instead of 16 bands. */
    "        float fogLevel = floor(fogRow16 / 16.0) * 16.0;\n"
    "        float fogFrac = fogRow16 - fogLevel;\n"
    "        float fogCell = (ditherIndex + 0.5 < fogFrac) ? 16.0 : 0.0;\n"
    "        float clutRow = 12.0 + fogLevel + fogCell * uDither;\n"
    "        clutRow = min(clutRow, 255.0);\n"
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
    "        float gouraudRow =\n"
    "            floor((vLight + ditherPhase * 256.0 + 128.0) / 256.0) + vRepMask;\n"
    "        vec2 clutUV = vec2((mColor + 0.5) / 256.0,\n"
    "                           (gouraudRow + 0.5) / 256.0);\n"
    "        color = texture2D(uCLUT, clutUV);\n"
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
static GLuint s_presentRectVbo = 0; /* arbitrary src/dst rect through the
                                       present shader (interior zoom) */
/* Left half of the scene texture.  Mono frames (menus, dialogue) render into
 * the left eye's half of the double-wide target; a mono present must sample
 * only that half or both eye images appear side by side in each eye. */
static GLuint s_fboHalfVbo = 0;
static GLuint s_texture = 0;
static GLint s_uTexLoc = -1;
static GLint s_uTexelLoc = -1;  /* present overlay: 1/width, 1/height */
static GLint s_uFilterLoc = -1; /* present overlay: 0 nearest, 1 bilinear, 2 Scale2x */
static GLint s_uSharpenLoc = -1;
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
/* Exterior stereo defers the 2D overlay until after the pair blit so it is
   composited at swapchain resolution instead of into the 1.5x scene target. */
static S32 s_overlayPending = 0;
static U32 s_overlayPendW = 0, s_overlayPendH = 0;
static const U32 *s_overlayPendPalette = NULL;
static S32 s_overlayPendFilter = 0;
static S32 s_overlayBBox[4] = {0, 0, 0, 0};
static S32 s_overlayBands[6][4];
static S32 s_overlayBandCount = 0;
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
    GLSL_FRAG_HEADER GLSL_IMAGE_ADJUST
    "uniform sampler2D uIndices;\n"
    "uniform sampler2D uPalette;\n"
    "uniform vec2 uTexel;\n"
    "varying vec2 vTexCoord;\n"
    "float idxAt(vec2 uv) {\n"
    "    return floor(texture2D(uIndices, uv).r * 255.0 + 0.5);\n"
    "}\n"
    "void main() {\n"
    /* NEAREST filtering makes v exactly i/255; rebuild the texel index and
     * step to the palette texel centre the same way the CLUT shader does.
     * The overlay itself is not upscaled: Scale2x/xBR are texture-level. */
    "    vec2 t = vTexCoord / uTexel;\n"
    "    vec2 c = (floor(t) + 0.5) * uTexel;\n"
    "    float e = idxAt(c);\n"
    /* Palette index 0 is transparent, so empty overlay pixels can skip the
       palette fetch and the blend; the rain/HUD leaves most of the screen
       empty. */
    "    if (e < 0.5) discard;\n"
    "    vec4 pal = texture2D(uPalette, vec2((e + 0.5) / 256.0, 0.5));\n"
    "    gl_FragColor = vec4(ImageAdjust(pal.rgb), pal.a);\n"
    "}\n";

/* ── FXAA (Fast Approximate Anti-Aliasing) ──────────────────────────────── */
/* Post-process edge-smoothing shader applied during the present pass. */

static const char *s_fxaaFragSrc =
    GLSL_FRAG_HEADER GLSL_IMAGE_ADJUST
    "uniform sampler2D uTex;\n"
    "uniform vec2 uTexel;\n"
    "uniform float uSharpen;\n"
    "varying vec2 vTexCoord;\n"
    /* Unsharp mask from the FXAA corner taps; applied after the edge blend so
       sharpening and FXAA can be used together. */
    "vec3 SharpenColor(vec3 c, vec3 nw, vec3 ne, vec3 sw, vec3 se) {\n"
    "    vec3 blur = (nw + ne + sw + se) * 0.25;\n"
    "    return c + (c - blur) * uSharpen;\n"
    "}\n"
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
    "    if (lumaMax - lumaMin <= 0.08) {\n"
    "        gl_FragColor = vec4(ImageAdjust(SharpenColor(\n"
    "            rgbM, rgbNW, rgbNE, rgbSW, rgbSE)), 1.0);\n"
    "        return;\n"
    "    }\n"
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
    "        gl_FragColor = vec4(ImageAdjust(SharpenColor(\n"
    "            rgbA, rgbNW, rgbNE, rgbSW, rgbSE)), 1.0);\n"
    "    else\n"
    "        gl_FragColor = vec4(ImageAdjust(SharpenColor(\n"
    "            rgbB, rgbNW, rgbNE, rgbSW, rgbSE)), 1.0);\n"
    "}\n";

static GLuint s_fxaaProgram = 0;
static GLint s_fxaa_uTex = -1;
static GLint s_fxaa_uTexel = -1;
static GLint s_fxaa_uSharpen = -1;
static GLint s_fxaa_aPos = -1;
static GLint s_fxaa_aTexCoord = -1;

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
static S32 s_arrCount; /* defined with the array manager below */
static U32 s_arrEvictions;
static U64 s_profAccumTicks[GL_PROF_BUCKET_COUNT] = {0};
static U32 s_profFrames = 0;
static U32 s_profFlushes = 0;      /* batch flushes (≈ CLUT-program draw calls) */
static U32 s_profTris = 0;         /* triangles submitted to the CLUT program */
static U32 s_profSpriteUps = 0;    /* gameplay sprite texture uploads */
static U32 s_profDepthSkips = 0;   /* depth readback consumes skipped (GPU behind) */
static U32 s_profOverlayEmpty = 0; /* presents whose 2D overlay had no content */
static U32 s_profOverlayReuse = 0; /* presents that reused eye 0's overlay scan */
static U32 s_profDirectScene = 0;  /* presents that can read the scene directly */
static U32 s_flushRsnAlpha = 0;    /* throwaway flush-reason attribution */
static U32 s_flushRsnFog = 0;
static U32 s_flushRsnAtlas = 0;
static U32 s_flushRsnZbuf = 0;
static U32 s_flushRsnNzw = 0;
static U32 s_profAtlasUploads = 0;     /* 2D page uploads */
static U32 s_profArrayUploads = 0;     /* array layer uploads */
static U32 s_profAtlasDirtyKnown = 0;  /* dirty events with a named page */
static U32 s_profAtlasDirtyGlobal = 0; /* dirty events forcing a full re-upload */

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
                 "rsn a=%.1f f=%.1f t=%.1f z=%.1f n=%.1f arr=%u/%u "
                 "atl=%u/%u dty=%u/%u",
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
                 s_arrCount, s_arrEvictions,
                 s_profAtlasUploads, s_profArrayUploads,
                 s_profAtlasDirtyKnown, s_profAtlasDirtyGlobal);
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
        s_profAtlasUploads = 0;
        s_profArrayUploads = 0;
        s_profAtlasDirtyKnown = 0;
        s_profAtlasDirtyGlobal = 0;
    }
}

/* ── GL state — CLUT shader ─────────────────────────────────────────────── */

static GLuint s_clutProgram = 0;
static GLuint s_clut3Program = 0; /* GLSL-3 variant sampling the layer array */
static GLint s_clut3_uAtlas = -1;
static GLint s_clut3_uCLUT = -1;
static GLint s_clut3_uPalette = -1;
static GLint s_clut3_uAtlasArr = -1;
static GLint s_clut3_uAtlasQuadArr = -1;
static GLint s_clut3_uAtlasQuadValid = -1;
static GLint s_clut3_uScreenScale = -1;
static GLint s_clut3_uAtlasScale = -1;
static GLint s_clut3_uCLUTBaseRow = -1;
static GLint s_clut3_uScaledFogNear = -1;
static GLint s_clut3_uFogRowScale = -1;
static GLint s_clut3_uBilinear = -1;
static GLint s_clut3_uTerrainFarClip = -1;
static GLint s_clut3_uTerrainPersp = -1;
static GLint s_clut3_uTerrainFar = -1;
static U8 s_batchArray = 0;   /* batch samples the layer array (RepMask 0x8000) */
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
    "uniform sampler2DArray uAtlasQuadArr;\n"
    "uniform int uAtlasQuadValid;\n"
    "float arrayXbrIndex(vec2 p, float layer) {\n"
    "    return texture(uAtlasArr, vec3((mod(p.x, 256.0) + 0.5) / 256.0,\n"
    "                                   (mod(p.y, 256.0) + 0.5) / 256.0,\n"
    "                                   layer)).r * 255.0;\n"
    "}\n"
    /* Scale2x on an array layer: same rules as clutScale2x, fetching the
       atlas neighbourhood from the layer instead of the 2D atlas. */
    "vec4 arrayScale2x(vec2 tc, float layer, float gouraudRow, float chroma) {\n"
    "    float u = mod(tc.x, 256.0);\n"
    "    float v = mod(tc.y, 256.0);\n"
    "    vec2 pC = vec2(floor(u), floor(v));\n"
    "    vec2 fp = vec2(u - pC.x, v - pC.y);\n"
    "    float e = arrayXbrIndex(pC, layer);\n"
    "    if (chroma > 0.5 && e < 0.5) discard;\n"
    "    float b = arrayXbrIndex(pC + vec2( 0.0, -1.0), layer);\n"
    "    float d = arrayXbrIndex(pC + vec2(-1.0,  0.0), layer);\n"
    "    float f = arrayXbrIndex(pC + vec2( 1.0,  0.0), layer);\n"
    "    float h = arrayXbrIndex(pC + vec2( 0.0,  1.0), layer);\n"
    "    float r = e;\n"
    "    if (b != h && d != f) {\n"
    "        if (fp.x < 0.5 && fp.y < 0.5) {\n"
    "            r = (d == b) ? d : e;\n"
    "        } else if (fp.x >= 0.5 && fp.y < 0.5) {\n"
    "            r = (b == f) ? f : e;\n"
    "        } else if (fp.x < 0.5) {\n"
    "            r = (d == h) ? d : e;\n"
    "        } else {\n"
    "            r = (h == f) ? f : e;\n"
    "        }\n"
    "    }\n"
    "    return vec4(xbrLit(r, gouraudRow, 0.0), 1.0);\n"
    "}\n"
    /* xBR on an array layer: same rules as clutXbr, fetching the atlas
       neighbourhood from the layer instead of the 2D atlas (full-page wrap,
       no sub-rect masks). */
    "vec4 arrayXbr(vec2 tc, float layer, float gouraudRow, float chroma) {\n"
    "    float u = mod(tc.x, 256.0);\n"
    "    float v = mod(tc.y, 256.0);\n"
    "    vec2 pC = vec2(floor(u), floor(v));\n"
    "    vec2 fp = vec2(u - pC.x, v - pC.y);\n"
    "    float eIdx = arrayXbrIndex(pC, layer);\n"
    "    if (chroma > 0.5 && eIdx < 0.5) discard;\n"
    "    float a1 = arrayXbrIndex(pC + vec2(-1.0, -2.0), layer);\n"
    "    float b1 = arrayXbrIndex(pC + vec2( 0.0, -2.0), layer);\n"
    "    float c1 = arrayXbrIndex(pC + vec2( 1.0, -2.0), layer);\n"
    "    float a  = arrayXbrIndex(pC + vec2(-1.0, -1.0), layer);\n"
    "    float b  = arrayXbrIndex(pC + vec2( 0.0, -1.0), layer);\n"
    "    float c  = arrayXbrIndex(pC + vec2( 1.0, -1.0), layer);\n"
    "    float d  = arrayXbrIndex(pC + vec2(-1.0,  0.0), layer);\n"
    "    float f  = arrayXbrIndex(pC + vec2( 1.0,  0.0), layer);\n"
    "    float g  = arrayXbrIndex(pC + vec2(-1.0,  1.0), layer);\n"
    "    float h  = arrayXbrIndex(pC + vec2( 0.0,  1.0), layer);\n"
    "    float i  = arrayXbrIndex(pC + vec2( 1.0,  1.0), layer);\n"
    "    float g5 = arrayXbrIndex(pC + vec2(-1.0,  2.0), layer);\n"
    "    float h5 = arrayXbrIndex(pC + vec2( 0.0,  2.0), layer);\n"
    "    float i5 = arrayXbrIndex(pC + vec2( 1.0,  2.0), layer);\n"
    "    float a0 = arrayXbrIndex(pC + vec2(-2.0, -1.0), layer);\n"
    "    float d0 = arrayXbrIndex(pC + vec2(-2.0,  0.0), layer);\n"
    "    float g0 = arrayXbrIndex(pC + vec2(-2.0,  1.0), layer);\n"
    "    float c4 = arrayXbrIndex(pC + vec2( 2.0, -1.0), layer);\n"
    "    float f4 = arrayXbrIndex(pC + vec2( 2.0,  0.0), layer);\n"
    "    float i4 = arrayXbrIndex(pC + vec2( 2.0,  1.0), layer);\n"
    "    vec4 lb = vec4(dot(xbrPal(b), xbrRgbw), dot(xbrPal(d), xbrRgbw),\n"
    "                   dot(xbrPal(h), xbrRgbw), dot(xbrPal(f), xbrRgbw));\n"
    "    vec4 lc = vec4(dot(xbrPal(c), xbrRgbw), dot(xbrPal(a), xbrRgbw),\n"
    "                   dot(xbrPal(g), xbrRgbw), dot(xbrPal(i), xbrRgbw));\n"
    "    vec4 ld = lb.yzwx;\n"
    "    vec4 le = vec4(dot(xbrPal(eIdx), xbrRgbw));\n"
    "    vec4 lf = lb.wxyz;\n"
    "    vec4 lg = lc.zwxy;\n"
    "    vec4 lh = lb.zwxy;\n"
    "    vec4 li = lc.wxyz;\n"
    "    vec4 li4 = vec4(dot(xbrPal(i4), xbrRgbw), dot(xbrPal(c1), xbrRgbw),\n"
    "                    dot(xbrPal(a0), xbrRgbw), dot(xbrPal(g5), xbrRgbw));\n"
    "    vec4 li5 = vec4(dot(xbrPal(i5), xbrRgbw), dot(xbrPal(c4), xbrRgbw),\n"
    "                    dot(xbrPal(a1), xbrRgbw), dot(xbrPal(g0), xbrRgbw));\n"
    "    vec4 lh5 = vec4(dot(xbrPal(h5), xbrRgbw), dot(xbrPal(f4), xbrRgbw),\n"
    "                    dot(xbrPal(b1), xbrRgbw), dot(xbrPal(d0), xbrRgbw));\n"
    "    vec4 lf4 = lh5.yzwx;\n"
    "    vec4 fx   = xbrAo * fp.y + xbrBo * fp.x;\n"
    "    vec4 fx_l = xbrAx * fp.y + xbrBx * fp.x;\n"
    "    vec4 fx_u = xbrAy * fp.y + xbrBy * fp.x;\n"
    "    vec4 irlv0 = xbrDiff(le, lf) * xbrDiff(le, lh);\n"
    "    vec4 irlv1 = irlv0 * ( xbrNeq(lf, lb) * xbrNeq(lf, lc)\n"
    "                         + xbrNeq(lh, ld) * xbrNeq(lh, lg)\n"
    "                         + xbrEq(le, li) * (xbrNeq(lf, lf4) * xbrNeq(lf, li4)\n"
    "                                          + xbrNeq(lh, lh5) * xbrNeq(lh, li5))\n"
    "                         + xbrEq(le, lg) + xbrEq(le, lc) );\n"
    "    vec4 irlv2l = xbrDiff(le, lg) * xbrDiff(ld, lg);\n"
    "    vec4 irlv2u = xbrDiff(le, lc) * xbrDiff(lb, lc);\n"
    "    vec4 fx45i = clamp((fx   + xbrDelta  - xbrCo - xbrCi) / (2.0 * xbrDelta),  0.0, 1.0);\n"
    "    vec4 fx45  = clamp((fx   + xbrDelta  - xbrCo)         / (2.0 * xbrDelta),  0.0, 1.0);\n"
    "    vec4 fx30  = clamp((fx_l + xbrDeltaL - xbrCx)         / (2.0 * xbrDeltaL), 0.0, 1.0);\n"
    "    vec4 fx60  = clamp((fx_u + xbrDeltaU - xbrCy)         / (2.0 * xbrDeltaU), 0.0, 1.0);\n"
    "    vec4 wd1 = abs(le - lc) + abs(le - lg) + abs(li - lh5) + abs(li - lf4)\n"
    "             + 4.0 * abs(lh - lf);\n"
    "    vec4 wd2 = abs(lh - ld) + abs(lh - li5) + abs(lf - li4) + abs(lf - lb)\n"
    "             + 4.0 * abs(le - li);\n"
    "    vec4 edri = step(wd1, wd2) * irlv0;\n"
    "    vec4 edr  = step(wd1 + vec4(0.1), wd2) * step(vec4(0.5), irlv1);\n"
    "    vec4 edr_l = step(2.0 * abs(lf - lg), abs(lh - lc)) * irlv2l * edr;\n"
    "    vec4 edr_u = step(abs(lf - lg), 2.0 * abs(lh - lc)) * irlv2u * edr;\n"
    "    fx45  = edr   * fx45;\n"
    "    fx30  = edr_l * fx30;\n"
    "    fx60  = edr_u * fx60;\n"
    "    fx45i = edri  * fx45i;\n"
    "    vec4 px = step(abs(le - lf), abs(le - lh));\n"
    "    vec4 maximos = max(max(fx30, fx60), max(fx45, fx45i));\n"
    "    vec3 cE = xbrLit(eIdx, gouraudRow, 0.0);\n"
    "    vec3 cB = xbrLit(b, gouraudRow, 0.0);\n"
    "    vec3 cD = xbrLit(d, gouraudRow, 0.0);\n"
    "    vec3 cF = xbrLit(f, gouraudRow, 0.0);\n"
    "    vec3 cH = xbrLit(h, gouraudRow, 0.0);\n"
    "    vec3 res1 = cE;\n"
    "    res1 = mix(res1, mix(cH, cF, px.x), maximos.x);\n"
    "    res1 = mix(res1, mix(cB, cD, px.z), maximos.z);\n"
    "    vec3 res2 = cE;\n"
    "    res2 = mix(res2, mix(cF, cB, px.y), maximos.y);\n"
    "    res2 = mix(res2, mix(cD, cH, px.w), maximos.w);\n"
    "    return vec4(mix(res1, res2, step(xbrCDF(cE, res1), xbrCDF(cE, res2))), 1.0);\n"
    "}\n"
    /* xBR Lite on an array layer: same five-tap cardinal blend as
       clutXbrLite, fetching the neighbourhood from the layer. */
    "vec4 arrayXbrLite(vec2 tc, float layer, float gouraudRow, float chroma) {\n"
    "    float u = mod(tc.x, 256.0);\n"
    "    float v = mod(tc.y, 256.0);\n"
    "    vec2 pC = vec2(floor(u), floor(v));\n"
    "    vec2 fp = vec2(u - pC.x, v - pC.y);\n"
    "    float e = arrayXbrIndex(pC, layer);\n"
    "    if (chroma > 0.5 && e < 0.5) discard;\n"
    "    float b = arrayXbrIndex(pC + vec2( 0.0, -1.0), layer);\n"
    "    float d = arrayXbrIndex(pC + vec2(-1.0,  0.0), layer);\n"
    "    float f = arrayXbrIndex(pC + vec2( 1.0,  0.0), layer);\n"
    "    float h = arrayXbrIndex(pC + vec2( 0.0,  1.0), layer);\n"
    "    if (chroma > 0.5) {\n"
    "        b = spriteChromaIdx(b, e, chroma);\n"
    "        d = spriteChromaIdx(d, e, chroma);\n"
    "        f = spriteChromaIdx(f, e, chroma);\n"
    "        h = spriteChromaIdx(h, e, chroma);\n"
    "    }\n"
    "    vec3 cE = xbrLit(e, gouraudRow, 0.0);\n"
    "    if (b == e && d == e && f == e && h == e)\n"
    "        return vec4(cE, 1.0);\n"
    "    vec3 cD = xbrLit(d, gouraudRow, 0.0);\n"
    "    vec3 cF = xbrLit(f, gouraudRow, 0.0);\n"
    "    vec3 c = cE;\n"
    "    float wl = max(1.0 - 2.0 * fp.x, 0.0);\n"
    "    float wr = max(2.0 * fp.x - 1.0, 0.0);\n"
    "    float wt = max(1.0 - 2.0 * fp.y, 0.0);\n"
    "    float wb = max(2.0 * fp.y - 1.0, 0.0);\n"
    "    if (d == b) c = mix(c, cD, wl * wt);\n"
    "    if (b == f) c = mix(c, cF, wr * wt);\n"
    "    if (d == h) c = mix(c, cD, wl * wb);\n"
    "    if (h == f) c = mix(c, cF, wr * wb);\n"
    "    return vec4(c, 1.0);\n"
    "}\n"
    "vec4 arraySample(vec2 tc, float rmask, float light, float mcolor,\n"
    "                 float chroma, float bilin) {\n"
    "    float layer = rmask - 32768.0;\n"
    "    float gouraudRow = floor((light + 128.0) / 256.0) + mcolor;\n"
    /* tc rides the vertex attribute's 8.8 fixed point (texel*256); fold
       through 65536 first so the division lands in [0,256) texel units. */
    "    float wu = mod(tc.x, 65536.0) / 256.0;\n"
    "    float wv = mod(tc.y, 65536.0) / 256.0;\n"
    "    if (bilin > 3.5) {\n"
    "        return arrayXbrLite(vec2(wu, wv), layer, gouraudRow, chroma);\n"
    "    }\n"
    "    if (bilin > 2.5) {\n"
    "        return arrayXbr(vec2(wu, wv), layer, gouraudRow, chroma);\n"
    "    }\n"
    "    if (bilin > 1.5) {\n"
    "        return arrayScale2x(vec2(wu, wv), layer, gouraudRow, chroma);\n"
    "    }\n"
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
    "    float t00, t10, t01, t11;\n"
    /* The quad layer stores the 2x2 neighbourhood in R,G,B,A, so the
       common bilinear case is one atlas fetch plus four CLUT lookups. */
    "    if (uAtlasQuadValid != 0) {\n"
    "        vec4 q = texture(uAtlasQuadArr, vec3((iu + 0.5) / 256.0,\n"
    "                                             (iv + 0.5) / 256.0, layer));\n"
    "        t00 = q.r * 255.0;\n"
    "        t10 = q.g * 255.0;\n"
    "        t01 = q.b * 255.0;\n"
    "        t11 = q.a * 255.0;\n"
    "    } else {\n"
    "        float iu1 = iu + 1.0;\n"
    "        if (iu1 > 255.0) iu1 = 0.0;\n"
    "        float iv1 = iv + 1.0;\n"
    "        if (iv1 > 255.0) iv1 = 0.0;\n"
    "        vec3 p00 = vec3((iu + 0.5) / 256.0,\n"
    "                        (iv + 0.5) / 256.0, layer);\n"
    "        vec3 p10 = vec3((iu1 + 0.5) / 256.0, p00.y, layer);\n"
    "        vec3 p01 = vec3(p00.x, (iv1 + 0.5) / 256.0, layer);\n"
    "        vec3 p11 = vec3(p10.x, p01.y, layer);\n"
    "        t00 = texture(uAtlasArr, p00).r * 255.0;\n"
    "        t10 = texture(uAtlasArr, p10).r * 255.0;\n"
    "        t01 = texture(uAtlasArr, p01).r * 255.0;\n"
    "        t11 = texture(uAtlasArr, p11).r * 255.0;\n"
    "    }\n"
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

/* Add the resolved terrain table sampler and the one-fetch branch to the
   transformed CLUT fragment source.  The branch replaces the bilinear
   CLUT call only when the material is not an animated tile and not a
   chroma-keyed INCRUST type; everything else keeps the exact legacy path. */
static char *GL_InjectTerrainResolved(char *src) {
    const char *uniformAnchor = strstr(src, "uniform sampler2D uPalette;");
    const char *bilinearAnchor =
        strstr(src, "        } else if (uBilinear != 0) {");
    if (uniformAnchor == NULL || bilinearAnchor == NULL) {
        Log_Warn("[GL] terrain resolved anchors missing; using the 8-tap path");
        return src;
    }
    const char *uniformEnd = strchr(uniformAnchor, '\n');
    if (uniformEnd == NULL) {
        return src;
    }
    size_t prefix = (size_t)(uniformEnd - src) + 1;
    size_t bilinearPos = (size_t)(bilinearAnchor - src);
    const char *uniforms =
        "uniform sampler2DArray uResolved;\n"
        "uniform int uResolvedValid;\n";
    const char *branch =
        "        } else if (uResolvedValid != 0 && mChroma < 0.5 &&\n"
        "            floor(mod(mFlags, 256.0) / 128.0 + 0.5) < 0.5) {\n"
        "            float ru = mod(tc.x, widthMask + 1.0) / 256.0;\n"
        "            float rv = mod(tc.y, heightMask + 1.0) / 256.0;\n"
        "            color = texture(uResolved, vec3(ru, rv, gouraudRow));\n"
        "        } else if (uBilinear != 0) {\n";
    const char *bilinearLine = "        } else if (uBilinear != 0) {\n";
    size_t cap = strlen(src) + strlen(uniforms) + strlen(branch) + 2;
    char *out = (char *)malloc(cap);
    if (out == NULL) {
        return src;
    }
    char *w = out;
    memcpy(w, src, prefix);
    w += prefix;
    memcpy(w, uniforms, strlen(uniforms));
    w += strlen(uniforms);
    memcpy(w, src + prefix, bilinearPos - prefix);
    w += bilinearPos - prefix;
    memcpy(w, branch, strlen(branch));
    w += strlen(branch);
    memcpy(w, src + bilinearPos + strlen(bilinearLine),
           strlen(src + bilinearPos + strlen(bilinearLine)) + 1);
    free(src);
    return out;
}

static GLint s_clut_aPos = -1;
static GLint s_clut_aTexCoord = -1;
static GLint s_clut_aLight = -1;
static GLint s_clut_aZ = -1;
static GLint s_clut_aRepMask = -1;
static GLint s_clut_aMaterial = -1;
static GLint s_clut_uSinglePass = -1;
static GLint s_clut_uEye = -1;
static GLint s_clut_uEyeWorld = -1;
static GLint s_clut_uEyeProj = -1;
static GLint s_clut_uBaseWorld = -1;
static GLint s_clut_uBaseProj = -1;
static GLint s_clut_uEyeFocal = -1;
static GLint s_clut_uEyeZScale = -1;
/* The array program is linked separately and some drivers assign its
   attribute slots independently of the legacy program; the vertex-array
   spec must name the slots of whichever program is current. */
static GLint s_clut3_aPos = -1;
static GLint s_clut3_aTexCoord = -1;
static GLint s_clut3_aLight = -1;
static GLint s_clut3_aZ = -1;
static GLint s_clut3_aRepMask = -1;
static GLint s_clut3_aMaterial = -1;
static GLint s_clut3_uSinglePass = -1;
static GLint s_clut3_uEye = -1;
static GLint s_clut3_uEyeWorld = -1;
static GLint s_clut3_uEyeProj = -1;
static GLint s_clut3_uBaseWorld = -1;
static GLint s_clut3_uBaseProj = -1;
static GLint s_clut3_uEyeFocal = -1;
static GLint s_clut3_uEyeZScale = -1;
static GLint s_clut3_uDither = -1;
static GLint s_clut_uAtlas = -1;
static GLint s_clut_uCLUT = -1;
static GLint s_clut_uPalette = -1;
static GLint s_clut_uScreenScale = -1;
static GLint s_clut_uAtlasScale = -1;
static GLint s_clut_uCLUTBaseRow = -1;
static GLint s_clut_uScaledFogNear = -1;
static GLint s_clut_uFogRowScale = -1;
static GLint s_clut_uBilinear = -1;
static GLint s_clut_uDither = -1;
static GLint s_clut_uTerrainFarClip = -1;
static GLint s_clut_uTerrainPersp = -1;
static GLint s_clut_uTerrainFar = -1;

/* ── GL state — static terrain shader ───────────────────────────────────── */
/* The terrain program shares the batch fragment shader (same CLUT, palette,
   alpha and chroma semantics) but projects cube-local world vertices itself,
   so a whole horizon cube is one VBO and one draw call. */
static GLuint s_terrainProgram = 0;
static GLint s_terrain_aPos = -1;
static GLint s_terrain_aTexCoord = -1;
static GLint s_terrain_aLight = -1;
static GLint s_terrain_aRepMask = -1;
static GLint s_terrain_aMaterial = -1;
static GLint s_terrain_aFogPos = -1;
static GLint s_terrain_uSinglePass = -1;
static GLint s_terrain_uEye = -1;
static GLint s_terrain_uEyeWorld = -1;
static GLint s_terrain_uEyeProj = -1;
static GLint s_terrain_uRot = -1;
static GLint s_terrain_uCamera = -1;
static GLint s_terrain_uFocal = -1;
static GLint s_terrain_uCentre = -1;
static GLint s_terrain_uProjShiftX = -1;
static GLint s_terrain_uScreenScale = -1;
static GLint s_terrain_uZBufferFactor = -1;
static GLint s_terrain_uNearClip = -1;
static GLint s_terrain_uStartZFog = -1;
static GLint s_terrain_uFogStepScale = -1;
static GLint s_terrain_uPalLevel = -1;
static GLint s_terrain_uAtlas = -1;
static GLint s_terrain_uCLUT = -1;
static GLint s_terrain_uPalette = -1;
static GLint s_terrain_uAtlasScale = -1;
static GLint s_terrain_uBilinear = -1;
static GLint s_terrain_uDither = -1;
static GLint s_terrain_uTerrainFarClip = -1;
static GLint s_terrain_uTerrainPersp = -1;
static GLint s_terrain_uTerrainFar = -1;
static GLint s_terrain_uResolved = -1;
static GLint s_terrain_uResolvedValid = -1;

/* One-fetch terrain CLUT: a 256-layer RGBA texture-array holds, for each
   gouraud row, the full 256x256 atlas page resolved through the palette.
   The terrain's RepMask is 0xFFFF, so the page wrap is the full 256-page
   repeat and needs no per-subrect tiling.  Animated sea/flower halves and
   chroma-keyed halves stay on the multi-tap path via the material bit. */
static GLuint s_terrainResolvedTex = 0;
static GLuint s_boundTerrainResolvedTex = 0;
static U32 s_terrainResolvedData[256 * 256 * 256];
static const void *s_terrainResolvedPage = NULL;
static U32 s_terrainResolvedCLUTGen = 0;
static U32 s_terrainResolvedPalGen = 0;
static S32 s_terrainResolvedValid = 0;
static S32 s_terrainResolvedDirty = 1;
static U32 s_terrainEpoch = 1; /* advances on context loss; caches drop stale */
static GLuint s_terrainBoundVBO = 0;

static GLuint s_layerProgram = 0;
static GLint s_layer_aPos = -1;
static GLint s_layer_aUV = -1;
static GLint s_layer_aMaterial = -1;
static GLint s_layer_uScreenScale = -1;
static GLint s_layer_uAtlasScale = -1;
static GLint s_layer_uBilinear = -1;
static GLint s_layer_uSinglePass = -1;
static GLint s_layer_uEye = -1;
static GLint s_layer_uDepth = -1;
static GLint s_layer_uWarp = -1;
static GLint s_layer_uAzMin = -1;
static GLint s_layer_uAzSpan = -1;
static GLint s_layer_uAzShift = -1;
static GLint s_layer_uMask = -1;
static GLint s_layer_uMaskOn = -1;
static GLint s_layer_uMaskRect = -1;
static GLuint s_layerMaskTex = 0;
static S32 s_layerMaskW = 0;
static S32 s_layerMaskH = 0;
static GLint s_layer_uShiftPx = -1;
static GLint s_layer_uShiftPerAz = -1;
typedef struct {
    GLuint tex;
    GLuint depthTex;
    S32 w;
    S32 h;
    U32 gen;
    S32 hasGen;
} T_LAYER_SLOT;
static T_LAYER_SLOT s_layerSlots[GL_LAYER_SLOTS];
static char *GL_MakeLayerFragSrc(void);

/* ── GL state — static decor program ─────────────────────────────────────── */
static GLuint s_decorProgram = 0;
static GLint s_decor_aPos = -1;
static GLint s_decor_aNormal = -1;
static GLint s_decor_aTexCoord = -1;
static GLint s_decor_aMaterial = -1;
static GLint s_decor_aFlat = -1;
static GLint s_decor_aLayer = -1;
static GLint s_decor_uSinglePass = -1;
static GLint s_decor_uEye = -1;
static GLint s_decor_uEyeWorld = -1;
static GLint s_decor_uEyeProj = -1;
static GLint s_decor_uBaseWorld = -1;
static GLint s_decor_uBaseProj = -1;
static GLint s_decor_uRot = -1;
static GLint s_decor_uCamera = -1;
static GLint s_decor_uDecorPos = -1;
static GLint s_decor_uFocal = -1;
static GLint s_decor_uCentre = -1;
static GLint s_decor_uProjShiftX = -1;
static GLint s_decor_uScreenScale = -1;
static GLint s_decor_uZBufferFactor = -1;
static GLint s_decor_uNearClip = -1;
static GLint s_decor_uFogRow = -1;
static GLint s_decor_uLight = -1;
static GLint s_decor_uLightFactor = -1;
static GLint s_decor_uAtlas = -1;
static GLint s_decor_uCLUT = -1;
static GLint s_decor_uPalette = -1;
static GLint s_decor_uAtlasArr = -1;
static GLint s_decor_uAtlasScale = -1;
static GLint s_decor_uBilinear = -1;
static GLint s_decor_uDither = -1;
static GLint s_decor_uTerrainFarClip = -1;
static GLint s_decor_uTerrainPersp = -1;
static GLint s_decor_uTerrainFar = -1;

/* ── GL state — textures ────────────────────────────────────────────────── */

static GLuint s_atlasTex = 0;   /* 256×256 LUMINANCE */
static GLuint s_clutTex = 0;    /* 256×256 RGBA (baked CLUT+palette) */
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
static U32 s_batchRingSize = 0;    /* bytes */
static GLsync s_ringWrapFence = 0; /* signals the GPU drained the last ring generation */
static U32 s_batchRingOffset = 0;  /* rotating upload offset into the ring */
static U32 s_batchRingBase = 0;    /* ring offset of the batch being flushed */
static U32 s_batchRingOff = 0;     /* next slot's byte offset */
static U32 s_batchRingSlot = 0;    /* current flush's byte offset */
static GLuint s_auxVBO = 0;        /* one-shot draws: shadow/sphere/shade/sprite */

/* Decor batch: the decor pass's screen-space triangles are collected here
   and drawn as opaque depth-writing, opaque depth-test-only, and alpha
   streams, instead of flushing the shared batch whenever a decor changes
   atlas class, alpha or depth-write mode. */
#define DECOR_BATCH_MAX_VERTS (128 * 1024)
static U8 *s_decorVerts[3] = {NULL, NULL, NULL};
static U32 s_decorVertCount[3] = {0, 0, 0};
static U32 s_decorVertCap[3] = {0, 0, 0};
/* Non-array decor polygons keep the legacy CLUT program and its ordering
   relative to the static decor VBOs; they still batch instead of going
   through the shared per-state path. */
static U8 *s_decorLegacyVerts[5] = {NULL, NULL, NULL, NULL, NULL};
static U32 s_decorLegacyCount[5] = {0, 0, 0, 0, 0};
static U32 s_decorLegacyCap[5] = {0, 0, 0, 0, 0};
static const void *s_decorLegacyPage[5] = {NULL, NULL, NULL, NULL, NULL};
static U16 s_decorLegacyMask[5] = {0, 0, 0, 0, 0};
static S32 s_decorBatch = 0;
static GLuint s_decorVBO = 0;
static void GL_DrawDecorLegacyStreams(void);

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
                ? 1
                : 0;
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
static S32 s_fboDirtyActive = 0;                 /* 1 while a snapshot is current */

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
    if (x0 < s_fboDirtyX0)
        s_fboDirtyX0 = x0;
    if (y0 < s_fboDirtyY0)
        s_fboDirtyY0 = y0;
    if (x1 > s_fboDirtyX1)
        s_fboDirtyX1 = x1;
    if (y1 > s_fboDirtyY1)
        s_fboDirtyY1 = y1;
}
static U16 s_currentMatFlags = 0; /* flags only — the flush trigger */
static U8 s_batchTextured = 0;    /* batch samples the atlas page */

/* ── Cached GL object state ──────────────────────────────────────────────── */
/* The flush path runs once per draw call; on GLES every redundant driver
   call costs real time on slow CPUs.  These track what the context already
   has so repeat flushes skip re-binding and re-specifying.  Anything that
   touches textures/programs/attribs outside the flush path (present quads,
   sprite/shadow/shade draws) invalidates through the sentinels below. */
static GLuint s_curBoundTex[GL_TEX_UNIT_CACHE] = {
    (GLuint)-1, (GLuint)-1, (GLuint)-1, (GLuint)-1,
    (GLuint)-1, (GLuint)-1, (GLuint)-1, (GLuint)-1};
static S32 s_curActiveUnit = -1;
static GLuint s_curProgram = (GLuint)-1;
static S32 s_clutAttribsDirty = 1;      /* vertex arrays need enable + re-spec */
static GLintptr s_clutAttribsBase = -1; /* ring offset the pointers point at */
static GLuint s_clutAttribsProgram = 0; /* program whose slots the pointers use */
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

/* Track the draw-framebuffer binding so the batch flush can skip the same
   scene bind hundreds of times per frame.  Every bind in this file goes
   through the wrapper below; the value is reset to "unknown" whenever another
   path may have changed the context state. */
static GLuint s_boundDrawFbo = 0;

static void GL_InvalidateGLState(void) {
    for (S32 u = 0; u < GL_TEX_UNIT_CACHE; u++)
        s_curBoundTex[u] = (GLuint)-1;
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

/* Define after the state reset and before the first bind in this file.  The
   wrapper body calls the real GL entry point; the macro covers every later
   call, including the one-shot snapshot, horizon and present paths. */
static void GL_BindFramebufferTracked(GLenum target, GLuint fbo) {
    glBindFramebuffer(target, fbo);
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
        s_boundDrawFbo = fbo;
    }
}
#define glBindFramebuffer GL_BindFramebufferTracked

/* Deleting the bound framebuffer silently rebinds zero, so the tracked ID
   is no longer trustworthy after a teardown. */
static void GL_ForgetFboBinding(void) {
    s_boundDrawFbo = (GLuint)-1;
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

static GLfloat s_ussX = -1.0f, s_ussY = -1.0f; /* uScreenScale */
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
static const void *s_batchAtlas = NULL; /* PtrMap at batch start */
static U16 s_batchMaxMask = 0;          /* OR of every RepMask queued for this batch */
static U32 s_currentTypePoly = 0;
static U32 s_currentColorPoly = 0;
static S32 s_bilinearCLUT = 0;                /* bilinear CLUT sampling flag for 3D */
static GLfloat s_batchCLUTBaseRow = 0.0f;     /* CLUT row offset captured at batch start */
static GLfloat s_batchScaledFogNear = 0.0f;   /* Fill_ScaledFogNear captured at batch start */
static GLfloat s_batchFogRowScale = 0.0f;     /* Fill_Fog_Factor / 65536 captured at batch start */
static const void *s_batchCLUTGouraud = NULL; /* PtrCLUTGouraud at batch start */
static U8 s_batchZBuffer = 0;                 /* Fill_Flag_ZBuffer captured at batch start */
static S32 s_glWorld = 0;                     /* GL renders the whole scene world */
static S32 s_batchWorld = 0;                  /* s_glWorld captured at batch start */
static S32 s_deferWorld = 0;                  /* s_batchWorld in the NZW defer pool */
static U8 s_batchNZW = 0;                     /* Fill_Flag_NZW captured at batch start */

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
    S32 gw = s_batchWorld;
    s_batchWorld = s_deferWorld;
    s_deferWorld = gw;
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
/* VR supersampling multiplier applied on top of s_renderQuality.  Integer
   RenderQuality only reaches 2x/4x; the headset row offers 1.25x/1.5x, so the
   scale is a fraction.  The scene target grows and the present pass
   downsamples it. */
static U32 s_ssNum = 1;
static U32 s_ssDen = 1;

/* Base render dimension -> scene-target dimension, combining the desktop
   RenderQuality factor and the VR supersampling fraction.  Rounds to nearest
   so odd dirty-box edges do not drift. */
static U32 GL_ScaleRenderDim(U32 v) {
    const U64 scaled = (U64)v * (U64)s_renderQuality * (U64)s_ssNum;
    return (U32)((scaled + (U64)(s_ssDen / 2)) / (U64)s_ssDen);
}

/* ── Multiview capability probe ─────────────────────────────────────────── */
/* GL_OVR_multiview2 is the only layered way to render both eyes in one
   scene pass on the Quest.  Probe the extension, the entry point and a
   two-layer FBO before the layered path is enabled; the active single-pass
   path uses the existing double-wide target instead. */
#ifndef GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR 0x9630
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR 0x9632
#endif
typedef void (*PFN_GLFRAMEBUFFERTEXTUREMULTIVIEWOVR)(GLenum, GLenum, GLuint,
                                                     GLint, GLint, GLsizei);
static PFN_GLFRAMEBUFFERTEXTUREMULTIVIEWOVR
    s_glFramebufferTextureMultiviewOVR = NULL;
static S32 s_multiviewAvailable = 0;

S32 GL_MultiviewAvailable(void) { return s_multiviewAvailable; }

static void GL_ProbeMultiview(U32 w, U32 h) {
    s_multiviewAvailable = 0;
    const char *exts = (const char *)glGetString(GL_EXTENSIONS);
    const S32 listed =
        (exts != NULL && (strstr(exts, "GL_OVR_multiview2") != NULL ||
                          strstr(exts, "GL_OVR_multiview") != NULL));
    s_glFramebufferTextureMultiviewOVR =
        (PFN_GLFRAMEBUFFERTEXTUREMULTIVIEWOVR)SDL_GL_GetProcAddress(
            "glFramebufferTextureMultiviewOVR");
    if (!listed || s_glFramebufferTextureMultiviewOVR == NULL) {
        Log_Info("[GL] multiview unavailable (extension=%d entry=%d)",
                 listed ? 1 : 0,
                 s_glFramebufferTextureMultiviewOVR != NULL ? 1 : 0);
        return;
    }
    if (w == 0 || h == 0) {
        return;
    }

    while (glGetError() != GL_NO_ERROR) {
    }
    GLuint color = 0;
    GLuint depth = 0;
    GLuint fbo = 0;
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D_ARRAY, color);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 2,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depth);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, (GLsizei)w,
                 (GLsizei)h, 2, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    s_glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       color, 0, 0, 2);
    s_glFramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                       depth, 0, 0, 2);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    const GLenum err = glGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status == GL_FRAMEBUFFER_COMPLETE && err == GL_NO_ERROR) {
        s_multiviewAvailable = 1;
        Log_Info("[GL] Multiview layered FBO probe %ux%u x2 OK", (unsigned)w,
                 (unsigned)h);
    } else {
        Log_Warn("[GL] Multiview layered FBO probe failed "
                 "(status=0x%x err=0x%x)",
                 (unsigned)status, (unsigned)err);
    }
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &depth);
    glDeleteTextures(1, &color);
}
static S32 s_antiAlias = 0; /* 0=off, 1=FXAA, 2=MSAA×2, 3=MSAA×4 */
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
static S32 s_singlePassStereo = 0;
static S32 s_stereoEye = -1;
static void GL_SetStereoTargetMode(S32 doubleWide);
static S32 GL_StereoDoubleWideAllowed(void);

/* Opt-in direct-to-swapchain stereo: a fresh FBO attaches the runtime's
 * acquired swapchain texture with EXT_multisampled_render_to_texture, so both
 * eyes and the HUD render into the presented image and the resolve/blit pair
 * disappears.  The FBO is per frame because a persistent attachment on a
 * runtime-owned texture survives acquire/release as orphaned memory on Adreno
 * (the same failure the present blit avoids). */
static GLuint s_directFbo = 0;
static S32 s_directActive = 0;
static S32 s_directUnsupported = 0;
static S32 s_directFailLogged = 0;
static S32 s_directLogOnce = 0;

static void GL_SetSceneViewport(void) {
    if (s_singlePassStereo && s_stereoEye == 0) {
        glViewport(0, 0, (GLsizei)s_sceneTargetW, (GLsizei)s_sceneTargetH);
        return;
    }
    glViewport(s_sceneEyeX, 0, (GLsizei)s_fboWidth, (GLsizei)s_fboHeight);
}

static GLuint GL_SceneColorTex(void) {
    if (s_msaaInTile && s_msaaColorTex)
        return s_msaaColorTex;
    return s_fboColorTex;
}

/* Scene quad for a mono composite.  A mono frame draws into the left half of
 * the double-wide target, so its present must not sample the stale right-eye
 * half: that squeezes both eye images into one eye. */
static GLuint GL_PresentSceneVbo(void);

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
    if (s_directActive && s_directFbo)
        return s_directFbo;
    if (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3))
        return s_msaaFbo;
    return s_fbo;
}
static S32 s_frameActive = 0;         /* 1 between GL_BeginFrame and GL_Present */
static U32 s_frameSeq = 0;            /* incremented in GL_BeginFrame */
static U32 s_atlasContentVersion = 1; /* bumped only for an unnamed dirty page */
/* A dirty pointer can name a page base or an offset inside it: the inventory
   draws BufferTexture while its fire/plasma effects live at offsets.  Treat
   both as the same 64 KB page so the base cache entry is refreshed too. */
static int GL_AtlasSamePage(const void *a, const void *b) {
    uintptr_t pa, pb;

    if (a == NULL || b == NULL) {
        return 0;
    }
    pa = (uintptr_t)a;
    pb = (uintptr_t)b;
    if (pa >= pb) {
        return (pa - pb) < 256u * 256u;
    }
    return (pb - pa) < 256u * 256u;
}
/* One GL texture per atlas page (PtrMap), not one shared texture: terrain,
   decor bodies, NPC bodies and the sea each own a texture page and their
   draws interleave within a frame, so a shared texture would need a full
   64 KB re-upload on every switch.  Entries evict LRU; version matches
   s_atlasContentVersion, and a named in-place mutation clears only that
   page's version so it re-uploads without touching the others. */
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
static U32 s_stereoFrameSeq = 0; /* increments when a stereo pair starts */
/* Set when eye 0's composite has been captured for the current frame and
 * not yet consumed by the OpenXR submit; cleared by any mono present so
 * menu/movie frames (no per-eye render) submit the mono layer instead of
 * a stale half. */
static S32 s_stereoPairReady = 0;
/* Set by eye 1's present when the 2D overlay is empty, so the right-eye
 * composite was skipped and OpenXR_PresentMono can read the resolved 3D
 * scene FBO directly.  Cleared by every present and by leaving stereo. */
static S32 s_stereoEye1SceneDirect = 0;

/* ── Desktop stereo harness (debug) ───────────────────────────────────────
   The only place the real stereo pair exists is the headset, and a
   compositor screenshot there carries a per-eye pose difference that makes
   the two halves incomparable.  LBA2_FAKE_STEREO=<px> renders the same
   single-pass pair in a desktop build instead: the scene target goes
   double-wide, eye 1 gets a synthesised delta that lands <px> of offset at
   a reference depth, and --screenshot writes both halves side by side so
   the pair can be inspected pixel by pixel.  Never set in a shipped run. */
static S32 s_fakeStereoPx = 0;
extern "C" U32 ModeDesiredX;
extern "C" U32 ModeDesiredY;

/* Single-pass stereo: eye 0 submits every 3D draw once as two instances,
   each instance writing its eye half of the double-wide target.  Eye 1 then
   draws only its 2D overlay and presents.  Eye offsets are the eye-1 total
   camera change relative to the eye-0 base camera. */
static float s_eye1World[3] = {0.0f, 0.0f, 0.0f};
static float s_eye1Proj[2] = {0.0f, 0.0f};
/* Common to both eyes: the interior hover rig leans the whole pair, so the
   two halves stay a true stereo pair of one camera. */
static float s_baseWorld[3] = {0.0f, 0.0f, 0.0f};
static float s_baseProj[2] = {0.0f, 0.0f};

void GL_SetSinglePassStereo(S32 on) {
    /* Both instances write halves of the double-wide target; without it the
       eye-1 half would fall outside the FBO. */
    s_singlePassStereo = (on && s_stereoDoubleWide) ? 1 : 0;
    if (!s_singlePassStereo) {
        s_baseWorld[0] = s_baseWorld[1] = s_baseWorld[2] = 0.0f;
        s_baseProj[0] = s_baseProj[1] = 0.0f;
    }
}

S32 GL_SinglePassStereoSupported(void) { return s_stereoDoubleWide ? 1 : 0; }

S32 GL_SinglePassStereoActive(void) { return s_singlePassStereo; }

void GL_SetSinglePassEyeDelta(float x, float y, float z, float projX,
                              float projY) {
    s_eye1World[0] = x;
    s_eye1World[1] = y;
    s_eye1World[2] = z;
    s_eye1Proj[0] = projX;
    s_eye1Proj[1] = projY;
}

void GL_SetSinglePassBaseDelta(float x, float y, float z, float projX,
                               float projY) {
    s_baseWorld[0] = x;
    s_baseWorld[1] = y;
    s_baseWorld[2] = z;
    s_baseProj[0] = projX;
    s_baseProj[1] = projY;
}

/* Union-culling margin, in screen pixels, for the second eye.  CPU culling
   projects once with the eye-0 camera; an object whose eye-0 projection
   misses the frame can still enter the eye-1 half after the shader applies
   the lateral/projection delta.  The disparity grows as depth approaches
   the near plane, so bounding it there covers every visible depth. */
void GL_GetSinglePassCullMargin(S32 *marginX, S32 *marginY) {
    if (marginX != NULL)
        *marginX = 0;
    if (marginY != NULL)
        *marginY = 0;
    if (!s_singlePassStereo)
        return;

    S32 depth = NearClip;
    if (depth < 64)
        depth = 64;
    const double invDepth = 1.0 / (double)depth;
    const double focalX = (double)FRatioX;
    const double focalY = focalX * fabs((double)FRatioY);
    /* Eye 0 is base, eye 1 is base + its own delta: bound both. */
    const double wx0 = fabs((double)s_baseWorld[0]);
    const double wx1 = fabs((double)s_baseWorld[0] + (double)s_eye1World[0]);
    const double wy0 = fabs((double)s_baseWorld[1]);
    const double wy1 = fabs((double)s_baseWorld[1] + (double)s_eye1World[1]);
    const double wx = (wx0 > wx1) ? wx0 : wx1;
    const double wy = (wy0 > wy1) ? wy0 : wy1;
    const double px0 = fabs((double)s_baseProj[0]);
    const double px1 = fabs((double)s_baseProj[0] + (double)s_eye1Proj[0]);
    const double py0 = fabs((double)s_baseProj[1]);
    const double py1 = fabs((double)s_baseProj[1] + (double)s_eye1Proj[1]);
    const double px = (px0 > px1) ? px0 : px1;
    const double py = (py0 > py1) ? py0 : py1;
    const S32 mx = (S32)ceil(wx * focalX * invDepth + px) + 2;
    const S32 my = (S32)ceil(wy * focalY * invDepth + py) + 2;
    if (marginX != NULL)
        *marginX = mx;
    if (marginY != NULL)
        *marginY = my;
}

/* Both instances write into one double-wide target with the shader mapping
   each eye to its own half.  Scissoring the pair per draw keeps vertices
   that project past their eye's screen edge out of the other eye's half. */
static void GL_BeginSinglePassEyeDraw(S32 eye) {
    if (!s_singlePassStereo)
        return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(eye == 1 ? (GLint)s_fboWidth : 0, 0, (GLsizei)s_fboWidth,
              (GLsizei)s_fboHeight);
}

static void GL_EndSinglePassEyeDraw(void) {
    if (!s_singlePassStereo)
        return;
    glDisable(GL_SCISSOR_TEST);
}

static void GL_SetClutEyeUniforms(void) {
    glUniform1f(s_clut_uSinglePass, 1.0f);
    glUniform3f(s_clut_uEyeWorld, s_eye1World[0], s_eye1World[1],
                s_eye1World[2]);
    glUniform2f(s_clut_uEyeProj, s_eye1Proj[0], s_eye1Proj[1]);
    glUniform3f(s_clut_uBaseWorld, s_baseWorld[0], s_baseWorld[1],
                s_baseWorld[2]);
    glUniform2f(s_clut_uBaseProj, s_baseProj[0], s_baseProj[1]);
    glUniform2f(s_clut_uEyeFocal, FRatioX, FRatioY);
    glUniform1f(s_clut_uEyeZScale,
                (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0));
}

static void GL_SetClut3EyeUniforms(void) {
    glUniform1f(s_clut3_uSinglePass, 1.0f);
    glUniform3f(s_clut3_uEyeWorld, s_eye1World[0], s_eye1World[1],
                s_eye1World[2]);
    glUniform2f(s_clut3_uEyeProj, s_eye1Proj[0], s_eye1Proj[1]);
    glUniform3f(s_clut3_uBaseWorld, s_baseWorld[0], s_baseWorld[1],
                s_baseWorld[2]);
    glUniform2f(s_clut3_uBaseProj, s_baseProj[0], s_baseProj[1]);
    glUniform2f(s_clut3_uEyeFocal, FRatioX, FRatioY);
    glUniform1f(s_clut3_uEyeZScale,
                (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0));
}

/* True during the redundant eye 1 CPU pass of single-pass stereo.  Eye 0's
   draw already covered both halves, so a per-pair path should not repeat its
   upload or state setup here; only the present still needs this pass. */
static S32 GL_SecondEyePass(void) {
    return s_singlePassStereo && s_stereoEye == 1;
}

static void GL_DrawStereoClut(GLenum mode, GLint first, GLsizei count) {
    if (s_singlePassStereo) {
        if (GL_SecondEyePass()) {
            return;
        }
        GL_SetClutEyeUniforms();
        glUniform1f(s_clut_uEye, 0.0f);
        GL_BeginSinglePassEyeDraw(0);
        glDrawArrays(mode, first, count);
        glUniform1f(s_clut_uEye, 1.0f);
        GL_BeginSinglePassEyeDraw(1);
        glDrawArrays(mode, first, count);
        GL_EndSinglePassEyeDraw();
        glUniform1f(s_clut_uEye, 0.0f);
        return;
    }
    /* Program state outlives a stereo frame; clear the single-pass switch
       before a mono or two-pass draw samples it. */
    glUniform1f(s_clut_uSinglePass, 0.0f);
    glUniform1f(s_clut_uEye, 0.0f);
    glDrawArrays(mode, first, count);
}

static void GL_DrawStereoClut3(GLenum mode, GLint first, GLsizei count) {
    if (s_singlePassStereo) {
        if (GL_SecondEyePass()) {
            return;
        }
        GL_SetClut3EyeUniforms();
        glUniform1f(s_clut3_uEye, 0.0f);
        GL_BeginSinglePassEyeDraw(0);
        glDrawArrays(mode, first, count);
        glUniform1f(s_clut3_uEye, 1.0f);
        GL_BeginSinglePassEyeDraw(1);
        glDrawArrays(mode, first, count);
        GL_EndSinglePassEyeDraw();
        glUniform1f(s_clut3_uEye, 0.0f);
        return;
    }
    /* Program state outlives a stereo frame; clear the single-pass switch
       before a mono or two-pass draw samples it. */
    glUniform1f(s_clut3_uSinglePass, 0.0f);
    glUniform1f(s_clut3_uEye, 0.0f);
    glDrawArrays(mode, first, count);
}

static GLuint GL_PresentSceneVbo(void) {
    if (s_stereoDoubleWide && s_stereoEye < 0)
        return s_fboHalfVbo;
    return s_fboVbo;
}

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
        GL_ForgetFboBinding();
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
    -1.0f, -1.0f, 0.0f, 1.0f, /* bottom-left  (flip Y: texcoord 1.0) */
    1.0f, -1.0f, 1.0f, 1.0f,  /* bottom-right */
    -1.0f, 1.0f, 0.0f, 0.0f,  /* top-left      (flip Y: texcoord 0.0) */
    1.0f, 1.0f, 1.0f, 0.0f,   /* top-right */
};

/* FBO quad — V is NOT flipped. The CLUT vertex shader already maps game
   Y=0 → FBO top (clip Y=+1), so V=1.0 at screen top samples the FBO
   correctly without an extra flip. */
static const GLfloat s_fboQuadVerts[] = {
    /* pos       texcoord */
    -1.0f, -1.0f, 0.0f, 0.0f, /* bottom-left  (V=0.0 = FBO bottom) */
    1.0f, -1.0f, 1.0f, 0.0f,  /* bottom-right */
    -1.0f, 1.0f, 0.0f, 1.0f,  /* top-left      (V=1.0 = FBO top) */
    1.0f, 1.0f, 1.0f, 1.0f,   /* top-right */
};

/* Left-eye half of a double-wide scene texture. */
static const GLfloat s_fboHalfQuadVerts[] = {
    /* pos       texcoord */
    -1.0f, -1.0f, 0.0f, 0.0f, /* bottom-left  (V=0.0 = FBO bottom) */
    1.0f, -1.0f, 0.5f, 0.0f,  /* bottom-right */
    -1.0f, 1.0f, 0.0f, 1.0f,  /* top-left      (V=1.0 = FBO top) */
    1.0f, 1.0f, 0.5f, 1.0f,   /* top-right */
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
        GL_ForgetFboBinding();
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
        GL_ForgetFboBinding();
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
            GL_ForgetFboBinding();
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
    /* The harness has no eye loop: hold eye 0 so the single-pass draws (which
       only issue from eye 0) run and the scene viewport stays double-wide. */
    if (s_fakeStereoPx > 0)
        eye = 0;

    /* Single-pass eye 1 suppresses 3D flushes, so drain eye 0's active and
     * deferred pools while the eye-0 half is still the one being written.
     * Otherwise the first aux draw in the eye-1 pass would discard queued
     * eye-0 geometry. */
    if (eye == 1 && s_singlePassStereo) {
        GL_FlushBatch();
    }

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
        s_stereoFrameSeq++;
        GL_DestroySnapshot();
        S32 wantDoubleWide = GL_StereoDoubleWideAllowed();
        if (wantDoubleWide != s_stereoDoubleWide)
            GL_SetStereoTargetMode(wantDoubleWide);
        s_stereoPairReady = 0;
    } else if (eye < 0 && s_stereoDoubleWide && s_fakeStereoPx == 0 &&
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

/* Direct-to-swapchain readiness.  It deliberately requires the resolve-in-tile
 * probe to have passed: the same driver path that accepts and resolves the
 * dedicated MSAA texture has to accept the runtime-owned one.  A non-neutral
 * display transform is excluded because the normal present applies it while
 * sampling the pair; direct rendering has no such pass. */
S32 GL_DirectStereoSupported(void) {
    if (s_directUnsupported || !s_initialized || !s_stereoDoubleWide ||
        !s_msaaInTile || s_msaaDepthRb == 0 || s_msaaEXTAvailable != 1 ||
        p_glFramebufferTexture2DMultisampleEXT == NULL ||
        (s_antiAlias != 2 && s_antiAlias != 3) || !GL_ImageAdjustNeutral())
        return 0;
    return 1;
}

S32 GL_BeginDirectStereo(U32 texture, U32 width, U32 height) {
    if (!Window_GLMakeCurrent())
        return 0;
    GL_EndDirectStereo();
    if (texture == 0 || width == 0 || height == 0)
        return 0;
    if (!GL_DirectStereoSupported())
        return 0;
    if (width != s_msaaW || height != s_msaaH)
        return 0;

    const GLsizei samples = (s_antiAlias == 3) ? 4 : 2;
    glGenFramebuffers(1, &s_directFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_directFbo);
    p_glFramebufferTexture2DMultisampleEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint)texture,
        0, samples);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, s_msaaDepthRb);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (!s_directFailLogged) {
            Log_Warn("[GL] Direct stereo target incomplete (0x%x) %ux%u "
                     "samples=%d",
                     (unsigned)status, (unsigned)width, (unsigned)height,
                     (int)samples);
            s_directFailLogged = 1;
        }
        if (s_directFbo) {
            glDeleteFramebuffers(1, &s_directFbo);
            s_directFbo = 0;
        }
        s_directUnsupported = 1;
        GL_ForgetFboBinding();
        return 0;
    }

    s_directActive = 1;
    s_frameActive = 0;
    GL_ForgetFboBinding();
    if (!s_directLogOnce) {
        Log_Info("[GL] Direct stereo target %ux%u samples=%d tex=%u", width,
                 height, (int)samples, (unsigned)texture);
        s_directLogOnce = 1;
    }
    return 1;
}

void GL_EndDirectStereo(void) {
    if (s_directFbo) {
        glDeleteFramebuffers(1, &s_directFbo);
        s_directFbo = 0;
        GL_ForgetFboBinding();
    }
    s_directActive = 0;
    s_frameActive = 0;
}

S32 GL_DirectStereoActive(void) {
    return s_directActive;
}

S32 GL_GetStereoEye(void) {
    return s_stereoEye;
}

U32 GL_GetStereoFrameSeq(void) {
    return s_stereoFrameSeq;
}

U32 GL_GetFrameSeq(void) { return s_frameSeq; }

S32 GL_StereoOverlayReusable(void) {
    /* Exteriors mix per-eye 2D content (rain drops, object incrusts) into
       Log, so the second eye must start from a cleared buffer.  Only the
       screen-space interior HUD is identical for the pair. */
    if (CubeMode != 0)
        return 0;
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
    glUniform1f(s_uSharpenLoc, 0.0f);

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

/* One sub-rect of the double-wide pair through the present program, so the
   display transform is applied in the same pass that crops or shrinks the
   eye.  Unlike the full-pair blit this leaves the viewport alone: the caller
   has already set it for the whole swapchain image. */
S32 GL_BlitStereoRectToCurrent(U32 srcX, U32 srcY, U32 srcWidth, U32 srcHeight,
                               U32 texWidth, U32 texHeight, S32 dstX, S32 dstY,
                               S32 dstWidth, S32 dstHeight, S32 viewportWidth,
                               S32 viewportHeight) {
    if (!s_initialized || !s_stereoDoubleWide || s_program == 0 ||
        srcWidth == 0 || srcHeight == 0 || texWidth == 0 || texHeight == 0 ||
        dstWidth <= 0 || dstHeight <= 0 || viewportWidth <= 0 ||
        viewportHeight <= 0 || srcX + srcWidth > texWidth ||
        srcY + srcHeight > texHeight)
        return 0;

    GLuint tex = GL_SceneColorTex();
    if (tex == 0)
        return 0;

    const S32 exact =
        ((S32)srcWidth == dstWidth && (S32)srcHeight == dstHeight);
    GLenum filter = exact ? GL_NEAREST : GL_LINEAR;

    GL_UseProgramCached(s_program);
    GL_BindTexUnit(0, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glUniform1i(s_uTexLoc, 0);
    GL_UploadImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                         s_adj_uSaturation);
    glUniform2f(s_uTexelLoc, 1.0f / (GLfloat)texWidth,
                1.0f / (GLfloat)texHeight);
    glUniform1i(s_uFilterLoc, exact ? 0 : 1);
    glUniform1f(s_uSharpenLoc, 0.0f);

    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SCISSOR_TEST);

    const GLfloat u0 = (GLfloat)srcX / (GLfloat)texWidth;
    const GLfloat v0 = (GLfloat)srcY / (GLfloat)texHeight;
    const GLfloat u1 = (GLfloat)(srcX + srcWidth) / (GLfloat)texWidth;
    const GLfloat v1 = (GLfloat)(srcY + srcHeight) / (GLfloat)texHeight;
    const GLfloat nx0 = 2.0f * (GLfloat)dstX / (GLfloat)viewportWidth - 1.0f;
    const GLfloat nx1 =
        2.0f * (GLfloat)(dstX + dstWidth) / (GLfloat)viewportWidth - 1.0f;
    const GLfloat ny0 = 2.0f * (GLfloat)dstY / (GLfloat)viewportHeight - 1.0f;
    const GLfloat ny1 =
        2.0f * (GLfloat)(dstY + dstHeight) / (GLfloat)viewportHeight - 1.0f;
    const GLfloat verts[4][4] = {
        {nx0, ny0, u0, v0}, /* bottom-left  (V=0 = FBO bottom) */
        {nx1, ny0, u1, v0}, /* bottom-right */
        {nx0, ny1, u0, v1}, /* top-left */
        {nx1, ny1, u1, v1}, /* top-right */
    };

    if (s_presentRectVbo == 0)
        glGenBuffers(1, &s_presentRectVbo);
    if (s_presentRectVbo == 0)
        return 0;
    GL_BindArrayBuffer(s_presentRectVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
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
    s_uSharpenLoc = glGetUniformLocation(s_program, "uSharpen");
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
        static const char *names[6] = {"aPos", "aTexCoord", "aLight",
                                       "aZ", "aRepMask", "aMaterial"};
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
    s_clut_uSinglePass = glGetUniformLocation(s_clutProgram, "uSinglePass");
    s_clut_uEye = glGetUniformLocation(s_clutProgram, "uEye");
    s_clut_uEyeWorld = glGetUniformLocation(s_clutProgram, "uEyeWorld");
    s_clut_uEyeProj = glGetUniformLocation(s_clutProgram, "uEyeProj");
    s_clut_uBaseWorld = glGetUniformLocation(s_clutProgram, "uBaseWorld");
    s_clut_uBaseProj = glGetUniformLocation(s_clutProgram, "uBaseProj");
    s_clut_uEyeFocal = glGetUniformLocation(s_clutProgram, "uEyeFocal");
    s_clut_uEyeZScale = glGetUniformLocation(s_clutProgram, "uEyeZScale");
    s_clut_uAtlas = glGetUniformLocation(s_clutProgram, "uAtlas");
    s_clut_uCLUT = glGetUniformLocation(s_clutProgram, "uCLUT");
    s_clut_uPalette = glGetUniformLocation(s_clutProgram, "uPalette");
    s_clut_uScreenScale = glGetUniformLocation(s_clutProgram, "uScreenScale");
    s_clut_uAtlasScale = glGetUniformLocation(s_clutProgram, "uAtlasScale");
    s_clut_uCLUTBaseRow = glGetUniformLocation(s_clutProgram, "uCLUTBaseRow");
    s_clut_uScaledFogNear = glGetUniformLocation(s_clutProgram, "uScaledFogNear");
    s_clut_uFogRowScale = glGetUniformLocation(s_clutProgram, "uFogRowScale");
    s_clut_uBilinear = glGetUniformLocation(s_clutProgram, "uBilinear");
    s_clut_uDither = glGetUniformLocation(s_clutProgram, "uDither");
    s_clut_uTerrainFarClip =
        glGetUniformLocation(s_clutProgram, "uTerrainFarClip");
    s_clut_uTerrainFar = glGetUniformLocation(s_clutProgram, "uTerrainFar");

    /* Sampler unit assignments are program state: set them once here
       instead of per draw call. */
    GL_UseProgramCached(s_clutProgram);
    glUniform1i(s_clut_uAtlas, 0);
    glUniform1i(s_clut_uCLUT, 1);
    glUniform1i(s_clut_uPalette, 2);
    glUniform1f(s_clut_uSinglePass, 0.0f);
    glUniform1f(s_clut_uEye, 0.0f);
    glUniform3f(s_clut_uEyeWorld, 0.0f, 0.0f, 0.0f);
    glUniform2f(s_clut_uEyeProj, 0.0f, 0.0f);
    glUniform2f(s_clut_uEyeFocal, 1.0f, 1.0f);
    glUniform1f(s_clut_uEyeZScale, 1.0f);
    glUniform1f(s_clut_uTerrainFarClip, 0.0f);
    GL_UseProgramCached(0);

    /* ── GLSL-3 CLUT variant with the texture-array sampler ──
       Mechanically derived from the legacy source: version bump, es3/130
       in/out and texture() renames, an injected arraySample() used when a
       polygon's RepMask carries the 0x8000|layer array stamp. */
    {
        static const char *attrNames[6] = {"aPos", "aTexCoord", "aLight",
                                           "aZ", "aRepMask", "aMaterial"};
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
            Log_Info("[GL] array CLUT program ready");
            s_clut3_aPos = glGetAttribLocation(s_clut3Program, "aPos");
            s_clut3_aTexCoord =
                glGetAttribLocation(s_clut3Program, "aTexCoord");
            s_clut3_aLight = glGetAttribLocation(s_clut3Program, "aLight");
            s_clut3_aZ = glGetAttribLocation(s_clut3Program, "aZ");
            s_clut3_aRepMask =
                glGetAttribLocation(s_clut3Program, "aRepMask");
            s_clut3_aMaterial =
                glGetAttribLocation(s_clut3Program, "aMaterial");
            s_clut3_uSinglePass =
                glGetUniformLocation(s_clut3Program, "uSinglePass");
            s_clut3_uEye = glGetUniformLocation(s_clut3Program, "uEye");
            s_clut3_uEyeWorld =
                glGetUniformLocation(s_clut3Program, "uEyeWorld");
            s_clut3_uEyeProj =
                glGetUniformLocation(s_clut3Program, "uEyeProj");
            s_clut3_uBaseWorld =
                glGetUniformLocation(s_clut3Program, "uBaseWorld");
            s_clut3_uBaseProj =
                glGetUniformLocation(s_clut3Program, "uBaseProj");
            s_clut3_uEyeFocal =
                glGetUniformLocation(s_clut3Program, "uEyeFocal");
            s_clut3_uEyeZScale =
                glGetUniformLocation(s_clut3Program, "uEyeZScale");
            s_clut3_uAtlas = glGetUniformLocation(s_clut3Program, "uAtlas");
            s_clut3_uCLUT = glGetUniformLocation(s_clut3Program, "uCLUT");
            s_clut3_uPalette = glGetUniformLocation(s_clut3Program, "uPalette");
            s_clut3_uAtlasArr =
                glGetUniformLocation(s_clut3Program, "uAtlasArr");
            s_clut3_uAtlasQuadArr =
                glGetUniformLocation(s_clut3Program, "uAtlasQuadArr");
            s_clut3_uAtlasQuadValid =
                glGetUniformLocation(s_clut3Program, "uAtlasQuadValid");
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
            s_clut3_uDither = glGetUniformLocation(s_clut3Program, "uDither");
            s_clut3_uTerrainFarClip =
                glGetUniformLocation(s_clut3Program, "uTerrainFarClip");
            s_clut3_uTerrainPersp =
                glGetUniformLocation(s_clut3Program, "uTerrainPersp");
            s_clut3_uTerrainFar =
                glGetUniformLocation(s_clut3Program, "uTerrainFar");
            GL_UseProgramCached(s_clut3Program);
            glUniform1i(s_clut3_uAtlas, 0);
            glUniform1i(s_clut3_uCLUT, 1);
            glUniform1i(s_clut3_uPalette, 2);
            glUniform1i(s_clut3_uAtlasArr, 3);
            glUniform1f(s_clut3_uSinglePass, 0.0f);
            glUniform1f(s_clut3_uEye, 0.0f);
            glUniform3f(s_clut3_uEyeWorld, 0.0f, 0.0f, 0.0f);
            glUniform2f(s_clut3_uEyeProj, 0.0f, 0.0f);
            glUniform2f(s_clut3_uEyeFocal, 1.0f, 1.0f);
            glUniform1f(s_clut3_uEyeZScale, 1.0f);
            glUniform1i(s_clut3_uAtlasQuadArr, 4);
            glUniform1i(s_clut3_uAtlasQuadValid, 0);
            glUniform1f(s_clut3_uTerrainFarClip, 0.0f);
            glUniform1f(s_clut3_uTerrainPersp, 0.0f);
            GL_UseProgramCached(0);
        }
    }

    /* ── Terrain shader: the batch fragment program with the CPU's
       projection moved into the vertex stage ── */
    {
        static const char *attrNames[6] = {"aPos", "aTexCoord", "aLight",
                                           "aRepMask", "aMaterial", "aFogPos"};
        GLuint tv = 0, tf = 0;
        char *vert3 = GL_ShaderToV3(s_terrainVertSrc, 1);
        char *frag3 = GL_ShaderToV3(s_clutFragSrc, 0);
        if (vert3 != NULL && frag3 != NULL) {
            frag3 = GL_InjectTerrainResolved(frag3);
            tv = CompileShader(GL_VERTEX_SHADER, vert3);
            tf = CompileShader(GL_FRAGMENT_SHADER, frag3);
            if (tv != 0 && tf != 0) {
                for (S32 i = 0; i < 6; i++) {
                    glBindAttribLocation(tv, i, attrNames[i]);
                }
                s_terrainProgram = LinkProgram(tv, tf);
            }
        }
        free(vert3);
        free(frag3);
        glDeleteShader(tv);
        glDeleteShader(tf);
        if (s_terrainProgram == 0) {
            /* GLES2 fallback: the exact 8-tap path, no resolved table. */
            tv = CompileShader(GL_VERTEX_SHADER, s_terrainVertSrc);
            tf = CompileShader(GL_FRAGMENT_SHADER, s_clutFragSrc);
            if (tv != 0 && tf != 0) {
                for (S32 i = 0; i < 6; i++) {
                    glBindAttribLocation(tv, i, attrNames[i]);
                }
                s_terrainProgram = LinkProgram(tv, tf);
            }
            glDeleteShader(tv);
            glDeleteShader(tf);
        }
        if (s_terrainProgram == 0) {
            Log_Warn("[GL] terrain program failed; horizon stays on the CPU path");
        } else {
            s_terrain_aPos = glGetAttribLocation(s_terrainProgram, "aPos");
            s_terrain_aTexCoord =
                glGetAttribLocation(s_terrainProgram, "aTexCoord");
            s_terrain_aLight = glGetAttribLocation(s_terrainProgram, "aLight");
            s_terrain_aRepMask =
                glGetAttribLocation(s_terrainProgram, "aRepMask");
            s_terrain_aMaterial =
                glGetAttribLocation(s_terrainProgram, "aMaterial");
            s_terrain_aFogPos = glGetAttribLocation(s_terrainProgram, "aFogPos");
            s_terrain_uSinglePass =
                glGetUniformLocation(s_terrainProgram, "uSinglePass");
            s_terrain_uEye = glGetUniformLocation(s_terrainProgram, "uEye");
            s_terrain_uEyeWorld =
                glGetUniformLocation(s_terrainProgram, "uEyeWorld");
            s_terrain_uEyeProj =
                glGetUniformLocation(s_terrainProgram, "uEyeProj");
            s_terrain_uRot = glGetUniformLocation(s_terrainProgram, "uRot");
            s_terrain_uCamera =
                glGetUniformLocation(s_terrainProgram, "uCamera");
            s_terrain_uFocal = glGetUniformLocation(s_terrainProgram, "uFocal");
            s_terrain_uCentre = glGetUniformLocation(s_terrainProgram, "uCentre");
            s_terrain_uProjShiftX =
                glGetUniformLocation(s_terrainProgram, "uProjShiftX");
            s_terrain_uScreenScale =
                glGetUniformLocation(s_terrainProgram, "uScreenScale");
            s_terrain_uZBufferFactor =
                glGetUniformLocation(s_terrainProgram, "uZBufferFactor");
            s_terrain_uNearClip =
                glGetUniformLocation(s_terrainProgram, "uNearClip");
            s_terrain_uStartZFog =
                glGetUniformLocation(s_terrainProgram, "uStartZFog");
            s_terrain_uFogStepScale =
                glGetUniformLocation(s_terrainProgram, "uFogStepScale");
            s_terrain_uPalLevel =
                glGetUniformLocation(s_terrainProgram, "uPalLevel");
            s_terrain_uAtlas = glGetUniformLocation(s_terrainProgram, "uAtlas");
            s_terrain_uCLUT = glGetUniformLocation(s_terrainProgram, "uCLUT");
            s_terrain_uPalette =
                glGetUniformLocation(s_terrainProgram, "uPalette");
            s_terrain_uAtlasScale =
                glGetUniformLocation(s_terrainProgram, "uAtlasScale");
            s_terrain_uBilinear =
                glGetUniformLocation(s_terrainProgram, "uBilinear");
            s_terrain_uDither =
                glGetUniformLocation(s_terrainProgram, "uDither");
            s_terrain_uTerrainFarClip =
                glGetUniformLocation(s_terrainProgram, "uTerrainFarClip");
            s_terrain_uTerrainPersp =
                glGetUniformLocation(s_terrainProgram, "uTerrainPersp");
            s_terrain_uTerrainFar =
                glGetUniformLocation(s_terrainProgram, "uTerrainFar");
            s_terrain_uResolved =
                glGetUniformLocation(s_terrainProgram, "uResolved");
            s_terrain_uResolvedValid =
                glGetUniformLocation(s_terrainProgram, "uResolvedValid");
            GL_UseProgramCached(s_terrainProgram);
            glUniform1i(s_terrain_uAtlas, 0);
            glUniform1i(s_terrain_uCLUT, 1);
            glUniform1i(s_terrain_uPalette, 2);
            glUniform1f(s_terrain_uSinglePass, 0.0f);
            glUniform1f(s_terrain_uEye, 0.0f);
            glUniform3f(s_terrain_uEyeWorld, 0.0f, 0.0f, 0.0f);
            glUniform2f(s_terrain_uEyeProj, 0.0f, 0.0f);
            if (s_terrain_uResolved >= 0) {
                glUniform1i(s_terrain_uResolved, 5);
            }
            glUniform1i(s_terrain_uResolvedValid, 0);
            /* uAtlasScale is a vec2 in the shared fragment program: the
               edge-directed filters and the direct-index path divide the v
               coordinate by its y component.  A scalar upload leaves y = 0,
               so every one of those taps resolves to the same clamped texel
               row and the terrain loses all texture detail. */
            glUniform2f(s_terrain_uAtlasScale, 256.0f, 256.0f);
            glUniform1i(s_terrain_uBilinear, 0);
            glUniform1f(s_terrain_uTerrainFarClip, 0.0f);
            glUniform1f(s_terrain_uTerrainPersp, 0.0f);
            GL_UseProgramCached(0);
            s_terrainEpoch++;
            Log_Info("[GL] Terrain GPU transform program ready%s.",
                     s_terrain_uResolved >= 0 ? " (resolved CLUT)" : "");
        }
    }

    /* ── Static decor program: CPU projection + camera-space light in the
       vertex stage, array-sampled textures in the injected fragment ── */
    {
        static const char *attrNames[6] = {"aPos", "aNormal", "aTexCoord",
                                           "aMaterial", "aFlat", "aLayer"};
        char *v3 = GL_ShaderToV3(s_decorVertSrc, 1);
        char *f3 = GL_ShaderToV3(s_clutFragSrc, 0);
        GLuint dv = 0, df = 0;
        if (v3 != NULL && f3 != NULL) {
            dv = CompileShader(GL_VERTEX_SHADER, v3);
            df = CompileShader(GL_FRAGMENT_SHADER, f3);
            for (S32 i = 0; i < 6; i++) {
                glBindAttribLocation(dv, i, attrNames[i]);
            }
            s_decorProgram = LinkProgram(dv, df);
        }
        free(v3);
        free(f3);
        glDeleteShader(dv);
        glDeleteShader(df);
        if (s_decorProgram == 0) {
            Log_Warn("[GL] decor program failed; decors stay on the CPU path");
        } else {
            s_decor_aPos = glGetAttribLocation(s_decorProgram, "aPos");
            s_decor_aNormal = glGetAttribLocation(s_decorProgram, "aNormal");
            s_decor_aTexCoord =
                glGetAttribLocation(s_decorProgram, "aTexCoord");
            s_decor_aMaterial =
                glGetAttribLocation(s_decorProgram, "aMaterial");
            s_decor_aFlat = glGetAttribLocation(s_decorProgram, "aFlat");
            s_decor_aLayer = glGetAttribLocation(s_decorProgram, "aLayer");
            s_decor_uSinglePass =
                glGetUniformLocation(s_decorProgram, "uSinglePass");
            s_decor_uEye = glGetUniformLocation(s_decorProgram, "uEye");
            s_decor_uEyeWorld =
                glGetUniformLocation(s_decorProgram, "uEyeWorld");
            s_decor_uEyeProj =
                glGetUniformLocation(s_decorProgram, "uEyeProj");
            s_decor_uBaseWorld =
                glGetUniformLocation(s_decorProgram, "uBaseWorld");
            s_decor_uBaseProj =
                glGetUniformLocation(s_decorProgram, "uBaseProj");
            s_decor_uRot = glGetUniformLocation(s_decorProgram, "uRot");
            s_decor_uCamera = glGetUniformLocation(s_decorProgram, "uCamera");
            s_decor_uDecorPos =
                glGetUniformLocation(s_decorProgram, "uDecorPos");
            s_decor_uFocal = glGetUniformLocation(s_decorProgram, "uFocal");
            s_decor_uCentre = glGetUniformLocation(s_decorProgram, "uCentre");
            s_decor_uProjShiftX =
                glGetUniformLocation(s_decorProgram, "uProjShiftX");
            s_decor_uScreenScale =
                glGetUniformLocation(s_decorProgram, "uScreenScale");
            s_decor_uZBufferFactor =
                glGetUniformLocation(s_decorProgram, "uZBufferFactor");
            s_decor_uNearClip =
                glGetUniformLocation(s_decorProgram, "uNearClip");
            s_decor_uFogRow = glGetUniformLocation(s_decorProgram, "uFogRow");
            s_decor_uLight = glGetUniformLocation(s_decorProgram, "uLight");
            s_decor_uLightFactor =
                glGetUniformLocation(s_decorProgram, "uLightFactor");
            s_decor_uAtlas = glGetUniformLocation(s_decorProgram, "uAtlas");
            s_decor_uCLUT = glGetUniformLocation(s_decorProgram, "uCLUT");
            s_decor_uPalette =
                glGetUniformLocation(s_decorProgram, "uPalette");
            s_decor_uAtlasArr =
                glGetUniformLocation(s_decorProgram, "uAtlasArr");
            s_decor_uAtlasScale =
                glGetUniformLocation(s_decorProgram, "uAtlasScale");
            s_decor_uBilinear =
                glGetUniformLocation(s_decorProgram, "uBilinear");
            s_decor_uDither =
                glGetUniformLocation(s_decorProgram, "uDither");
            s_decor_uTerrainFarClip =
                glGetUniformLocation(s_decorProgram, "uTerrainFarClip");
            s_decor_uTerrainPersp =
                glGetUniformLocation(s_decorProgram, "uTerrainPersp");
            s_decor_uTerrainFar =
                glGetUniformLocation(s_decorProgram, "uTerrainFar");
            GL_UseProgramCached(s_decorProgram);
            glUniform1i(s_decor_uAtlas, 0);
            glUniform1i(s_decor_uCLUT, 1);
            glUniform1i(s_decor_uPalette, 2);
            glUniform1i(s_decor_uAtlasArr, 3);
            glUniform1f(s_decor_uSinglePass, 0.0f);
            glUniform1f(s_decor_uEye, 0.0f);
            glUniform3f(s_decor_uEyeWorld, 0.0f, 0.0f, 0.0f);
            glUniform2f(s_decor_uEyeProj, 0.0f, 0.0f);
            glUniform2f(s_decor_uAtlasScale, 256.0f, 256.0f);
            glUniform1i(s_decor_uBilinear, 0);
            glUniform1f(s_decor_uTerrainFarClip, 1.0f);
            glUniform1f(s_decor_uTerrainPersp, 0.0f);
            GL_UseProgramCached(0);
            Log_Info("[GL] Static decor VBO program ready.");
        }
    }

    /* ── Indexed background layer: the shared CLUT fragment program with
       the no-wrap layer path enabled.  One full-image quad per scene, so the
       isometric brick layer costs one draw call instead of one per tile. ── */
    {
        char *layerFrag = GL_MakeLayerFragSrc();
        GLuint lv = 0, lf = 0;
        if (layerFrag != NULL) {
            lv = CompileShader(GL_VERTEX_SHADER, s_layerVertSrc);
            lf = CompileShader(GL_FRAGMENT_SHADER, layerFrag);
        }
        if (lv != 0 && lf != 0) {
            glBindAttribLocation(lv, 0, "aPos");
            glBindAttribLocation(lv, 1, "aUV");
            glBindAttribLocation(lv, 2, "aMaterial");
            s_layerProgram = LinkProgram(lv, lf);
        }
        free(layerFrag);
        glDeleteShader(lv);
        glDeleteShader(lf);
        if (s_layerProgram == 0) {
            Log_Warn("[GL] indexed layer program failed; the brick layer "
                     "stays on per-tile quads");
        } else {
            s_layer_aPos = glGetAttribLocation(s_layerProgram, "aPos");
            s_layer_aUV = glGetAttribLocation(s_layerProgram, "aUV");
            s_layer_aMaterial =
                glGetAttribLocation(s_layerProgram, "aMaterial");
            s_layer_uScreenScale =
                glGetUniformLocation(s_layerProgram, "uScreenScale");
            s_layer_uAtlasScale =
                glGetUniformLocation(s_layerProgram, "uAtlasScale");
            s_layer_uBilinear =
                glGetUniformLocation(s_layerProgram, "uBilinear");
            s_layer_uSinglePass =
                glGetUniformLocation(s_layerProgram, "uSinglePass");
            s_layer_uEye = glGetUniformLocation(s_layerProgram, "uEye");
            s_layer_uDepth =
                glGetUniformLocation(s_layerProgram, "uLayerDepth");
            s_layer_uWarp = glGetUniformLocation(s_layerProgram, "uLayerWarp");
            s_layer_uAzMin =
                glGetUniformLocation(s_layerProgram, "uLayerAzMin");
            s_layer_uAzSpan =
                glGetUniformLocation(s_layerProgram, "uLayerAzSpan");
            s_layer_uAzShift =
                glGetUniformLocation(s_layerProgram, "uLayerAzShift");
            s_layer_uMask = glGetUniformLocation(s_layerProgram, "uLayerMask");
            s_layer_uMaskOn =
                glGetUniformLocation(s_layerProgram, "uLayerMaskOn");
            s_layer_uMaskRect =
                glGetUniformLocation(s_layerProgram, "uLayerMaskRect");
            s_layer_uShiftPx =
                glGetUniformLocation(s_layerProgram, "uLayerShiftPx");
            s_layer_uShiftPerAz =
                glGetUniformLocation(s_layerProgram, "uLayerShiftPerAz");
            GL_UseProgramCached(s_layerProgram);
            glUniform1i(glGetUniformLocation(s_layerProgram, "uAtlas"), 0);
            glUniform1i(glGetUniformLocation(s_layerProgram, "uCLUT"), 1);
            glUniform1i(glGetUniformLocation(s_layerProgram, "uPalette"), 2);
            /* The depth map rides on its own unit: 0-2 are the atlas/CLUT/
               palette set and 3-5 hold the texture arrays the batch, decor and
               terrain programs sample, so a 2D texture on any of those would
               fight the program that owns the unit. */
            glUniform1i(s_layer_uDepth, GL_LAYER_DEPTH_UNIT);
            glUniform1i(s_layer_uMask, GL_LAYER_MASK_UNIT);
            glUniform1f(s_layer_uMaskOn, 0.0f);
            glUniform1f(
                glGetUniformLocation(s_layerProgram, "uSinglePass"), 0.0f);
            glUniform1f(glGetUniformLocation(s_layerProgram, "uEye"), 0.0f);
            glUniform1f(s_layer_uWarp, 0.0f);
            glUniform1f(s_layer_uAzMin, 0.0f);
            glUniform1f(s_layer_uAzSpan, 1.0f);
            glUniform1f(s_layer_uAzShift, 0.0f);
            glUniform2f(s_layer_uShiftPx, 0.0f, 0.0f);
            glUniform2f(s_layer_uShiftPerAz, 0.0f, 0.0f);
            glUniform1f(
                glGetUniformLocation(s_layerProgram, "uCLUTBaseRow"), 0.0f);
            glUniform1f(
                glGetUniformLocation(s_layerProgram, "uScaledFogNear"), 0.0f);
            glUniform1f(
                glGetUniformLocation(s_layerProgram, "uFogRowScale"), 0.0f);
            glUniform1f(
                glGetUniformLocation(s_layerProgram, "uTerrainFarClip"), 0.0f);
            GL_UseProgramCached(0);
            Log_Info("[GL] Indexed layer program ready.");
        }
    }

    {
        GLuint fv = CompileShader(GL_VERTEX_SHADER, s_vertSrc);
        GLuint ff = CompileShader(GL_FRAGMENT_SHADER, s_fxaaFragSrc);
        if (fv && ff) {
            s_fxaaProgram = LinkProgram(fv, ff);
            s_fxaa_uTex = glGetUniformLocation(s_fxaaProgram, "uTex");
            s_fxaa_uTexel = glGetUniformLocation(s_fxaaProgram, "uTexel");
            s_fxaa_uSharpen = glGetUniformLocation(s_fxaaProgram, "uSharpen");
            s_fxaaAdj_uBrightness = glGetUniformLocation(s_fxaaProgram, "uBrightness");
            s_fxaaAdj_uContrast = glGetUniformLocation(s_fxaaProgram, "uContrast");
            s_fxaaAdj_uSaturation = glGetUniformLocation(s_fxaaProgram, "uSaturation");
            s_fxaa_aPos = glGetAttribLocation(s_fxaaProgram, "aPos");
            s_fxaa_aTexCoord = glGetAttribLocation(s_fxaaProgram, "aTexCoord");
        }
        glDeleteShader(fv);
        glDeleteShader(ff);
    }

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

    /* Mono present from a double-wide scene: sample the left eye's half. */
    glGenBuffers(1, &s_fboHalfVbo);
    GL_BindArrayBuffer(s_fboHalfVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_fboHalfQuadVerts),
                 s_fboHalfQuadVerts, GL_STATIC_DRAW);
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

    /* CLUT texture (256×256 RGBA — full fog table already resolved through
       the palette; sampling it replaces the old CLUT(indices) + palette(1D)
       pair and removes one texture fetch per lookup). */
    glGenTextures(1, &s_clutTex);
    glBindTexture(GL_TEXTURE_2D, s_clutTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_UPLOAD_FORMAT,
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
        (PFN_GLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXT)(PFN_GLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXT)
            SDL_GL_GetProcAddress("glFramebufferTexture2DMultisampleEXT");
    glViewport(0, 0, resX, resY);
    glDisable(GL_DEPTH_TEST);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Create FBO — always needed so 3D renders into an offscreen buffer
       that can be composited at the letterbox viewport alongside the
       2D overlay.  Without FBO, GL_Present's letterbox clear erases
       any 3D drawn directly to the default framebuffer. */
    GL_CreateFBO(GL_ScaleRenderDim(resX), GL_ScaleRenderDim(resY));

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
    GL_ProbeMultiview(GL_ScaleRenderDim(s_texWidth),
                      GL_ScaleRenderDim(s_texHeight));
    GL_LogSurfaceBudget("init");

    if (getenv("LBA2_SS15") != NULL) {
        GL_SetSuperSample(2);
    }

    /* Desktop stereo harness: the pair the headset renders, without the
       headset (see s_fakeStereoPx). */
    {
        const char *fake = getenv("LBA2_FAKE_STEREO");
        if (fake != NULL && *fake != '\0') {
            s_fakeStereoPx = atoi(fake);
            if (s_fakeStereoPx < 0) {
                s_fakeStereoPx = 0;
            }
            if (s_fakeStereoPx > 0) {
                /* GL_SetStereoTargetMode doubles whatever eye size it finds
                   in s_fboWidth, which on a desktop also carries the
                   render-quality multiplier: hand it the unscaled eye size
                   so the harness target stays 2x the game resolution. */
                s_fboWidth = ModeDesiredX;
                s_fboHeight = ModeDesiredY;
                GL_SetStereoTargetMode(1);
                GL_SetSinglePassStereo(1);
                /* Interiors never call SetProjection, so a run that starts
                   inside has no focal yet and the synthesised delta would
                   collapse to zero.  The iso projection does not read it;
                   only the stereo eye shift does. */
                if (FRatioX <= 0.0f) {
                    FRatioX = 600.0f;
                    FRatioY = -1.0f;
                }
                /* Match the headset's supersampled target so the HUD path
                   (scissor bands at a non-1:1 scale) is exercised too. */
                if (getenv("LBA2_FAKE_STEREO_SS") != NULL)
                    GL_SetSuperSample(2);
                Log_Info("[GL] Fake stereo: %d px at az 1000, %s",
                         (int)s_fakeStereoPx,
                         s_singlePassStereo ? "single-pass" : "unavailable");
            }
        }
    }

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
    if (s_fboHalfVbo) {
        glDeleteBuffers(1, &s_fboHalfVbo);
        s_fboHalfVbo = 0;
    }
    if (s_presentRectVbo) {
        glDeleteBuffers(1, &s_presentRectVbo);
        s_presentRectVbo = 0;
    }
    if (s_batchVBO) {
        glDeleteBuffers(1, &s_batchVBO);
        s_batchVBO = 0;
    }
    if (s_decorVBO) {
        glDeleteBuffers(1, &s_decorVBO);
        s_decorVBO = 0;
    }
    for (S32 i = 0; i < 3; i++) {
        free(s_decorVerts[i]);
        s_decorVerts[i] = NULL;
        s_decorVertCount[i] = 0;
        s_decorVertCap[i] = 0;
    }
    for (S32 i = 0; i < 5; i++) {
        free(s_decorLegacyVerts[i]);
        s_decorLegacyVerts[i] = NULL;
        s_decorLegacyCount[i] = 0;
        s_decorLegacyCap[i] = 0;
    }
    s_decorBatch = 0;
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
        GL_ForgetFboBinding();
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
    if (s_clut3Program) {
        glDeleteProgram(s_clut3Program);
        s_clut3Program = 0;
    }
    if (s_terrainProgram) {
        glDeleteProgram(s_terrainProgram);
        s_terrainProgram = 0;
    }
    if (s_decorProgram) {
        glDeleteProgram(s_decorProgram);
        s_decorProgram = 0;
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
    s_batchWorld = 0;
    s_deferWorld = 0;
    s_batchNZW = 0;
    s_frameActive = 0;
    s_fboHasTerrain = 0;
    s_stereoDoubleWide = 0;
    s_sceneEyeX = 0;
    s_sceneTargetW = 0;
    s_sceneTargetH = 0;
    s_terrainBoundVBO = 0;
    s_terrainEpoch++;
    GL_InvalidateGLState();
    Log_Info("[GL] Renderer shut down.");
}

S32 GL_Renderer_Resize(U32 resX, U32 resY) {
    if (!s_initialized) {
        return TRUE;
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
    GL_CreateFBO(GL_ScaleRenderDim(resX), GL_ScaleRenderDim(resY));
    if ((s_antiAlias == 2 || s_antiAlias == 3)) {
        GL_CreateMSAAFbo(GL_ScaleRenderDim(resX), GL_ScaleRenderDim(resY),
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

    /* A resize that could not allocate its render targets must not leave the
       engine with a dead framebuffer.  Res_Switch treats FALSE as "revert to
       the previous size"; this is reachable when VR supersampling is active
       and the new resolution pushes the scaled target past the device
       budget. */
    if (s_fbo == 0) {
        Log_Error("[GL] Resize to %ux%u failed: no scene target",
                  (unsigned)resX, (unsigned)resY);
        return FALSE;
    }
    if (OpenXR_IsActive() && !s_stereoDoubleWide) {
        if (DisplayStereo3D) {
            if (s_stereoEyeFBO[0] == 0 || s_stereoEyeFBO[1] == 0) {
                Log_Error("[GL] Resize to %ux%u failed: no stereo eye targets",
                          (unsigned)resX, (unsigned)resY);
                return FALSE;
            }
        } else if (s_presentFBO == 0) {
            Log_Error("[GL] Resize to %ux%u failed: no present target",
                      (unsigned)resX, (unsigned)resY);
            return FALSE;
        }
    }
    return TRUE;
}

void GL_SetRenderQuality(U32 quality) {
    if (quality == 0)
        quality = 1;
    if (quality > 4)
        quality = 4;
    /* The desktop stereo harness compares eye halves pixel for pixel, so a
       supersampled target only makes the pair harder to read. */
    if (s_fakeStereoPx > 0)
        quality = 1;
#ifdef __ANDROID__
    /* Single choke point (boot AND the in-game menu): SSAA multiplies the
     * framebuffer per axis, and quality 4 at 1680x945 is a 25-megapixel
     * render target that stalls the whole frame loop on a mobile GPU. The
     * Display -> VR -> Supersampling row is the headset's explicit
     * 1.25x/1.5x version of the same trade-off. */
    if (quality > 1) {
        Log_Info("[GL] Android: RenderQuality %u clamped to 1", quality);
        quality = 1;
    }
#endif
    if (s_renderQuality == quality)
        return;
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
    GL_CreateFBO(GL_ScaleRenderDim(s_texWidth), GL_ScaleRenderDim(s_texHeight));
    if ((s_antiAlias == 2 || s_antiAlias == 3)) {
        GL_CreateMSAAFbo(GL_ScaleRenderDim(s_texWidth),
                         GL_ScaleRenderDim(s_texHeight),
                         (s_antiAlias == 3) ? 4 : 2);
    }
    Log_Info("[GL] SetRenderQuality: FBO %s (s_fbo=%u).",
             s_fbo ? "OK" : "FAILED", s_fbo);
}

/* VR supersampling: scale the scene target above the engine framebuffer and
   let the present pass downsample it.  Separate from RenderQuality so the
   headset menu can offer 1.25x/1.5x without adding resolution entries; the
   field of view is unchanged (the projection still uses ModeDesiredX/Y) and
   only edge detail improves.  Returns FALSE when the enlarged target cannot
   be allocated; the previous scale is restored in that case. */
S32 GL_SetSuperSample(S32 mode) {
    U32 num = 1;
    U32 den = 1;
    if (mode == 1) {
        num = 5;
        den = 4;
    } else if (mode == 2) {
        num = 3;
        den = 2;
    }

    if (num == s_ssNum && den == s_ssDen)
        return TRUE;

    const U32 oldNum = s_ssNum;
    const U32 oldDen = s_ssDen;
    s_ssNum = num;
    s_ssDen = den;

    if (!s_initialized)
        return TRUE;
    if (!Window_GLMakeCurrent()) {
        s_ssNum = oldNum;
        s_ssDen = oldDen;
        return FALSE;
    }

    s_fboHasTerrain = 0;
    GL_CreateFBO(GL_ScaleRenderDim(s_texWidth), GL_ScaleRenderDim(s_texHeight));
    if (s_antiAlias == 2 || s_antiAlias == 3) {
        GL_CreateMSAAFbo(GL_ScaleRenderDim(s_texWidth),
                         GL_ScaleRenderDim(s_texHeight),
                         (s_antiAlias == 3) ? 4 : 2);
    }
    if (s_fbo == 0) {
        Log_Error("[GL] Supersampling %s target failed; reverting to native",
                  mode == 2 ? "1.5x" : mode == 1 ? "1.25x"
                                                 : "off");
        s_ssNum = oldNum;
        s_ssDen = oldDen;
        GL_CreateFBO(GL_ScaleRenderDim(s_texWidth),
                     GL_ScaleRenderDim(s_texHeight));
        if (s_antiAlias == 2 || s_antiAlias == 3) {
            GL_CreateMSAAFbo(GL_ScaleRenderDim(s_texWidth),
                             GL_ScaleRenderDim(s_texHeight),
                             (s_antiAlias == 3) ? 4 : 2);
        }
        return FALSE;
    }

    Log_Info("[GL] Supersampling %s: scene target %ux%u",
             mode == 2 ? "1.5x" : mode == 1 ? "1.25x"
                                            : "off",
             (unsigned)s_fboWidth, (unsigned)s_fboHeight);
    GL_LogSurfaceBudget("supersample");
    return TRUE;
}

/* ── Antialiasing (MSAA render target + FXAA post-process) ───────────────── */

static void GL_DestroyMSAAFbo(void) {
    GL_EndDirectStereo();
    if (s_msaaResolveFbo) {
        glDeleteFramebuffers(1, &s_msaaResolveFbo);
        s_msaaResolveFbo = 0;
        GL_ForgetFboBinding();
    }
    if (s_msaaFbo) {
        glDeleteFramebuffers(1, &s_msaaFbo);
        s_msaaFbo = 0;
        GL_ForgetFboBinding();
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
                     "explicit resolve",
                     probe[0], probe[1], probe[2]);
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
 * FXAA) is required because that post-process needs a per-eye
 * single-sample composite.  Brightness/contrast/saturation do not force the
 * fallback: the final pair-to-swapchain pass applies them to the whole
 * composite.  The environment escape hatch lets a device run compare the old
 * path without a rebuild. */
static S32 GL_StereoDoubleWideAllowed(void) {
    /* The desktop harness asks for the pair explicitly. */
    if (s_fakeStereoPx > 0)
        return 1;
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

    /* A target switch recreates the depth renderbuffer and the scene size;
       a direct failure from the previous target must not poison the new one. */
    GL_EndDirectStereo();
    s_directUnsupported = 0;
    s_directFailLogged = 0;

    U32 w = s_fboWidth;
    U32 h = s_fboHeight;
    if (w == 0 || h == 0) {
        w = GL_ScaleRenderDim(s_texWidth);
        h = GL_ScaleRenderDim(s_texHeight);
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
    if (mode < 0)
        mode = 0;
    if (mode > 3)
        mode = 3;
#ifdef __ANDROID__
    /* 4xMSAA quadruples the tile bandwidth on mobile GPUs and stalled the
     * whole frame loop; a cfg value of 3 clamps to MSAA×2 (same reasoning
     * as the Android RenderQuality clamp). */
    if (mode == 3) {
        Log_Info("[GL] Android: AntiAlias 3 (MSAA×4) clamped to 2");
        mode = 2;
    }
#endif
    if (s_antiAlias == mode)
        return;
    Log_Info("[GL] SetAntiAlias: %d -> %d", s_antiAlias, mode);
    s_antiAlias = mode;
    if (!s_initialized || !Window_GLMakeCurrent())
        return;

    /* A target-mode change recreates the scene FBOs itself.  This also
     * covers leaving MSAA for FXAA, where the double-wide scene cannot be
     * used because the post-process needs a per-eye composite. */
    {
        S32 wantDoubleWide = GL_StereoDoubleWideAllowed();
        if (wantDoubleWide != s_stereoDoubleWide) {
            GL_SetStereoTargetMode(wantDoubleWide);
            return;
        }
    }

    if ((s_antiAlias == 2 || s_antiAlias == 3) && s_fbo) {
        GLint samples = (s_antiAlias == 3) ? 4 : 2;
        GL_CreateMSAAFbo(GL_ScaleRenderDim(s_texWidth),
                         GL_ScaleRenderDim(s_texHeight), samples);
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

/* CLUT + palette baked together.  The old shader did CLUT(index)→palette
   per texel (and 4x per bilinear tap), which is the dominant fragment cost
   at 1.5x supersampling.  A 256×256 RGBA table holding the final palette
   colors lets the shader return the color in one fetch.  Rebuilt only when
   the fog table or palette generation changes (scene/palette transitions). */
static U32 s_bakedCLUT[256 * 256];

void GL_TerrainResolvedInvalidate(void) {
    s_terrainResolvedValid = 0;
    s_terrainResolvedDirty = 1;
}

static void GL_UploadBakedCLUT(void) {
    static U32 s_upGen = 0;
    static const U8 *s_upPtr = NULL;
    static U32 s_upPalGen = 0;
    const U32 *lut = GL_GetPaletteLUT();
    if (PtrCLUTFog == NULL || lut == NULL) {
        return;
    }
    const U32 palGen = GL_GetPaletteGeneration();
    if (s_upGen == CLUTGeneration && s_upPtr == PtrCLUTFog &&
        s_upPalGen == palGen) {
        return;
    }

    U32 paletteCopy[256];
    memcpy(paletteCopy, lut, sizeof(paletteCopy));
    paletteCopy[0] &= 0x00FFFFFF; /* palette index 0 is transparent */

    const U8 *clut = PtrCLUTFog;
    for (U32 i = 0; i < 256u * 256u; i++) {
        s_bakedCLUT[i] = paletteCopy[clut[i]];
    }

    GL_BindTexUnit(0, s_clutTex);
#if defined(__ANDROID__)
    {
        static U32 bakedGL[256 * 256];
        GL_SwizzleBGRAtoRGBA(bakedGL, s_bakedCLUT, 256 * 256);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_UPLOAD_FORMAT,
                        GL_UNSIGNED_BYTE, bakedGL);
    }
#else
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_UPLOAD_FORMAT,
                    GL_UNSIGNED_BYTE, s_bakedCLUT);
#endif
    GL_BindTexUnit(0, 0);

    s_boundCLUT = PtrCLUTFog;
    s_upGen = CLUTGeneration;
    s_upPtr = PtrCLUTFog;
    s_upPalGen = palGen;
    GL_TerrainResolvedInvalidate();
}

static void GL_UploadPalette(void) {
    /* Uploaded once per frame per LUT pointer, not per polygon — same
       reasoning as GL_UploadCLUT above. */
    static U32 s_palUpGen = 0;
    const U32 *lut = GL_GetPaletteLUT();
    if (lut == NULL) {
        return;
    }
    const U32 gen = GL_GetPaletteGeneration();
    if (s_palUpGen == gen) {
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
    s_palUpGen = gen;

    /* The baked CLUT embeds palette colors; refresh it on a palette change
       even when the fog table pointer is unchanged. */
    GL_UploadBakedCLUT();
}

static void GL_UploadCLUT(void) {
    /* The baked CLUT+palette table is uploaded once per generation, not per
       polygon.  GL_RenderTriangleList runs for every terrain tile; a 256 KB
       upload per polygon would be worse than the old 64 KB LUMINANCE table.
       The table only changes at scene/palette transitions. */
    GL_UploadBakedCLUT();
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
    s_profAtlasUploads++;
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
    S32 pin; /* static decor VBOs hold their layers */
} GL_ArrSlot;
static GL_ArrSlot s_arrSlots[GL_ARR_LAYERS];
static U32 s_arrUseCounter = 0;
static GLuint s_arrayTex = 0;
static GLuint s_boundArrayTex = 0; /* array currently bound on unit 3 */
static GLuint s_arrayQuadTex = 0;  /* 2x2-packed RGBA array on unit 4 */
static GLuint s_boundArrayQuadTex = 0;
static S32 s_arrayQuadValid = 0;
static U8 s_arrStage[256 * 256];
static U8 s_arrQuadStage[256 * 256 * 4];

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
        if (s_arrSlots[i].pin == 0 &&
            (lru == NULL || s_arrSlots[i].lastUse < lru->lastUse)) {
            lru = &s_arrSlots[i];
        }
    }
    if (slot == NULL) {
        if (s_arrCount < GL_ARR_LAYERS) {
            slot = &s_arrSlots[s_arrCount++];
        } else {
            slot = lru;
        }
        if (slot == NULL) {
            return NULL; /* every layer is pinned by a static decor VBO */
        }
        slot->map = map;
        slot->w = (U16)w;
        slot->h = (U16)h;
        slot->ver = 0; /* force upload */
        slot->layer = (S32)(slot - s_arrSlots);
    }
    slot->lastUse = s_arrUseCounter;
    if (s_arrCount >= GL_ARR_LAYERS && slot == lru) {
        s_arrEvictions++; /* layer stolen from a map (may be in flight) */
    }
    return slot;
}

S32 GL_DecorArrayPin(const void *map, U32 w, U32 h) {
    /* Leave half the layers for the per-frame batch.  A pinned layer can
       never be evicted, and the batch path assumes GL_ArraySlotFor finds a
       slot; bounding the static cache keeps that invariant after many
       cubes without needing a null path through every batch caller. */
    U32 pinned = 0;
    for (S32 i = 0; i < s_arrCount; i++) {
        if (s_arrSlots[i].pin > 0) {
            pinned++;
        }
    }
    if (pinned >= GL_ARR_LAYERS / 2) {
        return -1;
    }
    GL_ArrSlot *slot = GL_ArraySlotFor(map, w, h);
    if (slot == NULL) {
        return -1;
    }
    slot->pin++;
    return slot->layer;
}

void GL_DecorArrayUnpin(S32 layer) {
    if (layer >= 0 && layer < s_arrCount && s_arrSlots[layer].pin > 0) {
        s_arrSlots[layer].pin--;
    }
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

    /* 2x2-packed companion array: each texel carries the centre, right,
       bottom and diagonal palette indices.  One fetch then feeds all four
       CLUT taps.  If allocation fails the shader falls back to four R8
       fetches through uAtlasQuadValid. */
    glGenTextures(1, &s_arrayQuadTex);
    if (s_arrayQuadTex != 0) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, s_arrayQuadTex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 256, 256,
                     GL_ARR_LAYERS, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                        GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,
                        GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
    }
    s_arrayQuadValid = (s_arrayQuadTex != 0) ? 1 : 0;
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    s_boundArrayTex = 0;
    s_boundArrayQuadTex = 0;
}

/* Upload stale layers as tiled regions, then bind the array on unit 3 and
   its 2x2-packed companion on unit 4.  Called from FlushActiveBatch on
   array batches. */
static void GL_ArrayBind(void) {
    S32 i;
    S32 uploaded = 0;
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
            if (s_arrayQuadTex != 0) {
                for (y = 0; y < 256; y++) {
                    const U8 *row0 = s_arrStage + y * 256;
                    const U8 *row1 = s_arrStage + ((y + 1) & 255) * 256;
                    U8 *dst = s_arrQuadStage + y * 256 * 4;
                    for (x = 0; x < 256; x++) {
                        dst[0] = row0[x];
                        dst[1] = row0[(x + 1) & 255];
                        dst[2] = row1[x];
                        dst[3] = row1[(x + 1) & 255];
                        dst += 4;
                    }
                }
                glBindTexture(GL_TEXTURE_2D_ARRAY, s_arrayQuadTex);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0,
                                s_arrSlots[i].layer, 256, 256, 1, GL_RGBA,
                                GL_UNSIGNED_BYTE, s_arrQuadStage);
            }
            s_profArrayUploads++;
            s_arrSlots[i].ver = s_atlasContentVersion;
            uploaded = 1;
        }
    }
    if (uploaded) {
        /* The uploads above may have rebound unit 3/4; force the final
           bind and leave the caches consistent. */
        s_boundArrayTex = 0;
        s_boundArrayQuadTex = 0;
    }
    /* GL_BindTexUnit binds the GL_TEXTURE_2D target; the array sampler
       reads the GL_TEXTURE_2D_ARRAY target on the unit, so bind the array
       texture directly under its own target. */
    if (s_curActiveUnit != 3) {
        glActiveTexture(GL_TEXTURE3);
        s_curActiveUnit = 3;
    }
    if (s_boundArrayTex != s_arrayTex) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, s_arrayTex);
        s_boundArrayTex = s_arrayTex;
    }
    if (s_curActiveUnit != 4) {
        glActiveTexture(GL_TEXTURE4);
        s_curActiveUnit = 4;
    }
    if (s_arrayQuadTex != 0 &&
        s_boundArrayQuadTex != s_arrayQuadTex) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, s_arrayQuadTex);
        s_boundArrayQuadTex = s_arrayQuadTex;
    }
    glUniform1i(s_clut3_uAtlasQuadValid, s_arrayQuadValid);
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
    if (prog == s_clut3Program) {
        if (s_clut3_uDither >= 0)
            glUniform1f(s_clut3_uDither, DisplayDither ? 1.0f : 0.0f);
    } else if (s_clut_uDither >= 0) {
        glUniform1f(s_clut_uDither, DisplayDither ? 1.0f : 0.0f);
    }

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
       RepMask Material.  Re-specified whenever the ring slot or the program
       changes (the legacy and array programs can assign different slots). */
    if (s_clutAttribsDirty || s_clutAttribsBase != (GLintptr)s_batchRingBase ||
        s_clutAttribsProgram != prog) {
        const GLvoid *ringBase = (const GLvoid *)(uintptr_t)s_batchRingBase;
        const S32 useArr = (prog == s_clut3Program);
        const GLint aPos = useArr ? s_clut3_aPos : s_clut_aPos;
        const GLint aTexCoord =
            useArr ? s_clut3_aTexCoord : s_clut_aTexCoord;
        const GLint aLight = useArr ? s_clut3_aLight : s_clut_aLight;
        const GLint aZ = useArr ? s_clut3_aZ : s_clut_aZ;
        const GLint aRepMask = useArr ? s_clut3_aRepMask : s_clut_aRepMask;
        const GLint aMaterial = useArr ? s_clut3_aMaterial : s_clut_aMaterial;
        glEnableVertexAttribArray(aPos);
        glVertexAttribPointer(aPos, 2, GL_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 0));
        glEnableVertexAttribArray(aTexCoord);
        glVertexAttribPointer(aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 4));
        glEnableVertexAttribArray(aLight);
        glVertexAttribPointer(aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 8));
        glEnableVertexAttribArray(aZ);
        glVertexAttribPointer(aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 10));
        glEnableVertexAttribArray(aRepMask);
        glVertexAttribPointer(aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 12));
        glEnableVertexAttribArray(aMaterial);
        glVertexAttribPointer(aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                              (const void *)((const U8 *)ringBase + 14));
        s_clutAttribsDirty = 0;
        s_clutAttribsBase = (GLintptr)s_batchRingBase;
        s_clutAttribsProgram = prog;
    }

    /* Depth testing: enable for ALL valid types (0–24).

       The SW rasterizer occludes non-OBJ_ZBUFFER objects (NPCs, sprites)
       behind terrain via ZBufBoxOverWrite2(): a post-process that restores
       closer terrain pixels over the already-rendered object.  The GL
       equivalent is a depth test during rendering: the depth buffer still
       holds terrain Z values from the prior AffGrilleExt() pass.

       GetZO3/GetZO4 compute Pt_ZO for perspective bodies and, while the
       GL world flag is active, for isometric interior bodies as well.  That
       gives all body polygons valid Z values for depth comparison.  This
       includes FLAT/SOLID types 0–7 which are common in NPC body models and
       decoration objects.

       Depth-write is gated separately:
       - Types 16–24 always write (they are inherently Z-buffered).
       - Types 0–15 write when Fill_Flag_ZBuffer was TRUE at batch start
         (terrain overlays/base rendered under FILL_POLY_FOG_ZBUFFER), or
         when the batch was collected while the GL world flag was active.
         The latter keeps interior bodies in the FBO depth buffer so their
         lines, spheres and sprite overlays can be occluded by the body.
       - Types 0–15 without Z-buffered fillers in the software path Z-test
         only — matches SW ZBufBoxOverWrite2 which reads terrain Z but never
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

        if (s_batchZBuffer || s_currentTypePoly >= POLY_TEXTURE_Z ||
            s_batchWorld) {
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

    /* Draw all batched triangles.  The first flush of the frame binds the
       scene target; mid-frame paths that bind another target either restore
       the scene themselves or invalidate the tracked binding.  Checking the
       cache avoids re-binding the same FBO on every one of the hundreds of
       flushes a terrain/object pass can emit. */
    if (s_fbo) {
        GLuint sceneFbo = (s_msaaFbo && (s_antiAlias == 2 || s_antiAlias == 3))
                              ? s_msaaFbo
                              : s_fbo;
        if (s_boundDrawFbo != sceneFbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        }
    }
    if (s_singlePassStereo) {
        if (s_stereoEye == 1) {
            /* Eye 1's 2D overlay is already present in both halves from the
               eye-0 pass; discard both pools so a deferred eye-1 batch cannot
               resurface under eye 0 next frame. */
            s_batchVertCount = 0;
            s_batchTriCount = 0;
            s_deferVertCount = 0;
            s_deferTriCount = 0;
            s_profAccumTicks[GL_PROF_FLUSH] +=
                SDL_GetPerformanceCounter() - profT0;
            return;
        }
        const GLint useArr = (prog == s_clut3Program);
        if (useArr) {
            glUniform1f(s_clut3_uSinglePass, 1.0f);
            glUniform3f(s_clut3_uEyeWorld, s_eye1World[0], s_eye1World[1],
                        s_eye1World[2]);
            glUniform2f(s_clut3_uEyeProj, s_eye1Proj[0], s_eye1Proj[1]);
            glUniform3f(s_clut3_uBaseWorld, s_baseWorld[0], s_baseWorld[1],
                        s_baseWorld[2]);
            glUniform2f(s_clut3_uBaseProj, s_baseProj[0], s_baseProj[1]);
            glUniform2f(s_clut3_uEyeFocal, FRatioX, FRatioY);
            glUniform1f(s_clut3_uEyeZScale,
                        (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0));
            glUniform1f(s_clut3_uEye, 0.0f);
            GL_BeginSinglePassEyeDraw(0);
            glDrawArrays(GL_TRIANGLES, 0, s_batchVertCount);
            glUniform1f(s_clut3_uEye, 1.0f);
            GL_BeginSinglePassEyeDraw(1);
            glDrawArrays(GL_TRIANGLES, 0, s_batchVertCount);
            GL_EndSinglePassEyeDraw();
            glUniform1f(s_clut3_uEye, 0.0f);
        } else {
            glUniform1f(s_clut_uSinglePass, 1.0f);
            glUniform3f(s_clut_uEyeWorld, s_eye1World[0], s_eye1World[1],
                        s_eye1World[2]);
            glUniform2f(s_clut_uEyeProj, s_eye1Proj[0], s_eye1Proj[1]);
            glUniform3f(s_clut_uBaseWorld, s_baseWorld[0], s_baseWorld[1],
                        s_baseWorld[2]);
            glUniform2f(s_clut_uBaseProj, s_baseProj[0], s_baseProj[1]);
            glUniform2f(s_clut_uEyeFocal, FRatioX, FRatioY);
            glUniform1f(s_clut_uEyeZScale,
                        (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0));
            glUniform1f(s_clut_uEye, 0.0f);
            GL_BeginSinglePassEyeDraw(0);
            glDrawArrays(GL_TRIANGLES, 0, s_batchVertCount);
            glUniform1f(s_clut_uEye, 1.0f);
            GL_BeginSinglePassEyeDraw(1);
            glDrawArrays(GL_TRIANGLES, 0, s_batchVertCount);
            GL_EndSinglePassEyeDraw();
            glUniform1f(s_clut_uEye, 0.0f);
        }
    } else {
        if (prog == s_clut3Program) {
            glUniform1f(s_clut3_uSinglePass, 0.0f);
            glUniform1f(s_clut3_uEye, 0.0f);
        } else {
            glUniform1f(s_clut_uSinglePass, 0.0f);
            glUniform1f(s_clut_uEye, 0.0f);
        }
        glDrawArrays(GL_TRIANGLES, 0, s_batchVertCount);
    }

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
    if (s_batchVertCount == 0 && s_deferVertCount == 0) {
        return;
    }
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

/* ═══════════════════════════════════════════════════════════════════════════
   Static terrain path — vertex-shader projection
   ═══════════════════════════════════════════════════════════════════════════ */

/* Animated polys (water, menu plasma) mutate pages in place.  A global
   version bump would re-upload every cached page and every active array
   layer on the next flush — a 16 MB spike when the water ticks.  Mark only
   the pages named by the mutators; GL_BindAtlasFor and GL_ArrayBind upload
   each at flush time.  An unnamed page or list overflow keeps the
   whole-cache bump.  Shared by the batch path and the terrain VBO path,
   which also samples the ground page without going through
   GL_RenderTriangleList. */
static void GL_ProcessAtlasDirty(void) {
    if (!AtlasTextureDirty) {
        return;
    }
    if (!AtlasTextureDirtyAll && AtlasTextureDirtyCount > 0) {
        /* Version 0 forces the flush-time upload without moving the global
           sequence, so unrelated pages stay resident. */
        for (S32 p = 0; p < AtlasTextureDirtyCount; p++) {
            const void *dirtyPage = (const void *)AtlasTextureDirtyPtrs[p];
            for (S32 i = 0; i < ATLAS_CACHE_SIZE; i++) {
                if (GL_AtlasSamePage(s_atlasTexCache[i].ptr, dirtyPage)) {
                    s_atlasTexCache[i].ver = 0;
                }
            }
            for (S32 i = 0; i < s_arrCount; i++) {
                if (GL_AtlasSamePage(s_arrSlots[i].map, dirtyPage)) {
                    s_arrSlots[i].ver = 0;
                }
            }
        }
        s_profAtlasDirtyKnown++;
    } else {
        s_atlasContentVersion++;
        s_profAtlasDirtyGlobal++;
    }
    AtlasTextureDirty = 0;
    AtlasTextureDirtyCount = 0;
    AtlasTextureDirtyAll = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Static terrain path — vertex-shader projection
   ═══════════════════════════════════════════════════════════════════════════ */

S32 GL_TerrainGPUActive(void) {
    static S32 s_force = -1;
    if (s_force < 0) {
        const char *e = getenv("LBA2_GPU_TERRAIN");
        s_force = (e != NULL && e[0] == '0')   ? 0
                  : (e != NULL && e[0] == '1') ? 1
                                               : -2;
    }
    if (s_force == 0) {
        return 0;
    }
    if (!s_initialized || s_terrainProgram == 0 || s_fbo == 0) {
        return 0;
    }
    /* Stereo is the target: both eyes re-project the same VBOs with their
       own camera uniforms, replacing the old horizon-capture shortcut.
       Mono stays on the CPU path unless explicitly forced for testing. */
    return (s_force == 1) ? 1 : (GL_GetStereoEye() >= 0);
}

U32 GL_TerrainVBOEpoch(void) { return s_terrainEpoch; }

U32 GL_CreateTerrainVBO(const T_GL_TerrainVertex *verts, U32 vertexCount) {
    if (!s_initialized || verts == NULL || vertexCount == 0) {
        return 0;
    }
    if (!Window_GLMakeCurrent()) {
        return 0;
    }

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    if (vbo == 0) {
        return 0;
    }
    GL_BindArrayBuffer(vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(vertexCount * sizeof(T_GL_TerrainVertex)), verts,
                 GL_STATIC_DRAW);
    GL_BindArrayBuffer(0);
    return (U32)vbo;
}

void GL_DeleteTerrainVBO(U32 vbo) {
    if (vbo == 0) {
        return;
    }
    if (s_terrainBoundVBO == vbo) {
        s_terrainBoundVBO = 0;
    }
    /* After shutdown the handle belongs to a dead context: dropping it is
       the only safe action, and the epoch counter stops the owner reusing
       it. */
    if (!s_initialized) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    GLuint glVbo = (GLuint)vbo;
    if (s_curArrayBuf == glVbo) {
        GL_BindArrayBuffer(0);
    }
    glDeleteBuffers(1, &glVbo);
}

/* Build and upload the one-fetch terrain table.  Row r of the array is the
   full 256x256 atlas page resolved through palette row r; sampling that
   layer with linear U/V returns exactly the four-tap bilinear CLUT mix. */
static void GL_EnsureTerrainResolved(const void *page) {
    if (page == NULL || s_terrainProgram == 0 || s_terrain_uResolved < 0) {
        s_terrainResolvedValid = 0;
        return;
    }
    const U32 palGen = GL_GetPaletteGeneration();
    if (s_terrainResolvedValid && !s_terrainResolvedDirty &&
        s_terrainResolvedPage == page &&
        s_terrainResolvedCLUTGen == CLUTGeneration &&
        s_terrainResolvedPalGen == palGen) {
        return;
    }

    const U8 *atlas = (const U8 *)page;
    for (U32 row = 0; row < 256; row++) {
        const U32 *clutRow = s_bakedCLUT + row * 256;
        U32 *dst = s_terrainResolvedData + row * 256 * 256;
        for (U32 i = 0; i < 256u * 256u; i++) {
            dst[i] = clutRow[atlas[i]];
        }
    }

    if (s_terrainResolvedTex == 0) {
        glGenTextures(1, &s_terrainResolvedTex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, s_terrainResolvedTex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 256, 256, 256, 0,
                     GL_UPLOAD_FORMAT, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, s_terrainResolvedTex);
#if defined(__ANDROID__)
    GL_SwizzleBGRAtoRGBA(s_terrainResolvedData, s_terrainResolvedData,
                         256u * 256u * 256u);
#endif
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 256, 256, 256,
                    GL_UPLOAD_FORMAT, GL_UNSIGNED_BYTE, s_terrainResolvedData);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    s_boundTerrainResolvedTex = 0;

    s_terrainResolvedPage = page;
    s_terrainResolvedCLUTGen = CLUTGeneration;
    s_terrainResolvedPalGen = palGen;
    s_terrainResolvedValid = 1;
    s_terrainResolvedDirty = 0;
}

void GL_DrawTerrainVBO(U32 vbo, U32 vertexCount, const void *atlasPage,
                       S32 startZFog, S32 clipZFar, S32 palLevel) {
    if (!s_initialized || s_terrainProgram == 0 || s_fbo == 0 || vbo == 0 ||
        vertexCount == 0) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    if (s_singlePassStereo && s_stereoEye == 1) {
        return;
    }

    /* Decors were queued just before this call; draw them first so the
       terrain depth-tests against them exactly as the CPU order does. */
    GL_FlushBatch();

    {
        GLuint sceneFbo = GL_SceneDrawFbo();
        if (sceneFbo != 0) {
            if (s_boundDrawFbo != sceneFbo) {
                glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
            }
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, (GLsizei)s_texWidth, (GLsizei)s_texHeight);
        }
    }

    GL_SetDepthTest(1);
    GL_SetDepthFunc(GL_GEQUAL);
    GL_SetDepthWrite(1);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* The CPU TestVuePoly accepts the screen-space winding that is CCW once
       the shader flips Y into NDC; GL's front face follows the same rule.
       LBA2_TERRAIN_NOCULL is a diagnostic escape hatch for comparing the
       VBO path against the batch renderer. */
    static S32 s_noCull = -1;
    if (s_noCull < 0) {
        s_noCull = getenv("LBA2_TERRAIN_NOCULL") ? 1 : 0;
    }
    if (!s_noCull) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    } else {
        glDisable(GL_CULL_FACE);
    }

    GL_UseProgramCached(s_terrainProgram);
    if (s_terrain_uDither >= 0)
        glUniform1f(s_terrain_uDither, DisplayDither ? 1.0f : 0.0f);

    GL_ProcessAtlasDirty();
    GL_UploadCLUT();
    GL_UploadPalette();
    GL_BindAtlasFor(atlasPage, 0xFFFF);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    GL_EnsureTerrainResolved(atlasPage);
    S32 useResolved = s_terrainResolvedValid && s_terrain_uResolved >= 0;
    if (useResolved) {
        if (s_curActiveUnit != 5) {
            glActiveTexture(GL_TEXTURE5);
            s_curActiveUnit = 5;
        }
        if (s_boundTerrainResolvedTex != s_terrainResolvedTex) {
            glBindTexture(GL_TEXTURE_2D_ARRAY, s_terrainResolvedTex);
            s_boundTerrainResolvedTex = s_terrainResolvedTex;
        }
        glUniform1i(s_terrain_uResolved, 5);
        glUniform1i(s_terrain_uResolvedValid, 1);
    } else {
        glUniform1i(s_terrain_uResolvedValid, 0);
    }

    /* MatriceWorld is row-major and float (X' = M11*x + M12*y + M13*z,
       LongRotatePointF loads the same F fields).  GLSL's mat3 * vec3 reads
       columns, so pass the transpose with transpose=off. */
    {
        GLfloat rot[9];
        rot[0] = MatriceWorld.F.M11;
        rot[1] = MatriceWorld.F.M21;
        rot[2] = MatriceWorld.F.M31;
        rot[3] = MatriceWorld.F.M12;
        rot[4] = MatriceWorld.F.M22;
        rot[5] = MatriceWorld.F.M32;
        rot[6] = MatriceWorld.F.M13;
        rot[7] = MatriceWorld.F.M23;
        rot[8] = MatriceWorld.F.M33;
        glUniformMatrix3fv(s_terrain_uRot, 1, GL_FALSE, rot);
    }
    glUniform3f(s_terrain_uCamera, (GLfloat)CameraXr, (GLfloat)CameraYr,
                (GLfloat)CameraZr);
    glUniform2f(s_terrain_uFocal, FRatioX, FRatioY);
    glUniform2f(s_terrain_uCentre, (GLfloat)XCentre, (GLfloat)YCentre);
    glUniform1f(s_terrain_uProjShiftX, (GLfloat)StereoProjShiftX);
    if (s_texWidth > 0 && s_texHeight > 0) {
        glUniform2f(s_terrain_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                    2.0f / (GLfloat)s_texHeight);
    }
    glUniform1f(s_terrain_uZBufferFactor,
                (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0));
    glUniform1f(s_terrain_uNearClip, (GLfloat)NearClip);
    glUniform1f(s_terrain_uStartZFog, (GLfloat)startZFog);
    if (clipZFar > startZFog) {
        glUniform1f(s_terrain_uFogStepScale,
                    15.0f / (GLfloat)(clipZFar - startZFog));
    } else {
        glUniform1f(s_terrain_uFogStepScale, 0.0f);
    }
    glUniform1f(s_terrain_uPalLevel, (GLfloat)palLevel);
    glUniform1i(s_terrain_uBilinear, s_bilinearCLUT);
    glUniform1f(s_terrain_uTerrainFarClip, 1.0f);
    glUniform1f(s_terrain_uTerrainFar, (GLfloat)clipZFar);

    GL_BindArrayBuffer((GLuint)vbo);
    const GLsizei stride = (GLsizei)sizeof(T_GL_TerrainVertex);
    glEnableVertexAttribArray(s_terrain_aPos);
    glVertexAttribPointer(
        s_terrain_aPos, 3, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_TerrainVertex, X));
    glEnableVertexAttribArray(s_terrain_aTexCoord);
    glVertexAttribPointer(
        s_terrain_aTexCoord, 2, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_TerrainVertex, U));
    glEnableVertexAttribArray(s_terrain_aLight);
    glVertexAttribPointer(
        s_terrain_aLight, 1, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_TerrainVertex, Light));
    glEnableVertexAttribArray(s_terrain_aRepMask);
    glVertexAttribPointer(
        s_terrain_aRepMask, 1, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_TerrainVertex, RepMask));
    glEnableVertexAttribArray(s_terrain_aMaterial);
    glVertexAttribPointer(
        s_terrain_aMaterial, 1, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_TerrainVertex, Material));
    glEnableVertexAttribArray(s_terrain_aFogPos);
    glVertexAttribPointer(
        s_terrain_aFogPos, 3, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_TerrainVertex, FogX));

    if (s_singlePassStereo) {
        glUniform1f(s_terrain_uSinglePass, 1.0f);
        glUniform3f(s_terrain_uEyeWorld, s_eye1World[0], s_eye1World[1],
                    s_eye1World[2]);
        glUniform2f(s_terrain_uEyeProj, s_eye1Proj[0], s_eye1Proj[1]);
        glUniform1f(s_terrain_uEye, 0.0f);
        GL_BeginSinglePassEyeDraw(0);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
        glUniform1f(s_terrain_uEye, 1.0f);
        GL_BeginSinglePassEyeDraw(1);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
        GL_EndSinglePassEyeDraw();
        glUniform1f(s_terrain_uEye, 0.0f);
    } else {
        glUniform1f(s_terrain_uSinglePass, 0.0f);
        glUniform1f(s_terrain_uEye, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
    }

    /* The batch's attribute pointers still name its ring; force a re-spec
       on the next batch flush.  Disable culling so the batch (and the
       one-shot draws) keep their unculled state. */
    s_terrainBoundVBO = vbo;
    s_clutAttribsDirty = 1;
    s_clutAttribsBase = -1;
    glDisable(GL_CULL_FACE);
    GL_BindArrayBuffer(0);
}

S32 GL_DecorGPUActive(void) {
    static S32 s_noDecorVBO = -1;
    if (s_noDecorVBO < 0) {
        s_noDecorVBO = getenv("LBA2_NO_DECOR_VBO") ? 1 : 0;
    }
    if (s_noDecorVBO) {
        return 0;
    }
    return (s_initialized && s_decorProgram != 0 && s_fbo != 0) ? 1 : 0;
}

U32 GL_DecorVBOEpoch(void) { return s_terrainEpoch; }

U32 GL_CreateDecorVBO(const T_GL_DecorVertex *verts, U32 vertexCount) {
    if (!s_initialized || verts == NULL || vertexCount == 0) {
        return 0;
    }
    if (!Window_GLMakeCurrent()) {
        return 0;
    }
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    if (vbo == 0) {
        return 0;
    }
    GL_BindArrayBuffer(vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(vertexCount * sizeof(T_GL_DecorVertex)), verts,
                 GL_STATIC_DRAW);
    GL_BindArrayBuffer(0);
    return (U32)vbo;
}

void GL_DeleteDecorVBO(U32 vbo) {
    if (vbo == 0 || !s_initialized) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    GLuint glVbo = (GLuint)vbo;
    if (s_curArrayBuf == glVbo) {
        GL_BindArrayBuffer(0);
    }
    glDeleteBuffers(1, &glVbo);
}

void GL_DrawDecorVBO(U32 vbo, U32 vertexCount, const void *atlasPage,
                     U16 atlasMask, S32 x, S32 y, S32 z, S32 fogRow,
                     S32 clipZFar) {
    if (!s_initialized || s_decorProgram == 0 || s_fbo == 0 || vbo == 0 ||
        vertexCount == 0) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    if (s_singlePassStereo && s_stereoEye == 1) {
        return;
    }
    /* Draw any CPU-fallback decor/render queued before this decor first. */
    GL_FlushBatch();
    GL_DrawDecorLegacyStreams();

    GLuint sceneFbo = GL_SceneDrawFbo();
    if (sceneFbo != 0) {
        if (s_boundDrawFbo != sceneFbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        }
        GL_SetSceneViewport();
    } else {
        glViewport(0, 0, (GLsizei)s_texWidth, (GLsizei)s_texHeight);
    }
    s_frameActive = 1;

    GL_SetDepthTest(1);
    GL_SetDepthFunc(GL_GEQUAL);
    GL_SetDepthWrite(1);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    static S32 s_decorNoCull = -1;
    if (s_decorNoCull < 0) {
        s_decorNoCull = getenv("LBA2_DECOR_NOCULL") ? 1 : 0;
    }
    if (!s_decorNoCull) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    } else {
        glDisable(GL_CULL_FACE);
    }

    GL_UseProgramCached(s_decorProgram);
    if (s_decor_uDither >= 0)
        glUniform1f(s_decor_uDither, DisplayDither ? 1.0f : 0.0f);
    GL_ProcessAtlasDirty();
    GL_UploadCLUT();
    GL_UploadPalette();
    GL_BindAtlasFor(atlasPage, atlasMask);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    {
        GLfloat rot[9];
        rot[0] = MatriceWorld.F.M11;
        rot[1] = MatriceWorld.F.M21;
        rot[2] = MatriceWorld.F.M31;
        rot[3] = MatriceWorld.F.M12;
        rot[4] = MatriceWorld.F.M22;
        rot[5] = MatriceWorld.F.M32;
        rot[6] = MatriceWorld.F.M13;
        rot[7] = MatriceWorld.F.M23;
        rot[8] = MatriceWorld.F.M33;
        glUniformMatrix3fv(s_decor_uRot, 1, GL_FALSE, rot);
    }
    glUniform3f(s_decor_uCamera, (GLfloat)CameraXr, (GLfloat)CameraYr,
                (GLfloat)CameraZr);
    glUniform3f(s_decor_uDecorPos, (GLfloat)x, (GLfloat)y, (GLfloat)z);
    glUniform2f(s_decor_uFocal, FRatioX, FRatioY);
    glUniform2f(s_decor_uCentre, (GLfloat)XCentre, (GLfloat)YCentre);
    glUniform1f(s_decor_uProjShiftX, (GLfloat)StereoProjShiftX);
    if (s_texWidth > 0 && s_texHeight > 0) {
        glUniform2f(s_decor_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                    2.0f / (GLfloat)s_texHeight);
    }
    glUniform1f(s_decor_uZBufferFactor,
                (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0));
    glUniform1f(s_decor_uNearClip, (GLfloat)NearClip);
    glUniform1f(s_decor_uFogRow, (GLfloat)fogRow);
    glUniform3f(s_decor_uLight, (GLfloat)CameraXLight,
                (GLfloat)CameraYLight, (GLfloat)CameraZLight);
    glUniform1f(s_decor_uLightFactor, 1.0f / 65536.0f);
    glUniform1f(s_decor_uTerrainFarClip, 1.0f);
    glUniform1f(s_decor_uTerrainPersp,
                DisplayPerspectiveTextures ? 1.0f : 0.0f);
    glUniform1f(s_decor_uTerrainFar, (GLfloat)clipZFar);
    /* Match the batch/terrain sampler; hardcoding nearest makes minified
       striped/stone decors alias into noise when the user picked bilinear. */
    glUniform1i(s_decor_uBilinear, s_bilinearCLUT);

    GL_BindArrayBuffer((GLuint)vbo);
    const GLsizei stride = (GLsizei)sizeof(T_GL_DecorVertex);
    glEnableVertexAttribArray(s_decor_aPos);
    glVertexAttribPointer(
        s_decor_aPos, 3, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_DecorVertex, X));
    glEnableVertexAttribArray(s_decor_aNormal);
    glVertexAttribPointer(
        s_decor_aNormal, 3, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_DecorVertex, NX));
    glEnableVertexAttribArray(s_decor_aTexCoord);
    glVertexAttribPointer(
        s_decor_aTexCoord, 2, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_DecorVertex, U));
    glEnableVertexAttribArray(s_decor_aMaterial);
    glVertexAttribPointer(
        s_decor_aMaterial, 1, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_DecorVertex, Material));
    glEnableVertexAttribArray(s_decor_aFlat);
    glVertexAttribPointer(
        s_decor_aFlat, 1, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_DecorVertex, Flat));
    glEnableVertexAttribArray(s_decor_aLayer);
    glVertexAttribPointer(
        s_decor_aLayer, 1, GL_FLOAT, GL_FALSE, stride,
        (const void *)(uintptr_t)offsetof(T_GL_DecorVertex, Layer));

    if (s_singlePassStereo) {
        glUniform1f(s_decor_uSinglePass, 1.0f);
        glUniform3f(s_decor_uEyeWorld, s_eye1World[0], s_eye1World[1],
                    s_eye1World[2]);
        glUniform2f(s_decor_uEyeProj, s_eye1Proj[0], s_eye1Proj[1]);
        glUniform3f(s_decor_uBaseWorld, s_baseWorld[0], s_baseWorld[1],
                    s_baseWorld[2]);
        glUniform2f(s_decor_uBaseProj, s_baseProj[0], s_baseProj[1]);
        glUniform1f(s_decor_uEye, 0.0f);
        GL_BeginSinglePassEyeDraw(0);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
        glUniform1f(s_decor_uEye, 1.0f);
        GL_BeginSinglePassEyeDraw(1);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
        GL_EndSinglePassEyeDraw();
        glUniform1f(s_decor_uEye, 0.0f);
    } else {
        glUniform1f(s_decor_uSinglePass, 0.0f);
        glUniform1f(s_decor_uEye, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
    }

    s_clutAttribsDirty = 1;
    s_clutAttribsBase = -1;
    s_clutAttribsProgram = 0;
    glDisable(GL_CULL_FACE);
    GL_BindArrayBuffer(0);
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
                       U32 level, S32 terrainBias) {
    Struc_Point pts[64];
    U16 tris[3 * 64];
    static U8 polyVerts[3 * 64 * BATCH_VERT_BYTES];
    S32 ntri;
    S32 nv;
    S32 i;
    /* Material: polyMode 6 (shadow multiply); the vertex colour byte is
       the shadow level — the shader derives (15 - level) / 15 from it. */
    const U16 shadowMat = (U16)(6 | ((U16)(level & 15) << 8));

    if (GL_SecondEyePass())
        return;
    if (!s_initialized || n < 3) {
        return;
    }
    if (n > 64) {
        n = 64;
    }

    /* An off-screen shadow has no fragments to contribute; culling it here
       avoids the batch flush and the full state setup below. */
    S32 mx0 = xs[0], mx1 = mx0, my0 = ys[0], my1 = my0;
    for (i = 1; i < n; i++) {
        if (xs[i] < mx0)
            mx0 = xs[i];
        if (xs[i] > mx1)
            mx1 = xs[i];
        if (ys[i] < my0)
            my0 = ys[i];
        if (ys[i] > my1)
            my1 = ys[i];
    }
    /* Keep a margin for the stereo eye offset: a shadow just outside the
       mono box can still land inside one eye. */
    if (mx1 < -64 || my1 < -64 || mx0 >= (S32)ModeDesiredX + 64 ||
        my0 >= (S32)ModeDesiredY + 64)
        return;

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
    /* The bias exists so the fill cannot punch a hole at its own centre
       against a hyperbolic terrain depth, which only the exterior has.
       Without terrain the bias would lift the shadow off the depth of the
       floor it lies on, and the eye offset goes as 1/aZ, so a few hundred aZ
       units show up as a visible offset in the right eye. */
    S32 bias = terrainBias ? (zmax - zmin) + 16 : 0;

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
    GL_MarkFBORegion(mx0, my0, mx1, my1);
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

        GL_DrawStereoClut(GL_TRIANGLES, 0, nv);
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
   body-poly acceptance.  The software path still sends non-Z-buffered body
   polys to Log; an FBO sphere there would be painted over by its own body
   (hair, eyes, buttons vanish).  The isometric GL world is different: the
   body polys themselves are FBO geometry, so spheres must follow them and
   use the isometric depth computed by GetZO3/GetZO4. */
S32 GL_SphereTargetsFBO(void) {
    if (!s_initialized) {
        return 0;
    }
    const S32 isoWorld = s_glWorld && TypeProj != TYPE_3D;
    if (!Fill_Flag_ZBuffer && !isoWorld) {
        return 0;
    }
    if (Fill_Flag_NZW && !s_fboHasTerrain && !isoWorld) {
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
    if (GL_SecondEyePass())
        return;
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
        memset(v + 4, 0, 4); /* uv = 0 */
        memset(v + 8, 0, 2); /* light = 0 */
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

    GL_DrawStereoClut(GL_TRIANGLE_FAN, 0, SEGMENTS + 2);
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
    if (GL_SecondEyePass())
        return;
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
        memset(v + 4, 0, 4); /* uv = 0 */
        memset(v + 8, 0, 2); /* light = 0 */
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

    GL_DrawStereoClut(GL_LINES, 0, 2);

    GL_SetDepthWrite(1);
    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

/* ── Batched flat rects for the interior particle flows ───────────────────
   A flow point paints a 2x2 palette block the way the SW BoxFlow does.  The
   scene lives in the FBO, so a flow drawn into Log is a fixed 2D stamp: in
   the near eye it stays at the mono position while the object it belongs to
   moves away from it.  Batching the blocks into one indexed draw with the
   point's own isometric depth keeps the flow on the room it belongs to. */
#define GL_FLOW_MAX_RECTS 512
static U8 s_flowVerts[GL_FLOW_MAX_RECTS * 6 * BATCH_VERT_BYTES];
static S32 s_flowRects = 0;
static S32 s_flowDepthTest = 0;

void GL_FlowBatchBegin(S32 depthTest) {
    s_flowRects = 0;
    s_flowDepthTest = depthTest;
}

static void GL_FlowBatchFlush(void) {
    if (GL_SecondEyePass()) {
        s_flowRects = 0;
        return;
    }
    if (s_flowRects <= 0) {
        return;
    }
    const S32 rects = s_flowRects;
    s_flowRects = 0;
    if (!s_initialized || !Window_GLMakeCurrent()) {
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

    GL_BindTexUnit(0, s_atlasTex);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    /* Exterior: depth-test the dots against the scene so a fountain behind a
       wall stays behind it.  The isometric interior keeps the painter order
       (its brick layer does not write depth). */
    GL_SetDepthTest(s_flowDepthTest);
    GL_SetDepthWrite(s_flowDepthTest);
    if (s_flowDepthTest) {
        GL_SetDepthFunc(GL_GEQUAL);
    }
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(s_batchVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizei)((size_t)rects * 6 * BATCH_VERT_BYTES), s_flowVerts,
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

    GL_DrawStereoClut(GL_TRIANGLES, 0, rects * 6);
    s_profTris += rects * 2;

    GL_SetDepthWrite(1);
    /* Keep the 3D depth path for the exterior so objects drawn after the
       flow still occlude correctly; the isometric interior stays 2D. */
    GL_SetDepthTest(s_flowDepthTest);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

void GL_FlowBatchRect(S32 x, S32 y, S32 colorIndex, U16 az) {
    if (GL_SecondEyePass())
        return;
    if (!s_initialized) {
        return;
    }
    if (x < -16384 || y < -16384 || x > 16384 || y > 16384) {
        return;
    }
    if (s_flowRects >= GL_FLOW_MAX_RECTS) {
        GL_FlowBatchFlush();
    }

    /* Material: polyMode 1 (flat palette colour), opaque. */
    const U16 mat = (U16)(1 | ((U16)(colorIndex & 0xFF) << 8));
    const S16 px[4] = {(S16)x, (S16)(x + 2), (S16)(x + 2), (S16)x};
    const S16 py[4] = {(S16)y, (S16)y, (S16)(y + 2), (S16)(y + 2)};
    static const S8 order[6] = {0, 1, 2, 0, 2, 3};
    U8 *dst = s_flowVerts + (size_t)s_flowRects * 6 * BATCH_VERT_BYTES;
    for (S32 i = 0; i < 6; i++) {
        U8 *v = dst + (size_t)i * BATCH_VERT_BYTES;
        const S32 c = order[i];
        const U16 uv = 0;
        const U16 light = 0;
        const U16 rep = 0;
        memcpy(v + 0, &px[c], 2);
        memcpy(v + 2, &py[c], 2);
        memcpy(v + 4, &uv, 2);
        memcpy(v + 6, &uv, 2);
        memcpy(v + 8, &light, 2);
        memcpy(v + 10, &az, 2);
        memcpy(v + 12, &rep, 2);
        memcpy(v + 14, &mat, 2);
    }
    s_flowRects++;
    GL_MarkFBORegion(x, y, x + 1, y + 1);
}

void GL_FlowBatchEnd(void) { GL_FlowBatchFlush(); }

/* ── Screen-space shade rect (GL ShadeBoxBlk replacement) ────────────────── */

/* The SW ShadeBoxBlk darkens the Log pixels in place through the fog
   window's fade rows — that works because in SW the Log holds the scene.
   In GL the Log overlay is empty where only 2D drew, so darkening it
   produces a solid band instead of a translucent one.  This multiplies
   the FBO rect's colors by (15 - level) / 15 instead — the linear
   counterpart of the fade row — with the depth test off so the band
   shades everything composited behind it. */
void GL_ShadeBox(S32 x0, S32 y0, S32 x1, S32 y1, S32 level) {
    if (GL_SecondEyePass())
        return;
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

    GL_DrawStereoClut(GL_TRIANGLES, 0, 6);

    GL_SetDepthWrite(1);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

static S32 GL_DecorBatchGrowBuf(U8 **verts, U32 *cap, U32 group) {
    U32 newCap = cap[group] ? cap[group] * 2 : 8192;
    if (newCap > DECOR_BATCH_MAX_VERTS) {
        newCap = DECOR_BATCH_MAX_VERTS;
    }
    if (newCap <= cap[group]) {
        return 0;
    }
    U8 *grown = (U8 *)realloc(verts[group], (size_t)newCap * BATCH_VERT_BYTES);
    if (grown == NULL) {
        return 0;
    }
    verts[group] = grown;
    cap[group] = newCap;
    return 1;
}

static S32 GL_DecorBatchGrow(U32 group) {
    return GL_DecorBatchGrowBuf(s_decorVerts, s_decorVertCap, group);
}

static S32 GL_DecorLegacyGrow(U32 group) {
    return GL_DecorBatchGrowBuf(s_decorLegacyVerts, s_decorLegacyCap, group);
}

/* Copy one 16-byte vertex per corner into a batch-format buffer:
   Pt_XE(2) + Pt_YE(2) + Pt_MapU(2) + Pt_MapV(2) +
   Pt_Light(2) + Pt_ZO(2) + Pt_RepMask(2) + Material(2).
   Pt_W (S32, offset 12 in Struc_Point) is skipped; Pt_RepMask (offset 16)
   lands at vertex offset 12 and the current polygon material at 14. */
static void GL_CopyBatchTriangle(U8 *dst, const Struc_Point *p0,
                                 const Struc_Point *p1,
                                 const Struc_Point *p2) {
    memcpy(dst + 0, p0, 12);                  /* XE, YE, MapU, MapV, Light, ZO */
    memcpy(dst + 12, (const U8 *)p0 + 16, 2); /* Pt_RepMask (skip Pt_W) */
    memcpy(dst + 14, &s_currentMaterial, 2);
    memcpy(dst + 16, p1, 12);
    memcpy(dst + 16 + 12, (const U8 *)p1 + 16, 2);
    memcpy(dst + 16 + 14, &s_currentMaterial, 2);
    memcpy(dst + 32, p2, 12);
    memcpy(dst + 32 + 12, (const U8 *)p2 + 16, 2);
    memcpy(dst + 32 + 14, &s_currentMaterial, 2);
}

static void GL_MarkTriangleRegion(const Struc_Point *p0, const Struc_Point *p1,
                                  const Struc_Point *p2) {
    S32 x0 = p0->Pt_XE, x1 = x0;
    S32 y0 = p0->Pt_YE, y1 = y0;
    if (p1->Pt_XE < x0)
        x0 = p1->Pt_XE;
    if (p1->Pt_XE > x1)
        x1 = p1->Pt_XE;
    if (p1->Pt_YE < y0)
        y0 = p1->Pt_YE;
    if (p1->Pt_YE > y1)
        y1 = p1->Pt_YE;
    if (p2->Pt_XE < x0)
        x0 = p2->Pt_XE;
    if (p2->Pt_XE > x1)
        x1 = p2->Pt_XE;
    if (p2->Pt_YE < y0)
        y0 = p2->Pt_YE;
    if (p2->Pt_YE > y1)
        y1 = p2->Pt_YE;
    GL_MarkFBORegion(x0, y0, x1, y1);
}

static void GL_AddTriangle(Struc_Point *p0, Struc_Point *p1,
                           Struc_Point *p2) {
    /* Decor window: append to the opaque or alpha decor stream.  The state
       that usually forces a batch flush (atlas class, alpha mode) rides per
       vertex or in the two stream split, so the decor pass becomes two draws
       at GL_DecorBatchEnd instead of a flush per state change. */
    if (s_decorBatch) {
        /* Match the shared batch's depth-write rule per polygon: alpha and
           non-Z-buffered decor polygons must not write depth, or their
           transparent/background texels can occlude the sea and sky. */
        const S32 alpha = (s_currentMatFlags & 8) ? 1 : 0;
        const S32 write =
            !alpha &&
            (s_batchZBuffer || s_currentTypePoly >= POLY_TEXTURE_Z);
        const S32 useArray = s_batchArray;
        const S32 fog = (s_batchNZW && s_currentTypePoly == POLY_TEXTURE_Z_FOG);
        const U32 group =
            useArray ? (alpha ? 1 : (write ? 0 : 2))
                     : (fog ? (alpha ? 4 : 3)
                            : (alpha ? 1 : (write ? 0 : 2)));
        if (!useArray && fog) {
            if (s_decorLegacyPage[group] == NULL) {
                s_decorLegacyPage[group] = s_batchAtlas;
            }
            s_decorLegacyMask[group] |= (U16)(RepMask & 0xFFFF);
        }
        U8 **verts = useArray ? s_decorVerts : s_decorLegacyVerts;
        U32 *counts = useArray ? s_decorVertCount : s_decorLegacyCount;
        U32 *caps = useArray ? s_decorVertCap : s_decorLegacyCap;

        /* Non-array decors precede the array stream; draw them before the
           first array decor so their image order stays that of the shared
           batch, which the sea relies on. */
        if (useArray && (s_decorLegacyCount[0] || s_decorLegacyCount[1] ||
                         s_decorLegacyCount[2] || s_decorLegacyCount[3] ||
                         s_decorLegacyCount[4])) {
            GL_DrawDecorLegacyStreams();
        }
        if (counts[group] + 3 > caps[group]) {
            const S32 grew =
                useArray ? GL_DecorBatchGrow(group) : GL_DecorLegacyGrow(group);
            if (!grew) {
                /* Out of room: fall back to the shared batch for the rest
                   of the window. */
                s_decorBatch = 0;
            }
        }
        if (s_decorBatch) {
            U8 *dst = verts[group] + (size_t)counts[group] * BATCH_VERT_BYTES;
            GL_CopyBatchTriangle(dst, p0, p1, p2);
            GL_MarkTriangleRegion(p0, p1, p2);
            counts[group] += 3;
            return;
        }
    }

    /* Flush if batch is full */
    if (s_batchVertCount + 3 > BATCH_MAX_VERTS) {
        GL_FlushBatch();
    }

    U8 *dst = s_batchVerts + s_batchVertCount * BATCH_VERT_BYTES;
    GL_CopyBatchTriangle(dst, p0, p1, p2);

    /* Track where the batch will draw (screen-space coords) for the
       region-bounded incremental restore. */
    GL_MarkTriangleRegion(p0, p1, p2);

    s_batchVertCount += 3;
    s_batchTriCount++;
}

static void GL_DrawDecorStream(const U8 *data, U32 vertices, S32 alpha,
                               S32 depthWrite) {
    if (vertices == 0) {
        return;
    }
    GLuint sceneFbo = GL_SceneDrawFbo();
    if (sceneFbo != 0) {
        if (s_boundDrawFbo != sceneFbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        }
        GL_SetSceneViewport();
    } else {
        glViewport(0, 0, (GLsizei)s_texWidth, (GLsizei)s_texHeight);
    }
    s_frameActive = 1;

    GL_SetDepthTest(1);
    GL_SetDepthFunc(GL_GEQUAL);
    GL_SetDepthWrite(depthWrite ? 1 : 0);
    GL_SetBlendFunc(alpha ? 1 : 0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    GL_UseProgramCached(s_clut3Program);
    GL_ProcessAtlasDirty();
    GL_UploadCLUT();
    GL_UploadPalette();
    GL_ArrayBind();
    GL_BindTexUnit(0, s_atlasTex);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);
    glUniform2f(s_clut3_uScreenScale, 2.0f / (GLfloat)s_texWidth,
                2.0f / (GLfloat)s_texHeight);
    glUniform2f(s_clut3_uAtlasScale, 256.0f, 256.0f);
    glUniform1f(s_clut3_uCLUTBaseRow, 0.0f);
    glUniform1f(s_clut3_uScaledFogNear, (GLfloat)Fill_ScaledFogNear);
    glUniform1f(s_clut3_uFogRowScale,
                (GLfloat)((double)Fill_Fog_Factor / 65536.0));
    glUniform1i(s_clut3_uBilinear, s_bilinearCLUT);
    glUniform1f(s_clut3_uTerrainFarClip, 0.0f);

    if (s_decorVBO == 0) {
        glGenBuffers(1, &s_decorVBO);
    }
    GL_BindArrayBuffer(s_decorVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertices * BATCH_VERT_BYTES, data,
                 GL_STREAM_DRAW);
    glEnableVertexAttribArray(s_clut3_aPos);
    glVertexAttribPointer(s_clut3_aPos, 2, GL_SHORT, GL_FALSE, 16,
                          (const void *)0);
    glEnableVertexAttribArray(s_clut3_aTexCoord);
    glVertexAttribPointer(s_clut3_aTexCoord, 2, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)4);
    glEnableVertexAttribArray(s_clut3_aLight);
    glVertexAttribPointer(s_clut3_aLight, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)8);
    glEnableVertexAttribArray(s_clut3_aZ);
    glVertexAttribPointer(s_clut3_aZ, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)10);
    glEnableVertexAttribArray(s_clut3_aRepMask);
    glVertexAttribPointer(s_clut3_aRepMask, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)12);
    glEnableVertexAttribArray(s_clut3_aMaterial);
    glVertexAttribPointer(s_clut3_aMaterial, 1, GL_UNSIGNED_SHORT, GL_FALSE, 16,
                          (const void *)14);

    GL_DrawStereoClut3(GL_TRIANGLES, 0, (GLsizei)vertices);

    s_clutAttribsDirty = 1;
    s_clutAttribsBase = -1;
    s_clutAttribsProgram = 0;
    GL_BindArrayBuffer(0);
}

/* Draw the non-array decor stream through the legacy CLUT program.  These
   polygons precede the array stream in the decor pass; the caller flushes
   them before the first array decor or a static decor VBO so the image
   order matches the shared batch. */
static void GL_DrawDecorLegacyStream(const U8 *data, U32 vertices, S32 alpha,
                                     S32 depthWrite, S32 depthAlways,
                                     const void *page, U16 mask) {
    if (vertices == 0) {
        return;
    }
    GLuint sceneFbo = GL_SceneDrawFbo();
    if (sceneFbo != 0) {
        if (s_boundDrawFbo != sceneFbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        }
        GL_SetSceneViewport();
    } else {
        glViewport(0, 0, (GLsizei)s_texWidth, (GLsizei)s_texHeight);
    }
    s_frameActive = 1;

    GL_SetDepthTest(1);
    GL_SetDepthFunc(depthAlways ? GL_ALWAYS : GL_GEQUAL);
    GL_SetDepthWrite(depthWrite ? 1 : 0);
    GL_SetBlendFunc(alpha ? 1 : 0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    GL_UseProgramCached(s_clutProgram);
    GL_ProcessAtlasDirty();
    GL_UploadCLUT();
    GL_UploadPalette();
    if (page != NULL) {
        GL_BindTexUnit(0, GL_BindAtlasFor(page, mask));
    } else {
        GL_BindTexUnit(0, s_atlasTex);
    }
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    {
        const GLfloat sx = 2.0f / (GLfloat)s_texWidth;
        const GLfloat sy = 2.0f / (GLfloat)s_texHeight;
        glUniform2f(s_clut_uScreenScale, sx, sy);
        s_ussX = sx;
        s_ussY = sy;
    }
    glUniform2f(s_clut_uAtlasScale, 256.0f, 256.0f);
    s_atlasScaleX = 256.0f;
    s_atlasScaleY = 256.0f;
    glUniform1f(s_clut_uCLUTBaseRow, 0.0f);
    s_clutRowCached = 0.0f;
    glUniform1f(s_clut_uScaledFogNear, (GLfloat)Fill_ScaledFogNear);
    s_fogNearCached = (GLfloat)Fill_ScaledFogNear;
    glUniform1f(s_clut_uFogRowScale,
                (GLfloat)((double)Fill_Fog_Factor / 65536.0));
    s_fogScaleCached = (GLfloat)((double)Fill_Fog_Factor / 65536.0);
    glUniform1i(s_clut_uBilinear, s_bilinearCLUT);
    s_bilinearCached = s_bilinearCLUT;

    if (s_decorVBO == 0) {
        glGenBuffers(1, &s_decorVBO);
    }
    GL_BindArrayBuffer(s_decorVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertices * BATCH_VERT_BYTES, data,
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

    GL_DrawStereoClut(GL_TRIANGLES, 0, (GLsizei)vertices);

    s_clutAttribsDirty = 1;
    s_clutAttribsBase = -1;
    s_clutAttribsProgram = 0;
    GL_BindArrayBuffer(0);
}

static void GL_DrawDecorLegacyStreams(void) {
    const U32 writeCount = s_decorLegacyCount[0];
    const U32 alphaCount = s_decorLegacyCount[1];
    const U32 noWriteCount = s_decorLegacyCount[2];
    const U32 fogWriteCount = s_decorLegacyCount[3];
    const U32 fogAlphaCount = s_decorLegacyCount[4];
    s_decorLegacyCount[0] = 0;
    s_decorLegacyCount[1] = 0;
    s_decorLegacyCount[2] = 0;
    s_decorLegacyCount[3] = 0;
    s_decorLegacyCount[4] = 0;
    if (!s_initialized || (writeCount == 0 && alphaCount == 0 &&
                           noWriteCount == 0 && fogWriteCount == 0 &&
                           fogAlphaCount == 0)) {
        return;
    }
    if (!Window_GLMakeCurrent()) {
        return;
    }
    GL_DrawDecorLegacyStream(s_decorLegacyVerts[0], writeCount, 0, 1, 0, NULL,
                             0);
    GL_DrawDecorLegacyStream(s_decorLegacyVerts[2], noWriteCount, 0, 0, 0,
                             NULL, 0);
    GL_DrawDecorLegacyStream(s_decorLegacyVerts[1], alphaCount, 1, 0, 0, NULL,
                             0);
    GL_DrawDecorLegacyStream(s_decorLegacyVerts[3], fogWriteCount, 0, 1, 1,
                             s_decorLegacyPage[3], s_decorLegacyMask[3]);
    GL_DrawDecorLegacyStream(s_decorLegacyVerts[4], fogAlphaCount, 1, 0, 1,
                             s_decorLegacyPage[4], s_decorLegacyMask[4]);
    s_decorLegacyPage[3] = NULL;
    s_decorLegacyPage[4] = NULL;
    s_decorLegacyMask[3] = 0;
    s_decorLegacyMask[4] = 0;
}

void GL_DecorBatchBegin(void) {
    s_decorVertCount[0] = 0;
    s_decorVertCount[1] = 0;
    s_decorVertCount[2] = 0;
    s_decorLegacyCount[0] = 0;
    s_decorLegacyCount[1] = 0;
    s_decorLegacyCount[2] = 0;
    s_decorLegacyCount[3] = 0;
    s_decorLegacyCount[4] = 0;
    s_decorLegacyPage[3] = NULL;
    s_decorLegacyPage[4] = NULL;
    s_decorLegacyMask[3] = 0;
    s_decorLegacyMask[4] = 0;
    static S32 s_noDecorBatch = -1;
    if (s_noDecorBatch < 0) {
        s_noDecorBatch = getenv("LBA2_NO_DECOR_BATCH") ? 1 : 0;
    }
    s_decorBatch = (!s_noDecorBatch && s_initialized && s_clut3Program != 0 &&
                    s_arrayWindow)
                       ? 1
                       : 0;
    if (s_decorBatch) {
        /* Anything queued before the decor window (previous pass) must keep
           its order relative to the decor stream. */
        GL_FlushBatch();
    }
}

void GL_DecorBatchEnd(void) {
    if (!s_decorBatch) {
        s_decorVertCount[0] = 0;
        s_decorVertCount[1] = 0;
        s_decorVertCount[2] = 0;
        s_decorLegacyCount[0] = 0;
        s_decorLegacyCount[1] = 0;
        s_decorLegacyCount[2] = 0;
        s_decorLegacyCount[3] = 0;
        s_decorLegacyCount[4] = 0;
        return;
    }
    s_decorBatch = 0;
    const U32 writeCount = s_decorVertCount[0];
    const U32 alphaCount = s_decorVertCount[1];
    const U32 noWriteCount = s_decorVertCount[2];
    const U32 legacyCount = s_decorLegacyCount[0] + s_decorLegacyCount[1] +
                            s_decorLegacyCount[2] + s_decorLegacyCount[3] +
                            s_decorLegacyCount[4];
    s_decorVertCount[0] = 0;
    s_decorVertCount[1] = 0;
    s_decorVertCount[2] = 0;
    if (!s_initialized ||
        (writeCount == 0 && alphaCount == 0 && noWriteCount == 0 &&
         legacyCount == 0)) {
        s_decorLegacyCount[0] = 0;
        s_decorLegacyCount[1] = 0;
        s_decorLegacyCount[2] = 0;
        s_decorLegacyCount[3] = 0;
        s_decorLegacyCount[4] = 0;
        return;
    }
    if (!Window_GLMakeCurrent()) {
        s_decorLegacyCount[0] = 0;
        s_decorLegacyCount[1] = 0;
        s_decorLegacyCount[2] = 0;
        s_decorLegacyCount[3] = 0;
        s_decorLegacyCount[4] = 0;
        return;
    }
    GL_DrawDecorLegacyStreams();
    GL_DrawDecorStream(s_decorVerts[0], writeCount, 0, 1);
    GL_DrawDecorStream(s_decorVerts[2], noWriteCount, 0, 0);
    GL_DrawDecorStream(s_decorVerts[1], alphaCount, 1, 0);
}

/* ── FBO clear (called at start of terrain rendering) ─────────────────────── */

/* Palette entry the scene FBO starts from.  The software renderer fills the
   screen with the clear colour (Cls), and every GL scene clear passes the
   same entry (see GL_SetSceneClearColor), so the pixels no polygon covers
   read alike.  Index 0 before the first scene is boot black. */
static U8 s_sceneClearPalIndex = 0;

void GL_SetSceneClearColor(U8 palIndex) {
    s_sceneClearPalIndex = palIndex;
}

void GL_ClearFBO(void) {
    if (!s_initialized) {
        return;
    }

    if (!Window_GLMakeCurrent()) {
        return;
    }

    if (s_singlePassStereo && s_stereoEye == 1) {
        return;
    }

    {
        GLuint drawFbo = GL_SceneDrawFbo();
        if (drawFbo != 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, drawFbo);
            GL_SetSceneViewport();
        } else {
            glViewport(0, 0, s_texWidth, s_texHeight);
        }
    }

    {
        /* Interior passthrough: pixels no polygon covers keep alpha 0 so the
         * present path can composite the room over the camera feed. */
        const GLfloat clearAlpha =
            OpenXR_InteriorPassthroughActive() ? 0.0f : 1.0f;
        const U32 *lut = GL_GetPaletteLUT();
        if (lut != NULL) {
            const U32 c = lut[s_sceneClearPalIndex];
            glClearColor((GLfloat)((c >> 16) & 0xFF) / 255.0f,
                         (GLfloat)((c >> 8) & 0xFF) / 255.0f,
                         (GLfloat)(c & 0xFF) / 255.0f, clearAlpha);
        }
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
    if (s_stereoDoubleWide && !s_singlePassStereo) {
        /* glClear ignores the viewport but honours the scissor.  Clear only
         * the current eye half, or eye 1 would erase eye 0's scene. */
        glEnable(GL_SCISSOR_TEST);
        glScissor(s_sceneEyeX, 0, (GLsizei)s_fboWidth, (GLsizei)s_fboHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    } else {
        /* Single-pass eye 0 owns both halves: they are written in this one
         * pass, so a half-scissored clear would leave last frame's second
         * eye in place. */
        glDisable(GL_SCISSOR_TEST);
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

    /* Screen-space rect -> FBO pixels (SSAA + VR supersampling), clamped. */
    S32 qx0 = (S32)GL_ScaleRenderDim((U32)s_fboDirtyX0);
    S32 qy0 = (S32)GL_ScaleRenderDim((U32)s_fboDirtyY0);
    S32 qx1 = (S32)GL_ScaleRenderDim((U32)(s_fboDirtyX1 + 1));
    S32 qy1 = (S32)GL_ScaleRenderDim((U32)(s_fboDirtyY1 + 1));
    if (qx0 < 0)
        qx0 = 0;
    if (qy0 < 0)
        qy0 = 0;
    if (qx1 > (S32)s_fboWidth)
        qx1 = (S32)s_fboWidth;
    if (qy1 > (S32)s_fboHeight)
        qy1 = (S32)s_fboHeight;
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
        glUniform1f(s_uSharpenLoc, 0.0f);
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
        GL_ForgetFboBinding();
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
    const S32 margin = (S32)GL_ScaleRenderDim(2);

    if (!s_fboDirtyActive || s_fboDirtyX1 < s_fboDirtyX0 ||
        s_fboWidth == 0 || s_fboHeight == 0) {
        return FALSE;
    }
    qx0 = (S32)GL_ScaleRenderDim((U32)s_fboDirtyX0) - margin;
    qy0 = (S32)GL_ScaleRenderDim((U32)s_fboDirtyY0) - margin;
    qx1 = (S32)GL_ScaleRenderDim((U32)(s_fboDirtyX1 + 1)) + margin;
    qy1 = (S32)GL_ScaleRenderDim((U32)(s_fboDirtyY1 + 1)) + margin;
    if (qx0 < 0)
        qx0 = 0;
    if (qy0 < 0)
        qy0 = 0;
    if (qx1 > (S32)s_fboWidth)
        qx1 = (S32)s_fboWidth;
    if (qy1 > (S32)s_fboHeight)
        qy1 = (S32)s_fboHeight;
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
        glUniform1f(s_uSharpenLoc, 0.0f);
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
    if (s_msaaColorTex)
        msaa += GL_TargetBytes(s_msaaW, s_msaaH, 4);
    if (s_msaaColorRb)
        msaa += GL_TargetBytes(s_msaaW, s_msaaH, 4);
    if (s_msaaDepthRb)
        msaa += GL_TargetBytes(s_msaaW, s_msaaH, 4);
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

    /* The present path and the window layer bind their own targets; the next
       frame's first flush must not trust the previous frame's cache. */
    GL_ForgetFboBinding();

    /* Read the texture filter toggle once per frame so GL_FlushBatch
       (which may fire mid-frame when the batch fills) uses the
       correct sampling mode for all batches.  The values map 1:1 onto the
       CLUT shader's uBilinear: 0 nearest, 1 bilinear, 2 Scale2x rules,
       3 xBR rules — the last two are per-texture edge-directed sampling. */
    s_bilinearCLUT = DisplayTexFilter;

    s_frameSeq++;
    GL_BatchFlushCount = 0;
    s_terrainRenderedThisFrame = 0;
    s_horizonValid = 0; /* horizon capture is per-frame (eye 0 only) */

    if (s_fakeStereoPx > 0 && s_singlePassStereo) {
        /* The harness has no eye loop: pretend eye 0 is current so the
           single-pass draws (which only issue from eye 0) run. */
        s_stereoEye = 0;
        /* Same formula the polygon stages apply: offset = -eye.x * focal *
           eyeZScale / aZ, so picking eye.x to land s_fakeStereoPx at
           aZ = 1000 reproduces a plausible interocular delta. */
        const double zScale = (double)Fill_ZBuffer_Factor / 65536.0;
        const double focal = (double)FRatioX;
        /* LBA2_INTERIOR_STEREO matches the depth scale the game applies to
           interior eye deltas, so the harness can show the flattened pair. */
        double depthScale = 1.0;
        const char *depthEnv = getenv("LBA2_INTERIOR_STEREO");
        if (depthEnv != NULL) {
            depthScale = (double)atoi(depthEnv) / 100.0;
            if (depthScale < 0.1)
                depthScale = 0.1;
            if (depthScale > 1.0)
                depthScale = 1.0;
        }
        if (focal > 0.0 && zScale > 0.0) {
            s_eye1World[0] = (float)(-(double)s_fakeStereoPx * depthScale *
                                     1000.0 / (focal * zScale));
            s_eye1World[1] = 0.0f;
            s_eye1World[2] = 0.0f;
            s_eye1Proj[0] = 0.0f;
            s_eye1Proj[1] = 0.0f;
        }
    }
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

    GLuint sceneDrawFbo = GL_SceneDrawFbo();
    if (sceneDrawFbo != 0 && sceneDrawFbo != s_fbo &&
        (s_antiAlias == 2 || s_antiAlias == 3)) {
        if (depthNative) {
            /* The PBO read below consumes a single-sample buffer, and at
               quality 1 that buffer is s_fbo itself — refresh only its
               depth (the color half of GL_ResolveMSAA is re-resolved by
               GL_Present and would be a full-surface copy wasted here). */
            glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneDrawFbo);
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

        /* With MSAA, the scene depth is in the scene target's depth renderbuffer:
       glBlitFramebuffer auto-resolves multisample to single-sample. */
        GLuint depthSrcFbo = sceneDrawFbo != 0 ? sceneDrawFbo : s_fbo;
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
/* World flag: when set, polygon paths that normally require a terrain depth
   buffer are allowed into the FBO as well.  Interiors establish their scene
   with the brick layer; the object pass supplies its own depth. */
static S32 s_spriteNoDepth = 0;
static S32 s_spriteFilterOverride = -1;
static S32 s_spriteUploadAlways = 0;

void GL_SetWorldGL(S32 active) {
    s_glWorld = active ? 1 : 0;
    Obj_GLWorld = s_glWorld;
}

void GL_SetSpriteNoDepth(S32 active) {
    s_spriteNoDepth = active ? 1 : 0;
}

void GL_SetSpriteFilterOverride(S32 filter) {
    s_spriteFilterOverride = filter;
}

void GL_SetSpriteUploadAlways(S32 active) {
    s_spriteUploadAlways = active ? 1 : 0;
}

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
    static S32 s_noCache = -1;
    if (s_noCache < 0)
        s_noCache = getenv("LBA2_NO_SPRITE_CACHE") ? 1 : 0;
    if (!s_noCache)
        for (S32 i = 0; i < SPRITE_CACHE_SIZE; i++) {
            SpriteTexEntry *e = &s_spriteTexCache[i];
            if (e->ptr != pixels || e->w != w || e->h != h)
                continue;
            GL_BindTexUnit(0, e->tex);
            if (s_spriteUploadAlways) {
                /* A composed patch reuses its backing buffer at the same
                   size, so the storage is already right: refresh it instead
                   of reallocating (glTexImage2D re-creates the texture and
                   stalls the pipe on every patch). */
                GLint unpackAlign = 4;
                glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlign);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_LUMINANCE,
                                GL_UNSIGNED_BYTE, pixels);
                glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlign);
                s_profSpriteUps++;
            }
            return e->tex;
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
    /* Sprite rows are tightly packed.  The default 4-byte unpack alignment
       pads any row whose width is not a multiple of four, shifting the
       remaining rows diagonally.  Restore the default afterwards for the
       RGBA/LUMINANCE uploads elsewhere that assume it. */
    GLint unpackAlign = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlign);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlign);
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
    /* Eye 0's call already issued both instances; repeating the sprite in
       the eye 1 pass would re-upload its page for a draw that is discarded. */
    if (GL_SecondEyePass())
        return;
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

    /* A sprite drawn before the first polygon batch would otherwise use the
       previous scene's palette texture.  Upload it before binding unit 0:
       GL_UploadPalette's temporary unit-0 bind would otherwise clear the
       sprite texture. */
    GL_UploadPalette();
    GL_BindTexUnit(0, GL_BindSpriteTexture(pixels, w, h));
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);

    /* The isometric brick layer is a painter-ordered background: it must
       not write depth or the first tile would reject every tile behind it.
       FBO sprites keep the depth path. */
    const S32 noDepth = s_spriteNoDepth;
    GL_SetDepthTest(noDepth ? 0 : 1);
    GL_SetDepthFunc(GL_GEQUAL);
    /* Opaque sprites write depth: they draw through the immediate aux path
       while bodies batch into the NZW defer pool drained at frame end, so
       the submission order between an overlay and the body polygons around
       it depends on the frame path (full redraw vs incremental) and on the
       camera.  Depth — not submission order — must decide occlusion, or a
       chest decal seen from behind (owner body nearer to the camera, drawn
       after it) stays visible through the body.  Transparent sprites keep
       write-off so their blend never clips geometry behind them. */
    GL_SetDepthWrite(noDepth ? 0 : (transparent ? 0 : 1));

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
    /* The mode-4 shader recovers texel units with tc/256 (the batch
       MapU/MapV convention: 8 fractional bits), so hand it texel<<8 —
       raw texel indices would divide down to 0 and sample texel (0,0)
       for every fragment, chroma-keying the whole quad away.  RepMask
       0xFFFF keeps mod(tc,256) from wrapping inside the sprite; the
       sampler's CLAMP_TO_EDGE absorbs uv past w/h (scaled paths sample
       the source span rounded up). */
    const U16 pu[4] = {(U16)(u0 << 8), (U16)(u1 << 8), (U16)(u1 << 8),
                       (U16)(u0 << 8)};
    const U16 pv[4] = {(U16)(v0 << 8), (U16)(v0 << 8), (U16)(v1 << 8),
                       (U16)(v1 << 8)};
    const U16 spriteRepMask = 0xFFFF;
    glUniform1i(s_clut_uBilinear, s_spriteFilterOverride >= 0
                                      ? s_spriteFilterOverride
                                      : DisplayTexFilter);
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
        memcpy(v + 12, &spriteRepMask, 2);
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

    GL_DrawStereoClut(GL_TRIANGLES, 0, 6);
    s_profTris += 2;

    GL_SetDepthWrite(1);
    GL_SetDepthTest(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_BindArrayBuffer(0);
}

/* ── Full-screen indexed background layer ────────────────────────────────
   The isometric brick layer is a single 2D composite, so it is drawn as one
   palette-indexed image instead of one quad per brick.  The sprite-quad
   path cannot serve it: that path encodes the wrap window in the 8-bit
   RepMask field, which caps a quad at 256 texels, and a whole layer is
   wider than that.  This program is the same fragment source compiled with
   LBA_LAYER_NO_WRAP, which takes the wrap window from the texture size and
   lets the sampler clamp instead of wrapping — the edge-directed filters
   then see the real neighbourhood across the whole image. */
/* Insert the no-wrap define after the version/precision header so the
   shared fragment source stays the single definition of the filters. */
static char *GL_MakeLayerFragSrc(void) {
    const char *nl = strchr(s_clutFragSrc, '\n');
    if (nl == NULL) {
        return NULL;
    }
    const char *define = "#define LBA_LAYER_NO_WRAP 1\n";
    size_t prefix = (size_t)(nl - s_clutFragSrc) + 1;
    size_t len = strlen(s_clutFragSrc);
    char *out = (char *)malloc(len + strlen(define) + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, s_clutFragSrc, prefix);
    memcpy(out + prefix, define, strlen(define));
    memcpy(out + prefix + strlen(define), s_clutFragSrc + prefix,
           len - prefix + 1);
    return out;
}

/* Push the per-eye half of the layer warp: the screen shift a point at depth
   `az` gets in the eye being drawn, in the terms the polygon vertex stage uses
   — a fixed projection offset plus a 1/depth term scaled by the focal lengths.
   Eye 0 renders with the reference camera, so both deltas are zero for it; a
   draw without a depth map stays flat in every eye. */
static void GL_SetLayerWarpUniforms(S32 eye, S32 hasDepth) {
    const GLfloat warp = (hasDepth && s_singlePassStereo) ? 1.0f : 0.0f;
    glUniform1f(s_layer_uWarp, warp);
    if (warp < 0.5f) {
        glUniform1f(s_layer_uAzShift, 0.0f);
        glUniform2f(s_layer_uShiftPx, 0.0f, 0.0f);
        glUniform2f(s_layer_uShiftPerAz, 0.0f, 0.0f);
        return;
    }
    /* Eye 0 renders from the reference camera plus the common hover delta;
       eye 1 adds its own eye delta on top. */
    const GLfloat zScale = (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0);
    const GLfloat wx = s_baseWorld[0] + ((eye == 0) ? 0.0f : s_eye1World[0]);
    const GLfloat wy = s_baseWorld[1] + ((eye == 0) ? 0.0f : s_eye1World[1]);
    const GLfloat wz = s_baseWorld[2] + ((eye == 0) ? 0.0f : s_eye1World[2]);
    const GLfloat px = s_baseProj[0] + ((eye == 0) ? 0.0f : s_eye1Proj[0]);
    const GLfloat py = s_baseProj[1] + ((eye == 0) ? 0.0f : s_eye1Proj[1]);
    glUniform1f(s_layer_uAzShift, wz * zScale);
    glUniform2f(s_layer_uShiftPx, px, py);
    glUniform2f(s_layer_uShiftPerAz, -wx * FRatioX * zScale,
                -wy * FRatioX * FRatioY * zScale);
}

void GL_GetLayerShiftMargin(U16 azMin, U16 azSpan, S32 *marginX, S32 *marginY) {
    if (marginX != NULL)
        *marginX = 0;
    if (marginY != NULL)
        *marginY = 0;
    if (!s_singlePassStereo)
        return;

    /* Same expression the fragment stage evaluates: az = azMin + dn * (azSpan
       - 1) + eyeZ * zScale over the map's byte depth, and the screen shift is
       proj + perAz / az.  The shift is largest where az is smallest, so the
       depth range's low end bounds it. */
    const GLfloat zScale = (GLfloat)((double)Fill_ZBuffer_Factor / 65536.0);
    /* Worst shift across the pair: eye 0 carries the base, eye 1 adds its
       own delta. */
    const GLfloat wz0 = s_baseWorld[2];
    const GLfloat wz1 = s_baseWorld[2] + s_eye1World[2];
    const GLfloat azShift = (fabs(wz0) > fabs(wz1)) ? wz0 : wz1;
    const GLfloat span = (azSpan > 1) ? (GLfloat)(azSpan - 1) : 0.0f;
    const GLfloat azLo = (GLfloat)azMin + azShift;
    const GLfloat azHi = (GLfloat)azMin + span + azShift;
    GLfloat az = (azLo < azHi) ? azLo : azHi;
    if (az < 1.0f)
        az = 1.0f;

    const GLfloat px0 = s_baseProj[0];
    const GLfloat px1 = s_baseProj[0] + s_eye1Proj[0];
    const GLfloat py0 = s_baseProj[1];
    const GLfloat py1 = s_baseProj[1] + s_eye1Proj[1];
    const GLfloat shiftX = (fabs(px0) > fabs(px1)) ? px0 : px1;
    const GLfloat shiftY = (fabs(py0) > fabs(py1)) ? py0 : py1;
    const GLfloat perAzX0 = -(s_baseWorld[0]) * FRatioX * zScale;
    const GLfloat perAzX1 =
        -(s_baseWorld[0] + s_eye1World[0]) * FRatioX * zScale;
    const GLfloat perAzY0 = -(s_baseWorld[1]) * FRatioX * FRatioY * zScale;
    const GLfloat perAzY1 =
        -(s_baseWorld[1] + s_eye1World[1]) * FRatioX * FRatioY * zScale;
    const GLfloat perAzX =
        (fabs(perAzX0) > fabs(perAzX1)) ? perAzX0 : perAzX1;
    const GLfloat perAzY =
        (fabs(perAzY0) > fabs(perAzY1)) ? perAzY0 : perAzY1;
    if (marginX != NULL)
        *marginX = (S32)ceil(fabs((double)shiftX) +
                             fabs((double)perAzX) / (double)az) +
                   1;
    if (marginY != NULL)
        *marginY = (S32)ceil(fabs((double)shiftY) +
                             fabs((double)perAzY) / (double)az) +
                   1;
}

void GL_DrawIndexedLayer(S32 slot, const void *pixels, const void *depth,
                         S32 w, S32 h, U32 generation, S32 filter, S32 x0,
                         S32 y0, S32 x1, S32 y1, U16 azMin, U16 azSpan,
                         const void *mask, S32 maskW, S32 maskH, S32 maskX,
                         S32 maskY) {
    GL_DrawIndexedLayerOffset(slot, pixels, depth, w, h, generation, filter,
                              x0, y0, x1, y1, 0, 0, azMin, azSpan, mask,
                              maskW, maskH, maskX, maskY);
}

void GL_DrawIndexedLayerOffset(S32 slot, const void *pixels, const void *depth,
                               S32 w, S32 h, U32 generation, S32 filter,
                               S32 x0, S32 y0, S32 x1, S32 y1, S32 offX,
                               S32 offY, U16 azMin, U16 azSpan,
                               const void *mask, S32 maskW, S32 maskH,
                               S32 maskX, S32 maskY) {
    if (GL_SecondEyePass())
        return;
    if (!s_initialized || pixels == NULL || w <= 0 || h <= 0 ||
        s_layerProgram == 0 || x1 <= x0 || y1 <= y0 || slot < 0 ||
        slot >= GL_LAYER_SLOTS) {
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

    T_LAYER_SLOT *ls = &s_layerSlots[slot];
    if (ls->tex == 0 || ls->depthTex == 0) {
        if (ls->tex == 0) {
            glGenTextures(1, &ls->tex);
        }
        if (ls->depthTex == 0) {
            glGenTextures(1, &ls->depthTex);
        }
        GLuint tex2D[2] = {ls->tex, ls->depthTex};
        for (S32 t = 0; t < 2; t++) {
            glBindTexture(GL_TEXTURE_2D, tex2D[t]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        /* These binds land on whatever unit was active, behind the binding
           cache's back. */
        GL_InvalidateGLState();
        ls->w = 0;
        ls->h = 0;
        ls->hasGen = 0;
    }

    const S32 needUpload = !ls->hasGen || generation != ls->gen ||
                           w != ls->w || h != ls->h;
    if (needUpload) {
        const S32 resized = (w != ls->w || h != ls->h);
        GLint unpackAlign = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlign);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, ls->tex);
        if (resized) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE,
                         GL_UNSIGNED_BYTE, pixels);
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_LUMINANCE,
                            GL_UNSIGNED_BYTE, pixels);
        }
        if (depth != NULL) {
            /* One byte per pixel, same dimensions, sampled at the same texel
               centres: the map is the composite's own geometry. */
            glBindTexture(GL_TEXTURE_2D, ls->depthTex);
            if (resized) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0,
                             GL_LUMINANCE, GL_UNSIGNED_BYTE, depth);
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_LUMINANCE,
                                GL_UNSIGNED_BYTE, depth);
            }
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlign);
        ls->w = w;
        ls->h = h;
        ls->gen = generation;
        ls->hasGen = 1;
        /* Two textures were just bound on the active unit, so the binding
           cache no longer describes the context. */
        GL_InvalidateGLState();
    }

    GL_UseProgramCached(s_layerProgram);

    const GLfloat ssx = 2.0f / (GLfloat)s_texWidth;
    const GLfloat ssy = 2.0f / (GLfloat)s_texHeight;
    glUniform2f(s_layer_uScreenScale, ssx, ssy);
    glUniform2f(s_layer_uAtlasScale, (GLfloat)w, (GLfloat)h);
    glUniform1i(s_layer_uBilinear, filter);
    glUniform1f(s_layer_uAzMin, (GLfloat)azMin);
    glUniform1f(s_layer_uAzSpan, (GLfloat)(azSpan > 1 ? azSpan - 1 : 0));

    if (mask != NULL && maskW > 0 && maskH > 0) {
        if (s_layerMaskTex == 0) {
            glGenTextures(1, &s_layerMaskTex);
            glBindTexture(GL_TEXTURE_2D, s_layerMaskTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
            GL_InvalidateGLState();
            s_layerMaskW = 0;
            s_layerMaskH = 0;
        }
        GLint align = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, s_layerMaskTex);
        if (maskW != s_layerMaskW || maskH != s_layerMaskH) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, maskW, maskH, 0,
                         GL_LUMINANCE, GL_UNSIGNED_BYTE, mask);
            s_layerMaskW = maskW;
            s_layerMaskH = maskH;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, maskW, maskH, GL_LUMINANCE,
                            GL_UNSIGNED_BYTE, mask);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, align);
        GL_InvalidateGLState();
        glUniform1f(s_layer_uMaskOn, 1.0f);
        glUniform4f(s_layer_uMaskRect, (GLfloat)maskX, (GLfloat)maskY,
                    1.0f / (GLfloat)maskW, 1.0f / (GLfloat)maskH);
    } else {
        glUniform1f(s_layer_uMaskOn, 0.0f);
    }

    GL_BindTexUnit(0, ls->tex);
    GL_BindTexUnit(1, s_clutTex);
    GL_BindTexUnit(2, s_paletteTex);
    GL_BindTexUnit(GL_LAYER_DEPTH_UNIT, ls->depthTex);
    if (mask != NULL && s_layerMaskTex != 0)
        GL_BindTexUnit(GL_LAYER_MASK_UNIT, s_layerMaskTex);

    /* The composed art already carries its own occlusion, so the background
       keeps out of the depth buffer and is placed by the painter order the
       layer was built in. */
    GL_SetDepthTest(0);
    GL_SetDepthWrite(0);
    GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GL_MarkFBORegion(x0 + offX, y0 + offY, x1 - 1 + offX, y1 - 1 + offY);

    /* One quad covering the whole layer.  Material: polyMode 4 (the texel is
       the palette index) with the chroma-key bit, so untouched texels stay
       transparent over the cleared scene instead of painting palette[0]. */
    static GLfloat layerVerts[6 * 5];
    static const S8 order[6] = {0, 1, 2, 0, 2, 3};
    const GLfloat u0 = (GLfloat)x0 / (GLfloat)w;
    const GLfloat v0 = (GLfloat)y0 / (GLfloat)h;
    const GLfloat u1 = (GLfloat)x1 / (GLfloat)w;
    const GLfloat v1 = (GLfloat)y1 / (GLfloat)h;
    /* The quad's screen position and the texels it samples are separable:
       a caller that composed at a quantized view offset slides the whole
       image by the leftover here, without re-uploading it. */
    const GLfloat px[4] = {(GLfloat)(x0 + offX), (GLfloat)(x1 + offX),
                           (GLfloat)(x1 + offX), (GLfloat)(x0 + offX)};
    const GLfloat py[4] = {(GLfloat)(y0 + offY), (GLfloat)(y0 + offY),
                           (GLfloat)(y1 + offY), (GLfloat)(y1 + offY)};
    const GLfloat pu[4] = {u0, u1, u1, u0};
    const GLfloat pv[4] = {v0, v0, v1, v1};
    const GLfloat layerMat = (GLfloat)(4 | (1 << 5));
    for (S32 i = 0; i < 6; i++) {
        GLfloat *v = layerVerts + i * 5;
        const S32 c = order[i];
        v[0] = px[c];
        v[1] = py[c];
        v[2] = pu[c];
        v[3] = pv[c];
        v[4] = layerMat;
    }

    GL_BindArrayBuffer(s_batchVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(layerVerts), layerVerts,
                 GL_STREAM_DRAW);
    glEnableVertexAttribArray(s_layer_aPos);
    glVertexAttribPointer(s_layer_aPos, 2, GL_FLOAT, GL_FALSE,
                          (GLsizei)(5 * sizeof(GLfloat)), (const void *)0);
    glEnableVertexAttribArray(s_layer_aUV);
    glVertexAttribPointer(s_layer_aUV, 2, GL_FLOAT, GL_FALSE,
                          (GLsizei)(5 * sizeof(GLfloat)),
                          (const void *)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(s_layer_aMaterial);
    glVertexAttribPointer(s_layer_aMaterial, 1, GL_FLOAT, GL_FALSE,
                          (GLsizei)(5 * sizeof(GLfloat)),
                          (const void *)(4 * sizeof(GLfloat)));

    /* Single-pass stereo draws both eyes from one instance: the layer needs
       the same switch the batch programs get from GL_DrawStereoClut, or each
       eye shows the unremapped full-width image.  The warp uniforms are per
       eye, so they change between the two draws. */
    if (s_singlePassStereo) {
        if (s_stereoEye == 0) {
            glUniform1f(s_layer_uSinglePass, 1.0f);
            glUniform1f(s_layer_uEye, 0.0f);
            GL_SetLayerWarpUniforms(0, depth != NULL);
            GL_BeginSinglePassEyeDraw(0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glUniform1f(s_layer_uEye, 1.0f);
            GL_SetLayerWarpUniforms(1, depth != NULL);
            GL_BeginSinglePassEyeDraw(1);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            GL_EndSinglePassEyeDraw();
            glUniform1f(s_layer_uEye, 0.0f);
        }
    } else {
        glUniform1f(s_layer_uSinglePass, 0.0f);
        glUniform1f(s_layer_uEye, 0.0f);
        glUniform1f(s_layer_uWarp, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    s_profTris += 2;

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

    /* The scene target is s_sceneTargetW wide, which is twice the per-eye
       width when the pair shares one allocation. */
    const U32 fboW = s_sceneTargetW;
    const U32 fboH = s_sceneTargetH;
    const U32 scrW = ModeDesiredX;
    const U32 scrH = ModeDesiredY;

    /* A double-wide target holds both eyes: dump them side by side at eye
       resolution.  Each half gets the same overlay composite, which is what
       GL_Present draws into each eye's half. */
    const U32 eyes = s_stereoDoubleWide ? 2 : 1;
    const U32 eyeW = fboW / eyes;
    const U32 outW = scrW * eyes;
    const U32 outH = scrH;
    if (fboW == 0 || fboH == 0 || eyeW == 0) {
        return;
    }

    U32 *fboBuf = (U32 *)malloc((size_t)fboW * fboH * sizeof(U32));
    /* Composite holds 0xAARRGGBB words, same layout as paletteLUT. */
    U32 *comp = (U32 *)malloc((size_t)outW * (size_t)outH * sizeof(U32));
    if (fboBuf == NULL || comp == NULL) {
        free(fboBuf);
        free(comp);
        return;
    }

    /* A harness frame has no eye-1 present to run the deferred MSAA
       resolve, so do it here before reading either half. */
    if (s_stereoDoubleWide && s_msaaFbo != 0)
        GL_ResolveMSAAColor();
    glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneReadFbo());
#if defined(__ANDROID__)
    glReadPixels(0, 0, fboW, fboH, GL_RGBA, GL_UNSIGNED_BYTE, fboBuf);
    GL_SwizzleBGRAtoRGBA(fboBuf, fboBuf, (U32)(fboW * fboH));
#else
    glReadPixels(0, 0, fboW, fboH, FMT_BGRA, GL_UNSIGNED_BYTE, fboBuf);
#endif

    /* Downsample (SSAA center pick) with the Y-flip glReadPixels implies,
       giving screen-space top-down ARGB like GL_Present's FBO draw. */
    const U32 scaleX = eyeW / scrW;
    const U32 scaleY = fboH / scrH;
    for (U32 eye = 0; eye < eyes; eye++) {
        for (U32 sy = 0; sy < scrH; sy++) {
            const U32 fy = fboH - 1 - sy * scaleY - scaleY / 2;
            const U32 *srcRow = fboBuf + fy * fboW + eye * eyeW;
            U32 *dstRow = comp + sy * outW + eye * scrW;
            if (scaleX == 1) {
                memcpy(dstRow, srcRow, scrW * sizeof(U32));
            } else {
                for (U32 sx = 0; sx < scrW; sx++) {
                    dstRow[sx] = srcRow[sx * scaleX + scaleX / 2];
                }
            }
        }
    }

    /* Log overlay on top: palette index 0 is transparent (alpha forced 0,
       same rule PresentFrame applies before the GPU composite).  The source
       and the pair are both row-major, so walk rows: indexing the pair
       linearly would write every eye but the first to the wrong place. */
    const U32 *lut = GL_GetPaletteLUT();
    const U8 *logIdx = (const U8 *)Log;
    if (lut != NULL) {
        for (U32 y = 0; y < scrH; y++) {
            for (U32 x = 0; x < scrW; x++) {
                const U32 idx = logIdx[(size_t)y * scrW + x];
                if (idx == 0)
                    continue;
                for (U32 eye = 0; eye < eyes; eye++)
                    comp[(size_t)y * outW + (size_t)eye * scrW + x] = lut[idx];
            }
        }
    }

    /* Texture filter modes sample inside the 3D pass; the present composite
       is what the player sees either way, so dump it as-is. */
    const U32 *outComp = comp;

    U8 *rgb = (U8 *)malloc((size_t)outW * outH * 3);
    if (rgb != NULL) {
        for (U32 i = 0; i < outW * outH; i++) {
            U32 c = outComp[i];
            rgb[i * 3 + 0] = (U8)(c >> 16);
            rgb[i * 3 + 1] = (U8)(c >> 8);
            rgb[i * 3 + 2] = (U8)(c);
        }
        SavePNGRGB(path, rgb, (S32)outW, (S32)outH);
        free(rgb);
    }

    /* The capture temporarily bound the read view; restore the draw target. */
    glBindFramebuffer(GL_FRAMEBUFFER, GL_SceneDrawFbo());

    free(fboBuf);
    free(comp);
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
    glUniform1f(s_uSharpenLoc, 0.0f); /* never sharpen the 2D overlay */
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
    s_overlayPending = 0;

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
            if (!s_directActive)
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
            if (vpW > winW)
                vpW = winW;
            vpX = (winW - vpW) / 2;
            vpY = 0;
        } else {
            /* Window taller than content → letterbox */
            vpW = winW;
            vpH = (int)((double)winW / contentAspect + 0.5);
            vpH = (vpH + 1) & ~1;
            if (vpH > winH)
                vpH = winH;
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
        GL_BindArrayBuffer(GL_PresentSceneVbo());
        if (s_antiAlias == 1 && s_fxaaProgram) {
            /* FXAA post-process: edge-detecting AA on the scene texture. */
            glUseProgram(s_fxaaProgram);
            glUniform1i(s_fxaa_uTex, 0);
            glUniform2f(s_fxaa_uTexel, 1.0f / (GLfloat)s_texWidth,
                        1.0f / (GLfloat)s_texHeight);
            glUniform1f(s_fxaa_uSharpen, GL_SharpenAmount());
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
        } else {
            glUseProgram(s_program);
            glUniform1i(s_uTexLoc, 0);
            /* The hi-res FBO is always drawn plainly; the Scale2x mode below is
         * for the 640x480 overlay only.  Pin it here or a stale Scale2x from
         * the previous frame's overlay draw would smear the FBO blit. */
            GL_UploadImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                                 s_adj_uSaturation);
            glUniform1i(s_uFilterLoc, 1);
            glUniform1f(s_uSharpenLoc, GL_SharpenAmount());
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

/* Draw the overlay once per non-transparent band, scissoring the full-screen
   quad to the content.  `offX/offY/dstW/dstH` describe the destination
   viewport (eye half in double-wide stereo, letterboxed window in mono);
   the band/bbox coordinates are in base overlay pixels. */
static void GL_DrawOverlayRegions(U32 width, U32 height,
                                  const U32 *paletteU32, S32 filterLinear,
                                  S32 offX, S32 offY, U32 dstW, U32 dstH,
                                  const S32 bands[6][4], S32 bandCount,
                                  const S32 bbox[4]) {
    S32 rects[7][4];
    S32 rectCount = 0;
    if (bandCount > 0 && bandCount <= 6) {
        for (S32 i = 0; i < bandCount; i++) {
            rects[rectCount][0] = bands[i][0];
            rects[rectCount][1] = bands[i][1];
            rects[rectCount][2] = bands[i][2];
            rects[rectCount][3] = bands[i][3];
            rectCount++;
        }
    } else if (bbox[2] > bbox[0] && bbox[3] > bbox[1]) {
        rects[0][0] = bbox[0];
        rects[0][1] = bbox[1];
        rects[0][2] = bbox[2];
        rects[0][3] = bbox[3];
        rectCount = 1;
    }

    if (rectCount > 0 && width > 0 && height > 0 && dstW > 0 && dstH > 0) {
        glEnable(GL_SCISSOR_TEST);
        for (S32 i = 0; i < rectCount; i++) {
            S32 bx0 = rects[i][0] - 2;
            S32 by0 = rects[i][1] - 2;
            S32 bx1 = rects[i][2] + 2;
            S32 by1 = rects[i][3] + 2;
            if (bx0 < 0) {
                bx0 = 0;
            }
            if (by0 < 0) {
                by0 = 0;
            }
            if (bx1 > (S32)width) {
                bx1 = (S32)width;
            }
            if (by1 > (S32)height) {
                by1 = (S32)height;
            }
            if (bx1 <= bx0 || by1 <= by0) {
                continue;
            }
            S32 sx = offX + (bx0 * (S32)dstW) / (S32)width;
            S32 sw = ((bx1 - bx0) * (S32)dstW) / (S32)width;
            S32 sy = offY + (S32)dstH -
                     (by1 * (S32)dstH) / (S32)height;
            S32 sh = ((by1 - by0) * (S32)dstH) / (S32)height;
            if (sw < 1) {
                sw = 1;
            }
            if (sh < 1) {
                sh = 1;
            }
            glScissor(sx, sy, (GLsizei)sw, (GLsizei)sh);
            GL_DrawIndexedOverlay(width, height, paletteU32, filterLinear);
        }
        glDisable(GL_SCISSOR_TEST);
    } else {
        GL_DrawIndexedOverlay(width, height, paletteU32, filterLinear);
    }
}

/* Draw the deferred exterior overlay into the bound swapchain image, once
   per eye half.  The scene pair was already blitted and display-transformed;
   the HUD gets the transform here so it is applied once. */
S32 GL_DrawStereoOverlayToSwapchain(U32 dstWidth, U32 dstHeight) {
    if (!s_overlayPending || dstWidth == 0 || dstHeight == 0)
        return 0;
    s_overlayPending = 0;
    if (!Window_GLMakeCurrent())
        return 0;
    glDisable(GL_DEPTH_TEST);
    const S32 savedAdjust = s_overlayImageAdjust;
    s_overlayImageAdjust = 1;
    for (S32 eye = 0; eye < 2; eye++) {
        const S32 vpX = eye ? (S32)dstWidth : 0;
        glViewport((GLint)vpX, 0, (GLsizei)dstWidth, (GLsizei)dstHeight);
        GL_DrawOverlayRegions(s_overlayPendW, s_overlayPendH,
                              s_overlayPendPalette, s_overlayPendFilter, vpX,
                              0, (S32)dstWidth, (S32)dstHeight,
                              s_overlayBands, s_overlayBandCount,
                              s_overlayBBox);
    }
    s_overlayImageAdjust = savedAdjust;
    return 1;
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
    s_overlayPending = 0;

    /* Same frame plumbing as GL_Present: flush 3D, pick the composite
     * target, letterbox, draw the 3D FBO content, then the 2D overlay. */
    S32 useDoubleWide = (s_stereoDoubleWide && s_stereoEye >= 0);
    if (useDoubleWide) {
        /* The stereo pair reads the scene target directly; the mono
           present target and snapshot/horizon captures are dead weight
           while it runs.  A later mono frame recreates them on demand. */
        GL_DestroySnapshot();
        GL_DestroyHorizon();
        GL_DestroyPresentFBO();
    }
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
                if (vpW > winW)
                    vpW = winW;
                vpX = (winW - vpW) / 2;
                vpY = 0;
            } else {
                vpW = winW;
                vpH = (int)((double)winW / contentAspect + 0.5);
                vpH = (vpH + 1) & ~1;
                if (vpH > winH)
                    vpH = winH;
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
    /* Bounding box of non-transparent overlay pixels (base framebuffer
       coords).  Drawing the full-screen quad at 1.5x is pure waste when the
       HUD occupies a small strip; the scissor limits the Scale2x shader to
       the content. */
    /* Up to six horizontal bands of non-transparent overlay content, kept
       at file scope so the deferred swapchain pass can reuse them. */
    /* Eye 1 can reuse eye 0's texture only when neither pass wrote moving
     * 2D content.  Moving boxes cover world sprites, dialogue and counters,
     * whose projection differs between the eyes; static boxes are the
     * screen-space HUD, which is identical for the pair. */
    /* A single-pass interior draws all world content into the FBO, so the
       Log moving boxes are HUD that is identical for the pair; the eye 1
       scan can reuse eye 0's without the conservative box check. */
    const S32 exteriorOverlay = (CubeMode != 0);
    const S32 overlayPairStatic = s_singlePassStereo && CubeMode == 0;
    if (exteriorOverlay && s_stereoEye == 0 && s_stereoDoubleWide) {
        /* Only eye 1's Log is presented for the pair; eye 0's overlay scan
           and upload would be thrown away. */
        overlayEmpty = 0;
    } else if (exteriorOverlay) {
        /* Exterior 2D content (rain above all) changes every frame; diffing
           and scanning 640x480 twice costs more than one full upload. */
        if (s_logIndexTex == 0 || s_logIndexTexW != width ||
            s_logIndexTexH != height) {
            if (s_logIndexTex == 0)
                glGenTextures(1, &s_logIndexTex);
            glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0,
                         GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            s_logIndexTexW = width;
            s_logIndexTexH = height;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_logIndexTex);
#ifdef GL_UNPACK_ROW_LENGTH
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pitch);
#endif
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)width,
                        (GLsizei)height, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                        indices);
#ifdef GL_UNPACK_ROW_LENGTH
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        overlayEmpty = 0;
        s_overlayBBox[0] = 0;
        s_overlayBBox[1] = 0;
        s_overlayBBox[2] = (S32)width;
        s_overlayBBox[3] = (S32)height;
        s_overlayBandCount = 0;
    } else if (s_stereoEye == 1 && s_overlayScanValid &&
               (overlayPairStatic ||
                (!s_overlayEye0Dynamic && !BoxHadMovingWrites()))) {
        overlayEmpty = s_overlayEmptyCached;
        s_profOverlayReuse++;
    } else {
        static U8 *s_prevUpload = NULL; /* mirror of s_logIndexTex content */
        static U8 *s_rowNonZero = NULL; /* per row: 1 = has any nonzero */
        static U16 *s_rowX0 = NULL;     /* first/last nonzero pixel per row */
        static U16 *s_rowX1 = NULL;
        static U32 s_prevCap = 0;
        static S32 s_prevH = 0;
        U32 needed = (U32)pitch * height;
        S32 fullUpload = (s_logIndexTex == 0 || s_logIndexTexW != width ||
                          s_logIndexTexH != height);
        if (s_prevCap < needed || s_prevH < (S32)height) {
            free(s_prevUpload);
            free(s_rowNonZero);
            free(s_rowX0);
            free(s_rowX1);
            s_prevUpload = (U8 *)calloc(1, needed);
            s_rowNonZero = (U8 *)calloc(1, height);
            s_rowX0 = (U16 *)malloc((size_t)height * sizeof(U16));
            s_rowX1 = (U16 *)malloc((size_t)height * sizeof(U16));
            if (s_rowX0 != NULL && s_rowX1 != NULL) {
                for (U32 y = 0; y < height; y++) {
                    s_rowX0[y] = (U16)width;
                    s_rowX1[y] = 0;
                }
            }
            s_prevCap = (s_prevUpload && s_rowNonZero && s_rowX0 && s_rowX1)
                            ? needed
                            : 0;
            s_prevH = s_prevCap ? (S32)height : 0;
            fullUpload = 1; /* mirror restarted: reseed the texture */
        }
        if (s_prevUpload != NULL && s_rowNonZero != NULL &&
            s_rowX0 != NULL && s_rowX1 != NULL) {
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
                    U16 x0 = (U16)width;
                    U16 x1 = 0;
                    for (U32 x = 0; x < width; x++) {
                        if (row[x] != 0) {
                            if (x0 == (U16)width) {
                                x0 = (U16)x;
                            }
                            x1 = (U16)(x + 1);
                        }
                    }
                    s_rowX0[y] = x0;
                    s_rowX1[y] = x1;
                    s_rowNonZero[y] = (x1 > x0) ? 1 : 0;
                    if (x1 > x0) {
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
                        U16 x0 = (U16)width;
                        U16 x1 = 0;
                        for (U32 x = 0; x < width; x++) {
                            if (row[x] != 0) {
                                if (x0 == (U16)width) {
                                    x0 = (U16)x;
                                }
                                x1 = (U16)(x + 1);
                            }
                        }
                        s_rowX0[y] = x0;
                        s_rowX1[y] = x1;
                        s_rowNonZero[y] = (x1 > x0) ? 1 : 0;
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
        {
            /* Global bbox of the whole overlay, not just the rows uploaded
               this frame: static HUD rows keep their per-row range. */
            S32 bx0 = (S32)width;
            S32 by0 = (S32)height;
            S32 bx1 = 0;
            S32 by1 = 0;
            if (s_rowNonZero != NULL && s_rowX0 != NULL && s_rowX1 != NULL) {
                for (U32 y = 0; y < height; y++) {
                    if (!s_rowNonZero[y]) {
                        continue;
                    }
                    if ((S32)s_rowX0[y] < bx0) {
                        bx0 = (S32)s_rowX0[y];
                    }
                    if ((S32)s_rowX1[y] > bx1) {
                        bx1 = (S32)s_rowX1[y];
                    }
                    if ((S32)y < by0) {
                        by0 = (S32)y;
                    }
                    by1 = (S32)y + 1;
                }
            }
            s_overlayBBox[0] = bx0;
            s_overlayBBox[1] = by0;
            s_overlayBBox[2] = bx1;
            s_overlayBBox[3] = by1;
        }
        {
            S32 bandStart[6];
            S32 bandEnd[6];
            S32 bandN = 0;
            S32 bStart = -1;
            S32 bLast = -1;
            if (s_rowNonZero != NULL) {
                for (U32 y = 0; y < height; y++) {
                    if (s_rowNonZero[y]) {
                        if (bStart < 0) {
                            bStart = (S32)y;
                        }
                        bLast = (S32)y;
                    } else if (bStart >= 0 && (S32)y - bLast > 8) {
                        if (bandN < 6) {
                            bandStart[bandN] = bStart;
                            bandEnd[bandN] = bLast;
                            bandN++;
                        } else {
                            bandN = -1;
                            break;
                        }
                        bStart = -1;
                    }
                }
            }
            if (bandN >= 0 && bStart >= 0) {
                if (bandN < 6) {
                    bandStart[bandN] = bStart;
                    bandEnd[bandN] = bLast;
                    bandN++;
                } else {
                    bandN = -1;
                }
            }
            if (bandN < 0) {
                s_overlayBandCount = 0; /* too fragmented: use the bbox */
            } else {
                s_overlayBandCount = bandN;
                for (S32 i = 0; i < bandN; i++) {
                    S32 ix0 = (S32)width;
                    S32 ix1 = 0;
                    for (S32 y = bandStart[i]; y <= bandEnd[i]; y++) {
                        if (s_rowNonZero[y]) {
                            if ((S32)s_rowX0[y] < ix0) {
                                ix0 = (S32)s_rowX0[y];
                            }
                            if ((S32)s_rowX1[y] > ix1) {
                                ix1 = (S32)s_rowX1[y];
                            }
                        }
                    }
                    s_overlayBands[i][0] = ix0;
                    s_overlayBands[i][1] = bandStart[i];
                    s_overlayBands[i][2] = ix1;
                    s_overlayBands[i][3] = bandEnd[i] + 1;
                }
                /* Dense content (rain, a full-screen cutscene) is cheaper as
                   one bbox draw than as several nearly-full-width bands:
                   splitting only pays when the bands leave a big empty gap. */
                if (s_overlayBandCount > 1) {
                    S32 bboxArea =
                        (s_overlayBBox[2] - s_overlayBBox[0]) *
                        (s_overlayBBox[3] - s_overlayBBox[1]);
                    S32 bandArea = 0;
                    for (S32 i = 0; i < s_overlayBandCount; i++) {
                        bandArea += (s_overlayBands[i][2] -
                                     s_overlayBands[i][0]) *
                                    (s_overlayBands[i][3] -
                                     s_overlayBands[i][1]);
                    }
                    if (bboxArea > 0 && bandArea * 2 >= bboxArea) {
                        s_overlayBandCount = 0;
                    }
                }
            }
        }
        if (s_stereoEye == 0) {
            s_overlayScanValid = (s_logIndexTex != 0 || overlayEmpty);
            s_overlayEmptyCached = overlayEmpty;
            s_overlayEye0Dynamic = BoxHadMovingWrites();
        }
    }

    if (useDoubleWide && s_stereoEye == 0) {
        /* Publish eye 0's scan so the eye 1 present can reuse its texture
           and bands; the double-wide path returns before the composite
           bookkeeping at the end of this function. */
        s_overlayScanValid = (s_logIndexTex != 0 || overlayEmpty);
        s_overlayEmptyCached = overlayEmpty;
        s_overlayEye0Dynamic = BoxHadMovingWrites();
    }

    if (useDoubleWide) {
        /* The HUD is the same picture in both eyes, so it is drawn once for
           the pair, after the scene resolve: placement is identical by
           construction, and the glyphs escape the MSAA softening the scene
           target applies on the way out — a dialogue strip is read, not
           looked at.  The harness has no eye-1 present, so it draws at eye 0
           instead. */
        const S32 drawHud = (s_stereoEye == 1) || s_fakeStereoPx > 0;
        if (drawHud) {
            /* A direct target resolves when the compositor samples the
               swapchain texture; the double-wide scene target needs the
               explicit resolve before the HUD lands on it. */
            if (!s_directActive)
                GL_ResolveMSAAColor();
            /* The resolve just wrote the single-sample scene texture; without
               one, keep drawing into whatever GL_SceneDrawFbo() selected
               (the tile-resolve MSAA target, or the direct swapchain image). */
            if (!s_directActive && !s_msaaInTile && s_msaaFbo != 0 &&
                s_fbo != 0)
                glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
            if (overlayEmpty) {
                s_profOverlayEmpty++;
            } else if (CubeMode != 0 && !s_directActive && s_stereoEye == 1) {
                /* Exterior stereo: hold the overlay for the pair blit, then
                   draw it at swapchain resolution.  Interiors keep it in the
                   scene target because the zoom scales the whole composite;
                   the fake-stereo harness draws at eye 0 and has no swap. */
                s_overlayPending = 1;
                s_overlayPendW = width;
                s_overlayPendH = height;
                s_overlayPendPalette = paletteU32;
                s_overlayPendFilter = filterLinear;
            } else {
                /* The overlay is a full-screen quad scaled into the 1.5x
                   scene target.  Draw only the non-transparent HUD bands: a
                   single bounding box would span a top HUD and a bottom
                   dialogue box, re-shading the empty middle at 1.5x.  The 2px
                   margin covers Scale2x's neighbour taps.  One pass per half,
                   from the same bands, so the pair cannot drift apart. */
                S32 savedAdjust = s_overlayImageAdjust;
                s_overlayImageAdjust = 0;
                for (S32 eye = 0; eye < 2; eye++) {
                    const S32 eyeX = eye ? (S32)s_fboWidth : 0;
                    glViewport((GLint)eyeX, 0, (GLsizei)s_fboWidth,
                               (GLsizei)s_fboHeight);
                    GL_DrawOverlayRegions(width, height, paletteU32,
                                          filterLinear, eyeX, 0, s_fboWidth,
                                          s_fboHeight, s_overlayBands,
                                          s_overlayBandCount, s_overlayBBox);
                }
                s_overlayImageAdjust = savedAdjust;
            }
        }

        if (s_stereoEye == 0 && s_fakeStereoPx == 0) {
            s_stereoPairReady = 0;
        } else {
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
                          s_antiAlias != 1 &&
                          GL_ImageAdjustNeutral();
    S32 sceneOnlyPresent = 0;
    if (directSceneSafe && OpenXR_IsActive() &&
        (s_stereoEye < 0 || s_stereoEye == 1)) {
        sceneOnlyPresent = 1;
        if (s_stereoEye == 1) {
            s_stereoEye1SceneDirect = 1;
        }
    }
    if (overlayEmpty)
        s_profOverlayEmpty++;
    if (directSceneSafe)
        s_profDirectScene++;

    if (s_fbo && !sceneOnlyPresent) {
        GL_SetBlendFunc(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GL_SceneColorTex());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_BindArrayBuffer(GL_PresentSceneVbo());
        if (s_antiAlias == 1 && s_fxaaProgram) {
            /* FXAA post-process: edge-detecting AA on the scene texture. */
            glUseProgram(s_fxaaProgram);
            glUniform1i(s_fxaa_uTex, 0);
            glUniform2f(s_fxaa_uTexel, 1.0f / (GLfloat)s_texWidth,
                        1.0f / (GLfloat)s_texHeight);
            glUniform1f(s_fxaa_uSharpen, GL_SharpenAmount());
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
        } else {
            glUseProgram(s_program);
            glUniform1i(s_uTexLoc, 0);
            /* The hi-res FBO is always drawn plainly; the Scale2x mode below is
         * for the 640x480 overlay only.  Pin it here or a stale Scale2x from
         * the previous frame's overlay draw would smear the FBO blit. */
            GL_UploadImageAdjust(s_adj_uBrightness, s_adj_uContrast,
                                 s_adj_uSaturation);
            glUniform1i(s_uFilterLoc, 1);
            glUniform1f(s_uSharpenLoc, GL_SharpenAmount());
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
        GL_DrawOverlayRegions(width, height, paletteU32, filterLinear, vpX,
                              vpY, (U32)vpW, (U32)vpH, s_overlayBands,
                              s_overlayBandCount, s_overlayBBox);
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

/* Camera-space vertices handed over for the polygon being submitted (see
   GL_SetPolyCamera).  Consumed by GL_RenderTriangleList so a nested call from
   the near-plane clip starts clean. */
static const S32 *s_polyCam = NULL;
static S32 s_polyCamCount = 0;

void GL_SetPolyCamera(const S32 *xyz, S32 count) {
    s_polyCam = xyz;
    s_polyCamCount = (xyz != NULL) ? count : 0;
}

/* Clip a body polygon against the near plane in camera space, project the
   result and submit it through the normal batch.  Returns 0 when nothing is
   in front of the plane (the software filler's own behavior) so the caller
   can fall back as before. */
static S32 GL_ClipNearPoly(S32 typePoly, S32 colorPoly, S32 nbPoints,
                           Struc_Point *ptrPoints, const S32 *cam) {
    const S32 nearD = (NearClip > 0) ? NearClip : 1;
    Struc_Point out[12];
    const S32 m = GL_ClipPolyToNear(cam, ptrPoints, nbPoints, nearD, out, 12);

    if (m < 3) {
        return 0;
    }
    for (S32 k = 1; k + 1 < m; k++) {
        Struc_Point tri[3];
        tri[0] = out[0];
        tri[1] = out[k];
        tri[2] = out[k + 1];
        GL_RenderTriangleList(typePoly, colorPoly, 3, tri);
    }
    return 1;
}

S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    const S32 *polyCam = s_polyCam;
    const S32 polyCamCount = s_polyCamCount;
    s_polyCam = NULL;
    s_polyCamCount = 0;

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
    if (!Fill_Flag_ZBuffer && !s_glWorld) {
        GL_LastRejectReason = 3;
        return 0;
    }

    /* Vertices the SW projection could not place (behind the near plane, or
       beyond S16 screen coords) carry the (-32768, -32768) sentinel.  The GPU
       would treat that as an ordinary clip-space position — a triangle flung
       across the whole screen that also poisons the depth buffer for the
       terrain drawn after it — so hand these polygons to the SW filler, which
       clips them against the screen rect the way the original engine did.

       Single-pass stereo draws both eyes from this one submission and puts
       SW fallbacks into one shared Log overlay, so a body polygon crossing
       the near plane would keep the eye-0 screen position in both eyes and
       slide against the world.  Body handlers supply camera-space vertices
       (GL_SetPolyCamera); clip and project those into the FBO instead. */
    S32 hasSentinel = 0;
    for (S32 i = 0; i < nbPoints; i++) {
        if (ptrPoints[i].Pt_XE == -32768 && ptrPoints[i].Pt_YE == -32768) {
            hasSentinel = 1;
            break;
        }
    }
    static S32 s_noNearClip = -1;
    if (s_noNearClip < 0) {
        s_noNearClip = getenv("LBA2_NO_NEARCLIP") ? 1 : 0;
    }
    const S32 canClip = !s_noNearClip && s_singlePassStereo &&
                        polyCam != NULL && polyCamCount == nbPoints &&
                        nbPoints >= 3 && nbPoints <= 4;
    if (hasSentinel && !canClip) {
        GL_LastRejectReason = 7;
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
    if (Fill_Flag_NZW && !s_fboHasTerrain && !s_glWorld) {
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

    /* Eye 1's single-pass pass: the checks above have already routed every
       SW fallback, and eye 0 drew this GL polygon into both halves, so only
       the pair-wide batch packing is skipped here. */
    if (GL_SecondEyePass())
        return 1;

    if (hasSentinel) {
        if (GL_ClipNearPoly(typePoly, colorPoly, nbPoints, ptrPoints,
                            polyCam)) {
            return 1;
        }
        GL_LastRejectReason = 7;
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
        /* Opt-in: the batch already carries aZ/1-over-W, so types 8-15
           (the affine POLYTEXT/POLYGTEX fillers) can take the same
           perspective-correct path as the Z-textured types.  Chroma-keyed
           types (12-15, the `type & 4` INCRUST bit) keep the original
           affine mapping even with the option on: the perspective divide
           moves their transparent holes relative to the bars, so a lattice
           or fence visibly shears against its own frame.  Opaque walls,
           which is what the option exists for, are unaffected. */
        if (DisplayPerspectiveTextures && s_arrayWindow &&
            typePoly < POLY_TEXTURE_Z && ((typePoly & 4) == 0)) {
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
                         (const U8 *)PtrCLUTFog) >>
                        8);
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
        /* The decor stream groups the blend state itself, so alpha modes
           can stay on the array path there. */
        arrNow = (U8)(!s_noArray && s_arrayWindow && s_clut3Program != 0 &&
                      ((matFlags & 7) == 0 || (matFlags & 7) == 4) &&
                      (!(matFlags & 0x18) || s_decorBatch));
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
        S32 rsnAlpha = ((s_currentMatFlags & 0x18) != (matFlags & 0x18));
        S32 rsnFog = (((matFlags & 7) == 5) !=
                      ((s_currentMatFlags & 7) == 5));
        S32 rsnAtlas = ((typePoly >= POLY_TEXTURE && !arrNow &&
                         s_batchAtlas != (const void *)PtrMap) ||
                        s_batchArray != arrNow);
        S32 rsnZbuf = (s_batchZBuffer != Fill_Flag_ZBuffer);
        S32 rsnNzw = (s_batchNZW != Fill_Flag_NZW);
        S32 trigger = (rsnAlpha || rsnFog || rsnAtlas || rsnZbuf || rsnNzw);
        if (trigger) {
            /* NZW alternation (terrain <-> animated per tile) is the dominant
               flush driver: route to the defer pool instead of flushing.
               Both pools draw depth-tested opaque geometry, so flushing
               NZW=0 before NZW=1 at frame end gives the same image. */
            S32 poolable = !(matFlags & 8) && ((matFlags & 7) != 5) &&
                           !(s_currentMatFlags & 8) &&
                           ((s_currentMatFlags & 7) != 5);
            if (poolable && rsnNzw) {
                if (s_deferVertCount > 0 &&
                    s_deferNZW == (U8)Fill_Flag_NZW) {
                    GL_SwapBatchWithDefer();
                } else if (s_deferVertCount == 0) {
                    GL_SwapBatchWithDefer();
                } else {
                    GL_FlushBatch();
                    s_flushRsnNzw++;
                }
            } else {
                GL_FlushBatch();
                if (rsnNzw) {
                    s_flushRsnNzw++;
                } else if (rsnAlpha) {
                    s_flushRsnAlpha++;
                } else if (rsnFog) {
                    s_flushRsnFog++;
                } else if (rsnAtlas) {
                    s_flushRsnAtlas++;
                } else if (rsnZbuf) {
                    s_flushRsnZbuf++;
                }
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
    s_batchWorld = s_glWorld;
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
    if (typePoly >= POLY_TEXTURE) {
        GL_ProcessAtlasDirty();
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
                               (const U8 *)PtrCLUTFog) >>
                              8)
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
    (void)resX;
    (void)resY;
    return FALSE;
}
void GL_Renderer_Shutdown(void) {}
S32 GL_Renderer_Resize(U32 resX, U32 resY) {
    (void)resX;
    (void)resY;
    return TRUE;
}
void GL_BeginFrame(void) {}
U32 GL_GetFrameSeq(void) { return 0; }
void GL_ClearFBO(void) {}
void GL_SetSceneClearColor(U8 palIndex) { (void)palIndex; }
void GL_ReadbackDepthBuffer(void) {}
void GL_ReadbackColorBuffer(void) {}
void GL_SnapshotTerrain(void) {}
void GL_SavePresentPNG(const char *path) {
    (void)path;
}
void GL_RestoreTerrainSnapshot(void) {}
void GL_DrawShadowPoly(const S16 *xs, const S16 *ys, const U16 *az, S32 n,
                       U32 level, S32 terrainBias) {
    (void)xs;
    (void)ys;
    (void)az;
    (void)n;
    (void)level;
}
void GL_ShadeBox(S32 x0, S32 y0, S32 x1, S32 y1, S32 level) {
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)level;
}
void GL_SetRenderQuality(U32 quality) { (void)quality; }
S32 GL_SetSuperSample(S32 mode) {
    (void)mode;
    return TRUE;
}
void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear) {
    (void)argbPixels;
    (void)pitch;
    (void)width;
    (void)height;
    (void)filterLinear;
}
void GL_PresentIndexed(const U8 *indices, U32 pitch, U32 width, U32 height,
                       const U32 *paletteU32, S32 filterLinear) {
    (void)indices;
    (void)pitch;
    (void)width;
    (void)height;
    (void)paletteU32;
    (void)filterLinear;
}
S32 GL_SphereTargetsFBO(void) { return 0; }
void GL_ArrayWindowBegin(void) {}
void GL_ArrayWindowEnd(void) {}
void GL_DecorBatchBegin(void) {}
void GL_DecorBatchEnd(void) {}
S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    (void)typePoly;
    (void)colorPoly;
    (void)nbPoints;
    (void)ptrPoints;
    return 0;
}
S32 GL_TerrainGPUActive(void) { return 0; }
U32 GL_TerrainVBOEpoch(void) { return 0; }
S32 GL_DecorGPUActive(void) { return 0; }
U32 GL_DecorVBOEpoch(void) { return 0; }
U32 GL_CreateDecorVBO(const T_GL_DecorVertex *verts, U32 vertexCount) {
    (void)verts;
    (void)vertexCount;
    return 0;
}
void GL_DeleteDecorVBO(U32 vbo) { (void)vbo; }
S32 GL_DecorArrayPin(const void *map, U32 w, U32 h) {
    (void)map;
    (void)w;
    (void)h;
    return -1;
}
void GL_DecorArrayUnpin(S32 layer) { (void)layer; }
void GL_DrawDecorVBO(U32 vbo, U32 vertexCount, const void *atlasPage,
                     U16 atlasMask, S32 x, S32 y, S32 z, S32 fogRow,
                     S32 clipZFar) {
    (void)vbo;
    (void)vertexCount;
    (void)x;
    (void)y;
    (void)z;
    (void)fogRow;
    (void)clipZFar;
}
U32 GL_CreateTerrainVBO(const T_GL_TerrainVertex *verts, U32 vertexCount) {
    (void)verts;
    (void)vertexCount;
    return 0;
}
void GL_DeleteTerrainVBO(U32 vbo) { (void)vbo; }
void GL_DrawTerrainVBO(U32 vbo, U32 vertexCount, const void *atlasPage,
                       S32 startZFog, S32 clipZFar, S32 palLevel) {
    (void)vbo;
    (void)vertexCount;
    (void)atlasPage;
    (void)startZFog;
    (void)clipZFar;
    (void)palLevel;
}

#endif /* USE_GPURENDERER */
