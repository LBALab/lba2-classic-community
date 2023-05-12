#include <POLYGON/TESTVUE.H>

#include <SYSTEM/UTILS.H>

// ╔═════════════════════════════════════════════════════════════════╗
// ║ TestVisible : Tests the visibility of a poly                    ║
// ╟─────────────────────────────────────────────────────────────────╢
// ║ Call: ESI = Pointer on an array of STRUC_POINT                  ║
// ╟─────────────────────────────────────────────────────────────────╢
// ║ Return: EAX = TRUE if polygon visible                           ║
// ║                                                                 ║
// ╚═════════════════════════════════════════════════════════════════╝
S32 TestVuePoly(Struc_Point *ptr)
{
  Struc_Point *ptr2 = ptr + 1;
  Struc_Point *ptr3 = ptr + 2;

  long double result = ((long double)ptr2->Pt_XE - (long double)ptr->Pt_XE)
                     * ((long double)ptr->Pt_YE - (long double)ptr3->Pt_YE)
                     - ((long double)ptr->Pt_XE - (long double)ptr3->Pt_XE)
                     * ((long double)ptr2->Pt_YE - (long double)ptr->Pt_YE);
  return ROUND(result) >= 0.0;
}
