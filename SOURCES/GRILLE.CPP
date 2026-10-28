#include "C_EXTERN.H"
#include "DIRECTORIES.H"
#include <stdio.h>
#include <stdlib.h>

#include <3D/CAMERA.H>
#include <3D/PROJ.H>
#include <3D/PROJREC.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>

#ifdef USE_GPURENDERER
#include <GL/RENDER_GL.H>
#endif

/*══════════════════════════════════════════════════════════════════════════*
		      █▀▀▀▀ █▀▀▀█  █    █     █     █▀▀▀▀
		      ██ ▀█ ██▀█▀  ██   ██    ██    ██▀▀
		      ▀▀▀▀▀ ▀▀  ▀  ▀▀   ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

#define HEADER_BLOCK 3

#define DXBLOCK 0
#define DYBLOCK 1
#define DZBLOCK 2

extern S32 XS0, YS0, XS1, YS1, XS2, YS2, XS3, YS3; // bezier.c

/*-------------------------------------------------------------------------*/

S32 XpOrgw;
S32 YpOrgw;

#define NB_COLON 256  // 28
#define MAX_BRICK 256 // 150

S16 NbBrickColon[NB_COLON];

typedef struct
{
    S16 Xm;
    S16 Ym;
    S16 Zm;
    S16 Xs; /* screen X — stored explicitly so DrawOverBrick can re-blit at the exact
              same position the original AffBrickBlock paint used. The col index
              alone is insufficient when Map2Screen's iso origin offset is not a
              multiple of 24 (col*24-24 then loses the remainder, off-by-up-to-23
              from the original XScreen — manifests as iso bricks 'jittering' under
              DrawOverBrick at non-traditional render widths). At the historical
              640x480 the offset is 288 which is 12*24 so the legacy col*24-24
              reconstruction was exact. */
    S16 Ys;
    S16 Brick;
    S16 Col;

} T_COLONB;

T_COLONB ListBrickColon[NB_COLON][MAX_BRICK];

#ifdef USE_GPURENDERER
/* The isometric brick bank stores RLE graphics.  The GL world path decodes
   each tile once and keeps the raw palette-index bitmap for a textured quad;
   the software AffGraph path keeps its own RLE walk when GL is off. */
#define BRICK_GL_CACHE_SIZE 1024
typedef struct {
    const U8 *src;
    U8 *raw;
    S32 w;
    S32 h;
} T_BRICK_GL_CACHE;
static T_BRICK_GL_CACHE BrickGLCache[BRICK_GL_CACHE_SIZE];
static S32 BrickGLCacheNext = 0;

static void ClearBrickGLCache(void) {
    for (S32 i = 0; i < BRICK_GL_CACHE_SIZE; i++) {
        if (BrickGLCache[i].raw != NULL)
            free(BrickGLCache[i].raw);
        BrickGLCache[i].src = NULL;
        BrickGLCache[i].raw = NULL;
    }
    BrickGLCacheNext = 0;
}

static S32 s_currentBrickGL = -1;
static const U8 *DecodeBrickGL(const U8 *src, S32 *outW, S32 *outH,
                               S32 *outHotX, S32 *outHotY) {
    for (S32 i = 0; i < BRICK_GL_CACHE_SIZE; i++) {
        if (BrickGLCache[i].src == src) {
            *outW = BrickGLCache[i].w;
            *outH = BrickGLCache[i].h;
            *outHotX = (S8)src[2];
            *outHotY = (S8)src[3];
            return BrickGLCache[i].raw;
        }
    }

    const S32 w = src[0];
    const S32 h = src[1];
    if (w <= 0 || h <= 0)
        return NULL;

    /* Let AffGraph do the RLE walk: the brick bank's stream is the same one
       the software layer decodes, so this avoids a second, subtly different
       reader.  It writes into a scratch Log row grid; copy the tile out. */
    U8 *scratch = (U8 *)malloc((size_t)ModeDesiredX * (size_t)h);
    U8 *raw = (U8 *)malloc((size_t)w * (size_t)h);
    if (scratch == NULL || raw == NULL) {
        free(scratch);
        free(raw);
        return NULL;
    }
    memset(scratch, 0, (size_t)ModeDesiredX * (size_t)h);
    memcpy(raw, scratch, 1); /* keep the allocator branch identical on failure */

    void *savedLog = Log;
    const S32 savedXMin = ClipXMin, savedYMin = ClipYMin;
    const S32 savedXMax = ClipXMax, savedYMax = ClipYMax;
    Log = scratch;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = w - 1;
    ClipYMax = h - 1;
    AffGraph(s_currentBrickGL, -(S8)src[2], -(S8)src[3], BufferBrick);
    Log = savedLog;
    ClipXMin = savedXMin;
    ClipYMin = savedYMin;
    ClipXMax = savedXMax;
    ClipYMax = savedYMax;

    for (S32 y = 0; y < h; y++)
        memcpy(raw + y * w, scratch + y * ModeDesiredX, (size_t)w);

    free(scratch);
    T_BRICK_GL_CACHE *e = &BrickGLCache[BrickGLCacheNext];
    BrickGLCacheNext = (BrickGLCacheNext + 1) % BRICK_GL_CACHE_SIZE;
    if (e->raw != NULL)
        free(e->raw);
    e->src = src;
    e->raw = raw;
    e->w = w;
    e->h = h;
    *outW = w;
    *outH = h;
    *outHotX = (S8)src[2];
    *outHotY = (S8)src[3];
    return raw;
}
static void DrawBrickGL(S32 brick, S32 x, S32 y) {
    if (BufferBrick == NULL || brick < 0)
        return;
    U8 *src = BufferBrick + ((U32 *)BufferBrick)[brick];
    S32 w, h, hotX, hotY;
    s_currentBrickGL = brick;
    const U8 *raw = DecodeBrickGL(src, &w, &h, &hotX, &hotY);
    s_currentBrickGL = -1;
    if (raw == NULL)
        return;
    GL_SetSpriteNoDepth(1);
    GL_DrawSpriteQuad(x + hotX, y + hotY,
                      x + hotX + w, y + hotY + h,
                      0, 0, w, h, raw, w, h, 0, 0);
    GL_SetSpriteNoDepth(0);
}

/* ── Foreground brick overlay ──────────────────────────────────────────────
   The software renderer does not redraw a foreground brick from its own
   pixels: CopyMask copies the pristine background from Screen through the
   brick's mask.  A brick can be painted behind another brick that shares its
   mask area, so drawing the overlay brick's texture there would paint the
   buried brick over the visible one (a wall cap chopped by a notch of stone
   when an actor stands in the doorway).

   The GL overlay therefore rebuilds the pristine brick background for the
   object's clip window and composites that background through the overlay
   brick masks.  The base brick order is needed because the topmost brick at
   a pixel is the one drawn last. */

/* Every painted brick in base order; the overlay replays this list to
   reconstruct what the base layer looked like before models were drawn.
   The list carries values, not bucket pointers: bricks at the extreme left
   edge are painted but deliberately not bucketed for DrawOverBrick. */
typedef struct {
    S16 Xs;
    S16 Ys;
    S16 Brick;
    U16 Az; /* scene depth of the tile's cube column, same scale as models */
} T_BRICK_ORDER_ENTRY;

#define MAX_BRICK_ORDER 16384
static T_BRICK_ORDER_ENTRY s_brickOrder[MAX_BRICK_ORDER];
static S32 s_brickOrderCount = 0;
static S32 s_brickOrderOverflow = 0;

/* Quest stereo interiors pan the view every tracked frame.  Rebuilding the
   whole brick composite for a one-pixel move costs more than the pan buys, so
   the view offset is quantized: the composite is built at the quantized
   offset and the composed image is slid by the leftover at draw time.  The
   leftover stays below one quantum, and the canvas carries a quantum of
   margin on every side so the slide never exposes an edge.  Because the
   shift is a whole number of pixels, every on-screen texel is the one the
   unquantized pan would have painted. */
#define INTERIOR_PAN_QUANTUM 64
#define INTERIOR_LAYER_MARGIN INTERIOR_PAN_QUANTUM
static S32 s_layerResX = 0; /* exact view offset minus its quantized value */
static S32 s_layerResY = 0;
static S32 s_layerPanX = 0; /* quantized across offset, screen pixels */
static S32 s_layerPanY = 0;

static void BrickOrderReset(void) {
    s_brickOrderCount = 0;
    s_brickOrderOverflow = 0;
    s_layerResX = 0;
    s_layerResY = 0;
    s_layerPanX = 0;
    s_layerPanY = 0;
#ifdef USE_GPURENDERER
    if (GPURendererActive && CubeMode == CUBE_INTERIEUR &&
        GL_SinglePassStereoActive()) {
        const S32 panX = XCentre - ((S32)ModeDesiredX / 2 - 9);
        const S32 panY = YCentre - ((S32)ModeDesiredY / 2);
        s_layerPanX = panX - panX % INTERIOR_PAN_QUANTUM;
        s_layerPanY = panY - panY % INTERIOR_PAN_QUANTUM;
        s_layerResX = panX - s_layerPanX;
        s_layerResY = panY - s_layerPanY;
    }
#endif
}

static void BrickOrderAdd(S32 brick, S32 xs, S32 ys, U16 az) {
    if (s_brickOrderCount < MAX_BRICK_ORDER) {
        T_BRICK_ORDER_ENTRY *e = &s_brickOrder[s_brickOrderCount++];
        /* Store the composite-space position: the quantized pan is already
           baked into the recorded order, the draw carries the rest. */
        e->Xs = (S16)(xs - s_layerResX);
        e->Ys = (S16)(ys - s_layerResY);
        e->Brick = (S16)brick;
        e->Az = az;
    } else
        s_brickOrderOverflow = 1;
}

/* Scene depth of a brick column, on the scale IsoDepth() gives the model
   polygons: the painter key is (x + z) / 2 in world units from the camera,
   and aZ counts down from the far end.  Tiles at the same (x, z) share it, so
   a wall keeps one depth over its whole height while the floor recedes by
   row. */
static U16 BrickDepthAz(S32 x, S32 z) {
    const S32 depth =
        (((x * SIZE_BRICK_XZ) - CameraX) + ((z * SIZE_BRICK_XZ) - CameraZ)) >>
        1;
    return IsoDepthToAz(depth);
}

/* ── Composed base brick layer ────────────────────────────────────────────
   An interior is a painter-ordered 2D composite.  Drawing it one quad per
   brick costs a texture upload and a draw call per tile; a room runs to
   ~1500 tiles, far past what the sprite texture cache can hold, so the
   interior frame is dominated by that per-tile overhead rather than by
   rasterisation.

   Instead, compose the layer through the same AffGraph walk the software
   renderer uses — identical order, identical clipping, so the pixels match
   the reference rasterizer by construction — and draw the composed image as
   one palette-indexed quad.  Composing on the CPU also produces the pristine
   background the foreground-brick overlay needs, so that path copies from
   this image instead of replaying the whole brick order per clip window.

   Brick data is immutable once a cube is loaded (BufferBrick is read-only),
   so the composite only changes when the cube or the painted order does;
   the key below captures both and skips the walk when neither moved. */

static U8 *s_baseLayer = NULL;
static S32 s_baseLayerW = 0;
static S32 s_baseLayerH = 0;
static U32 s_baseLayerKey = 0;
static S32 s_baseLayerValid = 0;
static U32 s_baseLayerGen = 0;

/* AffGraph addresses its rows through TabOffLine, which carries the screen's
   pitch.  The composed canvas is wider than the screen, so the walk needs a
   canvas pitch while it runs and the screen's table back afterwards. */
