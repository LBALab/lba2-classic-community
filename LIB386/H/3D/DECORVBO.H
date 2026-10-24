#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <POLYGON/POLY.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// One captured body vertex while building a static decor VBO.  Positions and
// normals are body-local; the decor rotation/translation is applied once by
// the builder.  Type is the Fill_Poly polygon type (0..15); Color is the raw
// body colour (not the flat/shaded colour the filler receives).
typedef struct {
    S16 X, Y, Z;
    S16 NX, NY, NZ;
    U16 U, V;
    U8 Type;
    U8 Color;
    U8 Flags;
    U8 Pad;
    const void *Map;
    U16 Mask;
} T_DECORVBO_VERT;

#define DECORVBO_FLAG_TEXTURED 1
#define DECORVBO_FLAG_GOURAUD 2
#define DECORVBO_FLAG_LIGHT 4
#define DECORVBO_FLAG_FLAT 8
#define DECORVBO_FLAG_FLAT_TEXTURE 16

//! Non-zero while BodyDisplay's polygon handlers are being replayed into the
//! decor VBO capture buffer instead of the frame's polygon batch.
extern S32 DecorVBOActive;
//! Set when the current body cannot be represented by the static path.
extern S32 DecorVBOUnsupported;

//! Replay one body in model space and return its captured vertices.  Returns
//! 0 when the body uses a path the static VBO does not cover (transparency,
//! alpha/stipple polygons, lines/spheres, environment mapping, ...).
S32 DecorVBORecord(void *body, T_DECORVBO_VERT **outVerts, U32 *outCount);

//! Hooks called by the polygon handlers while DecorVBOActive is set.
void DecorVBO_SetNormal(S32 slot, S32 index);
void DecorVBO_SetFlatColor(S32 color);
void DecorVBO_CapturePoly(S32 type, S32 color, S32 n, Struc_Point *pts);

// -----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
