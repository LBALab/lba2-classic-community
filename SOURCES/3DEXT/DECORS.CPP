#include "DEFINES.H"

#include <SYSTEM/LOG.H>
#include <3D/DECORVBO.H>
#include <3D/IMATSTD.H>
#include <3D/ROT3D.H>

#include <GL/RENDER_GL.H> /* GL_ArrayWindowBegin/End */

#include <SDL3/SDL.h> /* SDL_GetPerformanceCounter (warm-up budget) */

/* Eye-1 decor reuse: the pass runs once per loaded cube (the current cube
   plus up to eight horizon cubes), so keep one screen-list snapshot per cube
   for the current stereo pair.  The pair sequence keeps a snapshot from being
   reused by a later frame. */
#define DECOR_CACHE_SLOTS 9
typedef struct {
    S32 x, y;
    S32 screenCount;
    S32 decorCount;
    U32 seq;
} T_DECOR_CACHE_KEY;
static T_LIST_TRI s_decorCacheList[DECOR_CACHE_SLOTS][MAX_OBJ_DECORS];
static T_DECOR_CACHE_KEY s_decorCache[DECOR_CACHE_SLOTS];

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

/* Warm-up: a decor whose body has never been drawn still costs a body
   decode and a VBO build in the frame it first becomes visible.  With a
   tracked view that frame arrives when the player turns, so the cost lands
   as a visible hitch instead of being spread over the idle frames before
   it.  Cubes enqueue their decor list when LoadCube() first sees it; the
   step below turns that list into cached bodies/VBOs a few at a time. */
#define GPU_DECOR_WARM_CUBES 32
#define GPU_DECOR_WARM_QUEUE 512
#define GPU_DECOR_WARM_PER_FRAME 4
#define GPU_DECOR_WARM_BUDGET_US 500

typedef struct {
    S32 x, y;
    const void *list;
} T_GPU_DECOR_WARM_CUBE;

typedef struct {
    U16 body;
    U16 beta;
} T_GPU_DECOR_WARM_ITEM;

typedef struct {
    U32 vbo;
    U32 vertexCount;
    const void *map;
    U16 mask;
} T_GPU_DECOR_GROUP;

typedef struct {
    const void *body;
    /* The HQR index is the stable key: HQR_Get() may evict a block and
       reuse its address for a different body, while an index keeps its
       meaning for the whole island. */
    S32 bodyIndex;
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

static T_GPU_DECOR_WARM_CUBE s_gpuDecorWarmCubes[GPU_DECOR_WARM_CUBES];
static T_GPU_DECOR_WARM_ITEM s_gpuDecorWarmQueue[GPU_DECOR_WARM_QUEUE];
static U32 s_gpuDecorWarmHead = 0;
static U32 s_gpuDecorWarmTail = 0;
static U32 s_gpuDecorWarmCount = 0;
static U32 s_gpuDecorWarmFrame = 0;
static S32 s_gpuDecorWarmFrameValid = 0;
static U32 s_gpuDecorColdBuilds = 0;
static U32 s_gpuDecorWarmBuilds = 0;

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
    e->bodyIndex = -1;
    e->epoch = 0;
    e->lastUse = 0;
    e->valid = 0;
    e->eligible = 0;
}

static void GPU_DecorWarmReset(void) {
    for (S32 i = 0; i < GPU_DECOR_WARM_CUBES; i++) {
        s_gpuDecorWarmCubes[i].x = -1;
        s_gpuDecorWarmCubes[i].y = -1;
        s_gpuDecorWarmCubes[i].list = NULL;
    }
    s_gpuDecorWarmHead = 0;
    s_gpuDecorWarmTail = 0;
    s_gpuDecorWarmCount = 0;
    s_gpuDecorWarmFrameValid = 0;
}

void GPU_Decor_InvalidateAll(void) {
    U32 epoch = GL_DecorVBOEpoch();
    for (S32 i = 0; i < GPU_DECOR_CACHE_SIZE; i++) {
        GPU_DecorDrop(&s_gpuDecor[i], epoch);
    }
    GPU_DecorWarmReset();
}

