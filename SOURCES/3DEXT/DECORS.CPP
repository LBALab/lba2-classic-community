#include "DEFINES.H"

#include <SYSTEM/LOG.H>
#include <3D/DECORVBO.H>
#include <3D/IMATSTD.H>
#include <3D/ROT3D.H>

#include <GL/RENDER_GL.H> /* GL_ArrayWindowBegin/End */

/*══════════════════════════════════════════════════════════════════════════*
		      █▀▀▀▄ █▀▀▀▀ █▀▀▀▀ █▀▀▀█ █▀▀▀█ ██▀▀▀
		      ██  █ ██▀▀  ██    ██  █ ██▀█▀ ▀▀▀▀█
		      ▀▀▀▀  ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

#ifdef LBA_EDITOR // Special PERSO.EXE
#define INFO_DECORS 1024
extern S32 FlagInfos;

#define SIZE_NAME 19

typedef struct
{
    char Name[SIZE_NAME + 1];
    S16 Value;
} T_VAR;

extern T_VAR ListVarGame[];
#else
extern S16 ListVarGame[];
#endif

extern S32 FlagMasque;

#define MAX_SOUS_OBJET 20
#define SIZE_BRICK_Y 256

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
// affiche les objets de decors

#ifdef LBA_EDITOR // uniquement PERSO
extern void ClearScreenMinMax();
extern void AdjustScreenMinMax();

S32 NumDecorsDraw;

//****************************************************************************
void DrawCubeNoBox(S32 ox, S32 oy, S32 oz,
                   S32 xw0, S32 yw0, S32 zw0,
                   S32 xw1, S32 yw1, S32 zw1) {
    S32 x0, y0, x1, y1, x2, y2, x3, y3, x4, y4, x5, y5, x6, y6, x7, y7;
    S16 coul = 11;
    S16 coul1 = 13;

    ClearScreenMinMax();
    LongWorldRotatePoint(ox, oy, oz);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    Rect(Xp - 3, Yp - 3, Xp + 3, Yp + 3, coul); // point org

    CoulText(GREEN, -1);
    GraphPrintf(FALSE, Xp, Yp, "%d", NumDecorsDraw);

    LongWorldRotatePoint(xw0, yw0, zw0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x0 = Xp;
    y0 = Yp;
    LongWorldRotatePoint(xw1, yw0, zw0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x1 = Xp;
    y1 = Yp;
    LongWorldRotatePoint(xw1, yw0, zw1);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x2 = Xp;
    y2 = Yp;
    LongWorldRotatePoint(xw0, yw0, zw1);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x3 = Xp;
    y3 = Yp;
    LongWorldRotatePoint(xw0, yw1, zw0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x4 = Xp;
    y4 = Yp;
    LongWorldRotatePoint(xw1, yw1, zw0);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x5 = Xp;
    y5 = Yp;
    LongWorldRotatePoint(xw1, yw1, zw1);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x6 = Xp;
    y6 = Yp;
    LongWorldRotatePoint(xw0, yw1, zw1);
    if (!LongProjectPoint(X0, Y0, Z0))
        return;
    AdjustScreenMinMax();
    x7 = Xp;
    y7 = Yp;

    Line(x0, y0, x1, y1, coul);
    Line(x1, y1, x2, y2, coul);
    Line(x2, y2, x3, y3, coul);
    Line(x3, y3, x0, y0, coul);

    Line(x0, y0, x4, y4, coul1);
    Line(x1, y1, x5, y5, coul1);
    Line(x2, y2, x6, y6, coul1);
    Line(x3, y3, x7, y7, coul1);
    Line(x4, y4, x5, y5, coul1);
    Line(x5, y5, x6, y6, coul1);
    Line(x6, y6, x7, y7, coul1);
    Line(x7, y7, x4, y4, coul1);
}

// Version ZVs absolues
void AffZVObjet(T_DECORS *ptrobj) {
    DrawCubeNoBox(ptrobj->Xworld, ptrobj->Yworld, ptrobj->Zworld,
                  ptrobj->XMin, ptrobj->YMin, ptrobj->ZMin,
                  ptrobj->XMax, ptrobj->YMax, ptrobj->ZMax);
}
#endif

/*══════════════════════════════════════════════════════════════════════════*/
void FixeObjetsDecorsInvisibles(void) {
    S32 i;
    T_DECORS *ptrobj;
    S32 numvar;

    ptrobj = ListDecors;

    for (i = 0; i < NbObjDecors; i++, ptrobj++) {
        ptrobj->Body &= ~(DEC_DRAWN | DEC_INVISIBLE);

        // Regarde si objet visible/invisible
        numvar = ptrobj->Beta >> 16;

        if (numvar) {
#ifdef LBA_EDITOR
            if (((numvar < 0) AND !ListVarGame[-numvar].Value)
                    OR((numvar >= 0) AND ListVarGame[numvar].Value))
#else
            if (((numvar < 0) AND !ListVarGame[-numvar])
                    OR((numvar >= 0) AND ListVarGame[numvar]))
#endif
                ptrobj->Body |= DEC_INVISIBLE;
        }
    }
}

