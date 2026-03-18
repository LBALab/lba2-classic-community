/*
 * polyrec_asm_wrappers.cpp — ASM calling convention wrappers for polyrec replay
 *
 * The ASM polygon functions use Watcom register calling convention,
 * not C stack convention. This file provides C-callable versions that
 * set up registers before jumping to the ASM implementations.
 *
 * These functions override the CPP versions via --allow-multiple-definition.
 * Only linked into replay_polyrec_asm, not replay_polyrec_cpp.
 */

#include <POLYGON/POLY.H>
#include <SVGA/SCREEN.H>
#include <SVGA/CLIP.H>

/* ASM symbols from libpolyrec_asm_lib386.a.
 * - Fill_PolyFast, Switch_Fillers_ASM: keep original names (no CPP conflict)
 * - Fill_Sphere, Line_Entry, Line_ZBuffer, Line_ZBuffer_NZW: renamed to asm_*
 *   to avoid conflict with our wrapper definitions below. */
extern "C" S32 Fill_PolyFast(S32, S32, S32, Struc_Point *);
extern "C" void Switch_Fillers_ASM(U32);
/* Renamed ASM symbols (via objcopy --redefine-sym) */
extern "C" void asm_Fill_Sphere(void);
extern "C" void asm_Line_A(void);

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Switch_Fillers — use ASM version to set up ASM filler tables              */
/* ═══════════════════════════════════════════════════════════════════════════ */

extern "C" void Switch_Fillers(U32 Bank) {
    /* Switch_Fillers_ASM: Bank in EAX, C calling convention, saves/restores regs */
    __asm__ __volatile__(
        "call Switch_Fillers_ASM"
        :
        : "a"(Bank)
        : "memory", "cc"
    );
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Fill_Poly — dispatch through ASM Fill_PolyFast                           */
/* ═══════════════════════════════════════════════════════════════════════════ */

/*
 * ASM Fill_PolyFast expects:
 *   EAX = Type_Poly
 *   EBX = Color_Poly
 *   ECX = Nb_Points
 *   ESI = Ptr_Points
 *
 * It sets Fill_LeftSlope=0, calls SetScreenPitch, sets Fill_Type,
 * then dispatches: jmp [Fill_Saut_Normal + Type_Poly*4]
 *
 * The filler returns to our caller (since PolyFast was pushed by call).
 */
extern "C" S32 Fill_Poly(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
                          Struc_Point *Ptr_Points) {
    S32 result;

    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call Fill_PolyFast\n\t"
        "pop  %%ebp"
        : "=a"(result)
        : "a"(Type_Poly), "b"(Color_Poly),
          "c"(Nb_Points), "S"(Ptr_Points)
        : "edx", "edi", "memory", "cc"
    );

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Fill_Sphere — override CPP version with ASM register convention           */
/* ═══════════════════════════════════════════════════════════════════════════ */

/*
 * ASM Fill_Sphere: ESI=Type, EDX=Color, EAX=CentreX, EBX=CentreY,
 *                  ECX=Rayon, EDI=zBufValue
 */
extern "C" void Fill_Sphere(S32 Type_Sphere, S32 Color_Sphere,
                            S32 Centre_X, S32 Centre_Y,
                            S32 Rayon, S32 zBufferValue) {
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "call asm_Fill_Sphere\n\t"
        "pop  %%ebp"
        :
        : "S"(Type_Sphere), "d"(Color_Sphere),
          "a"(Centre_X), "b"(Centre_Y),
          "c"(Rayon), "D"(zBufferValue)
        : "memory", "cc"
    );
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*  Line_A — dispatch to ASM Line_Entry / Line_ZBuffer / Line_ZBuffer_NZW    */
/* ═══════════════════════════════════════════════════════════════════════════ */

/*
 * ASM Line_A: EAX=x0, EBX=y0, ECX=x1, EDX=y1, EBP=color
 * Plus ESI=z1, EDI=z2 for zbuffer variants.
 * Line_A internally dispatches to Line_Entry / Line_ZBuffer / Line_ZBuffer_NZW
 * based on Fill_Flag_NZW and Fill_Flag_ZBuffer globals.
 */
extern "C" void Line_A(S32 x0, S32 y0, S32 x1, S32 y1,
                        S32 col, S32 z1, S32 z2) {
    __asm__ __volatile__(
        "push %%ebp\n\t"
        "movl %4, %%ebp\n\t"
        "call asm_Line_A\n\t"
        "pop  %%ebp"
        :
        : "a"(x0), "b"(y0), "c"(x1), "d"(y1), "m"(col),
          "S"(z1), "D"(z2)
        : "memory", "cc"
    );
}
