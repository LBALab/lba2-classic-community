#include <POLYGON/TESTVUE.H>

#include <SYSTEM/UTILS.H>
#include <SYSTEM/DEBUG_TRACES.H>

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

    /* Screen coords are S16, so every product below is at most ~2^32 and
       the difference of the two at most ~2^33 — both exact in a 53-bit
       double, which makes this the same value the x87 80-bit evaluation
       produced, without ARM64's software-emulated long double. */
    double result = ((double)ptr2->Pt_XE - (double)ptr->Pt_XE) * ((double)ptr->Pt_YE - (double)ptr3->Pt_YE) - ((double)ptr->Pt_XE - (double)ptr3->Pt_XE) * ((double)ptr2->Pt_YE - (double)ptr->Pt_YE);
    return ROUND(result) >= 0.0;
}
