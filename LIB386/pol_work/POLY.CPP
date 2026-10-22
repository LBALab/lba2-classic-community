#include <POLYGON/POLY.H>

#include <POLYGON/POLY_JMP.H>

#include <SYSTEM/UTILS.H>
#include <SYSTEM/DEBUG_TRACES.H>
#include <3D/X87REAL.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <FILLER.H>

#ifdef ENABLE_POLY_RECORDING
#include <SNAPSHOT/POLY_RECORDING.H>
#endif

#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <fenv.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

typedef S32 (*Jmp_XSlopeFunc)(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC);
typedef S32 (*Jmp_LeftSlopeFunc)(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB);

extern Jmp_XSlopeFunc Jmp_XSlopeFPU[];
extern Jmp_XSlopeFunc Jmp_XSlopeZBufFPU[];
extern Jmp_XSlopeFunc *Current_Jmp_XSlope;

extern Jmp_LeftSlopeFunc Jmp_LeftSlopeFPU[];
extern Jmp_LeftSlopeFunc Jmp_LeftSlopeZBufFPU[];
extern Jmp_LeftSlopeFunc *Current_Jmp_LeftSlope;

typedef void (*Filler_SwitchFunc)(void);

extern Filler_SwitchFunc Table_Switch[];

// ASM float constants — MUST match POLY.ASM REAL4 values exactly.
// FInv_65536 is defined as REAL4 0.0000152588 in ASM (NOT exact 1/65536).
// The x87 FPU loads this 32-bit float and promotes to 80-bit extended.
static const float ASM_FInv_65536 = 0.0000152588f; // REAL4 0.0000152588
static const float ASM_FInv_256 = 0.00390625f;     // REAL4 0.00390625 (exact)
static const float ASM_F_256 = 256.0f;             // REAL4 256.0
static const float ASM_F_1 = 1.0f;                 // REAL4 1.0
static const float ASM_F_65536 = 65536.0f;         // REAL4 65536.0

U32 Fill_Type = 0; // Polygon type number

U32 **Fill_Restart = NULL;
Fill_Filler_Func Fill_Filler = NULL;
U32 Fill_Patch = 0;
T_FILL_COLOR Fill_Color = {0}; /* Num = 0 / Ptr = NULL: same union bits */

Struc_Point *Fill_FirstPoint = NULL;
Struc_Point *Fill_LastPoint = NULL;
Struc_Point *Fill_LeftPoint = NULL;
Struc_Point *Fill_RightPoint = NULL;

S32 Fill_LeftSlope = 0;  ///< Left slope
S32 Fill_RightSlope = 0; ///< Right slope

U32 Fill_CurY = 0;             ///< Current Y
U32 Fill_CurXMin = 0;          ///< Current XMin
U32 Fill_CurXMax = 0;          ///< Current XMax
PTR_U8 Fill_CurOffLine = NULL; ///< Offset of the current video line

S32 Fill_Cur_Z = 0;
S32 Fill_Cur_W = 0;    // Current W
S32 Fill_Next_W = 0;   // Next W
S32 Fill_Cur_MapU = 0; // all those variables for Texture Z
S32 Fill_Next_MapU = 0;
S32 Fill_Cur_MapV = 0;
S32 Fill_Next_MapV = 0;
S32 Fill_Cur_MapUOverW = 0;
S32 Fill_Next_MapUOverW = 0;
S32 Fill_Cur_MapVOverW = 0;
S32 Fill_Next_MapVOverW = 0;

S32 Fill_CurGouraud = 0;
S32 Fill_CurGouraudMin = 0;     // Left edge: gouraud value
S32 Fill_Gouraud_LeftSlope = 0; // Left slope for Gouraud shading
S32 Fill_Gouraud_XSlope = 0;    // X Slope for Gouraud shading

S32 Fill_CurMapUMin = 0;     // Left edge: MapU value
S32 Fill_MapU_LeftSlope = 0; // Left slope for MapU
S32 Fill_MapU_XSlope = 0;    // X Slope for MapU

S32 Fill_CurMapVMin = 0;     // Left edge: MapV value
S32 Fill_MapV_LeftSlope = 0; // Left slope for MapV
S32 Fill_MapV_XSlope = 0;    // X Slope for MapV

S32 Fill_CurWMin = 0;     // Left edge: W value
S32 Fill_W_LeftSlope = 0; // Left slope for W
S32 Fill_W_XSlope = 0;    // X Slope for W

S32 Fill_Z_LeftSlope = 0;
S32 Fill_Z_XSlope = 0;

U32 Fill_CurZBufMin = 0;     // Left edge: ZBuf value
S32 Fill_ZBuf_LeftSlope = 0; // Left slope for ZBuf
S32 Fill_ZBuf_XSlope = 0;    // X Slope for ZBuf
U32 Fill_CurZBuf = 0;        // Current value of ZBuf in the scanline

U32 Fill_CurZMin = 0;

U8 Fill_ReadFlag = FALSE;     ///< Flag telling which point to read
U8 Fill_ClipFlag = FALSE;     ///< Flag telling us what to clip
U8 Fill_Flag_Fog = FALSE;     // True if FOG active
U8 Fill_Flag_ZBuffer = FALSE; // True if ZBUFFER is active
U8 Fill_Flag_NZW = FALSE;     // True if NZW active

S32 Fill_Trame_Parity = 0; ///< Parity for trame polygons

Fill_Jump_Fn *Fill_Saut_Normal = Fill_N_Table_Jumps; // Filler table

// -----------------------------------------------------------------------------
// 1.31 fixed-point inverse: 2^31 / a. The original was an inline pragma in
// POLY.H (mov eax, 80000000h / xor edx, edx / idiv ebx), so EDX:EAX holds 2^31.
// NB_BITS_INV64 is 31. Callers pass a clipped depth, never 0, 1 or -1.
S32 INV64(S32 a) { return (S32)((1LL << 31) / a); }

S32 Read_Next_Right(Struc_Point *PtA, Struc_Point *PtB);

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Fill_Poly : Polygon filler                                               ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ Call  : EAX = Polygon type (ie flat, gouraud, textured...)               ║
// ║         EBX = Polygon color (if needed by the type)                      ║
// ║         ECX = Nb of points in that polygon                               ║
// ║         ESI = Ptr on the point list of this polygon                      ║
// ║               (list of STRUC_POLY_POINT)                                 ║
// ║                                                                          ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ Return: EAX = 0 if error (polygon not filled)                            ║
// ║                                                                          ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ ALL REGISTERS ARE MODIFIED (EXCEPT EBP)                                  ║
// ║                                                                          ║
// ║ The value given to EAX is not check for validity, so be sure to fill it  ║
// ║ with a correct poly type number                                          ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Fill_Poly(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
              Struc_Point *Ptr_Points) {
#ifdef ENABLE_POLY_RECORDING
    if (g_polyrec_recording)
        polyrec_record_fill_poly(Type_Poly, Color_Poly, Nb_Points, Ptr_Points);
#endif
    Fill_LeftSlope = 0;
    SetScreenPitch(TabOffLine);

    Fill_Type = Type_Poly;
    Color_Poly = Color_Poly & 0xFF;

    Fill_Jump_Fn filler = Fill_Saut_Normal[Type_Poly];
    return filler(Nb_Points, Ptr_Points, Color_Poly);
}

void Switch_Normal(void) {
    Fill_Saut_Normal = Fill_N_Table_Jumps;
    Fill_Flag_Fog = FALSE;
    Fill_Flag_ZBuffer = FALSE;
    Fill_Flag_NZW = FALSE;

    Current_Jmp_XSlope = Jmp_XSlopeFPU;
    Current_Jmp_LeftSlope = Jmp_LeftSlopeFPU;
}

void Switch_Fog(void) {
    Fill_Saut_Normal = Fill_Fog_Table_Jumps;
    Fill_Flag_ZBuffer = FALSE;
    Fill_Flag_NZW = FALSE;
    Fill_Flag_Fog = TRUE;

    Current_Jmp_XSlope = Jmp_XSlopeFPU;
    Current_Jmp_LeftSlope = Jmp_LeftSlopeFPU;
}

void Switch_ZBuffer(void) {
    Fill_Saut_Normal = Fill_ZBuf_Table_Jumps;
    Fill_Flag_Fog = FALSE;
    Fill_Flag_NZW = FALSE;
    Fill_Flag_ZBuffer = TRUE;

    Current_Jmp_XSlope = Jmp_XSlopeZBufFPU;
    Current_Jmp_LeftSlope = Jmp_LeftSlopeZBufFPU;
}

void Switch_FogZBuffer(void) {
    Fill_Saut_Normal = Fill_FogZBuf_Table_Jumps;
    Fill_Flag_Fog = TRUE;
    Fill_Flag_NZW = FALSE;
    Fill_Flag_ZBuffer = TRUE;

    Current_Jmp_XSlope = Jmp_XSlopeZBufFPU;
    Current_Jmp_LeftSlope = Jmp_LeftSlopeZBufFPU;
}

void Switch_NZW(void) {
    Fill_Saut_Normal = Fill_NZW_Table_Jumps;
    Fill_Flag_Fog = FALSE;
    Fill_Flag_ZBuffer = TRUE;
    Fill_Flag_NZW = TRUE;

    Current_Jmp_XSlope = Jmp_XSlopeZBufFPU;
    Current_Jmp_LeftSlope = Jmp_LeftSlopeZBufFPU;
}

void Switch_FogNZW(void) {
    Fill_Saut_Normal = Fill_FogNZW_Table_Jumps;
    Fill_Flag_Fog = TRUE;
    Fill_Flag_ZBuffer = TRUE;
    Fill_Flag_NZW = TRUE;

    Current_Jmp_XSlope = Jmp_XSlopeZBufFPU;
    Current_Jmp_LeftSlope = Jmp_LeftSlopeZBufFPU;
}

void Switch_Fillers(U32 Bank) {
#ifdef ENABLE_POLY_RECORDING
    if (g_polyrec_recording)
        polyrec_record_switch_fillers(Bank);
#endif
    Table_Switch[Bank]();
}

Filler_SwitchFunc Table_Switch[] = {
    Switch_Normal,
    Switch_Normal,
    Switch_Fog,
    Switch_Normal,
    Switch_ZBuffer,
    Switch_FogZBuffer,
    Switch_NZW,
    Switch_FogNZW};

PTR_U8 PtrCLUTGouraud = NULL;
PTR_U8 PtrCLUTFog = NULL;
PTR_U8 PtrTruePal = NULL;

PTR_U8 PtrMap = NULL;
PTR_U16 PtrZBuffer = NULL;
S32 RepMask = 0;

S32 Fill_Z_Fog_Near = 0;       ///< Fog Range
S32 Fill_Z_Fog_Far = 0;        ///< Fog Range
U32 Fill_ZBuffer_Factor = 0;   ///< normalizing factor for Z Buffer
U32 Fill_Fog_Factor = 0;       ///< Fog Factor
U32 Fill_ScaledFogNear = 0;    ///< Scaled Near Fog value
U32 IsPolygonHidden = 0;       ///< normalizing factor for Z Buffer
U8 Fill_Logical_Palette[256];  ///< Logical Palette
U32 ScreenPitch = 0;           ///< Screen Pitch
PTR_U32 PTR_TabOffLine = NULL; ///< Screen Pitch