/*══════════════════════════════════════════════════════════════════════════*/
// sous routine de tri temporaire

int sub_qsort_decors(const void *a, const void *b) {
    return (((T_LIST_TRI *)a)->Zrot - ((T_LIST_TRI *)b)->Zrot);
}

/*══════════════════════════════════════════════════════════════════════════*/
// sous routine de tri temporaire

int sub_qsort_decors_zbuf(const void *a, const void *b) {
    return (((T_LIST_TRI *)b)->Zrot - ((T_LIST_TRI *)a)->Zrot);
}

/*══════════════════════════════════════════════════════════════════════════*/

// Version ZVs absolues
int sub_qsort_zv_decors(const void *a, const void *b) {
    T_DECORS *ptrobja;
    T_DECORS *ptrobjb;

    S32 x0a, y0a, z0a, x1a, y1a, z1a;
    S32 x0b, y0b, z0b, x1b, y1b, z1b;

    ptrobja = ((T_LIST_TRI *)a)->PtrDec;
    ptrobjb = ((T_LIST_TRI *)b)->PtrDec;

    x0a = ptrobja->XMin;
    y0a = ptrobja->YMin;
    z0a = ptrobja->ZMin;
    x1a = ptrobja->XMax;
    y1a = ptrobja->YMax;
    z1a = ptrobja->ZMax;

    x0b = ptrobjb->XMin;
    y0b = ptrobjb->YMin;
    z0b = ptrobjb->ZMin;
    x1b = ptrobjb->XMax;
    y1b = ptrobjb->YMax;
    z1b = ptrobjb->ZMax;

    if (x0a >= x1b)
        goto pas_rec_sur_x;
    if (x1a <= x0b)
        goto pas_rec_sur_x;

    if (z0a >= z1b)
        goto pas_rec_sur_z;
    if (z1a <= z0b)
        goto pas_rec_sur_z;

    //	ordre sur y

    // affinement decision sur une des 2 faces paralelle
    if (abs(y0a - CameraY) < abs(y1a - CameraY))
        return (abs(y0b - CameraY) - abs(y0a - CameraY));
    else
        return (abs(y1b - CameraY) - abs(y1a - CameraY));

pas_rec_sur_z:

    //	ordre sur z

    if (abs(z0a - CameraZ) < abs(z1a - CameraZ))
        return (abs(z0b - CameraZ) - abs(z0a - CameraZ));
    else
        return (abs(z1b - CameraZ) - abs(z1a - CameraZ));

pas_rec_sur_x:

    if (z0a >= z1b)
        goto pas_rec_sur_z_2;
    if (z1a <= z0b)
        goto pas_rec_sur_z_2;

    if (abs(x0a - CameraZ) < abs(x1a - CameraZ))
        return (abs(x0b - CameraX) - abs(x0a - CameraX));
    else
        return (abs(x1b - CameraX) - abs(x1a - CameraX));

pas_rec_sur_z_2:

    // plus de methode dispo (obj en collision), utilise zrot

    return (((T_LIST_TRI *)b)->Zrot - ((T_LIST_TRI *)a)->Zrot);
}

