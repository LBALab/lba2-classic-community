
#pragma once
#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
enum PolyFillMethod {
  FILL_POLY_TEXTURES = 0,    ///< Normal
  FILL_POLY_NO_TEXTURES = 1, ///< Simplified (no mapping)
  FILL_POLY_FOG = 2,         ///< Fog
  FILL_POLY_NO_TEXTDITH = 3, ///< No Dithered-Gouraud
                             ///< texturing (replaced by see Gouraud texturing)
  FILL_POLY_ZBUFFER = 4,     ///< Z-Buffer
  FILL_POLY_FOG_ZBUFFER = 5, ///< Z-Buffer + Fog
  FILL_POLY_NZW = 6,         ///< Z-Buffer without zbuffer update
  FILL_POLY_FOG_NZW = 7      ///< Z-Buffer + Fog without zbuffer update
};

enum PolyType {
  POLY_SOLID = 0,
  POLY_FLAT = 1,
  POLY_TRANS = 2,
  POLY_TRAME = 3,
  POLY_GOURAUD = 4,
  POLY_DITHER = 5,
  POLY_GOURAUD_TABLE = 6,
  POLY_DITHER_TABLE = 7
};

#define POLY_TEXTURE 8
#define POLY_TEXTURE_FLAT 9
#define POLY_TEXTURE_GOURAUD 10
#define POLY_TEXTURE_DITHER 11
#define POLY_TEXTURE_INCRUST 12
#define POLY_TEXTURE_INCRUST_FLAT 13
#define POLY_TEXTURE_INCRUST_GOURAUD 14
#define POLY_TEXTURE_INCRUST_DITHER 15
#define POLY_TEXTURE_Z 16
#define POLY_TEXTURE_Z_FLAT 17
#define POLY_TEXTURE_Z_GOURAUD 18
#define POLY_TEXTURE_Z_DITHER 19
#define POLY_TEXTURE_Z_INCRUST 20
#define POLY_TEXTURE_Z_INCRUST_FLAT 21
#define POLY_TEXTURE_Z_INCRUST_GOURAUD 22
#define POLY_TEXTURE_Z_INCRUST_DITHER 23
#define POLY_TEXTURE_Z_FOG 24
#define POLY_FLAG_ZBUFFER 25

#define POLY_BIT_ENV_MAPPING (1 << 14)
#define POLY_ENV_MAPPING (POLY_BIT_ENV_MAPPING | POLY_TEXTURE)
#define POLY_ENV_MAPPING_FLAT (POLY_BIT_ENV_MAPPING | POLY_TEXTURE_FLAT)
#define POLY_ENV_MAPPING_GOURAUD (POLY_BIT_ENV_MAPPING | POLY_TEXTURE_GOURAUD)
#define POLY_ENV_MAPPING_DITHER (POLY_BIT_ENV_MAPPING | POLY_TEXTURE_DITHER)
#define POLY_ENV_MAPPING_INCRUST (POLY_BIT_ENV_MAPPING | POLY_TEXTURE_INCRUST)

#define POLY_ENV_MAPPING_INCRUST_FLAT                                          \
  (POLY_BIT_ENV_MAPPING | POLY_TEXTURE_INCRUST_FLAT)

#define POLY_ENV_MAPPING_INCRUST_GOURAUD                                       \
  (POLY_BIT_ENV_MAPPING | POLY_TEXTURE_INCRUST_GOURAUD)

#define POLY_ENV_MAPPING_INCRUST_DITHER                                        \
  (POLY_BIT_ENV_MAPPING | POLY_TEXTURE_INCRUST_DITHER)

// -----------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
  S16 Pt_XE;    ///< Screen coordinate X
  S16 Pt_YE;    ///< Screen coordinate Y
  U16 Pt_MapU;  ///< texture coordinate U
  U16 Pt_MapV;  ///< texture coordinate V
  U16 Pt_Light; ///< intensity (0.8.0)
  U16 Pt_ZO;    ///< normalized Z
  S32 Pt_W;     ///< 2^31 / Z (non normalized)
} Struc_Point;
#pragma pack(pop)

// -----------------------------------------------------------------------------
extern U32 ScreenPitch;         ///< Screen Pitch in bytes
extern PTR_U32 PTR_TabOffLine;   ///< Ptr on a LUT containing a list of line-offsets
extern PTR_U16 PtrZBuffer;      ///< ZBuffer pointer
extern PTR_U8 PtrCLUTGouraud;   ///< Pointer to a Gouraud CLUT
extern PTR_U8 PtrCLUTFog;       ///< Pointer to a Fog CLUT
extern PTR_U8 PtrTruePal;       ///< Pointer to the palette CLUT
extern S32 RepMask;             ///< Repeat zone for texture mapping
extern PTR_U8 PtrMap;           ///< Pointer of the texture map
extern S32 Fill_Z_Fog_Near;     ///< Near Fog
extern S32 Fill_Z_Fog_Far;      ///< Far Fog
extern U32 Fill_ZBuffer_Factor; ///< ZBuffer factor
extern U32 IsPolygonHidden;     ///< Flag for Poly type 25

