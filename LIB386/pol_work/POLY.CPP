#include <POLYGON/POLY.H>

#include <POLYGON/POLY_JMP.H>

#include <SYSTEM/UTILS.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>
#include <FILLER.H>

#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

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

#define FInv_65536 0.0000152588 // 1/65536 in float
#define FInv_256 0.00390625	// 1/256 in float

U32 Fill_Type = 0; // Polygon type number

U32 **Fill_Restart = NULL;
Fill_Filler_Func Fill_Filler = NULL;
U32 Fill_Patch = 0;
T_FILL_COLOR Fill_Color = { Num: 0 };

Struc_Point *Fill_FirstPoint = NULL;
Struc_Point *Fill_LastPoint = NULL;
Struc_Point *Fill_LeftPoint = NULL;
Struc_Point *Fill_RightPoint = NULL;

S32 Fill_LeftSlope = 0;      ///< Left slope
S32 Fill_RightSlope = 0;     ///< Right slope

U32 Fill_CurY = 0;           ///< Current Y
U32 Fill_CurXMin = 0;        ///< Current XMin
U32 Fill_CurXMax = 0;        ///< Current XMax
PTR_U8 Fill_CurOffLine = NULL;     ///< Offset of the current video line

S32 Fill_Cur_Z = 0;
S32 Fill_Cur_W = 0; // Current W
S32 Fill_Next_W = 0; // Next W
S32 Fill_Cur_MapU = 0; // all those variables for Texture Z
S32 Fill_Next_MapU = 0;
S32 Fill_Cur_MapV = 0;
S32 Fill_Next_MapV = 0;
S32 Fill_Cur_MapUOverW = 0;
S32 Fill_Next_MapUOverW = 0;
S32 Fill_Cur_MapVOverW = 0;
S32 Fill_Next_MapVOverW = 0;

S32 Fill_CurGouraud = 0;
S32 Fill_CurGouraudMin = 0; // Left edge: gouraud value
S32 Fill_Gouraud_LeftSlope = 0; // Left slope for Gouraud shading
S32 Fill_Gouraud_XSlope = 0; // X Slope for Gouraud shading

S32 Fill_CurMapUMin = 0; // Left edge: MapU value
S32 Fill_MapU_LeftSlope = 0; // Left slope for MapU
S32 Fill_MapU_XSlope = 0;	// X Slope for MapU

S32 Fill_CurMapVMin = 0; // Left edge: MapV value
S32 Fill_MapV_LeftSlope = 0; // Left slope for MapV
S32 Fill_MapV_XSlope = 0;	// X Slope for MapV

S32 Fill_CurWMin = 0; // Left edge: W value
S32 Fill_W_LeftSlope = 0; // Left slope for W
S32 Fill_W_XSlope = 0; // X Slope for W

S32 Fill_Z_LeftSlope = 0;
S32 Fill_Z_XSlope = 0;

U32 Fill_CurZBufMin = 0; // Left edge: ZBuf value
S32 Fill_ZBuf_LeftSlope = 0; // Left slope for ZBuf
S32 Fill_ZBuf_XSlope = 0; // X Slope for ZBuf
U32 Fill_CurZBuf = 0; // Current value of ZBuf in the scanline

U32 Fill_CurZMin = 0;

U8 Fill_ReadFlag = FALSE;        ///< Flag telling which point to read
U8 Fill_ClipFlag = FALSE;        ///< Flag telling us what to clip
U8 Fill_Flag_Fog = FALSE;     // True if FOG active
U8 Fill_Flag_ZBuffer = FALSE;	// True if ZBUFFER is active
U8 Fill_Flag_NZW = FALSE;     // True if NZW active

S32 Fill_Trame_Parity = 0;  ///< Parity for trame polygons

Fill_Jump_Fn *Fill_Saut_Normal = Fill_N_Table_Jumps; // Filler table

// -----------------------------------------------------------------------------
S32 INV64(S32 a) { return (S32)((1LL << 32) / a); }

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

void Switch_Fillers(U32 Bank)
{
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
  Switch_FogNZW
};

PTR_U8 PtrCLUTGouraud = NULL;
PTR_U8 PtrCLUTFog = NULL;
PTR_U8 PtrTruePal = NULL;

PTR_U8 PtrMap = NULL;
PTR_U16 PtrZBuffer = NULL;
S32 RepMask = 0;

S32 Fill_Z_Fog_Near = 0;     ///< Fog Range
S32 Fill_Z_Fog_Far = 0;      ///< Fog Range
U32 Fill_ZBuffer_Factor = 0; ///< normalizing factor for Z Buffer
U32 Fill_Fog_Factor = 0;     ///< Fog Factor
U32 Fill_ScaledFogNear = 0;  ///< Scaled Near Fog value
U32 IsPolygonHidden = 0;     ///< normalizing factor for Z Buffer
U8 Fill_Logical_Palette[256]; ///< Logical Palette
U32 ScreenPitch = 0;         ///< Screen Pitch
PTR_U32 PTR_TabOffLine = NULL;  ///< Screen Pitch

Struc_Point List_Clip_XMin[NB_MAX_EDGES];
Struc_Point List_Clip_XMax[NB_MAX_EDGES];
Struc_Point List_Clip_YMin[NB_MAX_EDGES];
Struc_Point List_Clip_YMax[NB_MAX_EDGES];

double Denom = 0;
double YB_YA = 0;
double YC_YA = 0;

S32 Clip_Num = 0;

void SetScreenPitch(U32 *New_TabOffLine)
{
  ScreenPitch = New_TabOffLine[1];
  PTR_TabOffLine = New_TabOffLine;
}

void SetCLUT(U32 defaultline)
{
  U8 *nextPtrTruePal = PtrCLUTFog + (defaultline << 8);

  // compare and jump to end if equal
  if (PtrTruePal == nextPtrTruePal)
  {
    return;
  }

  // update PtrTruePal and calculate new value for PtrCLUTGouraud
  PtrTruePal = nextPtrTruePal;
  PtrCLUTGouraud = PtrCLUTFog + ((defaultline << 8) & 0xF000);
  U8 *dst = (U8 *)Fill_Logical_Palette;
  U8 *src = (U8 *)PtrTruePal;

  for (int i = 0; i < 256 / 8; i++)
  {
    memcpy(dst, src, 8);
    dst += 8;
    src += 8;
  }
}