static T_GPU_DECOR *GPU_DecorFind(S32 bodyIndex, U16 beta, U32 epoch) {
    for (S32 i = 0; i < GPU_DECOR_CACHE_SIZE; i++) {
        T_GPU_DECOR *e = &s_gpuDecor[i];
        if (!e->valid) {
            continue;
        }
        if (e->epoch != epoch) {
            GPU_DecorDrop(e, epoch);
            continue;
        }
        if (e->bodyIndex == bodyIndex && e->beta == beta) {
            return e;
        }
    }
    return NULL;
}

static T_GPU_DECOR *GPU_DecorAlloc(const void *body, S32 bodyIndex, U16 beta,
                                   U32 epoch) {
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
    e->bodyIndex = bodyIndex;
    e->beta = beta;
    e->epoch = epoch;
    e->lastUse = ++s_gpuDecorUseSeq;
    e->valid = 1;
    return e;
}

static S32 GPU_DecorBuild(T_GPU_DECOR *e, S32 warm) {
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
    if (warm) {
        s_gpuDecorWarmBuilds++;
    } else {
        s_gpuDecorColdBuilds++;
    }
    return 1;
}

/* Enqueue every decor in a cube's list.  LoadCube() calls this the first
   time it sees a list, including for horizon cubes that are currently
   off-screen, so a turn later does not have to decode their bodies and
   build their VBOs in the frame they appear. */
void GPU_DecorWarmCube(S32 x, S32 y, const void *list, S32 count) {
    if (!GL_DecorGPUActive() || list == NULL || count <= 0) {
        return;
    }

    static S32 s_disabled = -1;
    if (s_disabled < 0) {
        s_disabled = getenv("LBA2_NO_DECOR_WARM") ? 1 : 0;
    }
    if (s_disabled) {
        return;
    }

    S32 slot = -1;
    for (S32 i = 0; i < GPU_DECOR_WARM_CUBES; i++) {
        T_GPU_DECOR_WARM_CUBE *c = &s_gpuDecorWarmCubes[i];
        if (c->list == list && c->x == x && c->y == y) {
            return;
        }
        if (slot < 0 && c->list == NULL) {
            slot = i;
        }
    }
    if (slot < 0) {
        /* Keep the most recently seen cubes; a cube that drops out is
           re-enqueued if it returns with a different list. */
        slot = (S32)((U32)y * 16u + (U32)x) % GPU_DECOR_WARM_CUBES;
    }
    s_gpuDecorWarmCubes[slot].x = x;
    s_gpuDecorWarmCubes[slot].y = y;
    s_gpuDecorWarmCubes[slot].list = list;

    const T_DECORS *decors = (const T_DECORS *)list;
    for (S32 i = 0; i < count; i++) {
        const U16 body = (U16)(decors[i].Body & 0xFFFF);
        const U16 beta = (U16)(decors[i].Beta & 0xFFFF);

        S32 queued = 0;
        for (U32 q = 0; q < s_gpuDecorWarmCount; q++) {
            const U32 idx =
                (s_gpuDecorWarmHead + q) % GPU_DECOR_WARM_QUEUE;
            if (s_gpuDecorWarmQueue[idx].body == body &&
                s_gpuDecorWarmQueue[idx].beta == beta) {
                queued = 1;
                break;
            }
        }
        if (queued) {
            continue;
        }
        if (s_gpuDecorWarmCount >= GPU_DECOR_WARM_QUEUE) {
            break;
        }
        s_gpuDecorWarmQueue[s_gpuDecorWarmTail].body = body;
        s_gpuDecorWarmQueue[s_gpuDecorWarmTail].beta = beta;
        s_gpuDecorWarmTail = (s_gpuDecorWarmTail + 1) % GPU_DECOR_WARM_QUEUE;
        s_gpuDecorWarmCount++;
    }
}

/* Turn queued decors into cached bodies and VBOs inside a small per-frame
   budget.  Visible decors still build on demand; this only moves the work
   for objects that are not on screen yet off the frame they appear. */