Struc_Point List_Clip_XMin[NB_MAX_EDGES];
Struc_Point List_Clip_XMax[NB_MAX_EDGES];
Struc_Point List_Clip_YMin[NB_MAX_EDGES];
Struc_Point List_Clip_YMax[NB_MAX_EDGES];

double Denom = 0;
double YB_YA = 0;
double YC_YA = 0;
volatile X87_REAL InvDenom = 0; // ASM's D = 256.0 / raw_Denom, kept at the x87's width (3D/X87REAL.H)

S32 Clip_Num = 0;

void SetScreenPitch(U32 *New_TabOffLine) {
    ScreenPitch = New_TabOffLine[1];
    PTR_TabOffLine = New_TabOffLine;
}

void SetCLUT(U32 defaultline) {
#ifdef ENABLE_POLY_RECORDING
    if (g_polyrec_recording)
        polyrec_record_set_clut(defaultline);
#endif
    U8 *nextPtrTruePal = PtrCLUTFog + (defaultline << 8);

    // compare and jump to end if equal
    if (PtrTruePal == nextPtrTruePal) {
        return;
    }

    // update PtrTruePal and calculate new value for PtrCLUTGouraud
    PtrTruePal = nextPtrTruePal;
    PtrCLUTGouraud = PtrCLUTFog + ((defaultline << 8) & 0xF000);
    U8 *dst = (U8 *)Fill_Logical_Palette;
    U8 *src = (U8 *)PtrTruePal;

    for (int i = 0; i < 256 / 8; i++) {
        memcpy(dst, src, 8);
        dst += 8;
        src += 8;
    }
}

void SetFog(S32 z_near, S32 z_far) {
#ifdef ENABLE_POLY_RECORDING
    if (g_polyrec_recording)
        polyrec_record_set_fog(z_near, z_far);
#endif
    if (z_far == 0)
        z_far = 1;

    Fill_Z_Fog_Near = z_near;
    Fill_Z_Fog_Far = z_far;

    Fill_ZBuffer_Factor = (U32)((1ULL << 32) / z_far);
    Fill_ScaledFogNear = (Fill_Z_Fog_Near * Fill_ZBuffer_Factor) >> 16;

    U32 temp = 65535 - Fill_ScaledFogNear;
    if (temp == 0)
        temp = 1;

    Fill_Fog_Factor = 256 * 65536 / temp;
}

void EnterClip(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA, S32 diff) {
    if (Fill_ClipFlag & CLIP_LIGHT) {
        dst->Pt_Light = pointA->Pt_Light + (S32)((S64)(pointB->Pt_Light - pointA->Pt_Light) * Clip_Num / diff);
    }

    if (Fill_ClipFlag & CLIP_TEXTURE) {
        dst->Pt_MapU = pointA->Pt_MapU + (S32)((S64)(pointB->Pt_MapU - pointA->Pt_MapU) * Clip_Num / diff);
        dst->Pt_MapV = pointA->Pt_MapV + (S32)((S64)(pointB->Pt_MapV - pointA->Pt_MapV) * Clip_Num / diff);
    }

    if (Fill_ClipFlag & CLIP_TEXTUREZ) {
        // W
        dst->Pt_W = pointA->Pt_W + (S32)((S64)(pointB->Pt_W - pointA->Pt_W) * Clip_Num / diff);

        // MapU — ASM uses shr/shl/or bit extraction for the /256, which
        // is arithmetic right shift (floor division).  C's / 256 truncates
        // towards zero.  For negative products the results differ by 1.
        // Use >> 8 to match the ASM behaviour.
        S32 uwA = (S32)((S64)pointA->Pt_MapU * pointA->Pt_W >> 8);
        S32 uwB = (S32)((S64)pointB->Pt_MapU * pointB->Pt_W >> 8);
        S32 U_tmp = (S32)((S64)(uwB - uwA) * Clip_Num / diff) + uwA;
        dst->Pt_MapU = (S32)((S64)U_tmp * 256 / dst->Pt_W);

        // MapV
        S32 vwA = (S32)((S64)pointA->Pt_MapV * pointA->Pt_W >> 8);
        S32 vwB = (S32)((S64)pointB->Pt_MapV * pointB->Pt_W >> 8);
        S32 V_tmp = (S32)((S64)(vwB - vwA) * Clip_Num / diff) + vwA;
        dst->Pt_MapV = (S32)((S64)V_tmp * 256 / dst->Pt_W);

        if (Fill_ClipFlag & CLIP_ZBUFFER) {
            S32 zA = (S32)((S64)pointA->Pt_ZO * pointA->Pt_W >> 8);
            S32 zB = (S32)((S64)pointB->Pt_ZO * pointB->Pt_W >> 8);
            S32 ZO_tmp = (S32)((S64)(zB - zA) * Clip_Num / diff) + zA;
            dst->Pt_ZO = (S32)((S64)ZO_tmp * 256 / dst->Pt_W);
        }
    } else {
        if (Fill_ClipFlag & CLIP_ZBUFFER) {
            dst->Pt_ZO = pointA->Pt_ZO + (S32)((S64)(pointB->Pt_ZO - pointA->Pt_ZO) * Clip_Num / diff);
        }
    }
}

// ESI = PtA; EBX = Pt_B with PtA: point to clip; and PtB: valid point
// y=yA+[(XMin-xa)*(yB-yA)]/(xB-xA)
void Generic_ClipXMin(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA) {
    S32 xDiff = pointB->Pt_XE - pointA->Pt_XE;

    Clip_Num = ClipXMin - pointA->Pt_XE;
    dst->Pt_XE = ClipXMin;
    // ASM interpolates Y in <<16 fixed-point to preserve fractional precision
    {
        S32 yA_shifted = (S32)pointA->Pt_YE << 16;
        S32 yDiff_shifted = ((S32)pointB->Pt_YE << 16) - yA_shifted;
        S32 yInterp = (S32)((S64)yDiff_shifted * Clip_Num / xDiff);
        dst->Pt_YE = (S16)((yA_shifted + yInterp) >> 16);
    }

    EnterClip(pointB, dst, pointA, xDiff);
}

// ESI = PtA; EBX = Pt_B
// y=yA+[(XMax-xa)*(yB-yA)]/(xB-xA)
void Generic_ClipXMax(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA) {
    S32 xDiff = pointB->Pt_XE - pointA->Pt_XE;

    // ClipXMax + 1 bcoz we don't draw the last pixel on a scanline
    Clip_Num = ClipXMax + 1 - pointA->Pt_XE;

    dst->Pt_XE = ClipXMax + 1;
    // ASM interpolates Y in <<16 fixed-point: (YA<<16) + ((YB-YA)<<16 * Clip_Num / xDiff)
    // then >>16 to get integer Y. Simple integer division truncates differently.
    {
        S32 yA_shifted = (S32)pointA->Pt_YE << 16;
        S32 yDiff_shifted = ((S32)pointB->Pt_YE << 16) - yA_shifted;
        S32 yInterp = (S32)((S64)yDiff_shifted * Clip_Num / xDiff);
        dst->Pt_YE = (S16)((yA_shifted + yInterp) >> 16);
    }

    EnterClip(pointB, dst, pointA, xDiff);
}

// ESI = PtA; EBX = Pt_B
// x=xa+[(YMin-ya)*(xB-xA)]/(yB-ya)
void Generic_ClipYMin(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA) {
    S32 yDiff = pointB->Pt_YE - pointA->Pt_YE;

    Clip_Num = ClipYMin - pointA->Pt_YE;
    // ASM interpolates X in <<16 fixed-point: ((XB-XA)<<16) * Clip_Num / diff, then >>16
    {
        S32 xDiff_shifted = ((S32)pointB->Pt_XE - (S32)pointA->Pt_XE) << 16;
        S32 xInterp = (S32)((S64)xDiff_shifted * Clip_Num / yDiff);
        dst->Pt_XE = (S16)((S32)pointA->Pt_XE + (xInterp >> 16));
    }
    dst->Pt_YE = ClipYMin;

    EnterClip(pointB, dst, pointA, yDiff);
}

// ESI = PtA; EBX = Pt_B
// x=xa+[(YMax-ya)*(xB-xA)]/(yB-ya)
void Generic_ClipYMax(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA) {
    S32 yDiff = pointB->Pt_YE - pointA->Pt_YE;

    Clip_Num = ClipYMax - pointA->Pt_YE;
    // ASM interpolates X in <<16 fixed-point: ((XB-XA)<<16) * Clip_Num / diff, then >>16
    {
        S32 xDiff_shifted = ((S32)pointB->Pt_XE - (S32)pointA->Pt_XE) << 16;
        S32 xInterp = (S32)((S64)xDiff_shifted * Clip_Num / yDiff);
        dst->Pt_XE = (S16)((S32)pointA->Pt_XE + (xInterp >> 16));
    }
    dst->Pt_YE = ClipYMax;

    EnterClip(pointB, dst, pointA, yDiff);
}

// Clip types
#define CLIP_XMIN 0
#define CLIP_XMAX 1
#define CLIP_YMIN 2
#define CLIP_YMAX 3

void Clipper_Jmp(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA, U8 type) {
    switch (type) {
    case CLIP_XMIN:
        Generic_ClipXMin(pointB, dst, pointA);
        break;
    case CLIP_XMAX:
        Generic_ClipXMax(pointB, dst, pointA);
        break;
    case CLIP_YMIN:
        Generic_ClipYMin(pointB, dst, pointA);
        break;
    case CLIP_YMAX:
        Generic_ClipYMax(pointB, dst, pointA);
        break;
    }
}

/*
 ╔═══════════════════════════════════════════════════════════════════════════╗
 ║ Clipping : Generic macro                                                  ║
 ╟───────────────────────────────────────────────────────────────────────────╢
 ║ Call : [Fill_FirstPoint] = Ptr on the beginning of the list               ║
 ║        [Fill_LastPoint]  = Ptr on the end of the list                     ║
 ╟───────────────────────────────────────────────────────────────────────────╢
 ║ Return: ESI = Ptr on the new points-list                                  ║
 ║         ECX = Nb of points in that new list                               ║
 ║                                                                           ║
 ╚═══════════════════════════════════════════════════════════════════════════╝
*/
S32 Fill_Clip(S32 Nb_Points, Struc_Point *Ptr_Points, U8 type) {
    Struc_Point *prevPoint = Fill_LastPoint;     // Previous point
    Struc_Point *currentPoint = Fill_FirstPoint; // Current point

    Struc_Point *clipPoints = NULL;

    S32 newNbPoints = 0;
    currentPoint--;

    if (type == CLIP_XMIN) {
        clipPoints = List_Clip_XMin;
    } else if (type == CLIP_XMAX) {
        clipPoints = List_Clip_XMax;
    } else if (type == CLIP_YMIN) {
        clipPoints = List_Clip_YMin;
    } else if (type == CLIP_YMAX) {
        clipPoints = List_Clip_YMax;
    }

    U8 isPointInvisible = 1;

    if ((type == CLIP_XMIN && Fill_LastPoint->Pt_XE >= ClipXMin) || (type == CLIP_XMAX && Fill_LastPoint->Pt_XE <= ClipXMax + 1) || (type == CLIP_YMIN && Fill_LastPoint->Pt_YE >= ClipYMin) || (type == CLIP_YMAX && Fill_LastPoint->Pt_YE <= ClipYMax)) {
        isPointInvisible = 0;
    }

    while (currentPoint != Fill_LastPoint) {
        currentPoint++;
        if ((type == CLIP_XMIN && currentPoint->Pt_XE >= ClipXMin) || (type == CLIP_XMAX && currentPoint->Pt_XE <= ClipXMax + 1) || (type == CLIP_YMIN && currentPoint->Pt_YE >= ClipYMin) || (type == CLIP_YMAX && currentPoint->Pt_YE <= ClipYMax)) {
            if (isPointInvisible) {
                Clipper_Jmp(prevPoint, clipPoints, currentPoint, type);
                clipPoints++;
                newNbPoints++;
            }

            memcpy(clipPoints, currentPoint, sizeof(Struc_Point));

            ++newNbPoints;
            clipPoints++;
            isPointInvisible = 0;
        } else if (!isPointInvisible) {
            // Current point is off-screen
            Clipper_Jmp(prevPoint, clipPoints, currentPoint, type);
            clipPoints++; // A point was created: registers it!
            newNbPoints++;
            isPointInvisible = 1; // Last point not visible
        }

        prevPoint = currentPoint;
    }

    Struc_Point *nextPtrPoints = NULL;
    if (type == CLIP_XMIN) {
        nextPtrPoints = List_Clip_XMin;
    } else if (type == CLIP_XMAX) {
        nextPtrPoints = List_Clip_XMax;
    } else if (type == CLIP_YMIN) {
        nextPtrPoints = List_Clip_YMin;
    } else if (type == CLIP_YMAX) {
        nextPtrPoints = List_Clip_YMax;
    }

    return Fill_PolyClip(newNbPoints, nextPtrPoints);
}