/*══════════════════════════════════════════════════════════════════════════*/

#ifdef LBA_EDITOR
void AffichageObjetDecorsZV(void) {
    S32 i;
    T_LIST_TRI *ptrtri;
    T_DECORS *ptrobj;

    // affiche les zv des objets statiques
    ptrtri = ListTriExt;
    for (i = 0; i < NbObjScreen; i++, ptrtri++) {
        ptrobj = ptrtri->PtrDec;

        if (ptrobj->Body & DEC_DRAWN) {
            UnsetClip();
            NumDecorsDraw = ptrtri->Num;
            AffZVObjet(ptrobj);
        }
    }

    UnsetClip();
}
#endif

/*══════════════════════════════════════════════════════════════════════════*
   Static decor VBO cache
 *══════════════════════════════════════════════════════════════════════════*/

#define GPU_DECOR_CACHE_SIZE 64
#define GPU_DECOR_MAX_GROUPS 32

typedef struct {
    U32 vbo;
    U32 vertexCount;
    const void *map;
    U16 mask;
} T_GPU_DECOR_GROUP;

typedef struct {
    const void *body;
    U16 beta;
    U32 epoch;
    U32 lastUse;
    U16 groupCount;
    T_GPU_DECOR_GROUP groups[GPU_DECOR_MAX_GROUPS];
    U8 valid;
    U8 eligible;
} T_GPU_DECOR;

static T_GPU_DECOR s_gpuDecor[GPU_DECOR_CACHE_SIZE];
static U32 s_gpuDecorUseSeq = 0;

static void GPU_DecorClearGroups(T_GPU_DECOR *e) {
    for (U16 i = 0; i < e->groupCount; i++) {
        if (e->groups[i].vbo != 0) {
            GL_DeleteDecorVBO(e->groups[i].vbo);
        }
    }
    e->groupCount = 0;
}

static void GPU_DecorDrop(T_GPU_DECOR *e, U32 epoch) {
    /* A handle from a previous context must not be deleted against the new
       context: names are reused. */
    if (e->epoch == epoch) {
        GPU_DecorClearGroups(e);
    } else {
        e->groupCount = 0;
    }
    e->body = NULL;
    e->epoch = 0;
    e->lastUse = 0;
    e->valid = 0;
    e->eligible = 0;
}

void GPU_Decor_InvalidateAll(void) {
    U32 epoch = GL_DecorVBOEpoch();
    for (S32 i = 0; i < GPU_DECOR_CACHE_SIZE; i++) {
        GPU_DecorDrop(&s_gpuDecor[i], epoch);
    }
}

static T_GPU_DECOR *GPU_DecorFind(const void *body, U16 beta, U32 epoch) {
    for (S32 i = 0; i < GPU_DECOR_CACHE_SIZE; i++) {
        T_GPU_DECOR *e = &s_gpuDecor[i];
        if (!e->valid) {
            continue;
        }
        if (e->epoch != epoch) {
            GPU_DecorDrop(e, epoch);
            continue;
        }
        if (e->body == body && e->beta == beta) {
            return e;
        }
    }
    return NULL;
}

static T_GPU_DECOR *GPU_DecorAlloc(const void *body, U16 beta, U32 epoch) {
    T_GPU_DECOR *e = NULL;
    for (S32 i = 0; i < GPU_DECOR_CACHE_SIZE; i++) {
        if (!s_gpuDecor[i].valid) {
            e = &s_gpuDecor[i];
            break;
        }
    }
    if (e == NULL) {
        e = &s_gpuDecor[0];
        for (S32 i = 1; i < GPU_DECOR_CACHE_SIZE; i++) {
            if (s_gpuDecor[i].lastUse < e->lastUse) {
                e = &s_gpuDecor[i];
            }
        }
    }
    GPU_DecorDrop(e, epoch);
    e->body = body;
    e->beta = beta;
    e->epoch = epoch;
    e->valid = 1;
    return e;
}