static U32 *s_basePitchSave = NULL;
static S32 s_basePitchSaveH = 0;

/* Per-pixel scene depth of the composite, one U8 per screen pixel, painted
   alongside the colour.  The whole background used to sit at aZ = 0, which the
   polygon shader treats as screen space: it got no eye offset while the models
   did, so the room read as a flat painting behind them.  Carrying each tile's
   own depth per pixel lets the stereo fragment stage shift every pixel by the
   offset its depth earns, which is what makes the background parallax with the
   models instead of as one plane.  Values are normalized over the frame's
   azMin..azMax so a room always uses the full byte range. */
static U8 *s_baseDepth = NULL;
static U16 s_baseAzMin = 0;  /* nearest plane in the frame, aZ units */
static U16 s_baseAzSpan = 1; /* far - near + 1, so 0 never divides */

/* Painted bounds: the layer image is screen-sized, but only this rectangle
   holds bricks, and the filter cost is per fragment. */
static S32 s_baseLayerX0 = 0, s_baseLayerY0 = 0;
static S32 s_baseLayerX1 = -1, s_baseLayerY1 = -1;

static U32 BaseLayerKey(void) {
    U32 h = 2166136261u;
    h = (h ^ (U32)(uintptr_t)BufferBrick) * 16777619u;
    h = (h ^ (U32)NumCube) * 16777619u;
    h = (h ^ (U32)ModeDesiredX) * 16777619u;
    h = (h ^ (U32)ModeDesiredY) * 16777619u;
    h = (h ^ (U32)s_brickOrderCount) * 16777619u;
    for (S32 i = 0; i < s_brickOrderCount; i++) {
        const T_BRICK_ORDER_ENTRY *p = &s_brickOrder[i];
        h = (h ^ (U32)(U16)p->Brick) * 16777619u;
        h = (h ^ (U32)(U16)p->Xs) * 16777619u;
        h = (h ^ (U32)(U16)p->Ys) * 16777619u;
        /* Depth follows the camera, not only the painted rectangle, so it is
           part of the key even when every tile keeps its screen position. */
        h = (h ^ (U32)p->Az) * 16777619u;
    }
    return h;
}

/* Paint one tile's coverage into the depth map.  The brick data is a
   run-length stream (LIB386/SVGA/GRAPH.CPP decodes it for the colour blit):
   each control byte is two opcode bits and a count, opcode 0 skips pixels,
   1 writes the following literals, 2 repeats the next byte.  Walking it here
   in the same order as the colour composite marks exactly the pixels the
   tile leaves visible, at its own depth.  Zero marks an untouched pixel; the
   colour composite chroma-keys those, so their depth is never sampled. */
static void BaseLayerDepthStamp(const U8 *data, S32 x, S32 y, U8 dn) {
    const S32 deltaX = data[0];
    const S32 nbLine = data[1];
    const S32 x0 = x + (S8)data[2];
    const S32 y0 = y + (S8)data[3];
    if (x0 > ClipXMax || y0 > ClipYMax || x0 + deltaX - 1 < ClipXMin ||
        y0 + nbLine - 1 < ClipYMin)
        return;

    const U8 *g = data + 4;
    for (S32 line = 0; line < nbLine; line++) {
        const S32 row = y0 + line;
        U8 *dst = (row >= ClipYMin && row <= ClipYMax)
                      ? s_baseDepth + (size_t)row * (size_t)s_baseLayerW
                      : NULL;
        const U8 nbBlock = *g++;
        S32 col = x0;
        for (S32 b = 0; b < nbBlock; b++) {
            const U8 control = *g++;
            const S32 op = (control >> 6) & 3;
            const S32 count = (control & 0x3F) + 1;
            if (op == 0) {
                col += count;
                continue;
            }
            S32 last = col + count - 1;
            if (last > ClipXMax)
                last = ClipXMax;
            if (dst != NULL) {
                for (S32 c = col < ClipXMin ? ClipXMin : col; c <= last; c++) {
                    /* Literal runs are transparent per texel, repeat runs
                       per run: index 0 is the chroma key. */
                    const U8 texel = (op == 1) ? g[c - col] : g[0];
                    if (texel != 0)
                        dst[c] = dn;
                }
            }
            /* `g` already sits past the control byte: a literal run carries
               `count` bytes, a repeat run one (its value). */
            g += (op == 1) ? count : 1;
            col += count;
        }
    }
}

/* Compose the recorded brick order into s_baseLayer.  Returns 0 when the
   layer cannot be produced (no cube, order overflow, allocation failure),
   in which case the caller keeps the per-brick path. */
static S32 BaseLayerCompose(void) {
    const S32 screenW = (S32)ModeDesiredX;
    const S32 screenH = (S32)ModeDesiredY;
    /* The canvas is one quantum larger than the screen on each side.  Brick
       positions are stored in composite space, so the extra band is only
       there to be slid into view by a sub-quantum pan. */
    const S32 w = screenW + 2 * INTERIOR_LAYER_MARGIN;
    const S32 h = screenH + 2 * INTERIOR_LAYER_MARGIN;
    if (screenW <= 0 || screenH <= 0 || BufferBrick == NULL ||
        s_brickOrderCount == 0 || s_brickOrderOverflow)
        return 0;

    if (s_basePitchSaveH < h) {
        U32 *grown =
            (U32 *)realloc(s_basePitchSave, (size_t)h * sizeof(U32));
        if (grown == NULL) {
            s_baseLayerValid = 0;
            return 0;
        }
        s_basePitchSave = grown;
        s_basePitchSaveH = h;
    }

    if (s_baseLayer == NULL || s_baseLayerW != w || s_baseLayerH != h) {
        U8 *grown = (U8 *)realloc(s_baseLayer, (size_t)w * (size_t)h);
        if (grown == NULL) {
            s_baseLayerValid = 0;
            return 0;
        }
        s_baseLayer = grown;
        U8 *grownDepth = (U8 *)realloc(s_baseDepth, (size_t)w * (size_t)h);
        if (grownDepth == NULL) {
            s_baseLayerValid = 0;
            return 0;
        }
        s_baseDepth = grownDepth;
        s_baseLayerW = w;
        s_baseLayerH = h;
        s_baseLayerValid = 0;
    }

    const U32 key = BaseLayerKey();
    if (s_baseLayerValid && key == s_baseLayerKey)
        return 1;
    memset(s_baseLayer, 0, (size_t)w * (size_t)h);
    memset(s_baseDepth, 0, (size_t)w * (size_t)h);
    void *savedLog = Log;
    const S32 savedXMin = ClipXMin, savedYMin = ClipYMin;
    const S32 savedXMax = ClipXMax, savedYMax = ClipYMax;
    ClipXMin = 0;
    ClipYMin = 0;
    ClipXMax = w - 1;
    ClipYMax = h - 1;
    /* Depth range of this frame's tiles, which normalizes the depth map. */
    U16 azMin = 65535, azMax = 0;
    for (S32 i2 = 0; i2 < s_brickOrderCount; i2++) {
        const U16 az = s_brickOrder[i2].Az;
        if (az < azMin)
            azMin = az;
        if (az > azMax)
            azMax = az;
    }
    const S32 azSpan = (S32)azMax - (S32)azMin + 1;

    /* Lend the canvas its own line pitch for the walk.  Restored below before
       any other draw touches the screen table. */
    for (S32 i2 = 0; i2 < h; i2++) {
        s_basePitchSave[i2] = TabOffLine[i2];
        TabOffLine[i2] = (U32)i2 * (U32)w;
    }

    Log = s_baseLayer;
    S32 bx0 = w, by0 = h, bx1 = -1, by1 = -1;
    for (S32 i2 = 0; i2 < s_brickOrderCount; i2++) {
        const T_BRICK_ORDER_ENTRY *p = &s_brickOrder[i2];
        const U8 *src = BufferBrick + ((const U32 *)BufferBrick)[p->Brick];
        const U8 dn = (U8)(((S32)p->Az - (S32)azMin) * 255 / azSpan);
        const S32 px = p->Xs + INTERIOR_LAYER_MARGIN;
        const S32 py = p->Ys + INTERIOR_LAYER_MARGIN;
        AffGraph(p->Brick, px, py, BufferBrick);
        /* Depth in the same painter order, so the map names the tile whose
           pixels are the ones left standing in the colour composite. */
        BaseLayerDepthStamp(src, px, py, dn);
        const S32 x0 = px + (S8)src[2];
        const S32 y0 = py + (S8)src[3];
        const S32 x1 = x0 + src[0];
        const S32 y1 = y0 + src[1];
        if (x0 < bx0)
            bx0 = x0;
        if (y0 < by0)
            by0 = y0;
        if (x1 > bx1)
            bx1 = x1;
        if (y1 > by1)
            by1 = y1;
    }

    s_baseAzMin = azMin;
    s_baseAzSpan = (U16)azSpan;

    for (S32 i2 = 0; i2 < h; i2++)
        TabOffLine[i2] = s_basePitchSave[i2];
    Log = savedLog;
    ClipXMin = savedXMin;
    ClipYMin = savedYMin;
    ClipXMax = savedXMax;
    ClipYMax = savedYMax;

    if (getenv("LBA2_DUMP_DEPTH") != NULL) {
        FILE *f = fopen("/tmp/opencode/parallax/depth.pgm", "wb");
        if (f) {
            fprintf(f, "P5 %d %d 255\n", w, h);
            for (S32 y = 0; y < h; y++)
                fwrite(s_baseDepth + (size_t)y * (size_t)w, 1, (size_t)w, f);
            fclose(f);
            FILE *c2 = fopen("/tmp/opencode/parallax/layer.pgm", "wb");
            if (c2) {
                fprintf(c2, "P5 %d %d 255\n", w, h);
                for (S32 y = 0; y < h; y++)
                    fwrite(s_baseLayer + (size_t)y * (size_t)w, 1, (size_t)w, c2);
                fclose(c2);
            }
        }
    }
    s_baseLayerX0 = bx0;
    s_baseLayerY0 = by0;
    s_baseLayerX1 = bx1;
    s_baseLayerY1 = by1;
    s_baseLayerKey = key;
    s_baseLayerValid = 1;
    s_baseLayerGen++;
    return 1;
}

/* Draw the composed layer as one palette-indexed image.  A scene-sized
   composite would otherwise need one quad per brick, and the sprite path
   cannot take it as a single quad because its wrap window is capped at 256
   texels.  The GL side uploads only when the composition generation changes,
   so a static camera costs one draw call per frame.

   Returns 0 when the composite could not be built, so the caller can fall
   back to one quad per brick rather than leaving the room unpainted. */
static S32 DrawBaseLayerGL(void) {
    if (!BaseLayerCompose())
        return 0;
    /* One quad carrying the colour and, next to it, the depth map the stereo
       fragment stage warps by.  A single image cannot open a seam, which is
       what a per-band split did wherever a near wall met a far floor. */
    GL_DrawIndexedLayerOffset(
        0, s_baseLayer, s_baseDepth, s_baseLayerW, s_baseLayerH,
        s_baseLayerGen, DisplayTexFilter, s_baseLayerX0, s_baseLayerY0,
        s_baseLayerX1, s_baseLayerY1,
        s_layerResX - INTERIOR_LAYER_MARGIN,
        s_layerResY - INTERIOR_LAYER_MARGIN, s_baseAzMin, s_baseAzSpan, NULL,
        0, 0, 0, 0);
    return 1;
}

/* Degraded path: the recorded order still names every painted brick, so the
   layer can fall back to the pre-composite draw when it is unavailable. */