S32 Fill_ClipXMin(S32 Nb_Points, Struc_Point *Ptr_Points) {
    return Fill_Clip(Nb_Points, Ptr_Points, CLIP_XMIN);
}

S32 Fill_ClipXMax(S32 Nb_Points, Struc_Point *Ptr_Points) {
    return Fill_Clip(Nb_Points, Ptr_Points, CLIP_XMAX);
}

S32 Fill_ClipYMin(S32 Nb_Points, Struc_Point *Ptr_Points) {
    return Fill_Clip(Nb_Points, Ptr_Points, CLIP_YMIN);
}

S32 Fill_ClipYMax(S32 Nb_Points, Struc_Point *Ptr_Points) {
    return Fill_Clip(Nb_Points, Ptr_Points, CLIP_YMAX);
}

// Truncation for the span of one triangle, as the ASM's fldcw [Status_Int] ..
// fldcw [Status_Float] pair. Restores round-to-nearest however the triangle
// returns.
struct TriangleTruncateScope {
    TriangleTruncateScope() {
        RoundType = ROUND_TYPE_INT;
        fesetround(FE_TOWARDZERO);
    }
    ~TriangleTruncateScope() {
        RoundType = ROUND_TYPE_FLOAT;
        fesetround(FE_TONEAREST);
    }
};

// ╔════════════════════════════════════════════════════════════════════════════════╗
// ║  Draw_Triangle : Remplissage d'un triangle                                     ║
// ╟────────────────────────────────────────────────────────────────────────────────╢
// ║  Appel : ESI = PTR sur une table du type Struc_Point (table des sommets)       ║
// ╟────────────────────────────────────────────────────────────────────────────────╢
// ║  Retour: Rien                                                                  ║
// ╟────────────────────────────────────────────────────────────────────────────────╢
// ║  Remarque: les sommets doivent etre ordonnes dans le sens trigo. (anti-        ║
// ║            horlogique).                                                        ║
// ║                                                                                ║
// ╟────────────────────────────────────────────────────────────────────────────────╢
// ║  Principe: (Exemple sur le Gouraud)                                            ║
// ║            (En fait le commentaire etait sur une routine dediee Gouraud et     ║
// ║            j'ai la flemme de le reecrire ;)                                    ║
// ║                                                                                ║
// ║                                                                                ║
// ║   Dans un triangle, l'increment du gouraud est constant.. Sisi, voici la       ║
// ║  preuve:                                                                       ║
// ║                                                                                ║
// ║                                                                                ║
// ║  Soit ABC notre triangle de coordonnees A(xa,ya,ga); B(xb,yb,gb); C(xc,yc,gc)  ║
// ║                                                                                ║
// ║                                                                                ║
// ║                   [(ga+y*Pente_Gouraud_Droite) - (ga+y*Pente_Gouraud_Gauche)]  ║
// ║  Inc_Gouraud(y) = ───────────────────────────────────────────────────────────  ║
// ║                           [(xa+y*Pente_Droite) - (xa+y*Pente_Gauche)]          ║
// ║                                                                                ║
// ║                                                                                ║
// ║                   Pente_Gouraud_Droite-Pente_Gouraud_Gauche                    ║
// ║                 = ─────────────────────────────────────────                    ║
// ║                           Pente_Droite-Pente_Gauche                            ║
// ║                                                                                ║
// ║                                                                                ║
// ║                 = Cste; d'ou gain.... :)                                       ║
// ║                                                                                ║
// ║                                                                                ║
// ║                                                                                ║
// ╚════════════════════════════════════════════════════════════════════════════════╝
S32 Draw_Triangle(Struc_Point *Ptr_Points) {
    LIB386_TRACE_CPP("Draw_Triangle", "1", "pts=%p p0=(%d,%d) p1=(%d,%d) p2=(%d,%d) type=%u",
                     (void *)Ptr_Points, Ptr_Points[0].Pt_XE, Ptr_Points[0].Pt_YE,
                     Ptr_Points[1].Pt_XE, Ptr_Points[1].Pt_YE,
                     Ptr_Points[2].Pt_XE, Ptr_Points[2].Pt_YE, Fill_Type);
    LIB386_TRACE_CPP("Draw_Triangle", "1b", "p0: u=%u v=%u w=%d light=%u zo=%u",
                     Ptr_Points[0].Pt_MapU, Ptr_Points[0].Pt_MapV, Ptr_Points[0].Pt_W,
                     Ptr_Points[0].Pt_Light, Ptr_Points[0].Pt_ZO);
    LIB386_TRACE_CPP("Draw_Triangle", "1c", "p1: u=%u v=%u w=%d light=%u zo=%u",
                     Ptr_Points[1].Pt_MapU, Ptr_Points[1].Pt_MapV, Ptr_Points[1].Pt_W,
                     Ptr_Points[1].Pt_Light, Ptr_Points[1].Pt_ZO);
    LIB386_TRACE_CPP("Draw_Triangle", "1d", "p2: u=%u v=%u w=%d light=%u zo=%u",
                     Ptr_Points[2].Pt_MapU, Ptr_Points[2].Pt_MapV, Ptr_Points[2].Pt_W,
                     Ptr_Points[2].Pt_Light, Ptr_Points[2].Pt_ZO);
    // *** Search the highest point in the list ***
    Fill_FirstPoint = Ptr_Points;
    Fill_Patch = 1;

    S16 currentY = Ptr_Points[0].Pt_YE;
    Struc_Point *highestPoint = Ptr_Points;
    S16 highestY = Ptr_Points[0].Pt_YE; // lowest value
    S16 nextY = Ptr_Points[1].Pt_YE;
    Struc_Point *nextPoint = Ptr_Points + 1;

    if (nextY <= highestY) {
        highestY = nextY;
        highestPoint = nextPoint;
    }

    S16 nextNextY = nextPoint[1].Pt_YE;
    Struc_Point *nextNextPoint = nextPoint + 1;
    if (nextNextY <= highestY) {
        highestY = nextNextY;
        highestPoint = nextNextPoint;
    }

    Fill_LeftPoint = highestPoint;
    Fill_RightPoint = highestPoint;

    LIB386_TRACE_CPP("Draw_Triangle", "2", "highestY=%d highestPt=%p (idx=%d)",
                     highestY, (void *)highestPoint, (int)(highestPoint - Ptr_Points));

    // Test if the poly is flat
    if (currentY == nextY && nextY == nextNextY) {
        LIB386_TRACE_CPP("Draw_Triangle", "9", "flat poly, early return");
        RoundType = ROUND_TYPE_FLOAT;
        return Fill_LeftSlope;
    }

    Fill_LastPoint = nextNextPoint;
    Fill_CurY = highestY;

    PTR_U8 screen = (PTR_U8)Log + PTR_TabOffLine[highestY];
    Fill_ReadFlag = READ_NEXT_L + READ_NEXT_R;
    Fill_CurOffLine = screen; // Current offset in log screen

    // *** Calculate the common denominator
    // for interpolation calculation ***
    Struc_Point *leftPoint = Fill_LeftPoint + 1;
    if (leftPoint > Fill_LastPoint) {
        leftPoint = Fill_FirstPoint;
    }

    Struc_Point *rightPoint = leftPoint + 1;
    if (rightPoint > Fill_LastPoint) {
        rightPoint = Fill_FirstPoint;
    }

    // esi = Fill_LeftPoint = PtA
    // edi = leftPoint = PtB
    // ebp = rightPoint = PtC
    // ASM sets fldcw [Status_Int] here — truncation rounding mode — and
    // fldcw [Status_Float] (round-to-nearest) at @@End. The whole triangle
    // (slopes, edges, filler) is drawn inside this scope; every lrint after
    // it, in the engine too, expects round-to-nearest, so the guard restores
    // it on every exit.
    TriangleTruncateScope truncate;

    Struc_Point *PtA = Fill_LeftPoint;
    Struc_Point *PtB = leftPoint;
    Struc_Point *PtC = rightPoint;

    YB_YA = PtB->Pt_YE - PtA->Pt_YE;
    YC_YA = PtC->Pt_YE - PtA->Pt_YE;

    volatile X87_REAL raw_Denom = (X87_REAL)YB_YA * (X87_REAL)(PtC->Pt_XE - PtA->Pt_XE) - (X87_REAL)YC_YA * (X87_REAL)(PtB->Pt_XE - PtA->Pt_XE);

    // FPU version divides Denom by 256.0, while integer versions
    // multiply by 256 the rest of values that will be divided by Denom.
    Denom = (double)raw_Denom / 256.0;

    LIB386_TRACE_CPP("Draw_Triangle", "3", "PtA=(%d,%d) PtB=(%d,%d) PtC=(%d,%d) YB_YA=%g YC_YA=%g raw_Denom=%Lg",
                     PtA->Pt_XE, PtA->Pt_YE, PtB->Pt_XE, PtB->Pt_YE, PtC->Pt_XE, PtC->Pt_YE,
                     YB_YA, YC_YA, (long double)raw_Denom);

    if (raw_Denom == 0) {
        LIB386_TRACE_CPP("Draw_Triangle", "9", "denom==0, early return");
        return Fill_LeftSlope;
    }

    // ASM: D = F_256 / raw_Denom (fdivp st(3),st where st(3)=256.0, st(0)=raw_Denom)
    // F_256 is loaded as REAL4 (32-bit float) then promoted to 80-bit
    InvDenom = (X87_REAL)ASM_F_256 / raw_Denom;

    LIB386_TRACE_CPP("Draw_Triangle", "4", "InvDenom=%Lg Denom=%g Fill_Type=%u",
                     (long double)InvDenom, Denom, Fill_Type);

    // ASM hardcodes Jmp_XSlopeZBufFPU regardless of rendering mode
    S32 result = Jmp_XSlopeZBufFPU[Fill_Type](Fill_Type, PtA, PtB, PtC);
    LIB386_TRACE_CPP("Draw_Triangle", "9", "return=%d LeftSlope=%d", result, Fill_LeftSlope);
    return result;
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Fill_PolyClip: Display a n-angle (provided that the poly is convex)      ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ Appel : ECX = Nb points                                                  ║
// ║         ESI = PTR on the list of points                                  ║
// ║                                                                          ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ Retour: EAX = 0 if error (non-visible poly)                              ║
// ║                                                                          ║
// ╟──────────────────────────────────────────────────────────────────────────╢
// ║ ALL REGISTERS MODIFIED                                                   ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Fill_PolyClip(S32 Nb_Points, Struc_Point *Ptr_Points) {
    LIB386_TRACE_CPP("Fill_PolyClip", "1", "nb_pts=%d pts=%p type=%u clipFlag=%u color=%u",
                     Nb_Points, (void *)Ptr_Points, Fill_Type, Fill_ClipFlag, Fill_Color.Num);
    // *** Get the poly-box so as to test the clipping ***
    Fill_FirstPoint = Ptr_Points;
    U32 counter = Nb_Points - 1;
    Struc_Point *points = Ptr_Points;

    S16 XMin = points[0].Pt_XE;
    S16 YMin = points[0].Pt_YE;
    S16 XMax = XMin;
    S16 YMax = YMin;
    points++;

    do {
        // @@Bounding_Box:

        // What follows has a bad pairing... How can we pair when we have NOTHING to do ?
        if (YMin > points->Pt_YE) {
            YMin = points->Pt_YE;
        }

        if (YMax < points->Pt_YE) {
            YMax = points->Pt_YE;
        }

        if (XMin > points->Pt_XE) {
            XMin = points->Pt_XE;
        }

        if (XMax < points->Pt_XE) {
            XMax = points->Pt_XE;
        }

        points++;
        counter--;
    } while (counter > 0);

    if (YMax <= YMin) {
        return Fill_LeftSlope; // Skip poly if DeltaY<=0
    }

    // *** Is the polygon at least visible ? ***

    // +1 because we don't draw the last pixel on a scanline
    if (XMin > ClipXMax + 1 || XMax < ClipXMin || YMin > ClipYMax || YMax < ClipYMin) {
        return Fill_LeftSlope;
    }

    // *** Should we clip the polygon ? ***
    points--;
    Fill_LastPoint = points;
    points = Ptr_Points; // Restore List pointer

    if (XMin < ClipXMin) {
        return Fill_ClipXMin(Nb_Points, Ptr_Points);
    }

    // +1 because we don't draw the last pixel on a scanline
    if (XMax > ClipXMax + 1) {
        return Fill_ClipXMax(Nb_Points, Ptr_Points);
    }

    if (YMin < ClipYMin) {
        return Fill_ClipYMin(Nb_Points, Ptr_Points);
    }

    if (YMax > ClipYMax) {
        return Fill_ClipYMax(Nb_Points, Ptr_Points);
    }

    while (true) {
        // Fill_PolyNoClip::
        if (Nb_Points == 3) {
            return Draw_Triangle(points);
        }

        Draw_Triangle(points);

        // @@Ret_Triangle:

        // Copy the first point in the new location (the following one)
        Struc_Point *next = points + 1;
        memcpy(next, points, sizeof(Struc_Point));
        points = next;

        Nb_Points--;
    }

    return Fill_LeftSlope;
}

