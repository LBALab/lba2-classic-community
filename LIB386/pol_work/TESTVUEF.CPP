#include <POLYGON/TESTVUE.H>

#include <SYSTEM/UTILS.H>
#include <SYSTEM/DEBUG_TRACES.H>
#include <3D/X87REAL.H>

// ╔═════════════════════════════════════════════════════════════════╗
// ║ TestVisible : Tests the visibility of a poly                    ║
// ╟─────────────────────────────────────────────────────────────────╢
// ║ Call: ESI = Pointer on an array of STRUC_POINT                  ║
// ╟─────────────────────────────────────────────────────────────────╢
// ║ Return: EAX = TRUE if polygon visible                           ║
// ║                                                                 ║
// ╚═════════════════════════════════════════════════════════════════╝
S32 TestVuePoly(Struc_Point *ptr) {
    LIB386_TRACE_CPP("TestVuePolyF", "1", "input=%p", (void *)ptr);
    Struc_Point *ptr2 = ptr + 1;
    Struc_Point *ptr3 = ptr + 2;

    X87_REAL result = ((X87_REAL)ptr2->Pt_XE - (X87_REAL)ptr->Pt_XE) * ((X87_REAL)ptr->Pt_YE - (X87_REAL)ptr3->Pt_YE) - ((X87_REAL)ptr->Pt_XE - (X87_REAL)ptr3->Pt_XE) * ((X87_REAL)ptr2->Pt_YE - (X87_REAL)ptr->Pt_YE);
    return ROUND(result) >= 0.0;
}
