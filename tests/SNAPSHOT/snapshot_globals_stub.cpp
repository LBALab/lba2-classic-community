/*
 * snapshot_globals_stub.cpp — Stub global variable definitions
 *
 * For replay_snapshot_asm: defines all the rendering engine global variables
 * that are referenced by the ASM library but normally defined in CPP source
 * files. This avoids linking full CPP source files which bring in unwanted
 * function implementations and SDL dependencies.
 */

#include <SYSTEM/ADELINE_TYPES.H>
#include <3D/CAMERA.H>
#include <3D/LIGHT.H>
#include <3D/DATAMAT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>
#include <SVGA/CLIP.H>
#include <POLYGON/POLY.H>
#include <OBJECT/AFF_OBJ.H>

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Camera globals (normally in LIB386/3D/CAMERA.CPP)                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

S32 CameraX = 0, CameraY = 0, CameraZ = 0;
S32 CameraAlpha = 0, CameraBeta = 0, CameraGamma = 0;
S32 CameraXr = 0, CameraYr = 0, CameraZr = 0;
S32 CameraZrClip = 0;
S32 NearClip = 0;
S32 Xp = 0, Yp = 0;
S32 X0 = 0, Y0 = 0, Z0 = 0;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Projection globals (normally in LIB386/3D/PROJ.CPP)                       */
/* ═══════════════════════════════════════════════════════════════════════════ */

S32 TypeProj = 0;
S32 XCentre = 0, YCentre = 0;
float FRatioX = 0.0f, FRatioY = 0.0f;
S32 LFactorX = 0, LFactorY = 0;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Light globals (normally in LIB386/3D/LIGHT.CPP)                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

S32 AlphaLight = 0, BetaLight = 0, GammaLight = 0;
S32 NormalXLight = 0, NormalYLight = 0, NormalZLight = 0;
S32 CameraXLight = 0, CameraYLight = 0, CameraZLight = 0;
S32 PosXLight = 0, PosYLight = 0, PosZLight = 0;
S32 TargetXLight = 0, TargetYLight = 0, TargetZLight = 0;
S32 LightNormalUnit = 15360;
float FactorLight = 1.0f / 65536.0f;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Matrix globals (normally in LIB386/3D/DATAMAT.CPP)                        */
/* ═══════════════════════════════════════════════════════════════════════════ */

TYPE_MAT MatriceWorld = {};
TYPE_MAT MatrixLib1 = {};
TYPE_MAT MatrixLib2 = {};
TYPE_MAT MatriceRot = {};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Screen globals (normally in LIB386/SVGA/SCREEN.CPP)                       */
/* ═══════════════════════════════════════════════════════════════════════════ */

void *Log = 0;
void *Screen = 0;
U32 ModeDesiredX = 0, ModeDesiredY = 0;
U32 TabOffLine[1024] = {};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  ScreenXY globals (normally in LIB386/SVGA/SCREENXY.CPP)                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

S32 ScreenXMin = 0, ScreenYMin = 0;
S32 ScreenXMax = 0, ScreenYMax = 0;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Clip globals (normally in LIB386/SVGA/CLIP.CPP)                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

S32 ClipXMin = 0, ClipYMin = 0;
S32 ClipXMax = 0, ClipYMax = 0;
S32 MemoClipXMin = 0, MemoClipYMin = 0;
S32 MemoClipXMax = 0, MemoClipYMax = 0;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Polygon globals (normally in LIB386/pol_work/POLY.CPP)                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

U32 ScreenPitch = 0;
PTR_U32 PTR_TabOffLine = 0;
PTR_U16 PtrZBuffer = 0;
PTR_U8 PtrCLUTGouraud = 0;
PTR_U8 PtrCLUTFog = 0;
PTR_U8 PtrTruePal = 0;
PTR_U8 PtrMap = 0;
S32 RepMask = 0;
S32 Fill_Z_Fog_Near = 0, Fill_Z_Fog_Far = 0;
U32 Fill_ZBuffer_Factor = 0;
U8 Fill_Flag_ZBuffer = 0, Fill_Flag_Fog = 0, Fill_Flag_NZW = 0;
U8 Fill_Logical_Palette[256] = {};
T_FILL_COLOR Fill_Color = {};
U32 IsPolygonHidden = 0;

/* Fill internal working variables — only define those NOT declared in headers */
S32 Fill_Cur_W = 0, Fill_Next_W = 0;
S32 Fill_Cur_MapU = 0, Fill_Next_MapU = 0;
S32 Fill_Cur_MapV = 0, Fill_Next_MapV = 0;
S32 Fill_Cur_MapUOverW = 0, Fill_Next_MapUOverW = 0;
S32 Fill_Cur_MapVOverW = 0, Fill_Next_MapVOverW = 0;
S32 Fill_W_XSlope = 0;

/* Fill scanline arrays */
S32 Fill_Left_YMin = 0, Fill_Left_YMax = 0;
S32 Fill_Right_YMin = 0, Fill_Right_YMax = 0;
S32 Fill_Left_TabX[1024] = {};
S32 Fill_Left_TabLight[1024] = {};
S32 Fill_Left_TabUOverW[1024] = {};
S32 Fill_Left_TabVOverW[1024] = {};
S32 Fill_Left_TabW[1024] = {};
S32 Fill_Left_TabZBuf[1024] = {};
S32 Fill_Right_TabX[1024] = {};
S32 Fill_Right_TabLight[1024] = {};
S32 Fill_Right_TabUOverW[1024] = {};
S32 Fill_Right_TabVOverW[1024] = {};
S32 Fill_Right_TabW[1024] = {};
S32 Fill_Right_TabZBuf[1024] = {};

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Object globals (normally in LIB386/OBJECT/AFF_OBJ.CPP)                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

U8 *ObjPtrMap = 0;
Func_TransNumPtr *TransFctBody = 0;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Misc stubs for functions referenced by ASM but not needed for replay      */
/* ═══════════════════════════════════════════════════════════════════════════ */