static void DrawBaseLayerFallbackGL(void) {
    for (S32 i = 0; i < s_brickOrderCount; i++) {
        const T_BRICK_ORDER_ENTRY *p = &s_brickOrder[i];
        /* The order stores composite space; this path has no composite to
           slide, so it puts each tile back on the exact screen position. */
        DrawBrickGL(p->Brick, p->Xs + s_layerResX, p->Ys + s_layerResY);
    }
}

/* Overlay mask patch, at screen stride in palette indices.  The pristine
   background it samples through the masks is the composed base layer. */
static U8 *s_overlayPatch = NULL;
static U8 *s_overlayDraw = NULL;
static U8 *s_overlayTile = NULL;
static S32 s_overlayAllocW = 0;
static S32 s_overlayAllocH = 0;
static S32 s_overlayDrawSize = 0;
static S32 s_overlayTileSize = 0;
static S32 s_overlayActive = 0;
static S32 s_overlayClipX0, s_overlayClipY0, s_overlayClipX1, s_overlayClipY1;
static S32 s_overlayX0, s_overlayY0, s_overlayX1, s_overlayY1;

/* Screen distance the near eye moves a point away from where mono draws it.
   The overlay has to cover the model where the near eye puts it, and it reads
   the wall's coverage one offset past that again, so both the marking window
   and the draw rect are sized from this.  Zero in mono, where the reference
   marking set is kept exactly. */
static S32 s_overlayMarginX = 0;
static S32 s_overlayMarginY = 0;
static S32 s_overlayWidened = 0;
static S32 s_overlayWinX0, s_overlayWinY0, s_overlayWinX1, s_overlayWinY1;

/* Eye 1's single-pass pass reuses the overlay eye 0 drew into both halves.
   Marking and flushing again would rebuild the same mask for a discarded
   draw, so the whole overlay is skipped there. */
static S32 s_overlaySkipPass = 0;

static S32 OverlaySecondEyePass(void) {
    return GPURendererActive && CubeMode == CUBE_INTERIEUR &&
           GL_SinglePassStereoActive() && GL_GetStereoEye() == 1;
}

/* Grow the candidate window for the marking walk and for the mask it writes.
   The model's own box is what the mono path marks and draws; stereo needs the
   band the other eye can move both the model and the wall into. */
static void OverlayWidenWindow(void) {
    if (s_overlaySkipPass)
        return;
    if (!GL_SinglePassStereoActive())
        return;
    GL_GetLayerShiftMargin(s_baseAzMin, s_baseAzSpan, &s_overlayMarginX,
                           &s_overlayMarginY);
    if (s_overlayMarginX == 0 && s_overlayMarginY == 0)
        return;
    s_overlayWidened = 1;

    const S32 mx = s_overlayMarginX * 2;
    const S32 my = s_overlayMarginY * 2;
    ClipXMin -= mx;
    ClipYMin -= my;
    ClipXMax += mx;
    ClipYMax += my;
    s_overlayClipX0 -= mx;
    s_overlayClipY0 -= my;
    s_overlayClipX1 += mx;
    s_overlayClipY1 += my;
    if (s_overlayClipX0 < 0)
        s_overlayClipX0 = 0;
    if (s_overlayClipY0 < 0)
        s_overlayClipY0 = 0;
    if (s_overlayClipX1 >= s_overlayAllocW)
        s_overlayClipX1 = s_overlayAllocW - 1;
    if (s_overlayClipY1 >= s_overlayAllocH)
        s_overlayClipY1 = s_overlayAllocH - 1;

    /* The walk writes the mask over that whole window, and a patch left over
       from an earlier frame would still be there — as coverage.  That is what
       made the near eye's occlusion alternate. */
    const S32 span = s_overlayClipX1 - s_overlayClipX0 + 1;
    for (S32 y = s_overlayClipY0; y <= s_overlayClipY1; y++)
        memset(s_overlayPatch + (size_t)y * s_overlayAllocW + s_overlayClipX0,
               0, span);
}

static void OverlayRestoreWindow(void) {
    ClipXMin = s_overlayWinX0;
    ClipYMin = s_overlayWinY0;
    ClipXMax = s_overlayWinX1;
    ClipYMax = s_overlayWinY1;
}
/* Depth of the bricks in the current patch: the overlay restores background
   pixels over a model, and it must sit at the same depth as the band they came
   from or it lands a few pixels off the rest of the background. */
static U32 s_overlayAzSum;
static U32 s_overlayAzCount;
static U16 s_overlayAz;

static void OverlayFreeBuffers(void) {
    free(s_overlayPatch);
    free(s_overlayDraw);
    free(s_overlayTile);
    s_overlayPatch = NULL;
    s_overlayDraw = NULL;
    s_overlayTile = NULL;
    s_overlayAllocW = 0;
    s_overlayAllocH = 0;
    s_overlayDrawSize = 0;
    s_overlayTileSize = 0;
}

static int OverlayEnsureBuffers(void) {
    const S32 w = (S32)ModeDesiredX;
    const S32 h = (S32)ModeDesiredY;
    if (w <= 0 || h <= 0)
        return 0;
    if (s_overlayPatch != NULL && s_overlayAllocW == w && s_overlayAllocH == h)
        return 1;
    OverlayFreeBuffers();
    s_overlayPatch = (U8 *)malloc((size_t)w * (size_t)h);
    if (s_overlayPatch == NULL) {
        OverlayFreeBuffers();
        return 0;
    }
    s_overlayAllocW = w;
    s_overlayAllocH = h;
    return 1;
}

static void DrawBrickOverlayFallback(T_COLONB *ptrlbc);

static void OverlayBegin(void) {
    s_overlayActive = 0;
    s_overlaySkipPass = 0;
    if (!(GPURendererActive && CubeMode == CUBE_INTERIEUR))
        return;
    if (OverlaySecondEyePass()) {
        s_overlaySkipPass = 1;
        return;
    }
    if (BufferBrick == NULL || !OverlayEnsureBuffers() || !s_baseLayerValid)
        return;

    S32 x0 = ClipXMin;
    S32 y0 = ClipYMin;
    S32 x1 = ClipXMax;
    S32 y1 = ClipYMax;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= s_overlayAllocW)
        x1 = s_overlayAllocW - 1;
    if (y1 >= s_overlayAllocH)
        y1 = s_overlayAllocH - 1;
    if (x1 < x0 || y1 < y0)
        return;

    s_overlayClipX0 = x0;
    s_overlayClipY0 = y0;
    s_overlayClipX1 = x1;
    s_overlayClipY1 = y1;
    s_overlayWinX0 = x0;
    s_overlayWinY0 = y0;
    s_overlayWinX1 = x1;
    s_overlayWinY1 = y1;
    s_overlayAzSum = 0;
    s_overlayAzCount = 0;
    s_overlayAz = 0;
    s_overlayMarginX = 0;
    s_overlayMarginY = 0;
    s_overlayWidened = 0;
    const S32 span = x1 - x0 + 1;
    for (S32 y = y0; y <= y1; y++)
        memset(s_overlayPatch + (size_t)y * s_overlayAllocW + x0, 0, span);
    s_overlayX0 = x1 + 1;
    s_overlayY0 = y1 + 1;
    s_overlayX1 = x0 - 1;
    s_overlayY1 = y0 - 1;
    s_overlayActive = 1;
}

/* Queue one foreground brick: copy the pristine background through its
   opaque texels into the patch. */
static void OverlayMark(T_COLONB *ptrlbc) {
    if (s_overlaySkipPass)
        return;
    if (!s_overlayActive) {
        DrawBrickOverlayFallback(ptrlbc);
        return;
    }
    const U8 *src = BufferBrick + ((const U32 *)BufferBrick)[ptrlbc->Brick];
    S32 w, h, hotX, hotY;
    s_currentBrickGL = ptrlbc->Brick;
    const U8 *raw = DecodeBrickGL(src, &w, &h, &hotX, &hotY);
    s_currentBrickGL = -1;
    if (raw == NULL)
        return;

    const S32 bx = ptrlbc->Xs + hotX;
    const S32 by = ptrlbc->Ys + hotY;
    S32 cx0 = bx;
    S32 cy0 = by;
    S32 cx1 = bx + w - 1;
    S32 cy1 = by + h - 1;
    if (cx0 < s_overlayClipX0)
        cx0 = s_overlayClipX0;
    if (cy0 < s_overlayClipY0)
        cy0 = s_overlayClipY0;
    if (cx1 > s_overlayClipX1)
        cx1 = s_overlayClipX1;
    if (cy1 > s_overlayClipY1)
        cy1 = s_overlayClipY1;
    if (cx1 < cx0 || cy1 < cy0)
        return;

    for (S32 y = cy0; y <= cy1; y++) {
        const U8 *srcRow = raw + (size_t)(y - by) * w + (cx0 - bx);
        U8 *dstRow = s_overlayPatch + (size_t)y * s_overlayAllocW + cx0;
        /* The patch stays screen-indexed; the composed layer sits in its own
           canvas, quantized pan and margin included. */
        const S32 ly = y - s_layerResY + INTERIOR_LAYER_MARGIN;
        for (S32 x = cx0; x <= cx1; x++, srcRow++, dstRow++) {
            if (*srcRow != 0) {
                const S32 lx = x - s_layerResX + INTERIOR_LAYER_MARGIN;
                if (lx >= 0 && lx < s_baseLayerW && ly >= 0 &&
                    ly < s_baseLayerH)
                    *dstRow = s_baseLayer[(size_t)ly * s_baseLayerW + lx];
            }
        }
    }

    if (cx0 < s_overlayX0)
        s_overlayX0 = cx0;
    if (cy0 < s_overlayY0)
        s_overlayY0 = cy0;
    if (cx1 > s_overlayX1)
        s_overlayX1 = cx1;
    if (cy1 > s_overlayY1)
        s_overlayY1 = cy1;
    s_overlayAzSum += BrickDepthAz(ptrlbc->Xm, ptrlbc->Zm);
    s_overlayAzCount++;
}

/* Draw the composed patch into the scene FBO.  Tiles keep the sprite path's
   implicit 256-texel repeat window from wrapping wide clip windows. */