S32 Triangle_ReadNextEdge(Struc_Point *PtA, Struc_Point *PtB)
// edi, esi
{
    LIB386_TRACE_CPP("Triangle_ReadNextEdge", "1", "PtA=%p PtB=%p readFlag=%u curY=%d",
                     (void *)PtA, (void *)PtB, Fill_ReadFlag, Fill_CurY);
    if ((Fill_ReadFlag & READ_NEXT_L) == 0) {
        return Read_Next_Right(PtA, PtB);
    }

    Struc_Point *newPoint = Fill_LeftPoint;
    Fill_ReadFlag &= ~READ_NEXT_L;

    while (1) {
        // @@Read_Next_L:
        Struc_Point *oldPoint = newPoint;
        newPoint++;
        if (newPoint > Fill_LastPoint) {
            newPoint = Fill_FirstPoint;
        }

        if (newPoint->Pt_YE != oldPoint->Pt_YE) {
            // If (NewY-OldY)<0 then we must end the rasterization
            if (newPoint->Pt_YE < oldPoint->Pt_YE) {
                LIB386_TRACE_CPP("Triangle_ReadNextEdge", "9", "left edge done (newY=%d < oldY=%d)",
                                 newPoint->Pt_YE, oldPoint->Pt_YE);
                RoundType = ROUND_TYPE_FLOAT;
                return Fill_LeftSlope;
            }

            // ASM adds 0x8000 (0.5) bias to Fill_CurXMin
            Fill_CurXMin = (oldPoint->Pt_XE << 16) + 0x8000;
            Fill_LeftPoint = newPoint;
            LIB386_TRACE_CPP("Triangle_ReadNextEdge", "2", "left: old=(%d,%d) new=(%d,%d) CurXMin=0x%08x diffX=%d diffY=%d",
                             oldPoint->Pt_XE, oldPoint->Pt_YE, newPoint->Pt_XE, newPoint->Pt_YE,
                             Fill_CurXMin, (newPoint->Pt_XE - oldPoint->Pt_XE) << 16,
                             newPoint->Pt_YE - oldPoint->Pt_YE);
            // ASM hardcodes Jmp_LeftSlopeZBufFPU regardless of rendering mode
            return Jmp_LeftSlopeZBufFPU[Fill_Type](
                Fill_Type,
                // For some reason diffX is << 16 but diffY is not
                (newPoint->Pt_XE - oldPoint->Pt_XE) << 16,
                (newPoint->Pt_YE - oldPoint->Pt_YE),
                oldPoint,
                newPoint);
        }
    }
}