void SetFog(S32 z_near, S32 z_far)
{
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

void EnterClip(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA, S32 diff)
{
  if (Fill_ClipFlag & CLIP_LIGHT)
	{
		dst->Pt_Light = pointA->Pt_Light + (pointB->Pt_Light - pointA->Pt_Light) * Clip_Num / diff;
	}

  if (Fill_ClipFlag & CLIP_TEXTURE)
  {
		dst->Pt_MapU = pointA->Pt_MapU + (pointB->Pt_MapU - pointA->Pt_MapU) * Clip_Num / diff;
		dst->Pt_MapV = pointA->Pt_MapV + (pointB->Pt_MapV - pointA->Pt_MapV) * Clip_Num / diff;
  }

  if (Fill_ClipFlag & CLIP_TEXTUREZ)
  {
		// W
		dst->Pt_W = pointA->Pt_W + (pointB->Pt_W - pointA->Pt_W) * Clip_Num / diff;

		// MapU
		S32 uwA = (pointA->Pt_MapU * pointA->Pt_W / 256);
		S32 uwB = (pointB->Pt_MapU * pointB->Pt_W / 256);
		S32 U_tmp = (uwB - uwA) * Clip_Num / diff + uwA;
		dst->Pt_MapU = U_tmp * 256 / dst->Pt_W;

		// MapV
		S32 vwA = (pointA->Pt_MapV * pointA->Pt_W / 256);
		S32 vwB = (pointB->Pt_MapV * pointB->Pt_W / 256);
		S32 V_tmp = (vwB - vwA) * Clip_Num / diff + vwA;
		dst->Pt_MapV = V_tmp * 256 / dst->Pt_W;

    if (Fill_ClipFlag & CLIP_ZBUFFER)
    {
			// ** ZBuf **
			// If persp correct and Zbuf,
			// then we clip the zbuf with a
			// perspective correction
			S32 zA = pointA->Pt_ZO * pointA->Pt_W / 256;
			S32 zB = pointB->Pt_ZO * pointB->Pt_W / 256;
			S32 ZO_tmp = (zB - zA) * Clip_Num / diff + zA;
			dst->Pt_ZO = ZO_tmp * 256 / dst->Pt_W;
    }
  }
  else
  {
    if ( Fill_ClipFlag & CLIP_ZBUFFER )
    {
			dst->Pt_ZO = pointA->Pt_ZO + (pointB->Pt_ZO - pointA->Pt_ZO) * Clip_Num / diff;
    }
  }
}

// ESI = PtA; EBX = Pt_B with PtA: point to clip; and PtB: valid point
// y=yA+[(XMin-xa)*(yB-yA)]/(xB-xA)
void Generic_ClipXMin(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
  S32 xDiff = pointB->Pt_XE - pointA->Pt_XE;

  Clip_Num = ClipXMin - pointA->Pt_XE;
	dst->Pt_XE = ClipXMin;
	dst->Pt_YE = pointA->Pt_YE + (pointB->Pt_YE - pointA->Pt_YE) * Clip_Num / xDiff;

	EnterClip(pointB, dst, pointA, xDiff);
}

// ESI = PtA; EBX = Pt_B
// y=yA+[(XMax-xa)*(yB-yA)]/(xB-xA)
void Generic_ClipXMax(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
	S32 xDiff = pointB->Pt_XE - pointA->Pt_XE;

	// ClipXMax + 1 bcoz we don't draw the last pixel on a scanline
	Clip_Num = ClipXMax + 1 - pointA->Pt_XE;

	dst->Pt_XE = ClipXMax + 1;
	dst->Pt_YE = pointA->Pt_YE + (pointB->Pt_YE - pointA->Pt_YE) * Clip_Num / xDiff;

	EnterClip(pointB, dst, pointA, xDiff);
}

// ESI = PtA; EBX = Pt_B
// x=xa+[(YMin-ya)*(xB-xA)]/(yB-ya)
void Generic_ClipYMin(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
	S32 yDiff = pointB->Pt_YE - pointA->Pt_YE;

	Clip_Num = ClipYMin - pointA->Pt_YE;
	dst->Pt_XE = pointA->Pt_XE + (pointB->Pt_XE - pointA->Pt_XE) * Clip_Num / yDiff;
	dst->Pt_YE = ClipYMin;

	EnterClip(pointB, dst, pointA, yDiff);
}

// ESI = PtA; EBX = Pt_B
// x=xa+[(YMax-ya)*(xB-xA)]/(yB-ya)
void Generic_ClipYMax(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA)
{
	S32 yDiff = pointB->Pt_YE - pointA->Pt_YE;

	Clip_Num = ClipYMax - pointA->Pt_YE;
	dst->Pt_XE = pointA->Pt_XE + (pointB->Pt_XE - pointA->Pt_XE) * Clip_Num / yDiff;
	dst->Pt_YE = ClipYMax;

	EnterClip(pointB, dst, pointA, yDiff);
}

// Clip types
#define CLIP_XMIN	0
#define CLIP_XMAX	1
#define CLIP_YMIN	2
#define CLIP_YMAX	3

void Clipper_Jmp(Struc_Point *pointB, Struc_Point *dst, Struc_Point *pointA, U8 type)
{
	switch (type)
	{
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
S32 Fill_Clip(S32 Nb_Points, Struc_Point *Ptr_Points, U8 type)
{
  Struc_Point *prevPoint = Fill_LastPoint; // Previous point
  Struc_Point *currentPoint = Fill_FirstPoint; // Current point

  Struc_Point *clipPoints = NULL;

  S32 newNbPoints = 0;
  currentPoint--;

	if (type == CLIP_XMIN)
	{
		clipPoints = List_Clip_XMin;
	}
	else if (type == CLIP_XMAX)
	{
		clipPoints = List_Clip_XMax;
	}
	else if (type == CLIP_YMIN)
	{
		clipPoints = List_Clip_YMin;
	}
	else if (type == CLIP_YMAX)
	{
		clipPoints = List_Clip_YMax;
	}

  U8 isPointInvisible = 1;

  if ((type == CLIP_XMIN && Fill_LastPoint->Pt_XE >= ClipXMin)
		|| (type == CLIP_XMAX && Fill_LastPoint->Pt_XE <= ClipXMax + 1)
		|| (type == CLIP_YMIN && Fill_LastPoint->Pt_YE >= ClipYMin)
		|| (type == CLIP_YMAX && Fill_LastPoint->Pt_YE <= ClipYMax))
	{
    isPointInvisible = 0;
	}

  while (currentPoint != Fill_LastPoint)
  {
    currentPoint++;
    if ((type == CLIP_XMIN && currentPoint->Pt_XE >= ClipXMin)
			|| (type == CLIP_XMAX && currentPoint->Pt_XE <= ClipXMax + 1)
			|| (type == CLIP_YMIN && currentPoint->Pt_YE >= ClipYMin)
			|| (type == CLIP_YMAX && currentPoint->Pt_YE <= ClipYMax))
    {
      if (isPointInvisible)
      {
        Clipper_Jmp(prevPoint, clipPoints, currentPoint, type);
        clipPoints++;
        newNbPoints++;
      }

			memcpy(clipPoints, currentPoint, sizeof(Struc_Point));

      ++newNbPoints;
      clipPoints++;
      isPointInvisible = 0;
    }
    else if (!isPointInvisible)
    {
			// Current point is off-screen
			Clipper_Jmp(prevPoint, clipPoints, currentPoint, type);
      clipPoints++; // A point was created: registers it!
      newNbPoints++;
      isPointInvisible = 1; // Last point not visible
    }

		prevPoint = currentPoint;
  }

	Struc_Point *nextPtrPoints = NULL;
	if (type == CLIP_XMIN)
	{
		nextPtrPoints = List_Clip_XMin;
	}
	else if (type == CLIP_XMAX)
	{
		nextPtrPoints = List_Clip_XMax;
	}
	else if (type == CLIP_YMIN)
	{
		nextPtrPoints = List_Clip_YMin;
	}
	else if (type == CLIP_YMAX)
	{
		nextPtrPoints = List_Clip_YMax;
	}

  return Fill_PolyClip(newNbPoints, nextPtrPoints);
}

S32 Fill_ClipXMin(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_XMIN);
}

S32 Fill_ClipXMax(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_XMAX);
}

S32 Fill_ClipYMin(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_YMIN);
}

