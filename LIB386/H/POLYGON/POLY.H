
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
  U32 Pt_W;     ///< 2^31 / Z (non normalized)
} Struc_Point;
#pragma pack(pop)

// -----------------------------------------------------------------------------
extern PTR_U8 PtrCLUTGouraud;
extern PTR_U8 PtrCLUTFog;
extern PTR_U8 PtrTruePal;

extern PTR_U8 PtrMap;
extern PTR_U16 PtrZBuffer;
extern S32 RepMask;

extern S32 Fill_Z_Fog_Near;     ///< Fog Range
extern S32 Fill_Z_Fog_Far;      ///< Fog Range
extern U32 Fill_ZBuffer_Factor; ///< normalizing factor for Z Buffer
extern U32 IsPolygonHidden;     ///< normalizing factor for Z Buffer

extern U8 Fill_ReadFlag;        ///< Flag telling which point to read
extern U8 Fill_ClipFlag;        ///< Flag telling us what to clip
extern U8 Fill_Flag_Fog;        ///< True if FOG is active
extern U8 Fill_Flag_ZBuffer;    ///< True if ZBUFFER is active
extern U8 Fill_Flag_NZW;        ///< True if NZW is active

extern U8 Fill_Logical_Palette[256];

// -----------------------------------------------------------------------------
typedef S32(FUNC_FILL_POLY)(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
                            Struc_Point *Ptr_Points);

// -----------------------------------------------------------------------------
extern void Switch_Fillers_ASM(U32 Bank); ///< Do Not call from C
void Switch_Fillers_C(U32 Bank);
#define Switch_Fillers Switch_Fillers_C

extern FUNC_FILL_POLY Fill_PolyFast; ///< Do Not call from C
S32 Fill_Poly_C(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
                Struc_Point *Ptr_Points);
#define Fill_Poly Fill_Poly_C

extern void Fill_Sphere(S32 Type_Sphere, S32 Color_Sphere, S32 Centre_X,
                        S32 Centre_Y, S32 Rayon); ///< Do Not call from C

/// Patch all the fillers routine with the new scanlines offsets
extern void SetScreenPitch(U32 *New_TabOffLine); ///< Do Not call from C
// FIXME: There are callers using SetScreenPitch(), must be careful with patch

/// Set the line (in PtrCLUTGouraud) corresponding to the actual palette.
extern void SetCLUT(U32 defaultline);

extern void SetFog(S32 z_near, S32 z_far);

S32 INV64(S32 a);

// -----------------------------------------------------------------------------
/// 16-bit Z standard for 16-bit zbuffer
#define GET_ZO(a) (U16)(((a)*Fill_ZBuffer_Factor) >> 16)

// =============================================================================
#ifdef __cplusplus
}
#endif