static void OverlayFlush(void) {
    if (s_overlaySkipPass) {
        s_overlaySkipPass = 0;
        return;
    }
    if (!s_overlayActive)
        return;
    s_overlayActive = 0;
    OverlayRestoreWindow();
    if (getenv("LBA2_NO_OVERLAY") != NULL)
        return;

    /* Redraw the model's box from the composed layer, under a coverage mask of
       the bricks that are in front of it.  The layer program shifts both the
       colour and that mask by the pixel's own depth, so the patch is the
       background's own pixels at the background's own offset — nothing here
       is resampled differently from the wall around it.  In stereo the quad
       and the mask window grow with the eye margin (OverlayWidenWindow),
       because the model and the sampled texel both move by one eye offset. */
    if (s_baseDepth != NULL && s_baseLayerW > 0 && s_baseLayerH > 0 &&
        getenv("LBA2_OVERLAY_OLD") == NULL &&
        s_overlayClipX1 >= s_overlayClipX0 &&
        s_overlayClipY1 >= s_overlayClipY0) {
        const int stereo = s_overlayWidened;
        S32 mx0, my0, mx1, my1;
        S32 dx0, dy0, dx1, dy1;
        if (stereo) {
            /* Mask over the whole marking window, so a texel outside a marked
               brick reads as no coverage instead of clamping the tile's
               border.  The quad covers the model's box widened by one eye
               offset: that is where the near eye puts the model, and the
               reference box would leave it standing in front of the wall. */
            mx0 = s_overlayClipX0;
            my0 = s_overlayClipY0;
            mx1 = s_overlayClipX1 + 1;
            my1 = s_overlayClipY1 + 1;
            dx0 = s_overlayWinX0 - s_overlayMarginX;
            dy0 = s_overlayWinY0 - s_overlayMarginY;
            dx1 = s_overlayWinX1 + 1 + s_overlayMarginX;
            dy1 = s_overlayWinY1 + 1 + s_overlayMarginY;
        } else {
            /* Mono: the marked patch's own bounds, exactly as the reference
               overlay drew it. */
            mx0 = s_overlayX0;
            my0 = s_overlayY0;
            mx1 = s_overlayX1 + 1;
            my1 = s_overlayY1 + 1;
            dx0 = mx0;
            dy0 = my0;
            dx1 = mx1;
            dy1 = my1;
        }
        if (mx0 < 0)
            mx0 = 0;
        if (my0 < 0)
            my0 = 0;
        if (mx1 > (S32)ModeDesiredX)
            mx1 = (S32)ModeDesiredX;
        if (my1 > (S32)ModeDesiredY)
            my1 = (S32)ModeDesiredY;
        if (dx0 < 0)
            dx0 = 0;
        if (dy0 < 0)
            dy0 = 0;
        if (dx1 > (S32)ModeDesiredX)
            dx1 = (S32)ModeDesiredX;
        if (dy1 > (S32)ModeDesiredY)
            dy1 = (S32)ModeDesiredY;
        const S32 mw = mx1 - mx0;
        const S32 mh = my1 - my0;
        if (mw > 0 && mh > 0) {
            if (s_overlayTile == NULL ||
                s_overlayTileSize < mw * mh) {
                U8 *grown = (U8 *)realloc(s_overlayTile, (size_t)(mw * mh));
                if (grown == NULL)
                    return;
                s_overlayTile = grown;
                s_overlayTileSize = mw * mh;
            }
            for (S32 y = 0; y < mh; y++) {
                const U8 *src = s_overlayPatch +
                                (size_t)(my0 + y) * (size_t)s_overlayAllocW +
                                mx0;
                U8 *dst = s_overlayTile + (size_t)y * mw;
                for (S32 x = 0; x < mw; x++)
                    dst[x] = (src[x] != 0) ? 0xFF : 0;
            }
            if (dx1 > dx0 && dy1 > dy0) {
                /* The screen rect is the model's exact box; the texels come
                   from the layer's quantized canvas, a margin and a residual
                   away.  The mask is read beside those texels, so it moves
                   to the canvas origin too. */
                const S32 srcOffX = s_layerResX - INTERIOR_LAYER_MARGIN;
                const S32 srcOffY = s_layerResY - INTERIOR_LAYER_MARGIN;
                GL_DrawIndexedLayerOffset(
                    0, s_baseLayer, s_baseDepth, s_baseLayerW, s_baseLayerH,
                    s_baseLayerGen, DisplayTexFilter, dx0 - srcOffX,
                    dy0 - srcOffY, dx1 - srcOffX, dy1 - srcOffY, srcOffX,
                    srcOffY, s_baseAzMin, s_baseAzSpan, s_overlayTile, mw,
                    mh, mx0 + INTERIOR_LAYER_MARGIN - s_layerResX,
                    my0 + INTERIOR_LAYER_MARGIN - s_layerResY);
            }
            return;
        }
    }

    if (s_overlayX1 < s_overlayX0 || s_overlayY1 < s_overlayY0)
        return;

    if (s_overlayAzCount > 0)
        s_overlayAz = (U16)(s_overlayAzSum / s_overlayAzCount);
    const S32 w = s_overlayX1 - s_overlayX0 + 1;
    const S32 h = s_overlayY1 - s_overlayY0 + 1;
    const S32 need = w * h;
    if (need > s_overlayDrawSize) {
        U8 *grown = (U8 *)realloc(s_overlayDraw, (size_t)need);
        if (grown == NULL)
            return;
        s_overlayDraw = grown;
        s_overlayDrawSize = need;
    }
    if (s_overlayTile == NULL || s_overlayTileSize < 240 * 240) {
        U8 *grown = (U8 *)realloc(s_overlayTile, (size_t)(240 * 240));
        if (grown == NULL)
            return;
        s_overlayTile = grown;
        s_overlayTileSize = 240 * 240;
    }

    for (S32 y = 0; y < h; y++)
        memcpy(s_overlayDraw + (size_t)y * w,
               s_overlayPatch + (size_t)(s_overlayY0 + y) * s_overlayAllocW +
                   s_overlayX0,
               (size_t)w);

    GL_SetSpriteNoDepth(1);
    GL_SetSpriteFilterOverride(0);
    GL_SetSpriteUploadAlways(1);
    const S32 tile = 240;
    for (S32 ty = 0; ty < h; ty += tile) {
        const S32 th = (h - ty < tile) ? h - ty : tile;
        for (S32 tx = 0; tx < w; tx += tile) {
            const S32 tw = (w - tx < tile) ? w - tx : tile;
            for (S32 y = 0; y < th; y++)
                memcpy(s_overlayTile + (size_t)y * tw,
                       s_overlayDraw + (size_t)(ty + y) * w + tx,
                       (size_t)tw);
            GL_DrawSpriteQuad(s_overlayX0 + tx, s_overlayY0 + ty,
                              s_overlayX0 + tx + tw, s_overlayY0 + ty + th,
                              0, 0, tw, th, s_overlayTile, tw, th,
                              s_overlayAz, 0);
        }
    }
    GL_SetSpriteUploadAlways(0);
    GL_SetSpriteFilterOverride(-1);
    GL_SetSpriteNoDepth(0);
}

/* Software/fallback route: masked copy of the pristine background. */
static void DrawBrickOverlayFallback(T_COLONB *ptrlbc) {
    if (GPURendererActive && CubeMode == CUBE_INTERIEUR) {
        DrawBrickGL(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys);
        return;
    }
    CopyMask(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys, BufferMaskBrick, Screen);
}
#else
/* Software-only builds keep the original CopyMask path and need no base-layer
   reconstruction; the overlay state exists only for the GL renderer. */
static void OverlayBegin(void) {}

static void OverlayWidenWindow(void) {}

static void OverlayRestoreWindow(void) {}

static void OverlayFlush(void) {}

static void OverlayMark(T_COLONB *ptrlbc) {
    CopyMask(ptrlbc->Brick, ptrlbc->Xs, ptrlbc->Ys, BufferMaskBrick, Screen);
}
#endif

U8 *BufferMaskBrick = NULL;
S32 NbBlock;
S32 DxBlock, DyBlock, DzBlock;

S32 ForbidenBrick = 0;

U8 CodeJeu = 0;
S32 XMap = 0, YMap = 0, ZMap = 0;
S32 XScreen = 0, YScreen = 0;
PTR_U8 BufCube = 0;
PTR_U8 BufMap = 0;
PTR_U8 TabBlock = 0;
PTR_U8 BufferBrick = 0;

extern S32 Nxw, Nyw, Nzw;
//void	ReajustPos( U8 col ) ;
/*--------------------------------------------------------------------------*/
typedef struct
{
    U16 Gri_Start;                //	Index of GRI
    U16 Grm_Start;                //	Index of GRM
    U16 Bll_Start;                //	Index of BLL
    U16 Brk_Start;                //	Index of BRK
    U16 Max_Brk;                  //	Nb Brk
    U16 ForbidenBrick;            //	NEW : Brick Interdite
    U32 Max_Size_Gri;             //	Max Size Gri
    U32 Max_Size_Bll;             //	Max Size Bll
    U32 Max_Size_Brick_Cube;      //	Max Ideal Style Size
    U32 Max_Size_Mask_Brick_Cube; //	Max Size Mask

} T_BKG_HEADER;

T_BKG_HEADER BkgHeader;

typedef struct
{
    U8 My_Bll;        //	Index Style ( + Bll_Start )
    U8 My_Grm;        //	Index Grm ( + Grm_Start )
    U8 UsedBlock[32]; //	Bluck Used in Bll ( Bits )

} T_GRI_HEADER;

T_GRI_HEADER *GriHeader;
/*--------------------------------------------------------------------------*/
/*══════════════════════════════════════════════════════════════════════════*
	     █   █ █▀▀▀█ █▀▀▀█       █▀▀█  █▀▀▀█  █    █▀▀▀▀ █  ▄▀
	     ██▀▀█ ██ ▄█ ██▀█▀       ██▀▀█ ██▀█▀  ██   ██    ██▀▄
	     ▀▀  ▀ ▀▀▀▀  ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀  ▀▀   ▀▀▀▀▀ ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/
#define MAX_BRICK_GAME 20000L
#define OFFSET_BUFFER_FLAG 153800L

/*--------------------------------------------------------------------------*/
S32 LoadUsedBrick() {
    U32 i, j, b;
    U8 *pt;
    U8 *ptb;
    U16 *ptw;
    U32 *ptoff;
    U8 *ptdata;
    U32 offset;
    U32 maxbrk;
    U32 brick;
    U32 offseek;
    U32 *ptseek;
    U32 handle;
    U16 *tabflag;
    U16 *ptflag;
    U32 min, max;
    U8 *ptdecomp;
    COMPRESSED_HEADER header;
    S32 nbbrick;
    char tmpFilePath[ADELINE_MAX_PATH];

    tabflag = (U16 *)((U8 *)Screen + OFFSET_BUFFER_FLAG);

    memset(tabflag, 0, MAX_BRICK_GAME * 2L); /* Table de U16 pour NewNumBrick */

    min = 60000;
    max = 0;

    //-------------------------------------- Premiere Passe, Préparation ptflag

    pt = GriHeader->UsedBlock; /* Debut de Used Block */

    nbbrick = 0;

    for (i = 1; i < 256; i++) {
        b = pt[i >> 3] & (1 << (7 - (i & 7))); /*	Recup Bit Block	*/
        if (!b)
            continue;

        ptb = TabBlock + *(U32 *)(TabBlock + ((i - 1) << 2));
        ptw = (U16 *)(ptb + 5);                  /* Jump dx dy dz et collis */
        maxbrk = *ptb * *(ptb + 1) * *(ptb + 2); /* dx*dy*dz*/

        for (j = 0; j < maxbrk; j++, ptw += 2) {
            brick = *ptw;

            if (brick) {
                brick--;

                if (brick < min)
                    min = brick;
                if (brick > max)
                    max = brick;

                if (tabflag[brick] == 0) {
                    nbbrick++;
                    tabflag[brick] = 1;
                }
            }
        }
    }

    //-------------------------------------- Deuxieme Passe, Load Brick

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);
    handle = OpenRead(tmpFilePath);
    if (!handle)
        return (0L);

    Seek(handle, BkgHeader.Brk_Start << 2, SEEK_FROM_START); // Begin offsets Brk

    Read(handle, Screen, BkgHeader.Max_Brk << 2); // Read Offsets Brk

    ptflag = tabflag + min;

    ptoff = (U32 *)BufferBrick;

    offset = (nbbrick + 1) * 4L;

    *ptoff++ = offset; /* First Offset        */

    ptdata = BufferBrick + offset; /* Jump nbbrick+1 Offset	*/

    nbbrick = 0;

    ptseek = (U32 *)Screen;

    for (i = min; i <= max; i++, ptflag++) {
        if (*ptflag) {
            nbbrick++;              /*	One More*/
            *ptflag = (S16)nbbrick; /*	Brick+1, for BLL */
            offseek = *(ptseek + i);
            Seek(handle, offseek, SEEK_FROM_START);
            Read(handle, &header, sizeof(header));
            switch (header.CompressMethod) {
            case 0: //	No compression
                Read(handle, ptdata, header.SizeFile);
                break;
            case 1: //	Compression LZSS
                ptdecomp = ptdata + header.SizeFile - header.CompressedSizeFile + 500;
                Read(handle, ptdecomp, header.CompressedSizeFile);
                //					ExpandLZSS( ptdecomp, ptdata, header.SizeFile )	;
                ExpandLZ(ptdata, ptdecomp, header.SizeFile, 2);
                break;
            }
            ptdata += header.SizeFile;
            offset += header.SizeFile;
            *ptoff++ = offset;
        }
    }

    //-------------------------------------- Troisieme passe, Rename Block

    pt = GriHeader->UsedBlock; /* Debut de Used Block */

    for (i = 1; i < 256; i++) {
        b = pt[i >> 3] & (1 << (7 - (i & 7))); /*	Recup Bit Block	*/
        if (!b)
            continue;

        ptb = TabBlock + *(U32 *)(TabBlock + ((i - 1) << 2));
        ptw = (U16 *)(ptb + 5);                  /* Jump dx dy dz et collis */
        maxbrk = *ptb * *(ptb + 1) * *(ptb + 2); /* dx*dy*dz*/

        for (j = 0; j < maxbrk; j++, ptw += 2) {
            if (*ptw)
                *ptw = tabflag[(*ptw) - 1];
        }
    }

    Close(handle);

    ForbidenBrick = tabflag[BkgHeader.ForbidenBrick];

    return (offset);
}
/*-------------------------------------------------------------------------*/
void InitGrille(U32 numcube) {
#ifdef USE_GPURENDERER
    ClearBrickGLCache();
#endif
    U32 id_gri;
    U32 id_bll;
    char tmpFilePath[ADELINE_MAX_PATH];

    numcube = TabAllCube[numcube].Num; //	Indirection

    id_gri = BkgHeader.Gri_Start + numcube;

    GriHeader = (T_GRI_HEADER *)BufMap;

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);
    Load_HQR(tmpFilePath, BufMap, id_gri);

    ManageSystem(); // for streaming

    id_bll = BkgHeader.Bll_Start + GriHeader->My_Bll; //+numcube

    Load_HQR(tmpFilePath, TabBlock, id_bll);

    //----------------------------------------------	NEW
    if (!LoadUsedBrick())
        return;
    //----------------------------------------------

    ManageSystem(); // for streaming

    CreateMaskGph(BufferBrick, BufferMaskBrick);

    ManageSystem(); // for streaming

    NbBlock = (*(U32 *)TabBlock) / 4;

    CopyMapToCube();

    ManageSystem(); // for streaming

    RedrawGRMs(); // au cas ou il y ait des GRMs actives

    return;
}