static S32 GPU_DecorBuild(T_GPU_DECOR *e) {
    T_DECORVBO_VERT *capture = NULL;
    U32 captureCount = 0;
    if (!DecorVBORecord((void *)e->body, &capture, &captureCount)) {
        return 0;
    }
    T_GL_DecorVertex *all = (T_GL_DecorVertex *)malloc(
        (size_t)captureCount * sizeof(T_GL_DecorVertex));
    U16 *groupOf = (U16 *)malloc((size_t)captureCount * sizeof(U16));
    if (all == NULL || groupOf == NULL) {
        free(all);
        free(groupOf);
        return 0;
    }

    TYPE_MAT mat;
    InitMatrixStd(&mat, 0, e->beta, 0);

    struct {
        const void *map;
        U16 mask;
        U32 count;
    } groups[GPU_DECOR_MAX_GROUPS];
    U32 groupCount = 0;
    S32 failed = 0;

    for (U32 i = 0; i < captureCount; i++) {
        const T_DECORVBO_VERT *in = &capture[i];
        T_GL_DecorVertex *out = &all[i];
        RotatePoint(&mat, in->X, in->Y, in->Z);
        out->X = (float)X0;
        out->Y = (float)Y0;
        out->Z = (float)Z0;
        if (in->Flags & DECORVBO_FLAG_LIGHT) {
            RotatePoint(&mat, in->NX, in->NY, in->NZ);
            out->NX = (float)X0;
            out->NY = (float)Y0;
            out->NZ = (float)Z0;
        } else {
            out->NX = 0.0f;
            out->NY = 0.0f;
            out->NZ = 0.0f;
        }
        out->U = (float)in->U;
        out->V = (float)in->V;

        const U32 type = in->Type;
        U16 matFlags;
        if (type == 0 || type == 1) {
            matFlags = 1; /* flat */
        } else if (type == 4 || type == 5) {
            matFlags = 2; /* gouraud */
        } else if (type == 6 || type == 7) {
            matFlags = 3; /* gouraud table */
        } else if (type == 8 || type == 12) {
            matFlags = 4; /* direct palette texture */
        } else if (type == 9 || type == 10 || type == 11 || type == 13 ||
                   type == 14 || type == 15) {
            matFlags = 0; /* CLUT texture */
        } else {
            failed = 1;
            break;
        }
        if (type >= 8 && (type & 4)) {
            matFlags |= 32; /* INCRUST chroma key */
        }
        U8 flat = (type == 1) ? 1 : ((type == 9 || type == 13) ? 2 : 0);
        U8 colorHi = 0;
        if (type < 8) {
            colorHi = in->Color;
        }
        out->Material = (float)(matFlags | ((U16)colorHi << 8));
        out->Flat = (float)flat;

        const void *map = NULL;
        U16 mask = 0;
        if (in->Flags & DECORVBO_FLAG_TEXTURED) {
            map = in->Map;
            mask = in->Mask;
        }
        U32 g = 0;
        for (; g < groupCount; g++) {
            if (groups[g].map == map && groups[g].mask == mask) {
                break;
            }
        }
        if (g == groupCount) {
            if (groupCount >= GPU_DECOR_MAX_GROUPS) {
                failed = 1;
                break;
            }
            groups[g].map = map;
            groups[g].mask = mask;
            groups[g].count = 0;
            groupCount++;
        }
        groups[g].count++;
        groupOf[i] = (U16)g;
        /* The 2D path carries the page RepMask in the vertex field the array
           path used for its pinned layer. */
        out->Layer = (float)mask;
    }

    if (!failed && captureCount > 0) {
        for (U32 g = 0; g < groupCount && !failed; g++) {
            T_GL_DecorVertex *groupVerts = (T_GL_DecorVertex *)malloc(
                (size_t)groups[g].count * sizeof(T_GL_DecorVertex));
            if (groupVerts == NULL) {
                failed = 1;
                break;
            }
            U32 outIndex = 0;
            for (U32 i = 0; i < captureCount; i++) {
                if (groupOf[i] == g) {
                    groupVerts[outIndex++] = all[i];
                }
            }
            U32 vbo = GL_CreateDecorVBO(groupVerts, outIndex);
            free(groupVerts);
            if (vbo == 0) {
                failed = 1;
                break;
            }
            e->groups[e->groupCount].vbo = vbo;
            e->groups[e->groupCount].vertexCount = outIndex;
            e->groups[e->groupCount].map = groups[g].map;
            e->groups[e->groupCount].mask = groups[g].mask;
            e->groupCount++;
        }
    } else {
        failed = 1;
    }
    free(all);
    free(groupOf);

    if (failed) {
        GPU_DecorClearGroups(e);
        return 0;
    }
    e->eligible = 1;
    return 1;
}

