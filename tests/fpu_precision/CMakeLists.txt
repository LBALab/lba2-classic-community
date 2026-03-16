# FPU Precision comparison tests — x87 ASM vs portable C approaches
#
# The C++ test code is compiled with default compiler settings (no FPU overrides)
# to measure the precision gap between x87 ASM and portable C approaches.
set(_FPU_ASM_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_asm_cpp_test(
    NAME            test_fpu_precision
    TEST_SOURCE     test_fpu_precision.cpp
    ASM_SOURCE      ${_FPU_ASM_DIR}/fpu_ops.asm
    RENAME_SYMS     fpu_fild_fistp_round
                    fpu_int_div_int
                    fpu_int_div_int_trunc
                    fpu_reciprocal_mul
                    fpu_reciprocal_mul_trunc
                    fpu_slope
                    fpu_slope_trunc
                    fpu_interp_factor
                    fpu_mul_add_chain
                    fpu_mul_sub_chain
                    fpu_float_to_int_round
                    fpu_float_to_int_trunc
                    fpu_perspective_uv
                    fpu_dot3
                    fpu_reciprocal_mul_256
                    fpu_xslope_cross
    LIBS            m
)
