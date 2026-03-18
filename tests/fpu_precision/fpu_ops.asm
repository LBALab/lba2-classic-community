;; fpu_ops.asm – Standalone x87 FPU reference operations for precision testing.
;;
;; Each function exercises ONE common FPU pattern found in the game's ASM code.
;; The C test calls each via cdecl and compares against portable C approaches.
;;
;; All functions use manual cdecl stack frames (push ebp / mov ebp,esp)
;; to avoid UASM PROC C / -zcw naming issues.  Parameters at [ebp+8], [ebp+12], etc.

.686
.model FLAT

.data

;; FPU control words
Status_Float   dw 0027Fh    ; round-to-nearest, 80-bit precision
Status_Trunc   dw 0F7Fh     ; truncate (round towards zero), 80-bit precision

F_1            dd 1.0
F_256          dd 256.0
FInv_65536     dd 0.0000152588   ; REAL4 - matches ASM_FInv_65536 in POLY.ASM

.code

;; ====================================================================
;; 1. S32 fpu_fild_fistp_round(S32 val)
;;    fild a 32-bit int, fistp back (identity through FPU, round-to-nearest)
;; ====================================================================
PUBLIC fpu_fild_fistp_round
fpu_fild_fistp_round PROC
    push  ebp
    mov   ebp, esp
    fild  dword ptr [ebp+8]
    push  eax
    fistp dword ptr [esp]
    pop   eax
    pop   ebp
    ret
fpu_fild_fistp_round ENDP

;; ====================================================================
;; 2. S32 fpu_int_div_int(S32 a, S32 b)
;;    (S32)a / (S32)b with fild/fdivp/fistp, round-to-nearest
;; ====================================================================
PUBLIC fpu_int_div_int
fpu_int_div_int PROC
    push  ebp
    mov   ebp, esp
    fild  dword ptr [ebp+8]     ; a
    fild  dword ptr [ebp+12]    ; b
    fdivp st(1), st             ; a / b
    push  eax
    fistp dword ptr [esp]
    pop   eax
    pop   ebp
    ret
fpu_int_div_int ENDP

;; ====================================================================
;; 3. S32 fpu_int_div_int_trunc(S32 a, S32 b)
;;    Same as #2 but with truncation rounding mode
;; ====================================================================
PUBLIC fpu_int_div_int_trunc
fpu_int_div_int_trunc PROC
    push  ebp
    mov   ebp, esp
    fldcw Status_Trunc
    fild  dword ptr [ebp+8]
    fild  dword ptr [ebp+12]
    fdivp st(1), st
    push  eax
    fistp dword ptr [esp]
    pop   eax
    fldcw Status_Float
    pop   ebp
    ret
fpu_int_div_int_trunc ENDP

;; ====================================================================
;; 4. S32 fpu_reciprocal_mul(S32 w, S32 val)
;;    256.0f / w * val → S32 (perspective correction pattern)
;; ====================================================================
PUBLIC fpu_reciprocal_mul
fpu_reciprocal_mul PROC
    push  ebp
    mov   ebp, esp
    fild  dword ptr [ebp+8]     ; w
    fld   F_256                  ; 256.0
    fdivrp st(1), st            ; 256.0 / w
    fild  dword ptr [ebp+12]    ; val
    fmulp st(1), st             ; val * (256.0 / w)
    push  eax
    fistp dword ptr [esp]
    pop   eax
    pop   ebp
    ret
fpu_reciprocal_mul ENDP

;; ====================================================================
;; 5. S32 fpu_reciprocal_mul_trunc(S32 w, S32 val)
;;    Same as #4 but truncation mode
;; ====================================================================
PUBLIC fpu_reciprocal_mul_trunc
fpu_reciprocal_mul_trunc PROC
    push  ebp
    mov   ebp, esp
    fldcw Status_Trunc
    fild  dword ptr [ebp+8]
    fld   F_256
    fdivrp st(1), st
    fild  dword ptr [ebp+12]
    fmulp st(1), st
    push  eax
    fistp dword ptr [esp]
    pop   eax
    fldcw Status_Float
    pop   ebp
    ret
fpu_reciprocal_mul_trunc ENDP

