set(_POLY_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/pol_work)
set(_POLY_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# pol_work tests are placeholders — the polygon rendering functions have heavy
# state dependencies (fill tables, edge buffers, Z-buffer, screen).
# Full tests require substantial fixture setup.

add_cpp_test(NAME test_poly       TEST_SOURCE test_poly.cpp       LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_poly_jmp   TEST_SOURCE test_poly_jmp.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polyclip   TEST_SOURCE test_polyclip.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polydisc   TEST_SOURCE test_polydisc.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polyflat   TEST_SOURCE test_polyflat.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polygour   TEST_SOURCE test_polygour.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polygtex   TEST_SOURCE test_polygtex.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polyline   TEST_SOURCE test_polyline.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polytext   TEST_SOURCE test_polytext.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polytexz   TEST_SOURCE test_polytexz.cpp   LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polytzf    TEST_SOURCE test_polytzf.cpp    LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_polytzg    TEST_SOURCE test_polytzg.cpp    LIBS poly svg sys SDL3::SDL3)
add_cpp_test(NAME test_testvuef   TEST_SOURCE test_testvuef.cpp   LIBS poly svg sys SDL3::SDL3)