void GPU_DecorWarmStep(void) {
    if (!GL_DecorGPUActive() || s_gpuDecorWarmCount == 0) {
        return;
    }

    const U32 frame = GL_GetFrameSeq();
    if (s_gpuDecorWarmFrameValid && s_gpuDecorWarmFrame == frame) {
        return;
    }
    s_gpuDecorWarmFrameValid = 1;
    s_gpuDecorWarmFrame = frame;

    const U32 epoch = GL_DecorVBOEpoch();
    const U64 budget = (SDL_GetPerformanceFrequency() * GPU_DECOR_WARM_BUDGET_US) / 1000000ULL;
    const U64 start = SDL_GetPerformanceCounter();
    U32 built = 0;

    while (s_gpuDecorWarmCount > 0 && built < GPU_DECOR_WARM_PER_FRAME) {
        const T_GPU_DECOR_WARM_ITEM item =
            s_gpuDecorWarmQueue[s_gpuDecorWarmHead];
        s_gpuDecorWarmHead = (s_gpuDecorWarmHead + 1) % GPU_DECOR_WARM_QUEUE;
        s_gpuDecorWarmCount--;

        const void *body = HQR_Get(HQR_Isle_Obj, item.body);
        if (body == NULL) {
            continue;
        }
        if (GPU_DecorFind(item.body, item.beta, epoch) != NULL) {
            continue;
        }
        T_GPU_DECOR *e = GPU_DecorAlloc(body, item.body, item.beta, epoch);
        if (e == NULL) {
            continue;
        }
        GPU_DecorBuild(e, 1);
        built++;
        if (SDL_GetPerformanceCounter() - start >= budget) {
            break;
        }
    }
}

/* Draw one static decor.  Returns 1 when the VBO handled it, 0 to let the
   caller use the per-frame CPU filler (unsupported/transparent body). */