;; ====================================================================
;; 6. S32 fpu_slope(S32 dx, S32 dy)
;;    Slope: (1.0 / dy) * dx + 1.0 → S32 (polygon edge slopes)
;; ====================================================================
PUBLIC fpu_slope
fpu_slope PROC
    push  ebp
    mov   ebp, esp
    fild  dword ptr [ebp+12]    ; dy
    fdivr F_1                   ; 1.0 / dy
    fild  dword ptr [ebp+8]     ; dx
    fmulp st(1), st             ; dx / dy
    fadd  F_1                   ; dx/dy + 1.0
    push  eax
    fistp dword ptr [esp]
    pop   eax
    pop   ebp
    ret
fpu_slope ENDP

;; ====================================================================
;; 7. S32 fpu_slope_trunc(S32 dx, S32 dy)
;;    Same as #6 but truncation mode
;; ====================================================================
PUBLIC fpu_slope_trunc
fpu_slope_trunc PROC
    push  ebp
    mov   ebp, esp
    fldcw Status_Trunc
    fild  dword ptr [ebp+12]
    fdivr F_1
    fild  dword ptr [ebp+8]
    fmulp st(1), st
    fadd  F_1
    push  eax
    fistp dword ptr [esp]
    pop   eax
    fldcw Status_Float
    pop   ebp
    ret
fpu_slope_trunc ENDP

;; ====================================================================
;; 8. S32 fpu_interp_factor(S32 zclip, S32 lastZ, S32 curZ,
;;                          S32 lastVal, S32 curVal)
;;    Z-clip interpolation: lastVal + (curVal-lastVal) * (zclip-lastZ)/(curZ-lastZ)
;; ====================================================================
PUBLIC fpu_interp_factor
fpu_interp_factor PROC
    push  ebp
    mov   ebp, esp
    ;; factor = (zclip - lastZ) / (curZ - lastZ)
    mov   eax, [ebp+8]          ; zclip
    sub   eax, [ebp+12]         ; zclip - lastZ
    push  eax
    fild  dword ptr [esp]
    mov   eax, [ebp+16]         ; curZ
    sub   eax, [ebp+12]         ; curZ - lastZ
    mov   [esp], eax
    fild  dword ptr [esp]
    fdivp st(1), st             ; factor

    ;; diff = curVal - lastVal
    mov   eax, [ebp+24]         ; curVal
    sub   eax, [ebp+20]         ; curVal - lastVal
    mov   [esp], eax
    fild  dword ptr [esp]
    fmulp st(1), st             ; diff * factor

    ;; result = lastVal + diff*factor
    fild  dword ptr [ebp+20]    ; lastVal
    faddp st(1), st
    fistp dword ptr [esp]
    pop   eax
    pop   ebp
    ret
fpu_interp_factor ENDP

;; ====================================================================
;; 9. void fpu_mul_add_chain(float a, float b, float c, float d, float *out)
;;    Matrix-style a*b + c*d → float
;; ====================================================================
PUBLIC fpu_mul_add_chain
fpu_mul_add_chain PROC
    push  ebp
    mov   ebp, esp
    fld   dword ptr [ebp+8]     ; a
    fmul  dword ptr [ebp+12]    ; a*b
    fld   dword ptr [ebp+16]    ; c
    fmul  dword ptr [ebp+20]    ; c*d
    faddp st(1), st             ; a*b + c*d
    mov   eax, [ebp+24]         ; out
    fstp  dword ptr [eax]
    pop   ebp
    ret
fpu_mul_add_chain ENDP

;; ====================================================================
;; 10. void fpu_mul_sub_chain(float a, float b, float c, float d, float *out)
;;     a*b - c*d → float
;; ====================================================================
PUBLIC fpu_mul_sub_chain
fpu_mul_sub_chain PROC
    push  ebp
    mov   ebp, esp
    fld   dword ptr [ebp+16]    ; c
    fmul  dword ptr [ebp+20]    ; c*d
    fld   dword ptr [ebp+8]     ; a
    fmul  dword ptr [ebp+12]    ; a*b
    fsubrp st(1), st            ; a*b - c*d
    mov   eax, [ebp+24]
    fstp  dword ptr [eax]
    pop   ebp
    ret
fpu_mul_sub_chain ENDP

