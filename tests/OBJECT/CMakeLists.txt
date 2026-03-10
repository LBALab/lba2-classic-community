set(_OBJ_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/OBJECT)
set(_OBJ_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

add_cpp_test(
    NAME test_aff_obj  TEST_SOURCE test_aff_obj.cpp
    LIBS obj 3D poly svg sys SDL3::SDL3
)
