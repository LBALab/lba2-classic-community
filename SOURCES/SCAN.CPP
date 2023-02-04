#include        "C_EXTERN.H"

#include <SYSTEM/KEYBOARD.H>

#ifdef _WIN32 // Portability - Intrinsics are different for MSVC and GCC
#include <intrin.h>
#endif

//──────────────────────────────────────────────────────────────────────────
static  U8      KeyState[256+16*8]   ;

//──────────────────────────────────────────────────────────────────────────
U32 GetBit(U32 val) {
  if (val == 0) {
    return 0;
  }

#ifdef _WIN32 // Portability - Intrinsics are different for MSVC and GCC
  unsigned long index;
  _BitScanForward(&index, val);
  return index;
#else // _WIN32
  return __builtin_ctz(val);
#endif // _WIN32
}

//──────────────────────────────────────────────────────────────────────────
S32     KeyDown()
{
        S32     i       ;
        S32     found   ;
        U32     *Joys, *OldJoys ;

	DetectJoys()	;
        GetJoys((U32*)(TabKeys+256))    ;
        ManageKeyboard();

        found = 0       ;

        for(i=255; i>=0; i--)
        {
                U8      key     ;

                key = (U8)(~KeyState[i]&0x80);

                KeyState[i] = TabKeys[i];

                TabKeys[i] &= key       ;

                // FIXME: Add checks back
                if(    /* (i>VK_MBUTTON)  &&
                        (i!=VK_EXECUTE) &&
                        (i!=VK_LWIN)    &&
                        (i!=VK_RWIN)    &&*/
                        !found&&TabKeys[i]      )
                {
                        found = i       ;
                }
        }

        OldJoys = (U32*)&KeyState[256]  ;
        Joys = (U32*)&TabKeys[256]      ;

        for(i=0; i<2*16; i++)
        {
                U32 bits        ;

                bits = ~OldJoys[i]      ;
                OldJoys[i] = Joys[i]    ;
                bits &= Joys[i]         ;
                Joys[i] = bits         	;

                if(!found&&bits)
                {
                        found = 256+i*32+GetBit(bits) ;
                }
        }

        return  found   ;
}

//──────────────────────────────────────────────────────────────────────────