/*--------------------------------------------------------------------------*/
void FreeGrille() {}
/*-------------------------------------------------------------------------*/
/*══════════════════════════════════════════════════════════════════════════*
		   █▀▀▀▀ ██▄ █ █▀▀▀▄       █   █ █▀▀▀█ █▀▀▀█
		   ██▀▀  ██▀██ ██  █       ██▀▀█ ██ ▄█ ██▀█▀
		   ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀        ▀▀  ▀ ▀▀▀▀  ▀▀  ▀
 *══════════════════════════════════════════════════════════════════════════*/
/*--------------------------------------------------------------------------*/
/*══════════════════════════════════════════════════════════════════════════*
	     █▀▀▀▀ █   █ ██▄ █ █▀▀▀▀ ▀▀█▀▀  █    █▀▀▀█ ██▄ █ ██▀▀▀
	     ██▀▀  ██  █ ██▀██ ██      ██   ██   ██  █ ██▀██ ▀▀▀▀█
	     ▀▀    ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀   ▀▀   ▀▀   ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/*--------------------------------------------------------------------------*/
U8 *GetAdrColonneMap(S32 x, S32 z) {
    U32 offset;

    offset = *(U16 *)(BufMap + sizeof(T_GRI_HEADER) + (x + (z * SIZE_CUBE_Z)) * 2);

    return (BufMap + sizeof(T_GRI_HEADER) + offset);
}
/*--------------------------------------------------------------------------*/
U8 *GetAdrColonneCube(S32 x, S32 z) {
    U32 offset;

    offset = (x * SIZE_CUBE_Y * 2) + (z * SIZE_CUBE_Z * SIZE_CUBE_Y * 2);

    return (BufCube + offset);
}

/*--------------------------------------------------------------------------*/
U8 WorldColBrickVisible(S32 oldx, S32 oldy, S32 oldz,
                        S32 xw, S32 yw, S32 zw) {
    U8 col = GetWorldColBrickVisible(xw, yw, zw);

    if (col)
        return (col);

    if (abs(xw - oldx) >= SIZE_BRICK_XZ
                              OR abs(yw - oldy) >= SIZE_BRICK_Y
                                                       OR abs(zw - oldz) >= SIZE_BRICK_XZ) {
        return (WorldColBrickVisible(oldx, oldy, oldz,
                                     oldx + (xw - oldx) / 2,
                                     oldy + (yw - oldy) / 2,
                                     oldz + (zw - oldz) / 2));
    }

    return (0); // Ok, cette fois, on est sûr !:)
}

/*--------------------------------------------------------------------------*/
/*U8	WorldColBrickVisible( S32 xw, S32 yw, S32 zw )
{
	if( GetBlockBrick( xw, yw, zw ) )
	{
		return( WorldColBrick( xw, yw, zw ) ) ;
	}
	else	return 0 ;
}
*/

/*--------------------------------------------------------------------------*/

void GetShadow(S32 xw, S32 yw, S32 zw) {
    S32 xm, ym, zm;
    U8 *ptc;
    S32 y;
    U8 *adr;
    S32 block;

    xm = xw / SIZE_BRICK_XZ;
    ym = yw / SIZE_BRICK_Y;
    zm = zw / SIZE_BRICK_XZ;

    ShadowX = Nxw = xw;
    ShadowZ = Nzw = zw;

    if (ym >= SIZE_CUBE_Y) // au-dessus du plafond de la map
    {
        ym = SIZE_CUBE_Y - 1;
    }

    ptc = BufCube + ym * 2 + (xm * SIZE_CUBE_Y * 2) + (zm * SIZE_CUBE_X * SIZE_CUBE_Y * 2);

    for (y = ym; y > 0; y--) {
        if (*ptc != 0)
            break;
        ptc -= 2;
    }

    XMap = xm;
    YMap = y;
    ZMap = zm;

    Nyw = (y + 1) * SIZE_BRICK_Y;

    ShadowCol = 0;

    if (*ptc != 0) {
        block = (*ptc++) - 1;
        adr = GetAdrBlock(block);
        adr += HEADER_BLOCK;
        adr += (*ptc) << 2; /* 4 Bytes to Jump	*/
        ShadowCol = *adr;
        ReajustPos(ShadowCol);
    }

    ShadowY = AdjustShadowObjects(xw, zw, Nyw, yw);
}

/*──────────────────────────────────────────────────────────────────────────*/
/*--------------------------------------------------------------------------*/
/* Shadow depth for the GL fill.  An interior shadow lies on the brick floor,
   so it takes the floor's depth -- the isometric painter key of the corner --
   exactly like the background bands and the model polygons.  Without it the
   corners keep the stale perspective GET_ZO values from the last exterior
   shadow, and in the headset the shadow sits several pixels off the floor it
   is supposed to darken. */
static U16 InteriorShadowAz(S32 worldX, S32 worldZ) {
    return IsoDepthToAz(((worldX - CameraX) + (worldZ - CameraZ)) >> 1);
}

/* Same key for anything else the room places by world position (particle
   flows), so it parallaxes with the bricks and models around it. */
U16 InteriorWorldAz(S32 worldX, S32 worldZ) {
    return InteriorShadowAz(worldX, worldZ);
}

S32 ProjectShadow(T_SORT *ptrs, S32 beta) {
    S32 xmin, zmin, xmax, zmax, xw, yw, zw;
    S32 ys, cy;
    S32 altitude_ref;
    S32 demix, demiz;

    ClearScreenMinMax();

    xw = ptrs->Xw;
    yw = ptrs->Yw;
    zw = ptrs->Zw;

    xmin = ptrs->XMin - xw;
    zmin = ptrs->ZMin - zw;
    xmax = ptrs->XMax - xw;
    zmax = ptrs->ZMax - zw;

    demix = (xmin + xmax) / 2;
    demiz = (zmin + zmax) / 2;

    // inclure ici la gestion des objets du decors
    altitude_ref = AdjustShadowObjects(xw, zw, yw, yw);

    Rotate(demix, zmin, beta);

    X0 += xw;
    Z0 += zw;
    ys = altitude_ref + SIZE_BRICK_Y;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    ShadowZO[0] = InteriorShadowAz(X0, Z0);
    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS0 = Xp;
    YS0 = Yp;
    AdjustScreenMinMax();

    Rotate(xmin, demiz, beta);

    X0 += xw;
    Z0 += zw;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    ShadowZO[1] = InteriorShadowAz(X0, Z0);
    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS1 = Xp;
    YS1 = Yp;
    AdjustScreenMinMax();

    Rotate(demix, zmax, beta);

    X0 += xw;
    Z0 += zw;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    ShadowZO[2] = InteriorShadowAz(X0, Z0);
    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS2 = Xp;
    YS2 = Yp;
    AdjustScreenMinMax();

    Rotate(xmax, demiz, beta);

    X0 += xw;
    Z0 += zw;

    GetShadow(X0, ys, Z0);
    cy = AdjustShadowObjects(X0, Z0, ShadowY, yw);

    if (cy < altitude_ref - ALTITUDE_MAX)
        cy = altitude_ref - ALTITUDE_MAX;
    else if (cy > altitude_ref + ALTITUDE_MAX)
        cy = altitude_ref + ALTITUDE_MAX;

    ShadowZO[3] = InteriorShadowAz(X0, Z0);
    if (!LongProjectPoint(X0, cy, Z0))
        return FALSE;
    XS3 = Xp;
    YS3 = Yp;
    AdjustScreenMinMax();

    UnsetClip();
    SetClip(ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax);

    return TRUE;
}

/*══════════════════════════════════════════════════════════════════════════*
		   █▄ ▄█ █▀▀▀▀ █▄ ▄█ █▀▀▀█  █    █▀▀▀█ █▀▀▀▀
		   ██▀ █ ██▀▀  ██▀ █ ██  █  ██   ██▀█▀ ██▀▀
		   ▀▀  ▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀  ▀▀   ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

U32 CreateMaskGph(U8 *ptsrc, U8 *ptdst) {
    U8 *ptd;
    U32 nbg, off;
    U32 *ptoff;
    U32 size, i;

    ptoff = (U32 *)ptdst;

    off = *(U32 *)ptsrc; /*	First Offset Src	*/

    *ptoff++ = off; /*	First Offset	*/

    ptd = ptdst + off;

    nbg = (off - 4) >> 2; /*	Nombre de Graph	*/

    for (i = 0; i < nbg; i++) {
        size = CalcGraphMsk(i, ptsrc, ptd);

        off += size;    /*	Maj Offset	*/
        *ptoff++ = off; /*	Write Offset 	*/
        ptd += size;    /*	Maj Pt Dest	*/
    }
    return (off);
}