;; ====================================================================
;; 11. S32 fpu_float_to_int_round(float val)
;;     fld float, fistp → S32 (round-to-nearest)
;; ====================================================================
PUBLIC fpu_float_to_int_round
fpu_float_to_int_round PROC
    push  ebp
    mov   ebp, esp
    fld   dword ptr [ebp+8]
    push  eax
    fistp dword ptr [esp]
    pop   eax
    pop   ebp
    ret
fpu_float_to_int_round ENDP

;; ====================================================================
;; 12. S32 fpu_float_to_int_trunc(float val)
;;     fld float, fistp → S32 (truncation mode)
;; ====================================================================
PUBLIC fpu_float_to_int_trunc
fpu_float_to_int_trunc PROC
    push  ebp
    mov   ebp, esp
    fldcw Status_Trunc
    fld   dword ptr [ebp+8]
    push  eax
    fistp dword ptr [esp]
    pop   eax
    fldcw Status_Float
    pop   ebp
    ret
fpu_float_to_int_trunc ENDP

;; ====================================================================
;; 13. void fpu_perspective_uv(S32 w, S32 u, S32 v, S32 *out_u, S32 *out_v)
;;     Perspective UV: 256.0/w cached, then multiply u and v
;; ====================================================================
PUBLIC fpu_perspective_uv
fpu_perspective_uv PROC
    push  ebp
    mov   ebp, esp
    ;; invW = 256.0 / w
    fild  dword ptr [ebp+8]     ; w
    fld   F_256
    fdivrp st(1), st            ; 256.0 / w

    ;; out_u = u * invW
    fld   st(0)                 ; dup invW
    fild  dword ptr [ebp+12]    ; u
    fmulp st(1), st
    mov   eax, [ebp+20]         ; out_u
    fistp dword ptr [eax]

    ;; out_v = v * invW
    fild  dword ptr [ebp+16]    ; v
    fmulp st(1), st
    mov   eax, [ebp+24]         ; out_v
    fistp dword ptr [eax]
    pop   ebp
    ret
fpu_perspective_uv ENDP

;; ====================================================================
;; 14. void fpu_dot3(float *a, float *b, float *out)
;;     Float dot product: a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
;; ====================================================================
PUBLIC fpu_dot3
fpu_dot3 PROC
    push  ebp
    mov   ebp, esp
    push  esi
    push  edi
    mov   esi, [ebp+8]          ; a
    mov   edi, [ebp+12]         ; b
    fld   dword ptr [esi]
    fmul  dword ptr [edi]       ; a0*b0
    fld   dword ptr [esi+4]
    fmul  dword ptr [edi+4]     ; a1*b1
    faddp st(1), st
    fld   dword ptr [esi+8]
    fmul  dword ptr [edi+8]     ; a2*b2
    faddp st(1), st
    mov   eax, [ebp+16]         ; out
    fstp  dword ptr [eax]
    pop   edi
    pop   esi
    pop   ebp
    ret
fpu_dot3 ENDP

;; ====================================================================
;; 15. S32 fpu_reciprocal_mul_256(S32 w, S32 val)
;;     256.0/w * val * 256.0 → S32 (TextureZ pattern)
;; ====================================================================
PUBLIC fpu_reciprocal_mul_256
fpu_reciprocal_mul_256 PROC
    push  ebp
    mov   ebp, esp
    fild  dword ptr [ebp+8]     ; w
    fld   F_256
    fdivrp st(1), st            ; 256.0 / w
    fild  dword ptr [ebp+12]    ; val
    fmulp st(1), st             ; val * invW
    fmul  F_256                 ; * 256.0
    push  eax
    fistp dword ptr [esp]
    pop   eax
    pop   ebp
    ret
fpu_reciprocal_mul_256 ENDP

