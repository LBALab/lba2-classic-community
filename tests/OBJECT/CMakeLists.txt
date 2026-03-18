set(_OBJ_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/OBJECT)
set(_OBJ_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

add_asm_cpp_test(
    NAME test_aff_obj  TEST_SOURCE test_aff_obj.cpp
    ASM_SOURCE ${_OBJ_ASM_DIR}/AFF_OBJ.ASM
    RENAME_SYMS BodyDisplay BodyDisplay_AlphaBeta ObjectDisplay ObjPtrMap TransFctBody Obj_ListRotatedPoints QuickSort QuickSortInv
    LIBS obj 3D poly svg sys SDL3::SDL3
    ASM_INCLUDE_DIRS ${_OBJ_ASM_INC}
)
# AFF_OBJ.ASM references Fill_PolyFast, Fill_Sphere, Line_A (cross-module ASM procs)
target_link_options(test_aff_obj PRIVATE -Wl,--unresolved-symbols=ignore-in-object-files)