/*-------------------------------------------------------------------------*/
void CopyMapToCube() {
    U8 *pts, *ptd;
    S32 x, z;

    /*-------------- Copy -------------------------*/
    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            pts = GetAdrColonneMap(x, z);
            ptd = GetAdrColonneCube(x, z);

            DecompColonne(pts, ptd);
        }
    }
}
/*──────────────────────────────────────────────────────────────────────────*/
void InitBufferCube() {
    S32 intmem;
    char tmpFilePath[ADELINE_MAX_PATH];

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);

    //	pas de RECOVER_AREA car BkgHeader en stored et pas en LZSS
    Load_HQR(tmpFilePath, &BkgHeader, 0); // HEADER

    // 	pas de RECOVER_AREA car TabAllCube en stored et pas en LZSS
    Load_HQR(tmpFilePath, (void *)TabAllCube, BkgHeader.Brk_Start + BkgHeader.Max_Brk);

    // 	Calcul des tailles nécessaires
    intmem = (BkgHeader.Max_Size_Brick_Cube + RECOVER_AREA)    // BufferBrick
             + (SIZE_CUBE_X * SIZE_CUBE_Y * SIZE_CUBE_Z * 2L); // BufCube

    //	PtrZBuffer sert aussi pour les décors intérieurs. The Z-buffer floor
    //	is set by Mem_ConfigureScreenBuffers() before InitMainBuffer runs (the
    //	SIZE_Z_BUFFER macro needs ModeDesiredX which isn't valid yet here);
    //	only grow above that floor if the cube's brick + cube data needs more.
    if (intmem > ListMem[MEM_PTR_ZBUFFER].Size)
        ListMem[MEM_PTR_ZBUFFER].Size = intmem;

    ListMem[MEM_BUFFER_MASK_BRICK].Size = BkgHeader.Max_Size_Mask_Brick_Cube;
    ListMem[MEM_BUF_MAP].Size = BkgHeader.Max_Size_Gri + RECOVER_AREA;
    ListMem[MEM_TAB_BLOCK].Size = BkgHeader.Max_Size_Bll + RECOVER_AREA;
}

/*══════════════════════════════════════════════════════════════════════════*
		   █▀▀▀▄  █    ██▀▀▀ █▀▀▀█ █     █▀▀▀█ █  ▄▀
		   ██  █  ██   ▀▀▀▀█ ██▀▀▀ ██    ██▀▀█ ██▀
		   ▀▀▀▀   ▀▀   ▀▀▀▀▀ ▀▀    ▀▀▀▀▀ ▀▀  ▀ ▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

#ifdef USE_GPURENDERER
/* A door frame protrudes one cell into the room.  When the actor stands in
   front of the doorway, the frame's "bricks devant" pass re-blits the top
   jamb over the head while the body remains in front of the door leaf.  Keep
   an actor that is on the room side of a nearby door frame in front of it, so
   the doorway reads as an opening rather than a mask. */
static S32 OverlayIsDoorFrame(S32 xm, S32 ym, S32 zm, T_COLONB *ptrlbc) {
    if (ptrlbc->Ym <= ym)
        return FALSE;

    for (S32 i = 0; i < NbObjets; i++) {
        T_OBJET *ptrobj = &ListObjet[i];
        if (!(ptrobj->Flags & SPRITE_CLIP))
            continue;

        const S32 ox = ptrobj->Obj.X / SIZE_BRICK_XZ;
        const S32 oz = ptrobj->Obj.Z / SIZE_BRICK_XZ;

        /* Painter order, not a screen-space guess: only reveal an actor that
           is nearer to the camera than the door. */
        if (xm + zm < ox + oz)
            continue;

        /* The jamb sits within two iso cells of the door anchor; three cells
           of z reach the frame in front of the leaf. */
        if (abs(ptrlbc->Xm - ox) <= 2 && abs(ptrlbc->Zm - oz) <= 3 &&
            abs(xm - ox) <= 2 && abs(zm - oz) <= 3)
            return TRUE;
    }
    return FALSE;
}
#endif

void DrawOverBrick(S32 xm, S32 ym, S32 zm) {
    register T_COLONB *ptrlbc;
    S32 col, i;
    S32 startcol, endcol;
    S32 xw, yw, zw;
    S32 recouvre;

    OverlayBegin();
    OverlayWidenWindow();

    // Bias -2, not -1: a wall one iso-cube south of the actor projects to
    // XScreen = actor_iso_XScreen - 24, i.e. col = actor_iso_col - 1. The
    // historical -1 only catches that wall when the actor's iso center happens
    // to land on a col-bucket boundary — true at widths where
    // (ModeDesiredX/2 - 32) % 24 == 0 (640 / 1024 / 1600). At 768 (offset
    // 352 = 14*24 + 16) the actor's iso center lands inside a col bucket and
    // the clip-derived `col(ClipXMin) - 1` sits in the *same* col as the
    // actor, missing the south-wall col entirely — DrawOverBrick never re-blits
    // those walls and they appear under the actor. -2 reliably covers one
    // extra col regardless of alignment. The extra col is a free harmless
    // scan when no walls live in it; when walls do live in it but don't
    // overlap the actor, CopyMask paints offscreen of the actor (same
    // wasted-CPU pattern the 640 build was already doing). See
    // WIDESCREEN_HARDCODED_DIMS_AUDIT.md.
    startcol = (ClipXMin + 24) / 24 - 2;
    if (startcol < 0)
        startcol = 0;
    endcol = (ClipXMax + 24) / 24;

    for (col = startcol; col <= endcol; col++) {
        ptrlbc = &ListBrickColon[col][0];

        for (i = 0; i < NbBrickColon[col]; i++) {
            /* bricks devant */
            if (ptrlbc->Ys + 38 > ClipYMin
                                      AND ptrlbc->Ys <= ClipYMax) {
                recouvre = FALSE;

                // essai correction bugs escaliers
                //			if( WorldColBrick(ptrlbc->Xm*SIZE_BRICK_XZ,ptrlbc->Ym*SIZE_BRICK_Y,ptrlbc->Zm*SIZE_BRICK_XZ) > 1 )
                if (ptrlbc->Col > 1) // collis complexe ?
                {
                    if (ptrlbc->Ym >= ym + 1) {
                        recouvre = TRUE;
                    }
                } else if (ptrlbc->Ym >= ym) {
                    recouvre = TRUE;
                }

#ifdef USE_GPURENDERER
                if (recouvre && GPURendererActive &&
                    OverlayIsDoorFrame(xm, ym, zm, ptrlbc))
                    recouvre = FALSE;
#endif
                if (recouvre) {
                    xw = ptrlbc->Xm * SIZE_BRICK_XZ;
                    yw = ptrlbc->Ym * SIZE_BRICK_Y;
                    zw = ptrlbc->Zm * SIZE_BRICK_XZ;

                    if ((ptrlbc->Xm >= xm AND ptrlbc->Zm >= zm)
                            OR(ptrlbc->Xm > xm
                                                AND WorldColBrick(xw - SIZE_BRICK_XZ, yw, zw) !=
                               1
                                   //				AND WorldColBrick(xw,yw,zw+SIZE_BRICK_XZ)==1 )
                                   AND WorldColBrick(xw, yw, zw + SIZE_BRICK_XZ) >= 1)
                                OR(ptrlbc->Zm > zm
                                                    AND WorldColBrick(xw, yw, zw - SIZE_BRICK_XZ) !=
                                   1
                                       //				AND WorldColBrick(xw+SIZE_BRICK_XZ,yw,zw)==1 ) )
                                       AND WorldColBrick(xw + SIZE_BRICK_XZ, yw, zw) >= 1)) {
                        OverlayMark(ptrlbc);
                    }
                }
            }
            ptrlbc++;
        }
    }
    OverlayFlush();
}

/*--------------------------------------------------------------------------*/
/* recouvrement reel pour obj qui ne depassent pas la ZV */

void DrawOverBrick3(S32 xm, S32 ym, S32 zm, S32 ymax) {
    register T_COLONB *ptrlbc;
    S32 col, i;
    S32 startcol, endcol;

    OverlayBegin();
    OverlayWidenWindow();
    startcol = (ClipXMin + 24) / 24 - 1;
    endcol = (ClipXMax + 24) / 24;

    for (col = startcol; col <= endcol; col++) {
        ptrlbc = &ListBrickColon[col][0];

        for (i = 0; i < NbBrickColon[col]; i++, ptrlbc++) {
            /* bricks devant */
            if (ptrlbc->Ys + 38 > ClipYMin
                                      AND ptrlbc->Ys <= ClipYMax) {
                if (ptrlbc->Ym > ymax) {
                    if ((ptrlbc->Zm >= zm)
                            OR(ptrlbc->Xm >= xm)) {
                        OverlayMark(ptrlbc);
                    }
                } else if (ptrlbc->Ym >= ym) {
                    if ((ptrlbc->Zm == zm)
                            AND(ptrlbc->Xm == xm)) {
                        OverlayMark(ptrlbc);
                    }

                    if ((ptrlbc->Zm > zm)
                            OR(ptrlbc->Xm > xm)) {
                        OverlayMark(ptrlbc);
                    }
                }
            }
        }
    }
    OverlayFlush();
}

/*--------------------------------------------------------------------------*/
// recouvrement reel pour obj qui ne depassent pas la ZV
// GROSSE RUSTINE: Cage à la fin de LBA2 pour ne pas recouvrir l'objet avec
// les briques du sol en 0

void DrawOverBrickCage(S32 xm, S32 ym, S32 zm) {
    register T_COLONB *ptrlbc;
    S32 col, i;
    S32 startcol, endcol;

    OverlayBegin();
    startcol = (ClipXMin + 24) / 24 - 1;
    endcol = (ClipXMax + 24) / 24;

    for (col = startcol; col <= endcol; col++) {
        ptrlbc = &ListBrickColon[col][0];

        for (i = 0; i < NbBrickColon[col]; i++, ptrlbc++) {
            /* bricks devant */
            if (ptrlbc->Ys + 38 > ClipYMin
                                      AND ptrlbc->Ys <= ClipYMax) {
                if (ptrlbc->Ym > 0 // diff ici avec DrawOverBrick3
                                 AND ptrlbc->Ym >= ym) {
                    if ((ptrlbc->Zm == zm)
                            AND(ptrlbc->Xm == xm)) {
                        OverlayMark(ptrlbc);
                    }

                    if ((ptrlbc->Zm > zm)
                            OR(ptrlbc->Xm > xm)) {
                        OverlayMark(ptrlbc);
                    }
                }
            }
        }
    }
    OverlayFlush();
}

/*--------------------------------------------------------------------------*/

static void Map2ScreenPanned(S32 x, S32 y, S32 z);

