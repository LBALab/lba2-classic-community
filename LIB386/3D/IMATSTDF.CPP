/*
Explication sur la composition de la matrice de rotation:

M(Alpha) =	1   0   0
		0   Ca  -Sa
		0   Sa  Ca

M(Gamma) =	Cg  -Sg 0
		Sg  Cg  0
		0   0   1


M(Beta)  =	Cb  0   Sb
		0   1   0
		-Sb 0   Cb


M(Finale) = {M(Alpha)*M(Gamma)}*M(Beta)
*/

#include <3D/IMATSTD.H>
#include <3D/SINTABF.H>

void InitMatrixStdF(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma)
{
	const float SINA = SinTabF[alpha & 4095];
	const float COSA = CosTabF[alpha & 4095];
	const float SINB = SinTabF[beta & 4095];
	const float COSB = CosTabF[beta & 4095];
	const float SING = SinTabF[gamma & 4095];
	const float COSG = CosTabF[gamma & 4095];

	float CgSb = COSG * SINB;
	float SbSa = SINB * COSA;
	float SgCa = SING * COSA;
	float CgCa = COSG * COSA;
	float SgCaCb = SgCa * COSB;
	float SbSg = SINB * SING * COSG;
	float Ca = COSA;
	float Sa = SINB * SING;
	float CaSb = COSA * SINB;
	float CaCb = Ca * COSB;

	MatDst->F.M11 = COSG * COSB;
	MatDst->F.M12 = -SING;
	MatDst->F.M13 = COSG * SINB;

	MatDst->F.M21 = SING * COSA * COSB + SINB * SINA;
	MatDst->F.M22 = COSA * COSG;
	MatDst->F.M23 = SINB * SING * COSA - COSB * SINA;

	MatDst->F.M31 = SINA * SING * COSB - COSA * SINB;
	MatDst->F.M32 = SINA * COSG;
	MatDst->F.M33 = SINA * SING * SINB + COSA * COSB;

  MatDst->F.TX = 0;
  MatDst->F.TY = 0;
  MatDst->F.TZ = 0;
}

Func_InitMatrix *InitMatrixStd = InitMatrixStdF;