typedef S32 (*Fill_Filler_Func)(U32 nbLines, U32 fillCurXMin, U32 fillCurXMax);

extern U32 **Fill_Restart;      ///< Return entry point from clipping procedure
extern Fill_Filler_Func Fill_Filler;       ///< Chooser filler
extern U32 Fill_Patch;          ///< Flag 1 means we should patch the filler proc; 0 means nothing to do

#pragma pack(push, 1)
typedef union {
  U8 *Ptr;
  U32 Num;
} T_FILL_COLOR;
#pragma pack(pop)
extern T_FILL_COLOR Fill_Color;          ///< The polygon color

extern Struc_Point *Fill_FirstPoint; // Pointer to the first point of the list
extern Struc_Point *Fill_LastPoint; // Pointer to the last point of the list
extern Struc_Point *Fill_LeftPoint; // Pointer of the current point of the left edge
extern Struc_Point *Fill_RightPoint; // Pointer of the current point of the right edge

extern S32 Fill_LeftSlope;      ///< Left slope
extern S32 Fill_RightSlope;     ///< Right slope

extern U32 Fill_CurY;           ///< Current Y
extern U32 Fill_CurXMin;        ///< Current XMin
extern U32 Fill_CurXMax;        ///< Current XMax
extern PTR_U8 Fill_CurOffLine;     ///< Offset of the current video line

extern S32 Fill_Cur_Z;
extern S32 Fill_Cur_W; // Current W
extern S32 Fill_Next_W; // Next W
extern S32 Fill_Cur_MapU; // all those variables for Texture Z
extern S32 Fill_Next_MapU;
extern S32 Fill_Cur_MapV;
extern S32 Fill_Next_MapV;
extern S32 Fill_Cur_MapUOverW;
extern S32 Fill_Next_MapUOverW;
extern S32 Fill_Cur_MapVOverW;
extern S32 Fill_Next_MapVOverW;

extern S32 Fill_CurGouraud;
extern S32 Fill_CurGouraudMin; // Left edge: gouraud value
extern S32 Fill_Gouraud_LeftSlope; // Left slope for Gouraud shading
extern S32 Fill_Gouraud_XSlope; // X Slope for Gouraud shading

extern S32 Fill_CurMapUMin; // Left edge: MapU value
extern S32 Fill_MapU_LeftSlope; // Left slope for MapU
extern S32 Fill_MapU_XSlope;	// X Slope for MapU

extern S32 Fill_CurMapVMin; // Left edge: MapV value
extern S32 Fill_MapV_LeftSlope; // Left slope for MapV
extern S32 Fill_MapV_XSlope;	// X Slope for MapV

extern S32 Fill_CurWMin; // Left edge: W value
extern S32 Fill_W_LeftSlope; // Left slope for W
extern S32 Fill_W_XSlope; // X Slope for W

extern S32 Fill_Z_LeftSlope;
extern S32 Fill_Z_XSlope;

extern U32 Fill_CurZBufMin; // Left edge: ZBuf value
extern S32 Fill_ZBuf_LeftSlope; // Left slope for ZBuf
extern S32 Fill_ZBuf_XSlope; // X Slope for ZBuf
extern U32 Fill_CurZBuf; // Current value of ZBuf in the scanline

extern U32 Fill_CurZMin;

extern U8 Fill_ReadFlag;        ///< Flag telling which point to read
extern U8 Fill_ClipFlag;        ///< Flag telling us what to clip
extern U8 Fill_Flag_Fog;        ///< True if FOG is active
extern U8 Fill_Flag_ZBuffer;    ///< True if ZBUFFER is active
extern U8 Fill_Flag_NZW;        ///< True if NZW is active

extern S32 Fill_Trame_Parity;  ///< Parity for trame polygons

extern U8 Fill_Logical_Palette[256];

// -----------------------------------------------------------------------------
void Switch_Fillers(U32 Bank);

S32 Fill_Poly(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
              Struc_Point *Ptr_Points);

extern void Fill_Sphere(S32 Type_Sphere, S32 Color_Sphere, S32 Centre_X,
                        S32 Centre_Y, S32 Rayon, S32 zBufferValue); ///< Do Not call from C

/// Patch all the fillers routine with the new scanlines offsets
extern void SetScreenPitch(U32 *New_TabOffLine); ///< Do Not call from C
// FIXME: There are callers using SetScreenPitch(), must be careful with patch

/// Set the line (in PtrCLUTGouraud) corresponding to the actual palette.
extern void SetCLUT(U32 defaultline);

extern void SetFog(S32 z_near, S32 z_far);

extern S32 Triangle_ReadNextEdge(Struc_Point *PtA, Struc_Point *PtB);
extern S32 Fill_PolyClip(S32 Nb_Points, Struc_Point *Ptr_Points);

S32 INV64(S32 a);

void Line_A(S32 x0, S32 y0, S32 x1, S32 y1, S32 col, S32 z1, S32 z2);

// -----------------------------------------------------------------------------
/// 16-bit Z standard for 16-bit zbuffer
#define GET_ZO(a) (U16)(((a)*Fill_ZBuffer_Factor) >> 16)

// =============================================================================
#ifdef __cplusplus
}
#endif