S32 Triangle_ReadNextEdgeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Gouraud_XSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC, S32 LB_LA, S32 LC_LA) {
    // ASM computes: M2 = LC_LA * YB_YA, M1 = LB_LA * YC_YA, F = M2 - M1, result = F * InvDenom
    // all in 80-bit x87 precision, then fistp rounds in the triangle's mode (truncation)
    volatile X87_REAL M1 = (X87_REAL)LB_LA * (X87_REAL)YC_YA;
    volatile X87_REAL M2 = (X87_REAL)LC_LA * (X87_REAL)YB_YA;
    volatile X87_REAL F = M2 - M1;
    volatile X87_REAL result = F * InvDenom;
    Fill_Gouraud_XSlope = X87Fistp(result);
    return Triangle_ReadNextEdge(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Gouraud shading                                 ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_GouraudXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_GouraudXSlopeFPU", "1", "type=%u", fillType);
    S32 LB_LA = PtB->Pt_Light - PtA->Pt_Light;
    S32 LC_LA = PtC->Pt_Light - PtA->Pt_Light;
    LIB386_TRACE_CPP("Calc_GouraudXSlopeFPU", "9", "Gouraud_XSlope=%d", Fill_Gouraud_XSlope);
    return Gouraud_XSlopeFPU(fillType, PtA, PtB, PtC, LB_LA, LC_LA);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Dithered Gouraud shading (the intensity is      ║
// ║ saturated)                                                               ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_DitherXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_DitherXSlopeFPU", "1", "type=%u", fillType);
    if (PtA->Pt_Light >= 14 * 256) {
        // @@SatEDIUp:
        PtA->Pt_Light = 14 * 256 - 1;
    } else if (PtA->Pt_Light <= 255) {
        // @@SatEDILow:
        PtA->Pt_Light = 256;
    }

    if (PtB->Pt_Light >= 14 * 256) {
        // @@SatESIUp:
        PtB->Pt_Light = 14 * 256 - 1;
    } else if (PtB->Pt_Light <= 255) {
        // @@SatESILow:
        PtB->Pt_Light = 256;
    }

    if (PtC->Pt_Light >= 14 * 256) {
        // @@SatEBPUp:
        PtC->Pt_Light = 14 * 256 - 1;
    } else if (PtC->Pt_Light <= 255) {
        // @@SatEBPLow:
        PtC->Pt_Light = 256;
    }

    S32 LB_LA = PtB->Pt_Light - PtA->Pt_Light;
    S32 LC_LA = PtC->Pt_Light - PtA->Pt_Light;
    LIB386_TRACE_CPP("Calc_DitherXSlopeFPU", "9", "Dither_XSlope=%d", Fill_Gouraud_XSlope);
    return Gouraud_XSlopeFPU(fillType, PtA, PtB, PtC, LB_LA, LC_LA);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Texture Mapping                                 ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_TextureXSlopeFPU", "1", "type=%u", fillType);
    volatile X87_REAL UB_UA = (X87_REAL)(S32)(PtB->Pt_MapU - PtA->Pt_MapU);
    volatile X87_REAL UC_UA = (X87_REAL)(S32)(PtC->Pt_MapU - PtA->Pt_MapU);
    volatile X87_REAL VB_VA = (X87_REAL)(S32)(PtB->Pt_MapV - PtA->Pt_MapV);
    volatile X87_REAL VC_VA = (X87_REAL)(S32)(PtC->Pt_MapV - PtA->Pt_MapV);

    volatile X87_REAL K1 = UC_UA * (X87_REAL)YB_YA;
    volatile X87_REAL K2 = UB_UA * (X87_REAL)YC_YA;
    volatile X87_REAL K3 = VB_VA * (X87_REAL)YC_YA;
    volatile X87_REAL M1 = K1 - K2;
    volatile X87_REAL K4 = VC_VA * (X87_REAL)YB_YA;
    volatile X87_REAL SlopeU = M1 * InvDenom;
    volatile X87_REAL M2 = K4 - K3;
    volatile X87_REAL SlopeV = M2 * InvDenom;

    Fill_MapU_XSlope = X87Fistp(SlopeU);
    Fill_MapV_XSlope = X87Fistp(SlopeV);
    LIB386_TRACE_CPP("Calc_TextureXSlopeFPU", "9", "U_XSlope=%d V_XSlope=%d", Fill_MapU_XSlope, Fill_MapV_XSlope);
    return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Calc_TextureGouraudXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_TextureGouraudXSlopeFPU", "1", "type=%u", fillType);
    volatile X87_REAL UB_UA = (X87_REAL)(S32)(PtB->Pt_MapU - PtA->Pt_MapU);
    volatile X87_REAL UC_UA = (X87_REAL)(S32)(PtC->Pt_MapU - PtA->Pt_MapU);
    volatile X87_REAL VB_VA = (X87_REAL)(S32)(PtB->Pt_MapV - PtA->Pt_MapV);
    volatile X87_REAL VC_VA = (X87_REAL)(S32)(PtC->Pt_MapV - PtA->Pt_MapV);
    volatile X87_REAL LB_LA = (X87_REAL)(S32)(PtB->Pt_Light - PtA->Pt_Light);
    volatile X87_REAL LC_LA = (X87_REAL)(S32)(PtC->Pt_Light - PtA->Pt_Light);

    volatile X87_REAL KBU = UB_UA * (X87_REAL)YC_YA;
    volatile X87_REAL KCU = UC_UA * (X87_REAL)YB_YA;
    volatile X87_REAL KBV = VB_VA * (X87_REAL)YC_YA;
    volatile X87_REAL MU = KCU - KBU;
    volatile X87_REAL KCV = VC_VA * (X87_REAL)YB_YA;
    volatile X87_REAL KBL = LB_LA * (X87_REAL)YC_YA;
    volatile X87_REAL KCL = LC_LA * (X87_REAL)YB_YA;
    volatile X87_REAL MV = KCV - KBV;
    volatile X87_REAL ML = KCL - KBL;

    volatile X87_REAL SlopeU = MU * InvDenom;
    volatile X87_REAL SlopeV = MV * InvDenom;
    volatile X87_REAL SlopeL = ML * InvDenom;

    Fill_MapU_XSlope = X87Fistp(SlopeU);
    Fill_MapV_XSlope = X87Fistp(SlopeV);
    Fill_Gouraud_XSlope = X87Fistp(SlopeL);
    LIB386_TRACE_CPP("Calc_TextureGouraudXSlopeFPU", "9", "U_XSlope=%d V_XSlope=%d G_XSlope=%d", Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_Gouraud_XSlope);
    return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Calc_TextureDitherXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_TextureDitherXSlopeFPU", "1", "type=%u", fillType);
    if (PtA->Pt_Light >= 14 * 256) {
        // @@SatEDIUp:
        PtA->Pt_Light = 14 * 256 - 1;
    } else if (PtA->Pt_Light <= 255) {
        // @@SatEDILow:
        PtA->Pt_Light = 256;
    }

    if (PtB->Pt_Light >= 14 * 256) {
        // @@SatESIUp:
        PtB->Pt_Light = 14 * 256 - 1;
    } else if (PtB->Pt_Light <= 255) {
        // @@SatESILow:
        PtB->Pt_Light = 256;
    }

    if (PtC->Pt_Light >= 14 * 256) {
        // @@SatEBPUp:
        PtC->Pt_Light = 14 * 256 - 1;
    } else if (PtC->Pt_Light <= 255) {
        // @@SatEBPLow:
        PtC->Pt_Light = 256;
    }

    LIB386_TRACE_CPP("Calc_TextureDitherXSlopeFPU", "9", "U_XSlope=%d V_XSlope=%d G_XSlope=%d", Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_Gouraud_XSlope);
    return Calc_TextureGouraudXSlopeFPU(fillType, PtA, PtB, PtC);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Texture Mapping with perspective correction     ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureZXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_TextureZXSlopeFPU", "1", "type=%u PtA=(%d,%d) PtB=(%d,%d) PtC=(%d,%d)",
                     fillType, PtA->Pt_XE, PtA->Pt_YE, PtB->Pt_XE, PtB->Pt_YE, PtC->Pt_XE, PtC->Pt_YE);
    LIB386_TRACE_CPP("Calc_TextureZXSlopeFPU", "1b", "UA=%u UB=%u UC=%u VA=%u VB=%u VC=%u WA=%d WB=%d WC=%d",
                     PtA->Pt_MapU, PtB->Pt_MapU, PtC->Pt_MapU, PtA->Pt_MapV, PtB->Pt_MapV, PtC->Pt_MapV,
                     PtA->Pt_W, PtB->Pt_W, PtC->Pt_W);
    LIB386_TRACE_CPP("Calc_TextureZXSlopeFPU", "1c", "YB_YA=%g YC_YA=%g InvDenom=%.20Lg",
                     YB_YA, YC_YA, (long double)InvDenom);
    volatile X87_REAL UAp = (X87_REAL)(S32)PtA->Pt_MapU * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL UBp = (X87_REAL)(S32)PtB->Pt_MapU * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL UCp = (X87_REAL)(S32)PtC->Pt_MapU * (X87_REAL)PtC->Pt_W;

    volatile X87_REAL UBp_UAp = UBp - UAp;
    volatile X87_REAL UCp_UAp = UCp - UAp;
    volatile X87_REAL MUC = UCp_UAp * (X87_REAL)YB_YA;
    volatile X87_REAL MUB = UBp_UAp * (X87_REAL)YC_YA;
    volatile X87_REAL Dp = InvDenom * (X87_REAL)ASM_FInv_65536;
    volatile X87_REAL MUC_MUB = MUC - MUB;

    volatile X87_REAL VAp = (X87_REAL)(S32)PtA->Pt_MapV * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL VBp = (X87_REAL)(S32)PtB->Pt_MapV * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL USlope = MUC_MUB * Dp;
    volatile X87_REAL VCp = (X87_REAL)(S32)PtC->Pt_MapV * (X87_REAL)PtC->Pt_W;
    Fill_MapU_XSlope = X87Fistp(USlope);

    volatile X87_REAL VBp_VAp = VBp - VAp;
    volatile X87_REAL VCp_VAp = VCp - VAp;
    volatile X87_REAL MVB = VBp_VAp * (X87_REAL)YC_YA;
    volatile X87_REAL MVC = VCp_VAp * (X87_REAL)YB_YA;

    volatile X87_REAL WA = (X87_REAL)PtA->Pt_W;
    volatile X87_REAL WB = (X87_REAL)PtB->Pt_W;
    volatile X87_REAL WC = (X87_REAL)PtC->Pt_W;
    volatile X87_REAL MVC_MVB = MVC - MVB;
    volatile X87_REAL WB_WA = WB - WA;
    volatile X87_REAL WC_WA = WC - WA;
    volatile X87_REAL VSlope = MVC_MVB * Dp;
    volatile X87_REAL MWB = WB_WA * (X87_REAL)YC_YA;
    volatile X87_REAL MWC = WC_WA * (X87_REAL)YB_YA;
    Fill_MapV_XSlope = X87Fistp(VSlope);
    volatile X87_REAL MWC_MWB = MWC - MWB;
    volatile X87_REAL WSlope = MWC_MWB * Dp;
    volatile X87_REAL WSlope_scaled = WSlope * (X87_REAL)ASM_F_256;
    Fill_W_XSlope = X87Fistp(WSlope_scaled);
    LIB386_TRACE_CPP("Calc_TextureZXSlopeFPU", "2", "Dp=%Lg USlope=%.20Lg VSlope=%.20Lg WSlope=%.20Lg",
                     (long double)Dp, (long double)USlope, (long double)VSlope, (long double)WSlope_scaled);
    LIB386_TRACE_CPP("Calc_TextureZXSlopeFPU", "9", "U_XSlope=%d V_XSlope=%d W_XSlope=%d",
                     Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope);
    return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Calc_TextureZGouraudXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_TextureZGouraudXSlopeFPU", "1", "type=%u lightA=%u lightB=%u lightC=%u",
                     fillType, PtA->Pt_Light & 0xFFFF, PtB->Pt_Light & 0xFFFF, PtC->Pt_Light & 0xFFFF);
    S32 lightA = PtA->Pt_Light & 0xFFFF;
    S32 lightB = PtB->Pt_Light & 0xFFFF;
    S32 lightC = PtC->Pt_Light & 0xFFFF;
    S32 lightB_A = lightB - lightA;
    S32 lightC_A = lightC - lightA;

    volatile X87_REAL UAp = (X87_REAL)(S32)PtA->Pt_MapU * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL UBp = (X87_REAL)(S32)PtB->Pt_MapU * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL UCp = (X87_REAL)(S32)PtC->Pt_MapU * (X87_REAL)PtC->Pt_W;

    volatile X87_REAL UBp_UAp = UBp - UAp;
    volatile X87_REAL UCp_UAp = UCp - UAp;
    volatile X87_REAL MUC = UCp_UAp * (X87_REAL)YB_YA;
    volatile X87_REAL MUB = UBp_UAp * (X87_REAL)YC_YA;
    volatile X87_REAL Dp = InvDenom * (X87_REAL)ASM_FInv_65536;
    volatile X87_REAL MUC_MUB = MUC - MUB;

    volatile X87_REAL VAp = (X87_REAL)(S32)PtA->Pt_MapV * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL VBp = (X87_REAL)(S32)PtB->Pt_MapV * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL USlope = MUC_MUB * Dp;
    volatile X87_REAL VCp = (X87_REAL)(S32)PtC->Pt_MapV * (X87_REAL)PtC->Pt_W;
    Fill_MapU_XSlope = X87Fistp(USlope);

    volatile X87_REAL VBp_VAp = VBp - VAp;
    volatile X87_REAL VCp_VAp = VCp - VAp;
    volatile X87_REAL MVB = VBp_VAp * (X87_REAL)YC_YA;
    volatile X87_REAL MVC = VCp_VAp * (X87_REAL)YB_YA;

    volatile X87_REAL WA = (X87_REAL)PtA->Pt_W;
    volatile X87_REAL WB = (X87_REAL)PtB->Pt_W;
    volatile X87_REAL WC = (X87_REAL)PtC->Pt_W;
    volatile X87_REAL MVC_MVB = MVC - MVB;
    volatile X87_REAL WB_WA = WB - WA;
    volatile X87_REAL WC_WA = WC - WA;
    volatile X87_REAL VSlope = MVC_MVB * Dp;

    volatile X87_REAL MLB = (X87_REAL)lightB_A * (X87_REAL)YC_YA;
    volatile X87_REAL MLC = (X87_REAL)lightC_A * (X87_REAL)YB_YA;

    Fill_MapV_XSlope = X87Fistp(VSlope);

    volatile X87_REAL MWB = WB_WA * (X87_REAL)YC_YA;
    volatile X87_REAL MWC = WC_WA * (X87_REAL)YB_YA;

    // Restore D from D': ASM does fmul [F_65536] (REAL4 65536.0)
    volatile X87_REAL D = Dp * (X87_REAL)ASM_F_65536;

    volatile X87_REAL MWC_MWB = MWC - MWB;
    volatile X87_REAL ML = MLC - MLB;
    volatile X87_REAL LSlope = ML * D;
    volatile X87_REAL WSlope = MWC_MWB * D;
    Fill_Gouraud_XSlope = X87Fistp(LSlope);
    volatile X87_REAL WSlope_scaled = WSlope * (X87_REAL)ASM_FInv_256;
    Fill_W_XSlope = X87Fistp(WSlope_scaled);
    return Triangle_ReadNextEdge(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for ZBuffering                                      ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
// esi = PtA
// edi = PtB
// ebp = PtC
void Calc_ZBufXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
//int a1<ebp>, int a2@<edi>, int a3@<esi>, X87_REAL a4@<st2>, X87_REAL a5@<st1>, X87_REAL a6@<st0>)
{
    volatile X87_REAL ZB_ZA = (X87_REAL)(S32)(PtB->Pt_ZO - PtA->Pt_ZO);
    volatile X87_REAL ZC_ZA = (X87_REAL)(S32)(PtC->Pt_ZO - PtA->Pt_ZO);
    volatile X87_REAL slope = (ZC_ZA * (X87_REAL)YB_YA - ZB_ZA * (X87_REAL)YC_YA) * InvDenom;
    LIB386_TRACE_CPP("Calc_ZBufXSlopeFPU", "1", "ZB_ZA=%.20Lg ZC_ZA=%.20Lg slope=%.20Lg InvDenom=%.20Lg",
                     (long double)ZB_ZA, (long double)ZC_ZA, (long double)slope, (long double)InvDenom);
    Fill_ZBuf_XSlope = X87Fistp(slope);
    LIB386_TRACE_CPP("Calc_ZBufXSlopeFPU", "9", "ZBuf_XSlope=%d", Fill_ZBuf_XSlope);
}

S32 Calc_XSlopeZBufferFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_XSlopeZBufferFPU", "1", "type=%u", fillType);
    Calc_ZBufXSlopeFPU(PtA, PtB, PtC);
    return Jmp_XSlopeFPU[fillType](fillType, PtA, PtB, PtC);
}

S32 Calc_TextureZXSlopeZBufFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_TextureZXSlopeZBufFPU", "1", "type=%u", fillType);
    // Z extracted from Pt_ZO (ASM reads 32-bit at Pt_Light offset, shr 16 gives Pt_ZO)
    volatile X87_REAL ZAp = (X87_REAL)(S32)(U32)PtA->Pt_ZO * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL ZBp = (X87_REAL)(S32)(U32)PtB->Pt_ZO * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL ZCp = (X87_REAL)(S32)(U32)PtC->Pt_ZO * (X87_REAL)PtC->Pt_W;

    volatile X87_REAL ZBp_ZAp = ZBp - ZAp;
    volatile X87_REAL ZCp_ZAp = ZCp - ZAp;
    volatile X87_REAL MZB = ZBp_ZAp * (X87_REAL)YC_YA;
    volatile X87_REAL MZC = ZCp_ZAp * (X87_REAL)YB_YA;
    volatile X87_REAL Dp = InvDenom * (X87_REAL)ASM_FInv_65536;
    // ASM computes (MZB - MZC) * D' (fsubp st(1),st with st(0)=MZC, st(1)=MZB)
    volatile X87_REAL MZB_MZC = MZB - MZC;
    volatile X87_REAL ZSlope = MZB_MZC * Dp;
    Fill_ZBuf_XSlope = X87Fistp(ZSlope);

    // U section (same as TextureZ)
    volatile X87_REAL UAp = (X87_REAL)(S32)PtA->Pt_MapU * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL UBp = (X87_REAL)(S32)PtB->Pt_MapU * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL UCp = (X87_REAL)(S32)PtC->Pt_MapU * (X87_REAL)PtC->Pt_W;
    volatile X87_REAL UBp_UAp = UBp - UAp;
    volatile X87_REAL UCp_UAp = UCp - UAp;
    volatile X87_REAL MUC = UCp_UAp * (X87_REAL)YB_YA;
    volatile X87_REAL MUB = UBp_UAp * (X87_REAL)YC_YA;
    volatile X87_REAL MUC_MUB = MUC - MUB;

    volatile X87_REAL VAp = (X87_REAL)(S32)PtA->Pt_MapV * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL VBp = (X87_REAL)(S32)PtB->Pt_MapV * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL USlope = MUC_MUB * Dp;
    volatile X87_REAL VCp = (X87_REAL)(S32)PtC->Pt_MapV * (X87_REAL)PtC->Pt_W;
    Fill_MapU_XSlope = X87Fistp(USlope);

    volatile X87_REAL VBp_VAp = VBp - VAp;
    volatile X87_REAL VCp_VAp = VCp - VAp;
    volatile X87_REAL MVB = VBp_VAp * (X87_REAL)YC_YA;
    volatile X87_REAL MVC = VCp_VAp * (X87_REAL)YB_YA;

    volatile X87_REAL WA = (X87_REAL)PtA->Pt_W;
    volatile X87_REAL WB = (X87_REAL)PtB->Pt_W;
    volatile X87_REAL WC = (X87_REAL)PtC->Pt_W;
    volatile X87_REAL MVC_MVB = MVC - MVB;
    volatile X87_REAL WB_WA = WB - WA;
    volatile X87_REAL WC_WA = WC - WA;
    volatile X87_REAL VSlope = MVC_MVB * Dp;
    volatile X87_REAL MWB = WB_WA * (X87_REAL)YC_YA;
    volatile X87_REAL MWC = WC_WA * (X87_REAL)YB_YA;
    Fill_MapV_XSlope = X87Fistp(VSlope);
    volatile X87_REAL MWC_MWB = MWC - MWB;
    volatile X87_REAL WSlope = MWC_MWB * Dp;
    volatile X87_REAL WSlope_scaled = WSlope * (X87_REAL)ASM_F_256;
    Fill_W_XSlope = X87Fistp(WSlope_scaled);
    LIB386_TRACE_CPP("Calc_TextureZXSlopeZBufFPU", "9", "U_XSlope=%d V_XSlope=%d W_XSlope=%d ZBuf_XSlope=%d", Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope);
    return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Calc_XSlopeZBufferFlagFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC) {
    LIB386_TRACE_CPP("Calc_XSlopeZBufferFlagFPU", "1", "type=%u", fillType);
    Calc_ZBufXSlopeFPU(PtA, PtB, PtC);
    return Jmp_XSlopeFPU[0](fillType, PtA, PtB, PtC);
}

