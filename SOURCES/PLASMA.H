#ifdef __cplusplus
extern "C" {
#endif

#ifndef	PLASMA_H
#define	PLASMA_H

#include <SYSTEM/ADELINE_TYPES.H>

typedef struct  {

        S16     *TabVirgule ;
        S16     *TabSpeed   ;
        S32     *TabAcc     ;
        U8      *TabColors  ;

        U8      *TexOffset  ;           // devenu ptr sur effet dans tex

        U8      Interleave ;
        U8      NbActivePoints ;
        U8      NbColors ;
        U8      Speed ;

        U8      data_start ;

        }       T_PLASMA ;

void Do_Plasma( T_PLASMA *PtrEffectStruct );

#endif

#ifdef __cplusplus
}
#endif
