#include "C_EXTERN.H" // canonical prelude: MAX_OBJETS, NUM_PERSO, T_OBJET, ListObjet, NbObjets, RenderInterp

#include "OBJ_INTERP.H"
#include <3D/RENDERINTERP.H> // Render_LerpCoord (pure core)

// A one-step position jump whose squared length exceeds this is a teleport, not motion: collapse the
// snapshot so the object is drawn at its destination instead of streaked across the gap. Normal
// per-step movement is a few tens of units; teleports / cube changes jump thousands, so 2000 units
// cleanly separates them (a false positive on a very fast fall costs one un-interpolated frame, which
// is harmless; a false negative would streak, which this comfortably avoids).
#define OBJINTERP_TELEPORT_SQ ((long long)2000 * (long long)2000)

typedef struct {
    S32 x, y, z;
} T_INTERP_VEC;

static T_INTERP_VEC s_prev[MAX_OBJETS];  // position after the previous sim step
static T_INTERP_VEC s_cur[MAX_OBJETS];   // position after the latest sim step
static T_INTERP_VEC s_saved[MAX_OBJETS]; // true position saved by Apply, restored by Restore
static U8 s_seeded[MAX_OBJETS];          // 0 until the first snapshot seeds prev = cur = Obj
static U8 s_applied[MAX_OBJETS];         // 1 = Apply overwrote this object this frame

// Phase 2 interpolates the hero only. Later phases replace this with the wider active set (all movers
// plus the side arrays), which is the only line that changes to widen coverage.
static int obj_interp_active(int i) {
    return i == NUM_PERSO;
}

void ObjInterp_Step(void) {
    int i;
    for (i = 0; i < NbObjets; i++) {
        if (!obj_interp_active(i))
            continue;

        S32 x = ListObjet[i].Obj.X;
        S32 y = ListObjet[i].Obj.Y;
        S32 z = ListObjet[i].Obj.Z;

        if (!s_seeded[i]) {
            s_prev[i].x = s_cur[i].x = x;
            s_prev[i].y = s_cur[i].y = y;
            s_prev[i].z = s_cur[i].z = z;
            s_seeded[i] = 1;
            continue;
        }

        long long dx = (long long)(x - s_cur[i].x);
        long long dy = (long long)(y - s_cur[i].y);
        long long dz = (long long)(z - s_cur[i].z);
        if (dx * dx + dy * dy + dz * dz > OBJINTERP_TELEPORT_SQ) {
            // Teleport: no valid previous-to-current interval, so draw at the destination.
            s_prev[i].x = s_cur[i].x = x;
            s_prev[i].y = s_cur[i].y = y;
            s_prev[i].z = s_cur[i].z = z;
        } else {
            s_prev[i] = s_cur[i];
            s_cur[i].x = x;
            s_cur[i].y = y;
            s_cur[i].z = z;
        }
    }
}

void ObjInterp_Apply(U32 num, U32 den, int fullFlip) {
    int i;
    for (i = 0; i < NbObjets; i++)
        s_applied[i] = 0;

    if (!RenderInterp || den == 0 || fullFlip)
        return;

    for (i = 0; i < NbObjets; i++) {
        if (!obj_interp_active(i) || !s_seeded[i])
            continue;

        s_saved[i].x = ListObjet[i].Obj.X;
        s_saved[i].y = ListObjet[i].Obj.Y;
        s_saved[i].z = ListObjet[i].Obj.Z;

        ListObjet[i].Obj.X = Render_LerpCoord(s_prev[i].x, s_cur[i].x, num, den);
        ListObjet[i].Obj.Y = Render_LerpCoord(s_prev[i].y, s_cur[i].y, num, den);
        ListObjet[i].Obj.Z = Render_LerpCoord(s_prev[i].z, s_cur[i].z, num, den);
        s_applied[i] = 1;
    }
}

void ObjInterp_Restore(void) {
    int i;
    for (i = 0; i < NbObjets; i++) {
        if (!s_applied[i])
            continue;
        ListObjet[i].Obj.X = s_saved[i].x;
        ListObjet[i].Obj.Y = s_saved[i].y;
        ListObjet[i].Obj.Z = s_saved[i].z;
        s_applied[i] = 0;
    }
}