S32 Test_Scan() {
    Struc_Point *right = Fill_RightPoint;
    Struc_Point *left = Fill_LeftPoint;
    S32 diffY = 0;

    // AGI
    if (left->Pt_YE < right->Pt_YE) {
        // Left
        Fill_ReadFlag = READ_NEXT_L;

        diffY = left->Pt_YE - Fill_CurY;
    } else if (left->Pt_YE > right->Pt_YE) {
        // Right
        Fill_ReadFlag = READ_NEXT_R;

        diffY = right->Pt_YE - Fill_CurY;
    } else {
        // Both
        Fill_ReadFlag = READ_NEXT_L | READ_NEXT_R;

        diffY = left->Pt_YE - Fill_CurY;
    }

    LIB386_TRACE_CPP("Test_Scan", "1", "diffY=%d readFlag=%u curY=%d leftY=%d rightY=%d xmin=0x%08x xmax=0x%08x",
                     diffY, Fill_ReadFlag, Fill_CurY, left->Pt_YE, right->Pt_YE, Fill_CurXMin, Fill_CurXMax);
    LIB386_TRACE_CPP("Test_Scan", "2", "LeftSlope=%d RightSlope=%d Filler=%p",
                     Fill_LeftSlope, Fill_RightSlope, (void *)(intptr_t)Fill_Filler);

    if (diffY == 0) {
        LIB386_TRACE_CPP("Test_Scan", "3", "diffY==0, recurse to ReadNextEdge");
        return Triangle_ReadNextEdge(right, left);
    }

    LIB386_TRACE_CPP("Test_Scan", "4", "calling filler with nbLines=%d xmin=0x%08x xmax=0x%08x",
                     diffY, Fill_CurXMin, Fill_CurXMax);
    return Fill_Filler(diffY, Fill_CurXMin, Fill_CurXMax);
}

S32 Read_Next_Right(Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Read_Next_Right", "1", "readFlag=%u curY=%d", Fill_ReadFlag, Fill_CurY);
    // edi, esi
    if ((Fill_ReadFlag & READ_NEXT_R) == 0) {
        return Test_Scan();
    }

    Struc_Point *point = Fill_RightPoint;
    Fill_ReadFlag &= ~READ_NEXT_R;
    Struc_Point *first = Fill_FirstPoint;
    S32 oldXMin = 0;
    S32 diffY = 0;
    Struc_Point *oldPoint = NULL;

    do {
        // @@Read_Next_R:
        oldPoint = point;
        point--;

        if (point < first) {
            point = Fill_LastPoint;
        }

        // @@No_LoopRight:
        S32 oldYMin = oldPoint->Pt_YE;
        S32 newYMin = point->Pt_YE;
        oldXMin = oldPoint->Pt_XE;
        diffY = newYMin - oldYMin;
    } while (diffY == 0); // If same Y then get next point

    if (diffY < 0) {
        LIB386_TRACE_CPP("Read_Next_Right", "9", "right edge done (diffY=%d)", diffY);
        RoundType = ROUND_TYPE_FLOAT;
        return Fill_LeftSlope;
    }

    // ASM adds 0x8000 (0.5) bias to Fill_CurXMax
    Fill_CurXMax = (oldXMin << 16) + 0x8000;
    Fill_RightPoint = point;
    S32 diffX = (point->Pt_XE - oldXMin) << 16;
    // ASM: fdivr F_1 (1/dY) → fild dX → fmulp (dX * 1/dY) → fadd F_1 → fistp
    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;
    volatile X87_REAL slope = (X87_REAL)diffX * inv_dY + (X87_REAL)1.0;
    Fill_RightSlope = X87Fistp(slope);

    LIB386_TRACE_CPP("Read_Next_Right", "2", "old=(%d) new=(%d,%d) CurXMax=0x%08x RightSlope=%d diffX=%d diffY=%d",
                     oldXMin, point->Pt_XE, point->Pt_YE, Fill_CurXMax, Fill_RightSlope, diffX, diffY);

    return Test_Scan();
}

S32 Read_Next_RightFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    // ASM: fdivr F_1 (1/dY) → fild dX → fmulp (dX * 1/dY) → fadd F_1 → fistp
    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;
    volatile X87_REAL slope = (X87_REAL)diffX * inv_dY + (X87_REAL)1.0;
    Fill_LeftSlope = X87Fistp(slope);
    return Read_Next_Right(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for Gouraud shading                              ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 CalcGouraudTableFPU(U32 fillType, S32 diffX, S32 diffY, S32 diffLight, Struc_Point *PtA, Struc_Point *PtB) {
    // ASM: fild dG → fmulp st(2) (dG*inv_dY) → fistp
    // The inv_dY was pre-computed as 1.0/diffY and left on the FPU stack
    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;
    Fill_Gouraud_LeftSlope = X87Fistp((X87_REAL)diffLight * inv_dY);
    // ASM: fild dX → fmul st,st(2) → fadd [F_1] → fistp
    Fill_LeftSlope = X87Fistp((X87_REAL)diffX * inv_dY + (X87_REAL)1.0);

    LIB386_TRACE_CPP("Calc_XSlopeZBufferFlagFPU", "9", "U_XSlope=%d V_XSlope=%d W_XSlope=%d ZBuf_XSlope=%d G_XSlope=%d", Fill_MapU_XSlope, Fill_MapV_XSlope, Fill_W_XSlope, Fill_ZBuf_XSlope, Fill_Gouraud_XSlope);
    return Read_Next_Right(PtA, PtB);
}

S32 Calc_GouraudLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_GouraudLeftSlopeFPU", "1", "type=%u", fillType);
    U32 lightA = (PtA->Pt_Light << 8) + 0x8000;
    U32 lightB = (PtB->Pt_Light + 0x80) << 8;
    S32 diffLight = lightB - lightA;
    Fill_CurGouraudMin = lightA + Fill_Color.Num;

    LIB386_TRACE_CPP("Calc_GouraudLeftSlopeFPU", "9", "LeftSlope=%d G_LS=%d curG=%d", Fill_LeftSlope, Fill_Gouraud_LeftSlope, Fill_CurGouraudMin);
    return CalcGouraudTableFPU(fillType, diffX, diffY, diffLight, PtA, PtB);
}

