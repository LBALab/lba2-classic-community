#ifndef DECORS_H
#define DECORS_H

/*--------------------------------------------------------------------------*/
extern void FixeObjetsDecorsInvisibles(void);
/*--------------------------------------------------------------------------*/
extern int sub_qsort_decors(const void *a, const void *b);
/*--------------------------------------------------------------------------*/
extern int sub_qsort_decors_zbuf(const void *a, const void *b);
/*--------------------------------------------------------------------------*/
extern void AffichageObjetDecorsZV(void);
/*--------------------------------------------------------------------------*/
extern void AffichageObjetDecorsZBuf(void);
/*--------------------------------------------------------------------------*/
extern S32 TestZVDecors(S32 xw, S32 yw, S32 zw, T_DECORS *objet);

/*--------------------------------------------------------------------------*/
/* Queue a cube's decor list for background body/VBO warm-up.  Called from
   LoadCube() so off-screen horizon cubes are warmed before a turn reveals
   them. */
extern void GPU_DecorWarmCube(S32 x, S32 y, const void *list, S32 count);
/* Build at most a small budget of queued decors this frame. */
extern void GPU_DecorWarmStep(void);

#endif // DECORS_H