;; ====================================================================
;; 16. S32 fpu_xslope_cross(S32 XB_XA, S32 YB_YA, S32 XC_XA, S32 YC_YA,
;;                          S32 DB_DA, S32 DC_DA)
;;     Simple non-perspective slope. See test_fpu_precision.cpp for docs.
;; ====================================================================
PUBLIC fpu_xslope_cross
fpu_xslope_cross PROC
    push  ebp
    mov   ebp, esp
    fild  dword ptr [ebp+8]     ; XB_XA
    fild  dword ptr [ebp+20]    ; YC_YA
    fild  dword ptr [ebp+16]    ; XC_XA
    fild  dword ptr [ebp+12]    ; YB_YA
    fxch  st(3)
    fmulp st(2), st
    fmulp st(2), st
    fsubrp st(1), st            ; Denom = XB_XA*YC_YA - XC_XA*YB_YA
    fld   F_256
    fdivrp st(1), st            ; InvDenom = 256.0 / Denom
    fild  dword ptr [ebp+24]    ; DB_DA
    fild  dword ptr [ebp+28]    ; DC_DA
    fild  dword ptr [ebp+20]    ; YC_YA
    fild  dword ptr [ebp+12]    ; YB_YA
    fmulp st(2), st
    fmulp st(2), st
    fsubrp st(1), st
    fmulp st(1), st
    fldcw Status_Trunc
    push  eax
    fistp dword ptr [esp]
    pop   eax
    fldcw Status_Float
    pop   ebp
    ret
fpu_xslope_cross ENDP

;; ====================================================================
;; 17. S32 fpu_texz_uslope_stacked(S32 rawDenom,
;;         S32 MapU_A, S32 W_A, S32 MapU_B, S32 W_B,
;;         S32 MapU_C, S32 W_C, S32 YB_YA, S32 YC_YA)
;;     Perspective-corrected TextureZ U-slope with InvDenom KEPT ON
;;     THE FPU STACK (never stored to memory). This matches the game ASM.
;;
;;     Formula:
;;       InvDenom = 256.0 / rawDenom          (stays on FPU stack)
;;       Dp = InvDenom * FInv_65536           (stays on FPU stack)
;;       UAp = MapU_A * W_A
;;       UBp = MapU_B * W_B
;;       UCp = MapU_C * W_C
;;       USlope = ((UCp-UAp)*YB_YA - (UBp-UAp)*YC_YA) * Dp
;;       return trunc(USlope)
;; ====================================================================
PUBLIC fpu_texz_uslope_stacked
fpu_texz_uslope_stacked PROC
    push  ebp
    mov   ebp, esp

    ;; Compute InvDenom = 256.0 / rawDenom (kept on stack!)
    fild  dword ptr [ebp+8]     ; rawDenom
    fld   F_256                 ; 256.0, rawDenom
    fdivrp st(1), st            ; InvDenom = 256.0/rawDenom

    ;; Compute Dp = InvDenom * FInv_65536 (kept on stack!)
    fmul  FInv_65536            ; Dp = InvDenom * FInv_65536
    ;; Stack: Dp

    ;; Compute UAp = MapU_A * W_A
    fild  dword ptr [ebp+12]    ; MapU_A, Dp
    fild  dword ptr [ebp+16]    ; W_A, MapU_A, Dp
    fmulp st(1), st             ; UAp, Dp

    ;; Compute UBp = MapU_B * W_B
    fild  dword ptr [ebp+20]    ; MapU_B, UAp, Dp
    fild  dword ptr [ebp+24]    ; W_B, MapU_B, UAp, Dp
    fmulp st(1), st             ; UBp, UAp, Dp

    ;; Compute UCp = MapU_C * W_C
    fild  dword ptr [ebp+28]    ; MapU_C, UBp, UAp, Dp
    fild  dword ptr [ebp+32]    ; W_C, MapU_C, UBp, UAp, Dp
    fmulp st(1), st             ; UCp, UBp, UAp, Dp

    ;; Stack: UCp, UBp, UAp, Dp
    ;; Compute (UCp-UAp) and (UBp-UAp)
    fxch  st(1)                 ; UBp, UCp, UAp, Dp
    fsub  st, st(2)             ; (UBp-UAp), UCp, UAp, Dp
    fxch  st(2)                 ; UAp, UCp, (UBp-UAp), Dp
    fsubp st(1), st             ; (UCp-UAp), (UBp-UAp), Dp

    ;; Compute MUC = (UCp-UAp) * YB_YA, MUB = (UBp-UAp) * YC_YA
    fild  dword ptr [ebp+36]    ; YB_YA, (UCp-UAp), (UBp-UAp), Dp
    fmulp st(1), st             ; MUC, (UBp-UAp), Dp

    fxch  st(1)                 ; (UBp-UAp), MUC, Dp
    fild  dword ptr [ebp+40]    ; YC_YA, (UBp-UAp), MUC, Dp
    fmulp st(1), st             ; MUB, MUC, Dp

    ;; MUC - MUB
    fsubp st(1), st             ; (MUC-MUB), Dp
    ;; WAIT: fsubp st(1),st = st(1) - st(0). st(0)=MUB, st(1)=MUC
    ;; so result = MUC - MUB. Correct.

    ;; USlope = (MUC-MUB) * Dp
    fmulp st(1), st             ; USlope

    ;; Truncate to S32
    fldcw Status_Trunc
    push  eax
    fistp dword ptr [esp]
    pop   eax
    fldcw Status_Float

    pop   ebp
    ret