S32 Calc_GouraudTableLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_GouraudTableLeftSlopeFPU", "1", "type=%u", fillType);
    U32 lightA = (PtA->Pt_Light << 8) + 0x8000;
    U32 lightB = (PtB->Pt_Light + 0x80) << 8;
    S32 diffLight = lightB - lightA;
    Fill_CurGouraudMin = lightA;

    LIB386_TRACE_CPP("Calc_GouraudTableLeftSlopeFPU", "9", "LeftSlope=%d G_LS=%d curG=%d", Fill_LeftSlope, Fill_Gouraud_LeftSlope, Fill_CurGouraudMin);
    return CalcGouraudTableFPU(fillType, diffX, diffY, diffLight, PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for Texture Mapping                              ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
//int __usercall Calc_TextureLeftSlope@<eax>(signed int a1@<ecx>, int a2@<edi>, int a3@<esi>)
{
    S32 dU = (S32)(PtB->Pt_MapU - PtA->Pt_MapU) << 8;
    S32 dV = (S32)(PtB->Pt_MapV - PtA->Pt_MapV) << 8;
    Fill_CurMapUMin = (S32)PtA->Pt_MapU << 8;
    Fill_CurMapVMin = (S32)PtA->Pt_MapV << 8;

    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;
    volatile X87_REAL XSlope = (X87_REAL)diffX * inv_dY;
    volatile X87_REAL USlope = (X87_REAL)dU * inv_dY;
    volatile X87_REAL LeftSlope = XSlope + (X87_REAL)1.0;
    volatile X87_REAL VSlope = (X87_REAL)dV * inv_dY;

    Fill_LeftSlope = X87Fistp(LeftSlope);
    Fill_MapU_LeftSlope = X87Fistp(USlope);
    Fill_MapV_LeftSlope = X87Fistp(VSlope);
    return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureGouraudLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_TextureGouraudLeftSlopeFPU", "1", "type=%u", fillType);
    S32 dU = (S32)(PtB->Pt_MapU - PtA->Pt_MapU) << 8;
    S32 dV = (S32)(PtB->Pt_MapV - PtA->Pt_MapV) << 8;
    S32 dL = (S32)(PtB->Pt_Light - PtA->Pt_Light) << 8;

    Fill_CurMapUMin = (S32)PtA->Pt_MapU << 8;
    Fill_CurMapVMin = (S32)PtA->Pt_MapV << 8;
    Fill_CurGouraudMin = ((S32)PtA->Pt_Light + 0x80) << 8;

    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;
    volatile X87_REAL XSlope = (X87_REAL)diffX * inv_dY;
    volatile X87_REAL VSlope = (X87_REAL)dV * inv_dY;
    volatile X87_REAL USlope = (X87_REAL)dU * inv_dY;
    volatile X87_REAL LeftSlope = XSlope + (X87_REAL)1.0;
    volatile X87_REAL GSlope = (X87_REAL)dL * inv_dY;

    Fill_LeftSlope = X87Fistp(LeftSlope);
    Fill_MapU_LeftSlope = X87Fistp(USlope);
    Fill_MapV_LeftSlope = X87Fistp(VSlope);
    Fill_Gouraud_LeftSlope = X87Fistp(GSlope);
    LIB386_TRACE_CPP("Calc_TextureGouraudLeftSlopeFPU", "9", "LeftSlope=%d U_LS=%d V_LS=%d G_LS=%d", Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_Gouraud_LeftSlope);
    return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureZLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_TextureZLeftSlopeFPU", "1", "type=%u dX=%d dY=%d PtA=(%d,%d) PtB=(%d,%d)",
                     fillType, diffX, diffY, PtA->Pt_XE, PtA->Pt_YE, PtB->Pt_XE, PtB->Pt_YE);
    LIB386_TRACE_CPP("Calc_TextureZLeftSlopeFPU", "1b", "A: u=%u v=%u w=%d  B: u=%u v=%u w=%d",
                     PtA->Pt_MapU, PtA->Pt_MapV, PtA->Pt_W, PtB->Pt_MapU, PtB->Pt_MapV, PtB->Pt_W);
    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;
    volatile X87_REAL inv256_dY = inv_dY * (X87_REAL)ASM_FInv_256;

    volatile X87_REAL UAp = (X87_REAL)(S32)PtA->Pt_MapU * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL UBp = (X87_REAL)(S32)PtB->Pt_MapU * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL VAp = (X87_REAL)(S32)PtA->Pt_MapV * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL VBp = (X87_REAL)(S32)PtB->Pt_MapV * (X87_REAL)PtB->Pt_W;

    volatile X87_REAL dUp = UBp - UAp;
    volatile X87_REAL dVp = VBp - VAp;
    volatile X87_REAL dW = (X87_REAL)PtB->Pt_W - (X87_REAL)PtA->Pt_W;

    Fill_CurWMin = PtA->Pt_W;
    volatile X87_REAL curU = UAp * (X87_REAL)ASM_FInv_256;
    volatile X87_REAL curV = VAp * (X87_REAL)ASM_FInv_256;
    Fill_CurMapUMin = X87Fistp(curU);
    Fill_CurMapVMin = X87Fistp(curV);

    volatile X87_REAL XSlope = (X87_REAL)diffX * inv_dY;
    volatile X87_REAL LeftSlope = XSlope + (X87_REAL)1.0;
    Fill_LeftSlope = X87Fistp(LeftSlope);

    volatile X87_REAL USlope = dUp * inv256_dY;
    volatile X87_REAL VSlope = dVp * inv256_dY;
    // ASM: WSlope = (WB-WA) * inv256_dY, then * F_256 before fistp
    volatile X87_REAL WSlope_raw = dW * inv256_dY;

    Fill_CurMapVMin = X87Fistp(curV);
    Fill_MapU_LeftSlope = X87Fistp(USlope);
    Fill_CurMapUMin = X87Fistp(curU);
    // ASM: fmul [F_256] before fistp [Fill_W_LeftSlope]
    volatile X87_REAL WSlope = WSlope_raw * (X87_REAL)ASM_F_256;
    Fill_MapV_LeftSlope = X87Fistp(VSlope);
    Fill_W_LeftSlope = X87Fistp(WSlope);

    LIB386_TRACE_CPP("Calc_TextureZLeftSlopeFPU", "9", "LeftSlope=%d U_LS=%d V_LS=%d W_LS=%d curU=%d curV=%d curW=%d",
                     Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope,
                     Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin);

    return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureZGouraudLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_TextureZGouraudLeftSlopeFPU", "1", "type=%u dX=%d dY=%d lightA=%u lightB=%u",
                     fillType, diffX, diffY, PtA->Pt_Light & 0xFFFF, PtB->Pt_Light & 0xFFFF);
    // Set current W minimum (ASM uses edi=PtA)
    Fill_CurWMin = PtA->Pt_W;

    // Extract light values and calculate Gouraud parameters
    S32 lightA = PtA->Pt_Light & 0xFFFF;
    S32 lightB = PtB->Pt_Light & 0xFFFF;

    S32 gouraudB = (lightB << 8) + 0x8000;
    S32 gouraudA = (lightA + 0x80) << 8;
    S32 diffLight = gouraudA - gouraudB;

    Fill_CurGouraudMin = gouraudB;

    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;

    // Gouraud slope
    volatile X87_REAL GSlope = (X87_REAL)diffLight * inv_dY;
    Fill_Gouraud_LeftSlope = X87Fistp(GSlope);

    // X slope calculation
    volatile X87_REAL XSlope = (X87_REAL)diffX * inv_dY;
    volatile X87_REAL LeftSlope = XSlope + (X87_REAL)1.0;
    Fill_LeftSlope = X87Fistp(LeftSlope);

    // Calculate perspective-corrected texture coordinates (use separate Pt_MapU/Pt_MapV)
    volatile X87_REAL UAp = (X87_REAL)(S32)PtA->Pt_MapU * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL UBp = (X87_REAL)(S32)PtB->Pt_MapU * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL VAp = (X87_REAL)(S32)PtA->Pt_MapV * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL VBp = (X87_REAL)(S32)PtB->Pt_MapV * (X87_REAL)PtB->Pt_W;

    // Calculate texture coordinate differences
    volatile X87_REAL dUp = UBp - UAp;
    volatile X87_REAL dVp = VBp - VAp;

    // Calculate texture slopes (scaled by 1/256)
    volatile X87_REAL inv256_dY = inv_dY * (X87_REAL)ASM_FInv_256;
    volatile X87_REAL WB_WA = (X87_REAL)PtB->Pt_W - (X87_REAL)PtA->Pt_W;
    volatile X87_REAL USlope = dUp * inv256_dY;
    volatile X87_REAL VSlope = dVp * inv256_dY;

    volatile X87_REAL curU = UAp * (X87_REAL)ASM_FInv_256;
    volatile X87_REAL curV = VAp * (X87_REAL)ASM_FInv_256;
    Fill_CurMapVMin = X87Fistp(curV);
    Fill_MapU_LeftSlope = X87Fistp(USlope);
    // ASM: WSlope = (WB-WA) * inv256_dY, then fmul [F_256]
    volatile X87_REAL WSlope_raw = WB_WA * inv256_dY;
    Fill_CurMapUMin = X87Fistp(curU);
    volatile X87_REAL WSlope = WSlope_raw * (X87_REAL)ASM_F_256;
    Fill_MapV_LeftSlope = X87Fistp(VSlope);
    Fill_W_LeftSlope = X87Fistp(WSlope);

    LIB386_TRACE_CPP("Calc_TextureZGouraudLeftSlopeFPU", "9", "LeftSlope=%d G_LS=%d U_LS=%d V_LS=%d W_LS=%d curG=%d curU=%d curV=%d curW=%d",
                     Fill_LeftSlope, Fill_Gouraud_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope,
                     Fill_CurGouraudMin, Fill_CurMapUMin, Fill_CurMapVMin, Fill_CurWMin);

    return Read_Next_Right(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for ZBuffer                                      ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
void Calc_ZBufLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    S32 diffZ = (S32)(PtB->Pt_ZO - PtA->Pt_ZO) << 8;
    Fill_CurZBufMin = (U32)((S32)PtA->Pt_ZO << 8);
    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;
    volatile X87_REAL ZSlope = (X87_REAL)diffZ * inv_dY;
    Fill_ZBuf_LeftSlope = X87Fistp(ZSlope);
    LIB386_TRACE_CPP("Calc_ZBufLeftSlopeFPU", "9", "ZBuf_LS=%d CurZBuf=%u", Fill_ZBuf_LeftSlope, Fill_CurZBufMin);
}

S32 Calc_LeftSlopeZBufferFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_LeftSlopeZBufferFPU", "1", "type=%u", fillType);
    Calc_ZBufLeftSlopeFPU(diffX, diffY, PtA, PtB);
    return Jmp_LeftSlopeFPU[fillType](fillType, diffX, diffY, PtA, PtB);
}

S32 Calc_TextureZLeftSlopeZBufFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_TextureZLeftSlopeZBufFPU", "1", "type=%u", fillType);
    // Set current W minimum (ASM uses edi=PtA)
    Fill_CurWMin = PtA->Pt_W;

    // Z extracted from Pt_ZO (ASM reads 32-bit at Pt_Light offset, shr 16 gives Pt_ZO)
    volatile X87_REAL inv_dY = (X87_REAL)1.0 / (X87_REAL)diffY;

    volatile X87_REAL ZAp = (X87_REAL)(S32)(U32)PtA->Pt_ZO * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL ZBp = (X87_REAL)(S32)(U32)PtB->Pt_ZO * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL ZA_scaled = ZAp * (X87_REAL)ASM_FInv_256;
    volatile X87_REAL ZB_scaled = ZBp * (X87_REAL)ASM_FInv_256;

    // fist stores to Fill_CurZBufMin (store without pop)
    Fill_CurZBufMin = (U32)X87Fistp(ZA_scaled);

    volatile X87_REAL dZ_scaled = ZB_scaled - ZA_scaled;
    volatile X87_REAL ZSlope = dZ_scaled * inv_dY;
    Fill_ZBuf_LeftSlope = X87Fistp(ZSlope);

    // X slope calculation
    volatile X87_REAL XSlope = (X87_REAL)diffX * inv_dY;
    volatile X87_REAL LeftSlope = XSlope + (X87_REAL)1.0;
    Fill_LeftSlope = X87Fistp(LeftSlope);

    // Calculate perspective-corrected texture coordinates (use separate Pt_MapU/Pt_MapV)
    volatile X87_REAL UAp = (X87_REAL)(S32)PtA->Pt_MapU * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL UBp = (X87_REAL)(S32)PtB->Pt_MapU * (X87_REAL)PtB->Pt_W;
    volatile X87_REAL VAp = (X87_REAL)(S32)PtA->Pt_MapV * (X87_REAL)PtA->Pt_W;
    volatile X87_REAL VBp = (X87_REAL)(S32)PtB->Pt_MapV * (X87_REAL)PtB->Pt_W;

    // Calculate texture coordinate differences
    volatile X87_REAL dUp = UBp - UAp;
    volatile X87_REAL dVp = VBp - VAp;

    // Calculate texture slopes (scaled by 1/256)
    volatile X87_REAL inv256_dY = inv_dY * (X87_REAL)ASM_FInv_256;
    volatile X87_REAL WB_WA = (X87_REAL)PtB->Pt_W - (X87_REAL)PtA->Pt_W;
    volatile X87_REAL USlope = dUp * inv256_dY;
    volatile X87_REAL VSlope = dVp * inv256_dY;

    volatile X87_REAL curU = UAp * (X87_REAL)ASM_FInv_256;
    volatile X87_REAL curV = VAp * (X87_REAL)ASM_FInv_256;
    Fill_CurMapVMin = X87Fistp(curV);
    Fill_MapU_LeftSlope = X87Fistp(USlope);
    // ASM: WSlope = (WB-WA) * inv256_dY, then fmul [F_256]
    volatile X87_REAL WSlope_raw = WB_WA * inv256_dY;
    Fill_CurMapUMin = X87Fistp(curU);
    volatile X87_REAL WSlope = WSlope_raw * (X87_REAL)ASM_F_256;
    Fill_MapV_LeftSlope = X87Fistp(VSlope);
    Fill_W_LeftSlope = X87Fistp(WSlope);

    LIB386_TRACE_CPP("Calc_TextureZLeftSlopeZBufFPU", "9", "LeftSlope=%d U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d", Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope);
    return Read_Next_Right(PtA, PtB);
}

S32 Calc_LeftSlopeZBufferFlagFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB) {
    LIB386_TRACE_CPP("Calc_LeftSlopeZBufferFlagFPU", "1", "type=%u", fillType);
    Calc_ZBufLeftSlopeFPU(diffX, diffY, PtA, PtB);
    LIB386_TRACE_CPP("Calc_LeftSlopeZBufferFlagFPU", "9", "LeftSlope=%d U_LS=%d V_LS=%d W_LS=%d ZBuf_LS=%d G_LS=%d", Fill_LeftSlope, Fill_MapU_LeftSlope, Fill_MapV_LeftSlope, Fill_W_LeftSlope, Fill_ZBuf_LeftSlope, Fill_Gouraud_LeftSlope);
    return Jmp_LeftSlopeFPU[0](fillType, diffX, diffY, PtA, PtB);
}

Jmp_XSlopeFunc Jmp_XSlopeFPU[] = {
    Triangle_ReadNextEdgeFPU,      // Solid
    Triangle_ReadNextEdgeFPU,      // Flat
    Triangle_ReadNextEdgeFPU,      // Transp.
    Triangle_ReadNextEdgeFPU,      // Trame
    Calc_GouraudXSlopeFPU,         // Gouraud
    Calc_DitherXSlopeFPU,          // Dither
    Calc_GouraudXSlopeFPU,         // Gouraud table
    Calc_DitherXSlopeFPU,          // Dither table
    Calc_TextureXSlopeFPU,         // Texture Solid
    Calc_TextureXSlopeFPU,         // Texture Flat
    Calc_TextureGouraudXSlopeFPU,  // Texture Gouraud
    Calc_TextureDitherXSlopeFPU,   // Texture Gouraud Dither
    Calc_TextureXSlopeFPU,         // Texture Solid Chroma Key
    Calc_TextureXSlopeFPU,         // Texture Flat Chroma Key
    Calc_TextureGouraudXSlopeFPU,  // Texture Gouraud Chroma Key
    Calc_TextureDitherXSlopeFPU,   // Texture Gouraud Dither Chroma Key
    Calc_TextureZXSlopeFPU,        // Texture Z Solid
    Calc_TextureZXSlopeFPU,        // Texture Z Flat
    Calc_TextureZGouraudXSlopeFPU, // Texture Z Gouraud
    Calc_TextureZGouraudXSlopeFPU, // Texture Z Dither
    Calc_TextureZXSlopeFPU,        // Texture Z Solid Chroma Key
    Calc_TextureZXSlopeFPU,        // Texture Z Flat Chroma Key
    Calc_TextureZGouraudXSlopeFPU, // Texture Z Gouraud Chroma Key
    Calc_TextureZGouraudXSlopeFPU, // Texture Z Dither Chroma Key
    Calc_TextureZXSlopeFPU,        // Texture Z Fog Smooth (non-ZBuf: Z handled by wrapper when in ZBuf mode)
    Calc_XSlopeZBufferFlagFPU,     // ZBuffer only
};

Jmp_XSlopeFunc Jmp_XSlopeZBufFPU[] = {
    Calc_XSlopeZBufferFPU,      // Solid
    Calc_XSlopeZBufferFPU,      // Flat
    Calc_XSlopeZBufferFPU,      // Transp.
    Calc_XSlopeZBufferFPU,      // Trame
    Calc_XSlopeZBufferFPU,      // Gouraud
    Calc_XSlopeZBufferFPU,      // Dither
    Calc_XSlopeZBufferFPU,      // Gouraud table
    Calc_XSlopeZBufferFPU,      // Dither table
    Calc_XSlopeZBufferFPU,      // Texture Solid
    Calc_XSlopeZBufferFPU,      // Texture Flat
    Calc_XSlopeZBufferFPU,      // Texture Gouraud
    Calc_XSlopeZBufferFPU,      // Texture Gouraud Dither
    Calc_XSlopeZBufferFPU,      // Texture Solid Chroma Key
    Calc_XSlopeZBufferFPU,      // Texture Flat Chroma Key
    Calc_XSlopeZBufferFPU,      // Texture Gouraud Chroma Key
    Calc_XSlopeZBufferFPU,      // Texture Gouraud Dither Chroma Key
    Calc_XSlopeZBufferFPU,      // Texture Z Solid
    Calc_XSlopeZBufferFPU,      // Texture Z Flat
    Calc_XSlopeZBufferFPU,      // Texture Z Gouraud
    Calc_XSlopeZBufferFPU,      // Texture Z Dither
    Calc_XSlopeZBufferFPU,      // Texture Z Solid Chroma Key
    Calc_XSlopeZBufferFPU,      // Texture Z Flat Chroma Key
    Calc_XSlopeZBufferFPU,      // Texture Z Gouraud Chroma Key
    Calc_XSlopeZBufferFPU,      // Texture Z Dither Chroma Key
    Calc_TextureZXSlopeZBufFPU, // Texture Z Fog Smooth
    Calc_XSlopeZBufferFlagFPU,  // ZBuffer only
};

Jmp_LeftSlopeFunc Jmp_LeftSlopeFPU[] = {
    Read_Next_RightFPU,               // Solid
    Read_Next_RightFPU,               // Flat
    Read_Next_RightFPU,               // Transp.
    Read_Next_RightFPU,               // Trame
    Calc_GouraudLeftSlopeFPU,         // Gouraud
    Calc_GouraudLeftSlopeFPU,         // Dither
    Calc_GouraudTableLeftSlopeFPU,    // Gouraud table
    Calc_GouraudTableLeftSlopeFPU,    // Dither table
    Calc_TextureLeftSlopeFPU,         // Texture Solid
    Calc_TextureLeftSlopeFPU,         // Texture Flat
    Calc_TextureGouraudLeftSlopeFPU,  // Texture Gouraud
    Calc_TextureGouraudLeftSlopeFPU,  // Texture Gouraud Dither
    Calc_TextureLeftSlopeFPU,         // Texture Solid Chroma Key
    Calc_TextureLeftSlopeFPU,         // Texture Flat Chroma Key
    Calc_TextureGouraudLeftSlopeFPU,  // Texture Gouraud Chroma Key
    Calc_TextureGouraudLeftSlopeFPU,  // Texture Gouraud Dither Chroma Key
    Calc_TextureZLeftSlopeFPU,        // Texture Z Solid
    Calc_TextureZLeftSlopeFPU,        // Texture Z Flat
    Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Gouraud
    Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Dither
    Calc_TextureZLeftSlopeFPU,        // Texture Z Solid Chroma Key
    Calc_TextureZLeftSlopeFPU,        // Texture Z Flat Chroma Key
    Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Gouraud Chroma Key
    Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Dither Chroma Key
    Calc_TextureZLeftSlopeFPU,        // Texture Z Fog Smooth (non-ZBuf: Z handled by wrapper)
    Calc_LeftSlopeZBufferFlagFPU,     // ZBuffer only
};

Jmp_LeftSlopeFunc Jmp_LeftSlopeZBufFPU[] = {
    Calc_LeftSlopeZBufferFPU,      // Solid
    Calc_LeftSlopeZBufferFPU,      // Flat
    Calc_LeftSlopeZBufferFPU,      // Transp.
    Calc_LeftSlopeZBufferFPU,      // Trame
    Calc_LeftSlopeZBufferFPU,      // Gouraud
    Calc_LeftSlopeZBufferFPU,      // Dither
    Calc_LeftSlopeZBufferFPU,      // Gouraud table
    Calc_LeftSlopeZBufferFPU,      // Dither table
    Calc_LeftSlopeZBufferFPU,      // Texture Solid
    Calc_LeftSlopeZBufferFPU,      // Texture Flat
    Calc_LeftSlopeZBufferFPU,      // Texture Gouraud
    Calc_LeftSlopeZBufferFPU,      // Texture Gouraud Dither
    Calc_LeftSlopeZBufferFPU,      // Texture Solid Chroma Key
    Calc_LeftSlopeZBufferFPU,      // Texture Flat Chroma Key
    Calc_LeftSlopeZBufferFPU,      // Texture Gouraud Chroma Key
    Calc_LeftSlopeZBufferFPU,      // Texture Gouraud Dither Chroma Key
    Calc_LeftSlopeZBufferFPU,      // Texture Z Solid
    Calc_LeftSlopeZBufferFPU,      // Texture Z Flat
    Calc_LeftSlopeZBufferFPU,      // Texture Z Gouraud
    Calc_LeftSlopeZBufferFPU,      // Texture Z Dither
    Calc_LeftSlopeZBufferFPU,      // Texture Z Solid Chroma Key
    Calc_LeftSlopeZBufferFPU,      // Texture Z Flat Chroma Key
    Calc_LeftSlopeZBufferFPU,      // Texture Z Gouraud Chroma Key
    Calc_LeftSlopeZBufferFPU,      // Texture Z Dither Chroma Key
    Calc_TextureZLeftSlopeZBufFPU, // Texture Z Fog Smooth
    Calc_LeftSlopeZBufferFlagFPU,  // ZBuffer only
};

Jmp_XSlopeFunc *Current_Jmp_XSlope = Jmp_XSlopeFPU;
Jmp_LeftSlopeFunc *Current_Jmp_LeftSlope = Jmp_LeftSlopeFPU;

// =============================================================================
#ifdef __cplusplus
}
#endif