void AffBrickBlock(S32 block, S32 brick, S16 x, S16 y, S16 z) {
    U8 *adr;
    S32 nb, col;
    register T_COLONB *ptrlbc;
    S16 numbrick;
    U8 brickcol;

    adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;

    adr += (brick << 2); /* 4 Bytes to Jump	*/
    brickcol = *adr;
    adr += 2;

    numbrick = *(U16 *)adr; /* & 32767 */

    if (numbrick AND numbrick != ForbidenBrick) {
        Map2ScreenPanned(x - StartXCube, y - StartYCube, z - StartZCube);

        // Left clip is -47 (one full brick width minus a pixel), not -24 (half-brick).
        // Many iso bricks have hotX=0 or +24 — their visible pixel range at XScreen=-32
        // is [0,15], 16 pixels on screen. The historical -24 threshold assumed
        // hotX=-24 and was silently correct only at widths where
        // (ModeDesiredX/2 - 32) % 24 == 0 (4:3 / 640: 288 = 12*24). At wider widths
        // the iso origin offset slides off that multiple and the rejected bricks
        // start having visible content. See WIDESCREEN_HARDCODED_DIMS_AUDIT.md.
        if (XScreen >= -47 AND XScreen < (S32)ModeDesiredX AND YScreen >= -38 AND YScreen < (S32)ModeDesiredY) {
#ifdef USE_GPURENDERER
            /* GL paints the whole brick layer as one composed background
               after this walk; recording the order is all the base pass
               does.  The software path still blits each brick here. */
            if (GPURendererActive && CubeMode == CUBE_INTERIEUR) {
                BrickOrderAdd(numbrick - 1, XScreen, YScreen,
                              BrickDepthAz(x, z));
            } else {
                AffGraph(numbrick - 1, XScreen, YScreen, BufferBrick);
                BrickOrderAdd(numbrick - 1, XScreen, YScreen,
                              BrickDepthAz(x, z));
            }
#else
            AffGraph(numbrick - 1, XScreen, YScreen, BufferBrick);
#endif

            // ListBrickColon is indexed by `col = (XScreen+24)/24`, which goes
            // negative below -24 and would corrupt the array. Bricks in the
            // newly-painted [-47,-24) strip aren't bucketed for DrawOverBrick
            // re-blits — acceptable, they sit on the leftmost pixel column and
            // their visible pixels are already painted by AffGraph above.
            if (XScreen < -24) {
                return;
            }

            col = (XScreen + 24) / 24; /* 48 / 2 colonne intercalée */

            nb = NbBrickColon[col];

            ptrlbc = &ListBrickColon[col][nb];

            if (nb < MAX_BRICK) {
                ptrlbc->Xm = x;
                ptrlbc->Ym = y;
                ptrlbc->Zm = z;
                ptrlbc->Xs = (S16)XScreen;
                ptrlbc->Ys = (S16)YScreen;
                ptrlbc->Brick = (S16)(numbrick - 1);
                ptrlbc->Col = brickcol;
                NbBrickColon[col]++;
            } else
                Message("Arg MAX_BRICK Z BUFFER atteint", TRUE);
        }
    }
}

/*--------------------------------------------------------------------------*/
/*ptc = BufCube + y*2 + (x*SIZE_CUBE_Y*2) + (z*SIZE_CUBE_X*SIZE_CUBE_Y*2) ;*/

void AffGrille() {
#ifdef USE_GPURENDERER
    /* Single-pass stereo draws both halves from eye 0.  Eye 1 still presents
       its half, but the brick order, colon list and composed layer are the
       same, so walking the whole cube again would only rebuild them. */
    if (GPURendererActive && CubeMode == CUBE_INTERIEUR &&
        GL_SinglePassStereoActive() && GL_GetStereoEye() == 1)
        return;
    if (GPURendererActive && CubeMode == CUBE_INTERIEUR && PtrPal != NULL)
        SetVideoPaletteSync(PtrPal);
    /* The isometric grid leaves the FBO edges uncovered where the room does
       not fill the screen.  The software path fills those with the clear
       colour, which is palette 0 here — not the exterior fog entry that
       AffGrilleExt uses — so pin the FBO rather than inherit the last
       exterior's sky colour. */
    GL_SetSceneClearColor(0);
    GL_ClearFBO();
#endif
    S32 block;
    U8 *ptc;
    S16 z, y, x;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    memset(NbBrickColon, 0, sizeof(S16) * NB_COLON);
#ifdef USE_GPURENDERER
    BrickOrderReset();
#endif

    ptc = BufCube;
    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            for (y = 0; y < SIZE_CUBE_Y; y++) {
                block = *ptc++;

                if (block) {
                    AffBrickBlock(block - 1, *ptc, x, y, z);
                }
                ptc++;
            }
        }
    }
#ifdef USE_GPURENDERER
    if (GPURendererActive && CubeMode == CUBE_INTERIEUR &&
        !DrawBaseLayerGL())
        DrawBaseLayerFallbackGL();
#endif
}

/*══════════════════════════════════════════════════════════════════════════*
	  █     █▀▀▀█ █▀▀█  █  ▄▀ █▀▀▀█  █    ██▄ █ ▀▀█▀▀ █   █ █▀▀▀▀
	  ██    ██▀▀█ ██▀▀█ ██▀   ██▀█▀  ██   ██▀██   ██  ██▀▀█ ██▀▀
	  ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀    ▀▀  ▀  ▀▀   ▀▀  ▀   ▀▀  ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/*--------------------------------------------------------------------------*/
// Calcule les masques du Z-Buffer pour DrawOverBrick sans dessiner la
// brick
void AffBrickBlockColon(S32 block, S32 brick, S16 x, S16 y, S16 z) {
    U8 *adr;
    S32 nb, col;
    register T_COLONB *ptrlbc;
    S16 numbrick;
    U8 brickcol;

    adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;

    adr += (brick << 2); /* 4 Bytes to Jump	*/
    brickcol = *adr;
    adr += 2;

    numbrick = *(U16 *)adr; /* & 32767 */

    if (numbrick AND numbrick != ForbidenBrick) {
        Map2ScreenPanned(x - StartXCube, y - StartYCube, z - StartZCube);

        if (XScreen >= -24 AND XScreen < (S32)ModeDesiredX AND YScreen >= -38 AND YScreen < (S32)ModeDesiredY) {
            col = (XScreen + 24) / 24; /* 48 / 2 colonne intercalée */

            nb = NbBrickColon[col];

            ptrlbc = &ListBrickColon[col][nb];

            if (nb < MAX_BRICK) {
                ptrlbc->Xm = x;
                ptrlbc->Ym = y;
                ptrlbc->Zm = z;
                ptrlbc->Xs = (S16)XScreen;
                ptrlbc->Ys = (S16)YScreen;
                ptrlbc->Brick = (S16)(numbrick - 1);
                ptrlbc->Col = brickcol;
                NbBrickColon[col]++;

            } else
                Message("Arg MAX_BRICK Z BUFFER atteint", TRUE);
        }
    }
}

// Calcule le Z-Buffer pour le DrawOverBrick de toute la grille sans
// afficher les bricks
void AffGrilleOnly() {
    S32 block;
    U8 *ptc;
    U8 *adr;
    S16 z, y, x;
    U8 codejeu;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    memset(NbBrickColon, 0, sizeof(S16) * NB_COLON);
#ifdef USE_GPURENDERER
    BrickOrderReset();
#endif

    ptc = BufCube;

    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            for (y = 0; y < SIZE_CUBE_Y; y++) {
                block = *ptc++;

                if (block) {
                    block--;

                    //-------------------
                    adr = GetAdrBlock(block);
                    adr += (HEADER_BLOCK + 1);

                    codejeu = (U8)((*adr) >> 4);

                    if (codejeu != CJ_LABYRINTHE)
                    //-------------------
                    {
                        AffBrickBlockColon(block, *ptc, x, y, z);
                    }
                }
                ptc++;
            }
        }
    }
}
/*--------------------------------------------------------------------------*/
// Affiche la brick sans calculer le Z-Buffer pour DrawOverBrick
void AffBrickBlockOnly(S32 block, S32 brick, S16 x, S16 y, S16 z) {
    U8 *adr;
    S32 numbrick;

    adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;

    adr += (brick << 2); /* 4 Bytes to Jump	*/
    adr += 2;

    numbrick = *(U16 *)adr; /* & 32767 */

    if (numbrick AND numbrick != ForbidenBrick) {
        Map2ScreenPanned(x - StartXCube, y - StartYCube, z - StartZCube);

        if (XScreen >= -24 AND XScreen < (S32)ModeDesiredX AND YScreen >= -38 AND YScreen < (S32)ModeDesiredY) {
#ifdef USE_GPURENDERER
            if (GPURendererActive && CubeMode == CUBE_INTERIEUR)
                DrawBrickGL(numbrick - 1, XScreen, YScreen);
            else
#endif
                AffGraph(numbrick - 1, XScreen, YScreen, BufferBrick);
        }
    }
}

// Redessine uniquement les block n'ayant pas le code jeu Invisible
void AffGrilleLabyrinthe() {
#ifdef USE_GPURENDERER
    /* Same background rule as AffGrille: palette 0, never the last
       exterior's fog entry. */
    GL_SetSceneClearColor(0);
    GL_ClearFBO();
#endif
    S32 block;
    U8 *ptc;
    U8 *adr;
    S16 z, y, x;
    U8 codejeu;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    ptc = BufCube;

    for (z = 0; z < SIZE_CUBE_Z; z++) {
        for (x = 0; x < SIZE_CUBE_X; x++) {
            for (y = 0; y < SIZE_CUBE_Y; y++) {
                block = *ptc++;

                if (block) {
                    block--;

                    adr = GetAdrBlock(block);
                    adr += (HEADER_BLOCK + 1);

                    codejeu = (U8)((*adr) >> 4);

                    if (codejeu != CJ_LABYRINTHE) {
                        AffBrickBlockOnly(block, *ptc, x, y, z);
                    }
                }
                ptc++;
            }
        }
    }
}

// Affiche un cube de block (3*3*3) autour du point passe (coordonnees world)
void AffOneBrick(S32 x, S32 y, S32 z) {
    S32 block;
    U8 *ptc;
    U8 *adr;
    S32 xmin, ymin, xmax, ymax;
    S16 xs, ys, zs;
    S16 zdeb;
    U8 codejeu;

    // Sentinel seeds for the min-accumulator below. Must be at least as large as
    // any ScreenXMin/YMin a drawn brick could produce, i.e. the framebuffer right
    // and bottom edges. At 640x480 these are the original 639/479; route through
    // ModeDesiredX/Y so wider builds catch bricks past x=640. See Phase 2 in
    // docs/WIDESCREEN.md.
    xmin = (S32)ModeDesiredX - 1;
    ymin = (S32)ModeDesiredY - 1;
    xmax = 0;
    ymax = 0;

    PtrProjectPoint(0, 0, 0);
    XpOrgw = Xp;
    YpOrgw = Yp;

    x /= SIZE_BRICK_XZ;
    y /= SIZE_BRICK_Y;
    z /= SIZE_BRICK_XZ;

    zdeb = (S16)(z - 1);
    if (zdeb < 0)
        zdeb = 0;

    for (zs = zdeb; zs <= z + 1; zs++) {
        for (xs = (S16)(x - 1); xs <= (S16)(x + 1); xs++) {
            ptc = GetAdrColonneCube(xs, zs);
            ptc += (y - 1) * 2;
            for (ys = (S16)(y - 1); ys <= (S16)(y + 1); ys++) {
                block = *ptc++;
                if (block) {
                    block--;

                    adr = GetAdrBlock(block);
                    adr += (HEADER_BLOCK + 1);

                    codejeu = (U8)((*adr) >> 4);

                    if (codejeu == CJ_LABYRINTHE) {
                        AffBrickBlockOnly(block, *ptc, xs, ys, zs);
                        if (ScreenXMin < xmin)
                            xmin = ScreenXMin;
                        if (ScreenXMax > xmax)
                            xmax = ScreenXMax;
                        if (ScreenYMin < ymin)
                            ymin = ScreenYMin;
                        if (ScreenYMax > ymax)
                            ymax = ScreenYMax;
                    }
                }
                ptc++;
            }
        }
    }

    SetClip(xmin, ymin, xmax, ymax);
    DrawRecover((x + 1) * SIZE_BRICK_XZ, y * SIZE_BRICK_Y, (z + 1) * SIZE_BRICK_XZ, 0, 0);
}

// Fin Labyrinthe

/*--------------------------------------------------------------------------*/