S32 Fill_ClipYMax(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	return Fill_Clip(Nb_Points, Ptr_Points, CLIP_YMAX);
}

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
S32 Draw_Triangle(Struc_Point *Ptr_Points)
{
	// *** Search the highest point in the list ***
  Fill_FirstPoint = Ptr_Points;
  Fill_Patch = 1;

  S16 currentY = Ptr_Points[0].Pt_YE;
  Struc_Point *highestPoint = Ptr_Points;
  S16 highestY = Ptr_Points[0].Pt_YE; // lowest value
  S16 nextY = Ptr_Points[1].Pt_YE;
  Struc_Point *nextPoint = Ptr_Points + 1;

  if (nextY <= highestY)
  {
    highestY = nextY;
    highestPoint = nextPoint;
  }

  S16 nextNextY = nextPoint[1].Pt_YE;
  Struc_Point *nextNextPoint = nextPoint + 1;
  if (nextNextY <= highestY)
  {
    highestY = nextNextY;
    highestPoint = nextNextPoint;
  }

  Fill_LeftPoint = highestPoint;
  Fill_RightPoint = highestPoint;

	// Test if the poly is flat
  if (currentY == nextY && nextY == nextNextY)
	{
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
  if (leftPoint > Fill_LastPoint)
	{
    leftPoint = Fill_FirstPoint;
	}

  Struc_Point *rightPoint = leftPoint + 1;
  if (rightPoint > Fill_LastPoint)
	{
    rightPoint = Fill_FirstPoint;
	}

	// esi = Fill_LeftPoint = PtA
	// edi = leftPoint = PtB
	// ebp = rightPoint = PtC
  RoundType = ROUND_TYPE_INT;

	Struc_Point *PtA = Fill_LeftPoint;
	Struc_Point *PtB = leftPoint;
	Struc_Point *PtC = rightPoint;

  YB_YA = PtB->Pt_YE - PtA->Pt_YE;
  YC_YA = PtC->Pt_YE - PtA->Pt_YE;

  Denom = YB_YA * (PtC->Pt_XE - PtA->Pt_XE)
				- YC_YA * (PtB->Pt_XE - PtA->Pt_XE);

	// FPU version divides Denom by 256.0, while integer versions
	// multiply by 256 the rest of values that will be divided by Denom.
	Denom /= 256.0;

	if (Denom == 0)
	{
    RoundType = ROUND_TYPE_FLOAT;
		return Fill_LeftSlope;
	}

	return Current_Jmp_XSlope[Fill_Type](Fill_Type, PtA, PtB, PtC);
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
S32 Fill_PolyClip(S32 Nb_Points, Struc_Point *Ptr_Points)
{
	// *** Get the poly-box so as to test the clipping ***
	Fill_FirstPoint = Ptr_Points;
	U32 counter = Nb_Points - 1;
	Struc_Point *points = Ptr_Points;

	S16 XMin = points[0].Pt_XE;
	S16 YMin = points[0].Pt_YE;
	S16 XMax = XMin;
	S16 YMax = YMin;
	points++;

	do
	{
		// @@Bounding_Box:

		// What follows has a bad pairing... How can we pair when we have NOTHING to do ?
		if (YMin > points->Pt_YE)
		{
			YMin = points->Pt_YE;
		}

		if (YMax < points->Pt_YE)
		{
			YMax = points->Pt_YE;
		}

		if (XMin > points->Pt_XE)
		{
			XMin = points->Pt_XE;
		}

		if (XMax < points->Pt_XE)
		{
			XMax = points->Pt_XE;
		}

		points++;
		counter--;
	} while (counter > 0);

	if (YMax <= YMin)
	{
		return Fill_LeftSlope; // Skip poly if DeltaY<=0
	}

	// *** Is the polygon at least visible ? ***

	// +1 because we don't draw the last pixel on a scanline
	if (XMin > ClipXMax + 1 || XMax < ClipXMin
			|| YMin > ClipYMax || YMax < ClipYMin)
	{
		return Fill_LeftSlope;
	}

	// *** Should we clip the polygon ? ***
	points--;
	Fill_LastPoint = points;
	points = Ptr_Points; // Restore List pointer

	if (XMin < ClipXMin)
	{
		return Fill_ClipXMin(Nb_Points, Ptr_Points);
	}

	// +1 because we don't draw the last pixel on a scanline
	if (XMax > ClipXMax + 1)
	{
		return Fill_ClipXMax(Nb_Points, Ptr_Points);
	}

	if (YMin < ClipYMin)
	{
		return Fill_ClipYMin(Nb_Points, Ptr_Points);
	}

	if (YMax > ClipYMax)
	{
		return Fill_ClipYMax(Nb_Points, Ptr_Points);
	}

	while (true)
	{
		// Fill_PolyNoClip::
		if (Nb_Points == 3)
		{
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
	if ((Fill_ReadFlag & READ_NEXT_L) == 0)
	{
		return Read_Next_Right(PtA, PtB);
	}

  Struc_Point *newPoint = Fill_LeftPoint;
  Fill_ReadFlag &= ~READ_NEXT_L;

  while (1)
  {
		// @@Read_Next_L:
    Struc_Point *oldPoint = newPoint;
    newPoint++;
    if (newPoint > Fill_LastPoint)
		{
      newPoint = Fill_FirstPoint;
		}

    if (newPoint->Pt_YE != oldPoint->Pt_YE)
    {
			// If (NewY-OldY)<0 then we must end the rasterization
			if (newPoint->Pt_YE < oldPoint->Pt_YE)
			{
        RoundType = ROUND_TYPE_FLOAT;
				return Fill_LeftSlope;
			}

      Fill_CurXMin = (oldPoint->Pt_XE << 16) + 0x8000;
      Fill_LeftPoint = newPoint;
      return Current_Jmp_LeftSlope[Fill_Type](
        Fill_Type,
				// For some reason diffX is << 16 but diffY is not
				(newPoint->Pt_XE - oldPoint->Pt_XE) << 16,
				(newPoint->Pt_YE - oldPoint->Pt_YE),
				oldPoint,
				newPoint);
    }
  }
}

S32 Triangle_ReadNextEdgeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Gouraud_XSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC, S32 LB_LA, S32 LC_LA)
{
  Fill_Gouraud_XSlope = ROUND((YB_YA * LC_LA - YC_YA * LB_LA) / Denom);
  return Triangle_ReadNextEdge(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Gouraud shading                                 ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_GouraudXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  S32 LB_LA = PtB->Pt_Light - PtA->Pt_Light;
  S32 LC_LA = PtC->Pt_Light - PtA->Pt_Light;
  return Gouraud_XSlopeFPU(fillType, PtA, PtB, PtC, LB_LA, LC_LA);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Dithered Gouraud shading (the intensity is      ║
// ║ saturated)                                                               ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_DitherXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  if (PtA->Pt_Light >= 14 * 256)
  {
    // @@SatEDIUp:
    PtA->Pt_Light = 14 * 256 - 1;
  } else if (PtA->Pt_Light <= 255)
  {
    // @@SatEDILow:
    PtA->Pt_Light = 256;
  }

  if (PtB->Pt_Light >= 14 * 256)
  {
    // @@SatESIUp:
    PtB->Pt_Light = 14 * 256 - 1;
  }
  else if (PtB->Pt_Light <= 255)
  {
    // @@SatESILow:
    PtB->Pt_Light = 256;
  }

  if (PtC->Pt_Light >= 14 * 256)
  {
    // @@SatEBPUp:
    PtC->Pt_Light = 14 * 256 - 1;
  }
  else if (PtC->Pt_Light <= 255)
  {
    // @@SatEBPLow:
    PtC->Pt_Light = 256;
  }

  S32 LB_LA = PtB->Pt_Light - PtA->Pt_Light;
  S32 LC_LA = PtC->Pt_Light - PtA->Pt_Light;
  return Gouraud_XSlopeFPU(fillType, PtA, PtB, PtC, LB_LA, LC_LA);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Texture Mapping                                 ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  double UB_UA = PtB->Pt_MapU - PtA->Pt_MapU;
  double UC_UA = PtC->Pt_MapU - PtA->Pt_MapU;
  double VB_VA = PtB->Pt_MapV - PtA->Pt_MapV;
  double VC_VA = PtC->Pt_MapV - PtA->Pt_MapV;
  Fill_MapU_XSlope = ROUND((YB_YA * UC_UA - YC_YA * UB_UA) / Denom);
  Fill_MapV_XSlope = ROUND((YB_YA * VC_VA - YC_YA * VB_VA) / Denom);
  return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Calc_TextureGouraudXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  double UB_UA = PtB->Pt_MapU - PtA->Pt_MapU;
  double UC_UA = PtC->Pt_MapU - PtA->Pt_MapU;
  double VB_VA = PtB->Pt_MapV - PtA->Pt_MapV;
  double VC_VA = PtC->Pt_MapV - PtA->Pt_MapV;
  double LC_LA = PtC->Pt_Light - PtA->Pt_Light;
  double LB_LA = PtB->Pt_Light - PtA->Pt_Light;
  Fill_MapU_XSlope = ROUND((YB_YA * UC_UA - YC_YA * UB_UA) / Denom);
  Fill_MapV_XSlope = ROUND((YB_YA * VC_VA - YC_YA * VB_VA) / Denom);
  Fill_Gouraud_XSlope = ROUND((YB_YA * LC_LA - YC_YA * LB_LA) / Denom);
  return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Calc_TextureDitherXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  if (PtA->Pt_Light >= 14 * 256)
  {
    // @@SatEDIUp:
    PtA->Pt_Light = 14 * 256 - 1;
  } else if (PtA->Pt_Light <= 255)
  {
    // @@SatEDILow:
    PtA->Pt_Light = 256;
  }

  if (PtB->Pt_Light >= 14 * 256)
  {
    // @@SatESIUp:
    PtB->Pt_Light = 14 * 256 - 1;
  }
  else if (PtB->Pt_Light <= 255)
  {
    // @@SatESILow:
    PtB->Pt_Light = 256;
  }

  if (PtC->Pt_Light >= 14 * 256)
  {
    // @@SatEBPUp:
    PtC->Pt_Light = 14 * 256 - 1;
  }
  else if (PtC->Pt_Light <= 255)
  {
    // @@SatEBPLow:
    PtC->Pt_Light = 256;
  }

  return Calc_TextureGouraudXSlopeFPU(fillType, PtA, PtB, PtC);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for Texture Mapping with perspective correction     ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureZXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  double UA = (double)PtA->Pt_MapU * (double)PtA->Pt_W;
  double UB = (double)PtB->Pt_MapU * (double)PtB->Pt_W;
  double UC = (double)PtC->Pt_MapU * (double)PtC->Pt_W;
  double VA = (double)PtA->Pt_MapV * (double)PtA->Pt_W;
  double VB = (double)PtB->Pt_MapV * (double)PtB->Pt_W;
  double VC = (double)PtC->Pt_MapV * (double)PtC->Pt_W;

  double UB_UA = UB - UA;
  double UC_UA = UC - UA;
  double VB_VA = VB - VA;
  double VC_VA = VC - VA;
  double WB_WA = (double)PtB->Pt_W - (double)PtA->Pt_W;
  double WC_WA = (double)PtC->Pt_W - (double)PtA->Pt_W;
  Fill_MapU_XSlope = ROUND((YB_YA * UC_UA - YC_YA * UB_UA) * FInv_65536 / Denom);
  Fill_MapV_XSlope = ROUND((YB_YA * VC_VA - YC_YA * VB_VA) * FInv_65536 / Denom);
  Fill_W_XSlope = ROUND((YB_YA * WC_WA - YC_YA * WB_WA) * 256.0 * FInv_65536 / Denom);
  return Triangle_ReadNextEdge(PtA, PtB);
}

S32 Calc_TextureZGouraudXSlopeFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	// Calculate slopes for texture mapping with perspective correction and Gouraud shading
	
	// Extract texture coordinates U and V from the packed MapU value
	int uA = PtA->Pt_MapU & 0xFFFF;
	int uB = PtB->Pt_MapU & 0xFFFF;
	int uC = PtC->Pt_MapU & 0xFFFF;
	
	int vA = (PtA->Pt_MapU >> 16) & 0xFFFF;
	int vB = (PtB->Pt_MapU >> 16) & 0xFFFF;
	int vC = (PtC->Pt_MapU >> 16) & 0xFFFF;
	
	// Extract light values
	int lightA = PtA->Pt_Light & 0xFFFF;
	int lightB = PtB->Pt_Light & 0xFFFF;
	int lightC = PtC->Pt_Light & 0xFFFF;
	
	// Calculate light differences relative to point A
	int lightB_A = lightB - lightA;
	int lightC_A = lightC - lightA;
	
	// Calculate perspective-corrected U coordinates: U * W
	double uA_w = (double)uA * PtA->Pt_W;
	double uB_w = (double)uB * PtB->Pt_W;
	double uC_w = (double)uC * PtC->Pt_W;
	
	// Calculate U differences
	double uB_A_corr = uB_w - uA_w;
	double uC_A_corr = uC_w - uA_w;
	
	// Calculate U slope numerator
	double u_numerator = (uC_A_corr * YB_YA) - (uB_A_corr * YC_YA);
	Fill_MapU_XSlope = ROUND((u_numerator / Denom) * (1.0 / 65536.0));
	
	// Calculate perspective-corrected V coordinates: V * W
	double vA_w = (double)vA * PtA->Pt_W;
	double vB_w = (double)vB * PtB->Pt_W;
	double vC_w = (double)vC * PtC->Pt_W;
	
	// Calculate V differences
	double vB_A_corr = vB_w - vA_w;
	double vC_A_corr = vC_w - vA_w;
	
	// Calculate V slope numerator
	double v_numerator = (vC_A_corr * YB_YA) - (vB_A_corr * YC_YA);
	Fill_MapV_XSlope = ROUND((v_numerator / Denom) * (1.0 / 65536.0));
	
	// Calculate W slope for perspective correction
	double wB_A = PtB->Pt_W - PtA->Pt_W;
	double wC_A = PtC->Pt_W - PtA->Pt_W;
	
	double w_numerator = (wC_A * YB_YA) - (wB_A * YC_YA);
	Fill_W_XSlope = ROUND((w_numerator / Denom) * (65536.0 / 256.0));
	
	// Calculate Gouraud shading slope
	double lightB_w = (double)lightB_A * PtB->Pt_W;
	double lightC_w = (double)lightC_A * PtC->Pt_W;
	
	double light_numerator = (lightC_w * YB_YA) - (lightB_w * YC_YA);
	Fill_Gouraud_XSlope = ROUND(light_numerator / Denom);
	
	return 0;
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the XSlope for ZBuffering                                      ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
// esi = PtA
// edi = PtB
// ebp = PtC
void Calc_ZBufXSlopeFPU(Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
  //int a1<ebp>, int a2@<edi>, int a3@<esi>, long double a4@<st2>, long double a5@<st1>, long double a6@<st0>)
{
  double ZB_ZA = PtB->Pt_ZO - PtA->Pt_ZO;
  double ZC_ZA = PtC->Pt_ZO - PtA->Pt_ZO;

  Fill_ZBuf_XSlope = ROUND(
    (ZC_ZA * YB_YA - ZB_ZA * YC_YA) / Denom
  );
}

S32 Calc_XSlopeZBufferFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  Calc_ZBufXSlopeFPU(PtA, PtB, PtC);
  return Jmp_XSlopeFPU[fillType](fillType, PtA, PtB, PtC);
}

S32 Calc_TextureZXSlopeZBufFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
	// Calculate slopes for texture mapping with perspective correction and Z-buffering
	
	// Extract Z values from high 16 bits of Pt_Light
	int zA = (PtA->Pt_Light >> 16) & 0xFFFF;
	int zB = (PtB->Pt_Light >> 16) & 0xFFFF;
	int zC = (PtC->Pt_Light >> 16) & 0xFFFF;
	
	// Extract texture coordinates U and V from MapU
	int uA = PtA->Pt_MapU & 0xFFFF;
	int uB = PtB->Pt_MapU & 0xFFFF;
	int uC = PtC->Pt_MapU & 0xFFFF;
	
	int vA = (PtA->Pt_MapU >> 16) & 0xFFFF;
	int vB = (PtB->Pt_MapU >> 16) & 0xFFFF;
	int vC = (PtC->Pt_MapU >> 16) & 0xFFFF;
	
	// Calculate perspective-corrected Z coordinates: Z * W
	double zA_w = (double)zA * PtA->Pt_W;
	double zB_w = (double)zB * PtB->Pt_W;
	double zC_w = (double)zC * PtC->Pt_W;
	
	// Calculate Z differences
	double zB_A_corr = zB_w - zA_w;
	double zC_A_corr = zC_w - zA_w;
	
	// Calculate Z-buffer slope numerator
	double z_numerator = (zC_A_corr * YB_YA) - (zB_A_corr * YC_YA);
	Fill_ZBuf_XSlope = ROUND((z_numerator / Denom) * (1.0 / 65536.0));
	
	// Calculate perspective-corrected U coordinates: U * W
	double uA_w = (double)uA * PtA->Pt_W;
	double uB_w = (double)uB * PtB->Pt_W;
	double uC_w = (double)uC * PtC->Pt_W;
	
	// Calculate U differences
	double uB_A_corr = uB_w - uA_w;
	double uC_A_corr = uC_w - uA_w;
	
	// Calculate U slope numerator
	double u_numerator = (uC_A_corr * YB_YA) - (uB_A_corr * YC_YA);
	Fill_MapU_XSlope = ROUND((u_numerator / Denom) * (1.0 / 65536.0));
	
	// Calculate perspective-corrected V coordinates: V * W
	double vA_w = (double)vA * PtA->Pt_W;
	double vB_w = (double)vB * PtB->Pt_W;
	double vC_w = (double)vC * PtC->Pt_W;
	
	// Calculate V differences
	double vB_A_corr = vB_w - vA_w;
	double vC_A_corr = vC_w - vA_w;
	
	// Calculate V slope numerator
	double v_numerator = (vC_A_corr * YB_YA) - (vB_A_corr * YC_YA);
	Fill_MapV_XSlope = ROUND((v_numerator / Denom) * (1.0 / 65536.0));
	
	// Calculate W slope for perspective correction
	double wB_A = PtB->Pt_W - PtA->Pt_W;
	double wC_A = PtC->Pt_W - PtA->Pt_W;
	
	double w_numerator = (wC_A * YB_YA) - (wB_A * YC_YA);
	Fill_W_XSlope = ROUND((w_numerator / Denom) * 256.0);
	
	return 0;
}

S32 Calc_XSlopeZBufferFlagFPU(U32 fillType, Struc_Point *PtA, Struc_Point *PtB, Struc_Point *PtC)
{
  Calc_ZBufXSlopeFPU(PtA, PtB, PtC);
  return Jmp_XSlopeFPU[0](fillType, PtA, PtB, PtC);
}

S32 Test_Scan()
{
	Struc_Point *right = Fill_RightPoint;
	Struc_Point *left = Fill_LeftPoint;
	S32 diffY = 0;

	// AGI
	if (left->Pt_YE < right->Pt_YE)
	{
		// Left
		Fill_ReadFlag = READ_NEXT_L;

		diffY = left->Pt_YE - Fill_CurY;
	}
	else if (left->Pt_YE > right->Pt_YE)
	{
		// Right
		Fill_ReadFlag = READ_NEXT_R;

		diffY = right->Pt_YE - Fill_CurY;
	}
	else
	{
		// Both
		Fill_ReadFlag = READ_NEXT_L | READ_NEXT_R;

		diffY = left->Pt_YE - Fill_CurY;
	}

	if (diffY == 0)
	{
		return Triangle_ReadNextEdge(right, left);
	}

	return Fill_Filler(diffY, Fill_CurXMin, Fill_CurXMax);
}

S32 Read_Next_Right(Struc_Point *PtA, Struc_Point *PtB)
{
	// edi, esi
	if ((Fill_ReadFlag & READ_NEXT_R) == 0)
	{
		return Test_Scan();
	}

	Struc_Point *point = Fill_RightPoint;
	Fill_ReadFlag &= ~READ_NEXT_R;
	Struc_Point *first = Fill_FirstPoint;
	S32 oldXMin = 0;
	S32 diffY = 0;
	Struc_Point *oldPoint = NULL;

	do
	{
		// @@Read_Next_R:
		oldPoint = point;
		point--;

		if (point < first)
		{
			point = Fill_LastPoint;
		}

		// @@No_LoopRight:
		S32 oldYMin = oldPoint->Pt_YE;
		S32 newYMin = point->Pt_YE;
		oldXMin = oldPoint->Pt_XE;
		diffY = newYMin - oldYMin;
	} while (diffY == 0); // If same Y then get next point

	if (diffY < 0)
	{
    RoundType = ROUND_TYPE_FLOAT;
		return Fill_LeftSlope;
	}

	Fill_CurXMax = (oldXMin << 16) + 0x8000;
	Fill_RightPoint = point;
	S32 diffX = (point->Pt_XE - oldXMin) << 16;
	Fill_RightSlope = ROUND((float)diffX / (float)diffY + 1.0);

	return Test_Scan();
}

S32 Read_Next_RightFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	Fill_LeftSlope = diffX / diffY + 1;
	return Read_Next_Right(PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for Gouraud shading                              ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 CalcGouraudTableFPU(U32 fillType, S32 diffX, S32 diffY, S32 diffLight, Struc_Point *PtA, Struc_Point *PtB)
{
  Fill_Gouraud_LeftSlope = diffLight / diffY;
  Fill_LeftSlope = diffX / diffY + 1;

  return Read_Next_Right(PtA, PtB);
}

S32 Calc_GouraudLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  U32 lightA = (PtA->Pt_Light << 8) + 0x8000;
  U32 lightB = (PtB->Pt_Light + 0x80) << 8;
  S32 diffLight = lightB - lightA;
  Fill_CurGouraudMin = lightA + Fill_Color.Num;

  return CalcGouraudTableFPU(fillType, diffX, diffY, diffLight, PtA, PtB);
}

S32 Calc_GouraudTableLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  U32 lightA = (PtA->Pt_Light << 8) + 0x8000;
  U32 lightB = (PtB->Pt_Light + 0x80) << 8;
  S32 diffLight = lightB - lightA;
  Fill_CurGouraudMin = lightA;

  return CalcGouraudTableFPU(fillType, diffX, diffY, diffLight, PtA, PtB);
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for Texture Mapping                              ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
S32 Calc_TextureLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
//int __usercall Calc_TextureLeftSlope@<eax>(signed int a1@<ecx>, int a2@<edi>, int a3@<esi>)
{
  Fill_CurMapUMin = PtA->Pt_MapU << 8;
  Fill_MapU_LeftSlope = ((PtB->Pt_MapU - PtA->Pt_MapU) << 8) / diffY;
  Fill_CurMapVMin = PtA->Pt_MapV << 8;
  Fill_MapV_LeftSlope = ((PtB->Pt_MapV - PtA->Pt_MapV) << 8) / diffY;
	Fill_LeftSlope = diffX / diffY + 1;
  return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureGouraudLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  double diffU = (PtB->Pt_MapU - PtA->Pt_MapU) << 8;
  double diffLight = (PtB->Pt_Light - PtA->Pt_Light) << 8;
  double diffV = (PtB->Pt_MapV - PtA->Pt_MapV) << 8;

  Fill_CurMapUMin = (PtA->Pt_MapU /*+ 0x80*/) << 8;
  Fill_CurMapVMin = (PtA->Pt_MapV /*+ 0x80*/) << 8;
  Fill_CurGouraudMin = (PtA->Pt_Light + 0x80) << 8;

  Fill_Gouraud_LeftSlope = ROUND(diffLight / diffY);
  Fill_MapV_LeftSlope = ROUND(diffV / diffY);
  Fill_MapU_LeftSlope = ROUND(diffU / diffY);
  Fill_LeftSlope = ROUND(diffX / diffY + 1);

  return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureZLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  double UA = (double)(PtA->Pt_MapU /*+ 0x80*/) * PtA->Pt_W;
  double UB = (double)(PtB->Pt_MapU /*+ 0x80*/) * PtB->Pt_W;
  double VA = (double)(PtA->Pt_MapV /*+ 0x80*/) * PtA->Pt_W;
  double VB = (double)(PtB->Pt_MapV /*+ 0x80*/) * PtB->Pt_W;

  double UA2 = UA * FInv_256;
  double UB2 = UB * FInv_256;
  double VA2 = VA * FInv_256;
  double VB2 = VB * FInv_256;

  double UB_UA = UB - UA;
  double VB_VA = VB - VA;
  double WB_WA = PtB->Pt_W - PtA->Pt_W;

  Fill_CurWMin = PtA->Pt_W;
  Fill_CurMapUMin = ROUND(UA2);
  Fill_CurMapVMin = ROUND(VA2);

  Fill_MapV_LeftSlope = ROUND(VB_VA * FInv_256 / diffY);
  Fill_MapU_LeftSlope = ROUND(UB_UA * FInv_256 / diffY);
  Fill_W_LeftSlope = ROUND(WB_WA / diffY);
  Fill_LeftSlope = ROUND(diffX / diffY + 1);

  return Read_Next_Right(PtA, PtB);
}

S32 Calc_TextureZGouraudLeftSlopeFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	// Calculate left slopes for texture mapping with perspective correction and Gouraud shading
	
	// Extract texture coordinates
	int uA = PtA->Pt_MapU & 0xFFFF;
	int uB = PtB->Pt_MapU & 0xFFFF;
	int vA = (PtA->Pt_MapU >> 16) & 0xFFFF;
	int vB = (PtB->Pt_MapU >> 16) & 0xFFFF;
	
#ifdef CORRECT_TEXTURE
	uA += 0x80;
	uB += 0x80;
	vA += 0x80;
	vB += 0x80;
#endif
	
	// Set current W minimum
	Fill_CurWMin = PtB->Pt_W;
	
	// Extract light values and calculate Gouraud parameters
	int lightA = PtA->Pt_Light & 0xFFFF;
	int lightB = PtB->Pt_Light & 0xFFFF;
	
	int gouraudB = (lightB << 8) + 0x8000;
	int gouraudA = (lightA + 0x80) << 8;
	int diffLight = gouraudA - gouraudB;
	
	Fill_CurGouraudMin = gouraudB;
	
	// Calculate slopes using floating point
	float invDY = 1.0f / diffY;
	
	// Gouraud slope
	Fill_Gouraud_LeftSlope = ROUND((float)diffLight * invDY);
	
	// X slope calculation
	float xSlope = (float)diffX * invDY;
	Fill_LeftSlope = ROUND(xSlope + 1.0f);
	
	// Calculate perspective-corrected texture coordinates
	float uA_w = (float)uA * PtA->Pt_W;
	float uB_w = (float)uB * PtB->Pt_W;
	float vA_w = (float)vA * PtA->Pt_W;
	float vB_w = (float)vB * PtB->Pt_W;
	
	// Calculate texture coordinate differences
	float diffU_w = uB_w - uA_w;
	float diffV_w = vB_w - vA_w;
	
	// Calculate texture slopes
	Fill_MapU_LeftSlope = ROUND(diffU_w * invDY);
	Fill_MapV_LeftSlope = ROUND(diffV_w * invDY);
	
	// Set current texture minimums (perspective corrected)
	Fill_CurMapUMin = ROUND(uA_w / 256.0f);
	Fill_CurMapVMin = ROUND(vA_w / 256.0f);
	
	// Calculate W slope for perspective correction
	float diffW = PtB->Pt_W - PtA->Pt_W;
	Fill_W_LeftSlope = ROUND(diffW * invDY * 256.0f);
	
	return 0;
}

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ Calculate the LeftSlope for ZBuffer                                      ║
// ║                                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
void Calc_ZBufLeftSlopeFPU(S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  float diffZ = (PtB->Pt_ZO - PtA->Pt_ZO) << 8;
  Fill_CurZBufMin = PtA->Pt_ZO << 8;
  Fill_ZBuf_LeftSlope = ROUND(diffZ / diffY);
}

S32 Calc_LeftSlopeZBufferFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  Calc_ZBufLeftSlopeFPU(diffX, diffY, PtA, PtB);
  return Jmp_LeftSlopeFPU[fillType](fillType, diffX, diffY, PtA, PtB);
}

S32 Calc_TextureZLeftSlopeZBufFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
	// Calculate left slopes for texture mapping with perspective correction and Z-buffering
	
	// Extract texture coordinates
	int uA = PtA->Pt_MapU & 0xFFFF;
	int uB = PtB->Pt_MapU & 0xFFFF;
	int vA = (PtA->Pt_MapU >> 16) & 0xFFFF;
	int vB = (PtB->Pt_MapU >> 16) & 0xFFFF;
	
#ifdef CORRECT_TEXTURE
	uA += 0x80;
	uB += 0x80;
	vA += 0x80;
	vB += 0x80;
#endif
	
	// Set current W minimum
	Fill_CurWMin = PtB->Pt_W;
	
	// Extract Z values from high 16 bits of Pt_Light
	int zA = (PtA->Pt_Light >> 16) & 0xFFFF;
	int zB = (PtB->Pt_Light >> 16) & 0xFFFF;
	
	// Calculate perspective-corrected Z coordinates
	float zA_w = (float)zA * PtA->Pt_W / 256.0f;
	float zB_w = (float)zB * PtB->Pt_W / 256.0f;
	
	// Set current Z-buffer minimum and calculate Z slope
	Fill_CurZBufMin = ROUND(zA_w);
	float diffZ_w = zB_w - zA_w;
	Fill_ZBuf_LeftSlope = ROUND(diffZ_w / diffY);
	
	// Calculate slopes using floating point
	float invDY = 1.0f / diffY;
	
	// X slope calculation
	float xSlope = (float)diffX * invDY;
	Fill_LeftSlope = ROUND(xSlope + 1.0f);
	
	// Calculate perspective-corrected texture coordinates
	float uA_w = (float)uA * PtA->Pt_W;
	float uB_w = (float)uB * PtB->Pt_W;
	float vA_w = (float)vA * PtA->Pt_W;
	float vB_w = (float)vB * PtB->Pt_W;
	
	// Calculate texture coordinate differences
	float diffU_w = uB_w - uA_w;
	float diffV_w = vB_w - vA_w;
	
	// Calculate texture slopes
	Fill_MapU_LeftSlope = ROUND(diffU_w * invDY / 256.0f);
	Fill_MapV_LeftSlope = ROUND(diffV_w * invDY / 256.0f);
	
	// Set current texture minimums (perspective corrected)
	Fill_CurMapUMin = ROUND(uA_w / 256.0f);
	Fill_CurMapVMin = ROUND(vA_w / 256.0f);
	
	// Calculate W slope for perspective correction
	float diffW = PtB->Pt_W - PtA->Pt_W;
	Fill_W_LeftSlope = ROUND(diffW * invDY * 256.0f);
	
	return 0;
}

S32 Calc_LeftSlopeZBufferFlagFPU(U32 fillType, S32 diffX, S32 diffY, Struc_Point *PtA, Struc_Point *PtB)
{
  Calc_ZBufLeftSlopeFPU(diffX, diffY, PtA, PtB);
  return Jmp_LeftSlopeFPU[0](fillType, diffX, diffY, PtA, PtB);
}

Jmp_XSlopeFunc Jmp_XSlopeFPU[] = {
	Triangle_ReadNextEdgeFPU, // Solid
	Triangle_ReadNextEdgeFPU, // Flat
	Triangle_ReadNextEdgeFPU, // Transp.
	Triangle_ReadNextEdgeFPU, // Trame
	Calc_GouraudXSlopeFPU, // Gouraud
	Calc_DitherXSlopeFPU, // Dither
	Calc_GouraudXSlopeFPU, // Gouraud table
	Calc_DitherXSlopeFPU, // Dither table
	Calc_TextureXSlopeFPU, // Texture Solid
	Calc_TextureXSlopeFPU, // Texture Flat
	Calc_TextureGouraudXSlopeFPU, // Texture Gouraud
	Calc_TextureDitherXSlopeFPU, // Texture Gouraud Dither
	Calc_TextureXSlopeFPU, // Texture Solid Chroma Key
	Calc_TextureXSlopeFPU, // Texture Flat Chroma Key
	Calc_TextureGouraudXSlopeFPU, // Texture Gouraud Chroma Key
	Calc_TextureDitherXSlopeFPU, // Texture Gouraud Dither Chroma Key
	Calc_TextureZXSlopeFPU, // Texture Z Solid
	Calc_TextureZXSlopeFPU, // Texture Z Flat
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Gouraud
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Dither
	Calc_TextureZXSlopeFPU, // Texture Z Solid Chroma Key
	Calc_TextureZXSlopeFPU, // Texture Z Flat Chroma Key
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Gouraud Chroma Key
	Calc_TextureZGouraudXSlopeFPU, // Texture Z Dither Chroma Key
	Calc_TextureZXSlopeZBufFPU, // Texture Z Solid
	Calc_XSlopeZBufferFlagFPU, // ZBuffer only
};

Jmp_XSlopeFunc Jmp_XSlopeZBufFPU[] = {
  Calc_XSlopeZBufferFPU, // Solid
	Calc_XSlopeZBufferFPU, // Flat
	Calc_XSlopeZBufferFPU, // Transp.
	Calc_XSlopeZBufferFPU, // Trame
	Calc_XSlopeZBufferFPU, // Gouraud
	Calc_XSlopeZBufferFPU, // Dither
	Calc_XSlopeZBufferFPU, // Gouraud table
	Calc_XSlopeZBufferFPU, // Dither table
	Calc_XSlopeZBufferFPU, // Texture Solid
	Calc_XSlopeZBufferFPU, // Texture Flat
	Calc_XSlopeZBufferFPU, // Texture Gouraud
	Calc_XSlopeZBufferFPU, // Texture Gouraud Dither
	Calc_XSlopeZBufferFPU, // Texture Solid Chroma Key
	Calc_XSlopeZBufferFPU, // Texture Flat Chroma Key
	Calc_XSlopeZBufferFPU, // Texture Gouraud Chroma Key
	Calc_XSlopeZBufferFPU, // Texture Gouraud Dither Chroma Key
	Calc_XSlopeZBufferFPU, // Texture Z Solid
	Calc_XSlopeZBufferFPU, // Texture Z Flat
	Calc_XSlopeZBufferFPU, // Texture Z Gouraud
	Calc_XSlopeZBufferFPU, // Texture Z Dither
	Calc_XSlopeZBufferFPU, // Texture Z Solid Chroma Key
	Calc_XSlopeZBufferFPU, // Texture Z Flat Chroma Key
	Calc_XSlopeZBufferFPU, // Texture Z Gouraud Chroma Key
	Calc_XSlopeZBufferFPU, // Texture Z Dither Chroma Key
	Calc_TextureZXSlopeZBufFPU, // Texture Z Solid
	Calc_XSlopeZBufferFlagFPU, // ZBuffer only
};

Jmp_LeftSlopeFunc Jmp_LeftSlopeFPU[] = {
	Read_Next_RightFPU, // Solid
	Read_Next_RightFPU, // Flat
	Read_Next_RightFPU, // Transp.
	Read_Next_RightFPU, // Trame
	Calc_GouraudLeftSlopeFPU, // Gouraud
	Calc_GouraudLeftSlopeFPU, // Dither
	Calc_GouraudTableLeftSlopeFPU, // Gouraud table
	Calc_GouraudTableLeftSlopeFPU, // Dither table
	Calc_TextureLeftSlopeFPU, // Texture Solid
	Calc_TextureLeftSlopeFPU, // Texture Flat
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud Dither
	Calc_TextureLeftSlopeFPU, // Texture Solid Chroma Key
	Calc_TextureLeftSlopeFPU, // Texture Flat Chroma Key
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud Chroma Key
	Calc_TextureGouraudLeftSlopeFPU, // Texture Gouraud Dither Chroma Key
	Calc_TextureZLeftSlopeFPU, // Texture Z Solid
	Calc_TextureZLeftSlopeFPU, // Texture Z Flat
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Gouraud
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Dither
	Calc_TextureZLeftSlopeFPU, // Texture Z Solid Chroma Key
	Calc_TextureZLeftSlopeFPU, // Texture Z Flat Chroma Key
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Gouraud Chroma Key
	Calc_TextureZGouraudLeftSlopeFPU, // Texture Z Dither Chroma Key
	Calc_TextureZLeftSlopeZBufFPU, // Texture Z Solid
	Calc_LeftSlopeZBufferFlagFPU, // ZBuffer only
};

Jmp_LeftSlopeFunc Jmp_LeftSlopeZBufFPU[] = {
  Calc_LeftSlopeZBufferFPU, // Solid
  Calc_LeftSlopeZBufferFPU, // Flat
  Calc_LeftSlopeZBufferFPU, // Transp.
  Calc_LeftSlopeZBufferFPU, // Trame
  Calc_LeftSlopeZBufferFPU, // Gouraud
  Calc_LeftSlopeZBufferFPU, // Dither
  Calc_LeftSlopeZBufferFPU, // Gouraud table
  Calc_LeftSlopeZBufferFPU, // Dither table
  Calc_LeftSlopeZBufferFPU, // Texture Solid
  Calc_LeftSlopeZBufferFPU, // Texture Flat
  Calc_LeftSlopeZBufferFPU, // Texture Gouraud
  Calc_LeftSlopeZBufferFPU, // Texture Gouraud Dither
  Calc_LeftSlopeZBufferFPU, // Texture Solid Chroma Key
  Calc_LeftSlopeZBufferFPU, // Texture Flat Chroma Key
  Calc_LeftSlopeZBufferFPU, // Texture Gouraud Chroma Key
  Calc_LeftSlopeZBufferFPU, // Texture Gouraud Dither Chroma Key
  Calc_LeftSlopeZBufferFPU, // Texture Z Solid
  Calc_LeftSlopeZBufferFPU, // Texture Z Flat
  Calc_LeftSlopeZBufferFPU, // Texture Z Gouraud
  Calc_LeftSlopeZBufferFPU, // Texture Z Dither
  Calc_LeftSlopeZBufferFPU, // Texture Z Solid Chroma Key
  Calc_LeftSlopeZBufferFPU, // Texture Z Flat Chroma Key
  Calc_LeftSlopeZBufferFPU, // Texture Z Gouraud Chroma Key
  Calc_LeftSlopeZBufferFPU, // Texture Z Dither Chroma Key
  Calc_TextureZLeftSlopeZBufFPU, // Texture Z Solid
  Calc_LeftSlopeZBufferFlagFPU, // ZBuffer only
};

Jmp_XSlopeFunc *Current_Jmp_XSlope = Jmp_XSlopeFPU;
Jmp_LeftSlopeFunc *Current_Jmp_LeftSlope = Jmp_LeftSlopeFPU;

// =============================================================================
#ifdef __cplusplus
}
#endif
