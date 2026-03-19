#include <3D/ROTVECT.H>

#include <3D/IMATSTD.H>
#include <3D/LROT3D.H>
#include <SYSTEM/DEBUG_TRACES.H>

void RotateVector(S32 norme, S32 alpha, S32 beta, S32 gamma) {
    LIB386_TRACE_CPP("RotateVector", "1", "norme=%d a=%d b=%d g=%d", norme, alpha, beta, gamma);
    InitMatrixStd(&MatrixLib2, alpha, beta, gamma);
    LongRotatePoint(&MatrixLib2, 0, 0, norme);
}