/* Draw one static decor.  Returns 1 when the VBO handled it, 0 to let the
   caller use the per-frame CPU filler (unsupported/transparent body). */
static S32 GPU_DecorDraw(T_DECORS *ptrobj, S32 fogRow) {
    if (!GL_DecorGPUActive()) {
        return 0;
    }
    const void *body = HQR_Get(HQR_Isle_Obj, ptrobj->Body & 0xFFFF);
    const U16 beta = (U16)(ptrobj->Beta & 0xFFFF);
    const U32 epoch = GL_DecorVBOEpoch();
    T_GPU_DECOR *e = GPU_DecorFind(body, beta, epoch);
    if (e == NULL) {
        e = GPU_DecorAlloc(body, beta, epoch);
        if (e == NULL) {
            return 0;
        }
        GPU_DecorBuild(e);
    }
    if (!e->eligible || e->groupCount == 0) {
        return 0;
    }
    e->lastUse = ++s_gpuDecorUseSeq;
    for (U16 g = 0; g < e->groupCount; g++) {
        const T_GPU_DECOR_GROUP *gr = &e->groups[g];
        if (gr->vbo != 0 && gr->vertexCount != 0) {
            GL_DrawDecorVBO(gr->vbo, gr->vertexCount, gr->map, gr->mask,
                            ptrobj->Xworld, ptrobj->Yworld, ptrobj->Zworld,
                            fogRow, ClipZFar);
        }
    }
    return 1;
}

/*══════════════════════════════════════════════════════════════════════════*/