void IncrustGrm(T_ZONE *ptrz) {
    S32 numgrm, xo, yo, zo;
    U8 *pts, *ptd;
    S32 x, z;
    S32 dx, dy, dz;
    char tmpFilePath[ADELINE_MAX_PATH];

    numgrm = ptrz->Info0;

    GetResPath(tmpFilePath, ADELINE_MAX_PATH, BKG_HQR_NAME);
    Load_HQR(tmpFilePath, Screen,
             BkgHeader.Grm_Start + GriHeader->My_Grm + numgrm);

    xo = ptrz->X0 / SIZE_BRICK_XZ;
    yo = ptrz->Y0 / SIZE_BRICK_Y;
    zo = ptrz->Z0 / SIZE_BRICK_XZ;

    pts = (U8 *)Screen;

    dx = *pts++;
    dy = *pts++;
    dz = *pts++;

    yo <<= 1; //	Cause S16
    dy <<= 1; //	Cause S16

    for (z = zo; z < zo + dz; z++) {
        for (x = xo; x < xo + dx; x++) {
            ptd = GetAdrColonneCube(x, z);
            ptd += yo;
            memcpy(ptd, pts, dy);
            pts += dy;
        }
    }

    FirstTime = AFF_ALL_FLIP;
}

/*--------------------------------------------------------------------------*/
void DesIncrustGrm(T_ZONE *ptrz) {
    U8 *pts, *ptd;
    U8 col[50]; //	1 Colonne decompressee
    S32 x, y, z, dy;
    S32 z0, z1, x0, x1;
    S32 y0, y1;

    x0 = ptrz->X0 / SIZE_BRICK_XZ;
    x1 = ptrz->X1 / SIZE_BRICK_XZ;

    y0 = ptrz->Y0 / SIZE_BRICK_Y;
    y1 = ptrz->Y1 / SIZE_BRICK_Y;

    z0 = ptrz->Z0 / SIZE_BRICK_XZ;
    z1 = ptrz->Z1 / SIZE_BRICK_XZ;

    y = y0 << 1; //	Cause S16

    dy = ((y1 - y0) + 1) << 1;

    /*-------------- Copy -------------------------*/
    for (z = z0; z <= z1; z++) {
        for (x = x0; x <= x1; x++) {
            pts = GetAdrColonneMap(x, z);
            DecompColonne(pts, col);

            ptd = GetAdrColonneCube(x, z);
            memcpy(ptd + y, col + y, dy);
        }
    }

    FirstTime = AFF_ALL_FLIP;
}

/*--------------------------------------------------------------------------*/
void RedrawGRMs(void) {
    S32 n;
    T_ZONE *ptrz = ListZone;

    for (n = 0; n < NbZones; n++, ptrz++) {
        switch (ptrz->Type) {
        case 3: // GRM
            if (ptrz->Info2) {
                IncrustGrm(ptrz);
            }
            break;
        }
    }
}
/*--------------------------------------------------------------------------*/

#include <stdio.h>

static S32 logical_shift_right_s32(S32 value, U32 bits) {
    return (S32)(((U32)value) >> bits);
}

static S32 arithmetic_shift_right_8(S32 value) {
    if (value >= 0) {
        return value >> 8;
    }

    return (S32)(-(((-(long long)value) + 255LL) >> 8));
}

U8 WorldColBrick(S32 xw, S32 yw, S32 zw) {
    XMap = logical_shift_right_s32(xw, 9);
    YMap = logical_shift_right_s32(yw, 8);
    ZMap = logical_shift_right_s32(zw, 9);

    if (XMap < 0 || XMap >= 64) {
        return 0;
    }

    int cubeIndex = (XMap * 50);

    if (YMap <= -1) {
        YMap = 0;
        return 1;
    } else if (YMap < 0 || YMap >= 25) {
        return 0;
    }

    cubeIndex += YMap * 2;

    if (ZMap < 0 || ZMap >= 64) {
        return 0;
    }

    cubeIndex += ZMap * 3200;

    U8 value = BufCube[cubeIndex];

    if (value == 0) {
        return BufCube[cubeIndex + 1];
    }

    U32 numBlock = value - 1;
    U8 *addr = GetAdrBlock(numBlock);
    addr += HEADER_BLOCK;
    addr += BufCube[cubeIndex + 1] * 4;

    return *addr;
}

//-------------------------------------------------------------------
U8 WorldColBrickFull(S32 xw, S32 yw, S32 zw, S32 ymax) {
    XMap = logical_shift_right_s32(xw, 9);
    YMap = arithmetic_shift_right_8(yw);
    ZMap = logical_shift_right_s32(zw, 9);

    if (XMap < 0 || XMap >= SIZE_CUBE_X)
        return 0;

    if (YMap <= -1) {
        return 1;
    }

    if (YMap >= SIZE_CUBE_Y)
        return 0;

    if (ZMap < 0 || ZMap >= SIZE_CUBE_Z)
        return 0;

    U32 colIdx = XMap * 50 + YMap * 2 + ZMap * 3200;

    U8 col = BufCube[colIdx];
    if (col == 0) {
        // eventuelle col transparente
        col = BufCube[colIdx + 1];

        // check full height
        U32 something = ((U32)(ymax + 255)) >> 8;
        S32 scanY = YMap;
        do {
            if (scanY >= SIZE_CUBE_Y - 1) {
                return col;
            }

            colIdx += 2;
            ++scanY;

            if (BufCube[colIdx] != 0 || BufCube[colIdx + 1] != 0) {
                return 1;
            }
            something--;
        } while (something != 0);

        return col;
    }

    S32 block = BufCube[colIdx] - 1;
    U8 *adr = GetAdrBlock(block);
    adr += HEADER_BLOCK;
    adr += BufCube[colIdx + 1] * 4; // 4 Bytes to Jump
    col = *adr;

    // check full height
    U32 something = ((U32)(ymax + 255)) >> 8;
    S32 scanY = YMap;
    do {
        if (scanY >= SIZE_CUBE_Y - 1) {
            return col;
        }

        colIdx += 2;
        ++scanY;

        if (BufCube[colIdx] != 0 || BufCube[colIdx + 1] != 0) {
            return 1;
        }
        something--;
    } while (something != 0);

    return col;
}

//-------------------------------------------------------------------
U8 WorldCodeBrick(S32 xw, S32 yw, S32 zw) {
    XMap = logical_shift_right_s32(xw, 9);

    U8 code = 0;

    if (yw > -1) {
        U8 *ptc = BufCube;

        YMap = logical_shift_right_s32(yw, 8);
        ZMap = logical_shift_right_s32(zw, 9);

        ptc += XMap * 50 + YMap * 2 + ZMap * 3200;

        U8 col = *ptc;
        if (col) {
            U8 *adr = GetAdrBlock(col - 1);
            adr += HEADER_BLOCK;
            code = *(adr + 1);
        }
    }
    return code;
}

//-------------------------------------------------------------------
U8 GetBlockBrick(S32 xw, S32 yw, S32 zw) {
    XMap = logical_shift_right_s32(xw, 9);
    YMap = logical_shift_right_s32(yw, 8);
    ZMap = logical_shift_right_s32(zw, 9);

    if (XMap < 0 || XMap >= 64) {
        return 0; // ovfw
    }

    if (YMap <= -1) {
        YMap = 0;
        return 1; // col_1
    }

    if (YMap < 0 || YMap >= 25) {
        return 0; // ovfw
    }

    if (ZMap < 0 || ZMap >= 64) {
        return 0; // ovfw
    }

    U32 offset = XMap * 50 + YMap * 2 + ZMap * 3200;
    U8 *ptc = BufCube + offset;

    return *ptc;
}

//▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
U8 GetWorldColBrickVisible(S32 x, S32 y, S32 z) {
    XMap = logical_shift_right_s32(x, 9);
    YMap = logical_shift_right_s32(y, 8);
    ZMap = logical_shift_right_s32(z, 9);

    if (XMap >= 0 && XMap < 64) {
        if (YMap <= -1) {
            YMap = 0;
            return 1;
        }
        if (YMap >= 0 && YMap < 25 && ZMap >= 0 && ZMap < 64) {
            U8 *ptc = 3200 * ZMap + 50 * XMap + BufCube + 2 * YMap;

            if (*ptc != 0) {
                U8 block = (*ptc++) - 1;
                U8 *adr = GetAdrBlock(block);
                adr += HEADER_BLOCK;
                adr += (*ptc) << 2; /* 4 Bytes to Jump	*/
                return *adr;
            }
        }
    }

    return 0;
}

//-------------------------------------------------------------------
void DecompColonne(U8 *pts, U8 *ptd) {
    U8 *dstData = ptd;
    U8 count = 0;
    U8 *srcData = pts;

    // Get Nb Entity
    U8 nbEntity = *srcData;
    srcData++;

    // For Every Entity
    do {
        // NewEntity:
        while (1) {
            U8 opCode = 0;
            while (1) {
                opCode = *srcData++;
                count = (opCode & 0x3F) + 1;
                // Incrust?
                if (opCode & 0xC0) {
                    break;
                }
                // Incrust ( Full off Nothing )
                opCode = 0;
                while (count) {
                    dstData[0] = 0;
                    dstData[1] = 0;
                    dstData += 2;
                    --count;
                }
                --nbEntity;
                if (!nbEntity) {
                    return;
                }
            }
            // Diff brick ?
            if (opCode & 0x40) {
                break;
            }
            U8 numBlock = srcData[0];
            U8 posBlock = srcData[1];
            srcData += 2;
            while (count) {
                dstData[0] = numBlock;
                dstData[1] = posBlock;
                dstData += 2;
                --count;
            }
            if (!--nbEntity) {
                return;
            }
        }
        while (count) {
            dstData[0] = srcData[0];
            dstData[1] = srcData[1];
            srcData += 2;
            dstData += 2;
            --count;
        }
        --nbEntity;
    } while (nbEntity);
}

//-------------------------------------------------------------------
// Map a brick's (x, y, z) world position onto the screen. The 24/12/15
// factors are the isometric step geometry: each grid step shifts the
// brick 24 px horizontally / 12 px vertically (the 2:1 iso tile shape),
// and each vertical world unit shifts -15 px screen-down. The constant
// offset places brick (0, 0, 0) on screen — historically at (288, 226)
// at the default 640×480, which is the framebuffer centre minus a
// (32, 14) shim from the iso grid's geometry (not a derivable formula;
// the shim is where the retail content expects the iso grid origin to
// land relative to the projection centre at SetIsoProjection's
// 320-9 = 311). Derive both axes from the framebuffer dimensions so the
// iso grid stays centred at any render-space size; at 640×480 this is
// the original (288, 226). See docs/WIDESCREEN.md Phase 2.
void Map2Screen(S32 x, S32 y, S32 z) {
    XScreen = 24 * (x - z) + ((S32)ModeDesiredX / 2 - 32);
    YScreen = 12 * (z + x) - 15 * y + ((S32)ModeDesiredY / 2 - 14);
    Projrec_Map2Screen(x, y, z, XScreen, YScreen);
}

/* The brick grid is derived from the framebuffer centre, while the polygon
   path projects through XCentre/YCentre.  SetIsoProjection moves the
   projection centre (the interior hover rig pans the view that way), so the
   background must take the same delta or models slide against a stationary
   room.  Keep Map2Screen itself untouched: its ASM twin and the projection
   corpus pin its output. */
static void Map2ScreenPanned(S32 x, S32 y, S32 z) {
    Map2Screen(x, y, z);
    XScreen += XCentre - ((S32)ModeDesiredX / 2 - 9);
    YScreen += YCentre - ((S32)ModeDesiredY / 2);
}

//-------------------------------------------------------------------
U8 *GetAdrBlock(S32 numblock) {
    return TabBlock + ((U32 *)TabBlock)[numblock];
}