static S32 GPU_DecorDraw(T_DECORS *ptrobj, S32 fogRow) {
    if (!GL_DecorGPUActive()) {
        return 0;
    }
    const S32 bodyIndex = ptrobj->Body & 0xFFFF;
    const void *body = HQR_Get(HQR_Isle_Obj, bodyIndex);
    const U16 beta = (U16)(ptrobj->Beta & 0xFFFF);
    const U32 epoch = GL_DecorVBOEpoch();
    T_GPU_DECOR *e = GPU_DecorFind(bodyIndex, beta, epoch);
    if (e == NULL) {
        e = GPU_DecorAlloc(body, bodyIndex, beta, epoch);
        if (e == NULL) {
            return 0;
        }
        GPU_DecorBuild(e, 0);
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

    ObjPtrMap = ObjTexture; // texture obj

#ifdef USE_GPURENDERER
    /* The VBO survives a cube change: it is a copy of the body in model
       space, keyed by HQR index, and the island HQR is what a cube change
       can replace (LoadIsland/InitSeaCube invalidate).  Warming a few
       queued decors here keeps the frames before they become visible
       cheap. */
    if (GL_GetStereoEye() <= 0) {
        GPU_DecorWarmStep();
    }
#endif

#ifdef USE_GPURENDERER
    /* Decor polys bin into the shared texture array (see RENDER_GL.CPP):
       the pass interleaves per-offset texture pages that would otherwise
       flush the batch once per distinct offset. */
    GL_ArrayWindowBegin();
    GL_DecorBatchBegin();
#endif

    /* Eye 1 is render-only and its camera is only the IPD away from eye 0.
       Reuse eye 0's decor screen list/sort instead of re-projecting and
       qsorting the same cube; the lateral reject already keeps generous
       frame margins, so the IPD shift cannot hide a decor.  The pass runs
       once per loaded cube (current plus horizon), so the snapshot is keyed
       by cube and by the stereo pair. */
    const S32 stereoEye = GL_GetStereoEye();
    const U32 decorPairSeq = GL_GetStereoFrameSeq();
    S32 decorCubeX, decorCubeY;
    GPU_TerrainDecorCube(&decorCubeX, &decorCubeY);
#ifdef USE_GPURENDERER
    /* Belt-and-braces for a cube LoadCube() saw before the decor program
       existed: this pass knows the exact cube these globals belong to. */
    if (stereoEye <= 0) {
        GPU_DecorWarmCube(decorCubeX, decorCubeY, ListDecors, NbObjDecors);
    }
#endif
    S32 reuseDecorList = 0;
    if (stereoEye == 1 && decorCubeX >= 0 && decorCubeX < 16 &&
        decorCubeY >= 0 && decorCubeY < 16) {
        for (S32 c = 0; c < DECOR_CACHE_SLOTS; c++) {
            if (s_decorCache[c].seq == decorPairSeq &&
                s_decorCache[c].x == decorCubeX &&
                s_decorCache[c].y == decorCubeY &&
                s_decorCache[c].decorCount == NbObjDecors) {
                memcpy(ListTriExt, s_decorCacheList[c],
                       (size_t)s_decorCache[c].screenCount * sizeof(T_LIST_TRI));
                NbObjScreen = s_decorCache[c].screenCount;
                reuseDecorList = 1;
                break;
            }
        }
    }

    ptrtri = ListTriExt;
    ptrobj = ListDecors;

    // projection des centres
    S32 culled4 = 0;
#ifdef USE_GPURENDERER
    /* Same union bound as the object list: eye 1 can see a decor whose
       eye-0 projection sits just outside the lateral margin. */
    S32 cullMarginX = 0;
    S32 cullMarginY = 0;
    GL_GetSinglePassCullMargin(&cullMarginX, &cullMarginY);
#endif
    if (!reuseDecorList) {
        NbObjScreen = 0;
    }
    for (i = 0; i < NbObjDecors && !reuseDecorList; i++, ptrobj++) {
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
#ifdef USE_GPURENDERER
            const S32 viewX0 = -50 - cullMarginX;
            const S32 viewY0 = -30 - cullMarginY;
            const S32 viewX1 = (S32)ModeDesiredX + 40 + cullMarginX;
            const S32 viewY1 = (S32)ModeDesiredY + 100 + cullMarginY;
#else
            const S32 viewX0 = -50, viewY0 = -30;
            const S32 viewX1 = (S32)ModeDesiredX + 40;
            const S32 viewY1 = (S32)ModeDesiredY + 100;
#endif
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

    // trie les objet en z 'front to back' (eye 0 only; eye 1 reuses it)
    if (!reuseDecorList) {
        if (NbObjScreen)
            qsort(ListTriExt, NbObjScreen, sizeof(T_LIST_TRI),
                  sub_qsort_decors_zbuf);
    }

    /* Snapshot the sorted list for eye 1, one slot per loaded cube. */
    if (stereoEye == 0 && decorCubeX >= 0 && decorCubeX < 16 &&
        decorCubeY >= 0 && decorCubeY < 16) {
        S32 slot = -1;
        for (S32 c = 0; c < DECOR_CACHE_SLOTS; c++) {
            if (s_decorCache[c].seq == decorPairSeq &&
                s_decorCache[c].x == decorCubeX &&
                s_decorCache[c].y == decorCubeY) {
                slot = c;
                break;
            }
            if (slot < 0 && s_decorCache[c].seq != decorPairSeq)
                slot = c;
        }
        if (slot < 0)
            slot = 0;
        memcpy(s_decorCacheList[slot], ListTriExt,
               (size_t)NbObjScreen * sizeof(T_LIST_TRI));
        s_decorCache[slot].x = decorCubeX;
        s_decorCache[slot].y = decorCubeY;
        s_decorCache[slot].screenCount = NbObjScreen;
        s_decorCache[slot].decorCount = NbObjDecors;
        s_decorCache[slot].seq = decorPairSeq;
    }

    if (!NbObjScreen) {
#ifdef USE_GPURENDERER
        /* The decor window is also the queue interval for GL_AddTriangle.
           Leaving it open on a pass with no visible decor makes the next
           sea/body polygons enter the decor streams, where they are never
           submitted.  Close the window before the early return. */
        GL_DecorBatchEnd();
        GL_ArrayWindowEnd();
#endif
        return;
    }

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
            Log_Info("DecorsPerf: total=%d screened=%d cullable=%d "
                     "cold=%u warm=%u",
                     (S32)NbObjDecors, (S32)NbObjScreen, culled4,
                     s_gpuDecorColdBuilds, s_gpuDecorWarmBuilds);
            s_gpuDecorColdBuilds = 0;
            s_gpuDecorWarmBuilds = 0;
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