fpu_texz_uslope_stacked ENDP

;; ====================================================================
;; 18. S32 fpu_texz_uslope_stored(S32 rawDenom,
;;         S32 MapU_A, S32 W_A, S32 MapU_B, S32 W_B,
;;         S32 MapU_C, S32 W_C, S32 YB_YA, S32 YC_YA)
;;     Same computation as _stacked, but InvDenom and Dp are STORED TO
;;     80-bit MEMORY (long double) and RELOADED, simulating the CPP port's
;;     volatile long double behavior.
;; ====================================================================
PUBLIC fpu_texz_uslope_stored
fpu_texz_uslope_stored PROC
    push  ebp
    mov   ebp, esp
    sub   esp, 12               ; 10 bytes for tbyte Dp + 2 padding

    ;; Compute InvDenom = 256.0 / rawDenom
    fild  dword ptr [ebp+8]     ; rawDenom
    fld   F_256                 ; 256.0, rawDenom
    fdivrp st(1), st            ; InvDenom

    ;; Store InvDenom to 80-bit memory (same as volatile long double)
    fstp  tbyte ptr [ebp-12]    ; Store to memory, FPU stack now empty

    ;; Reload InvDenom, compute Dp = InvDenom * FInv_65536
    fld   tbyte ptr [ebp-12]    ; Reload InvDenom
    fmul  FInv_65536            ; Dp

    ;; Store Dp to 80-bit memory (same as volatile long double)
    fstp  tbyte ptr [ebp-12]    ; Store Dp, FPU stack empty

    ;; Now compute UAp, UBp, UCp with Dp loaded from memory
    fild  dword ptr [ebp+12]    ; MapU_A
    fild  dword ptr [ebp+16]    ; W_A, MapU_A
    fmulp st(1), st             ; UAp

    fild  dword ptr [ebp+20]    ; MapU_B, UAp
    fild  dword ptr [ebp+24]    ; W_B, MapU_B, UAp
    fmulp st(1), st             ; UBp, UAp

    fild  dword ptr [ebp+28]    ; MapU_C, UBp, UAp
    fild  dword ptr [ebp+32]    ; W_C, MapU_C, UBp, UAp
    fmulp st(1), st             ; UCp, UBp, UAp

    ;; Compute (UCp-UAp) and (UBp-UAp)
    fxch  st(1)                 ; UBp, UCp, UAp
    fsub  st, st(2)             ; (UBp-UAp), UCp, UAp
    fxch  st(2)                 ; UAp, UCp, (UBp-UAp)
    fsubp st(1), st             ; (UCp-UAp), (UBp-UAp)

    fild  dword ptr [ebp+36]    ; YB_YA, (UCp-UAp), (UBp-UAp)
    fmulp st(1), st             ; MUC, (UBp-UAp)

    fxch  st(1)                 ; (UBp-UAp), MUC
    fild  dword ptr [ebp+40]    ; YC_YA, (UBp-UAp), MUC
    fmulp st(1), st             ; MUB, MUC

    fsubp st(1), st             ; (MUC-MUB)

    ;; Reload Dp from memory and multiply
    fld   tbyte ptr [ebp-12]    ; Dp, (MUC-MUB)
    fmulp st(1), st             ; USlope = (MUC-MUB) * Dp

    ;; Truncate to S32
    fldcw Status_Trunc
    push  eax
    fistp dword ptr [esp]
    pop   eax
    fldcw Status_Float

    add   esp, 12
    pop   ebp
    ret
fpu_texz_uslope_stored ENDP

END
