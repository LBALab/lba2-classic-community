set(_ANIM_LIBS ani obj 3D poly svg sys SDL3::SDL3)

# All ANIM ASM files use .model SMALL → CPP-only tests
add_cpp_test(NAME test_clear      TEST_SOURCE test_clear.cpp      LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_body       TEST_SOURCE test_body.cpp       LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_texture    TEST_SOURCE test_texture.cpp    LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_anim       TEST_SOURCE test_anim.cpp       LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_frame      TEST_SOURCE test_frame.cpp      LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_intanim    TEST_SOURCE test_intanim.cpp    LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_interdep   TEST_SOURCE test_interdep.cpp   LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_intframe   TEST_SOURCE test_intframe.cpp   LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_stoframe   TEST_SOURCE test_stoframe.cpp   LIBS ${_ANIM_LIBS})
add_cpp_test(NAME test_libinit    TEST_SOURCE test_libinit.cpp    LIBS ${_ANIM_LIBS})