void AffichageObjetDecorsZBuf(void) {
    S32 i;
    T_LIST_TRI *ptrtri;
    T_DECORS *ptrobj;
    S32 clipzstart;
    S32 numvar;
    S32 zr;

    /* The decor VBO cache belongs to the current cube; dropping it on a cube
       change also releases the texture-array layers it pinned. */
    static S32 s_gpuDecorCubeX = 0x7FFFFFFF;
    static S32 s_gpuDecorCubeY = 0x7FFFFFFF;
    if (s_gpuDecorCubeX != CurrentCubeX || s_gpuDecorCubeY != CurrentCubeY) {
        GPU_Decor_InvalidateAll();
        s_gpuDecorCubeX = CurrentCubeX;
        s_gpuDecorCubeY = CurrentCubeY;
    }

    ObjPtrMap = ObjTexture; // texture obj

#ifdef USE_GPURENDERER
    /* Decor polys bin into the shared texture array (see RENDER_GL.CPP):
       the pass interleaves per-offset texture pages that would otherwise
       flush the batch once per distinct offset. */
    GL_ArrayWindowBegin();
    GL_DecorBatchBegin();
#endif

    ptrtri = ListTriExt;
    ptrobj = ListDecors;

    // projection des centres
    NbObjScreen = 0;
    S32 culled4 = 0;
    for (i = 0; i < NbObjDecors; i++, ptrobj++) {
        ptrobj->Body &= ~(DEC_DRAWN | DEC_INVISIBLE);

        // Regarde si objet visible/invisible
        numvar = ptrobj->Beta >> 16;

        if (numvar) {
#ifdef LBA_EDITOR
            if (((numvar < 0) AND !ListVarGame[-numvar].Value)
                    OR((numvar >= 0) AND ListVarGame[numvar].Value))
#else
            if (((numvar < 0) AND !ListVarGame[-numvar])
                    OR((numvar >= 0) AND ListVarGame[numvar]))
#endif
                ptrobj->Body |= DEC_INVISIBLE;
            //			else	ptrobj->Body &= ~(DEC_INVISIBLE) ;
        }

        // SEB le 12/04/96
        if (!(ptrobj->Body & DEC_INVISIBLE)) {
            /* Lateral frustum reject: project the four ZV corners and skip
               decors whose projections all fall beyond one frame edge.  The
               projected corner hull contains every body point, so such a
               decor can never contribute a pixel — its BodyDisplay call was
               pure transform-and-queue cost feeding one throwaway draw call
               per decor texture page.  DEC_DRAWN is still set for skipped
               decors: the flag means "would have rendered" to the shadow
               height logic (AdjustShadowDecors), and a hero on-screen can
               never stand inside a decor whose whole projection is off
               frame.  Corners behind the camera (projection fails) veto the
               reject. */
            static const S32 corners[4][3] = {
                {0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}};
            /* Frame bounds mirror COMMON.H's VIEW_* (3DEXT's DEFINES.H has
               no copy). */
            const S32 viewX0 = -50, viewY0 = -30;
            const S32 viewX1 = (S32)ModeDesiredX + 40;
            const S32 viewY1 = (S32)ModeDesiredY + 100;
            U8 andFlags = 0xFF;
            S32 cornerOK = 1;
            for (S32 ci = 0; ci < 4; ci++) {
                LongWorldRotatePoint(corners[ci][0] ? ptrobj->XMax
                                                    : ptrobj->XMin,
                                     ptrobj->YMin,
                                     corners[ci][2] ? ptrobj->ZMax
                                                    : ptrobj->ZMin);
                zr = CameraZr - Z0;
                if (zr > ClipZFar) {
                    andFlags = 0;
                    continue;
                }
                if (LongProjectPoint(X0, Y0, Z0)) {
                    U8 f = 0;
                    if (Xp < viewX0)
                        f = 1;
                    else if (Xp > viewX1)
                        f = 2;
                    else if (Yp < viewY0)
                        f = 4;
                    else if (Yp > viewY1)
                        f = 8;
                    andFlags &= f;
                } else {
                    cornerOK = 0;
                    andFlags = 0;
                }
            }
            if (cornerOK && andFlags) {
                static S32 s_noDecClip = -1;
                if (s_noDecClip < 0) {
                    s_noDecClip = getenv("LBA2_NO_DECCLIP") ? 1 : 0;
                }
                culled4++;
                if (!s_noDecClip) {
                    ptrobj->Body |= DEC_DRAWN;
                    continue;
                }
            }

            LongWorldRotatePoint(ptrobj->XMin,
                                 ptrobj->YMin,
                                 ptrobj->ZMin);
            zr = CameraZr - Z0;
            if (zr > ClipZFar)
                continue;

            LongWorldRotatePoint(ptrobj->XMin,
                                 ptrobj->YMin,
                                 ptrobj->ZMax);
            zr = CameraZr - Z0;
            if (zr > ClipZFar)
                continue;

            LongWorldRotatePoint(ptrobj->XMax,
                                 ptrobj->YMin,
                                 ptrobj->ZMin);
            zr = CameraZr - Z0;
            if (zr > ClipZFar)
                continue;

            LongWorldRotatePoint(ptrobj->XMax,
                                 ptrobj->YMin,
                                 ptrobj->ZMax);
            zr = CameraZr - Z0;
            if (zr > ClipZFar)
                continue;

            // point chaud obj pour calcul profondeur fog
            LongWorldRotatePoint(ptrobj->Xworld,
                                 ptrobj->Yworld,
                                 ptrobj->Zworld);

            if (LongProjectPoint(X0, Y0, Z0)) {
                // Test preclipping a mettre ici
                //				if( (Xp>VIEW_EXT_X0) AND (Xp<VIEW_EXT_X1)
                //				AND (Yp>VIEW_EXT_Y0) AND (Yp<VIEW_EXT_Y1) )
                {
                    // add dans listtri
                    ptrtri->Zrot = CameraZr - Z0;
                    ptrtri->Num = i;
                    ptrtri->PtrDec = ptrobj;
                    ptrtri++;
                    NbObjScreen++;
                }
            }
        }
    }

    if (!NbObjScreen)
        return;

    // trie les objet en z 'front to back'
    qsort(ListTriExt, NbObjScreen, sizeof(T_LIST_TRI), sub_qsort_decors_zbuf);

    clipzstart = ClipZFar - StartZFog;

    //	Switch_Fillers( FILL_POLY_FOG_ZBUFFER );

    // affiche les objets statiques
    ptrtri = ListTriExt;
    for (i = 0; i < NbObjScreen; i++, ptrtri++) {
        UnsetClip();
        ptrobj = ptrtri->PtrDec;

        S32 fog = PalLevel + 16 * BoundRegleTrois(0, 15, clipzstart,
                                                  ptrtri->Zrot - StartZFog);
        SetCLUT(fog);

        if (GPU_DecorDraw(ptrobj, fog & 0xF0)) {
            ptrobj->Body |= DEC_DRAWN;
            continue;
        }

        if (BodyDisplay(ptrobj->Xworld,
                        ptrobj->Yworld,
                        ptrobj->Zworld,
                        0,
                        (ptrobj->Beta & 0xFFFF), 0,
                        HQR_Get(HQR_Isle_Obj, ptrobj->Body & 0xFFFF))) {
            SetClip(ScreenXMin, ScreenYMin, ScreenXMax, ScreenYMax);
            if (ClipXMin <= ClipXMax
                                AND ClipYMin <= ClipYMax) {
                ptrobj->Body |= DEC_DRAWN;
            }
        }
    }

    //	Switch_Fillers( FILL_POLY_TEXTURES_ON );

    UnsetClip();

#ifdef USE_GPURENDERER
    GL_DecorBatchEnd();
    GL_ArrayWindowEnd();
#endif

    {
        static U32 s_decCensusFrame = 0;
        s_decCensusFrame++;
        if (s_decCensusFrame % 300 == 150) {
            Log_Info("DecorsPerf: total=%d screened=%d cullable=%d",
                     (S32)NbObjDecors, (S32)NbObjScreen, culled4);
        }
    }
}

/*══════════════════════════════════════════════════════════════════════════*
	  █▀▀▀▀ █▀▀▀█ █     █      █    ██▀▀▀  █    █▀▀▀█ ██▄ █ ██▀▀▀
	  ██    ██  █ ██    ██     ██   ▀▀▀▀█  ██   ██  █ ██▀██ ▀▀▀▀█
	  ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀  ▀▀   ▀▀▀▀▀  ▀▀   ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

// Version ZVs absolues précalculées
// on teste par rapport a un seul point
S32 TestZVDecors(S32 xw, S32 yw, S32 zw, T_DECORS *objet) {
    if (objet->Body & DEC_INVISIBLE)
        return FALSE;

    if (objet->XMin > xw)
        return FALSE;
    if (objet->XMax < xw)
        return FALSE;

    if (objet->YMin > yw)
        return FALSE;
    if (objet->YMax < yw)
        return FALSE;

    if (objet->ZMin > zw)
        return FALSE;
    if (objet->ZMax < zw)
        return FALSE;

    return TRUE;
}
