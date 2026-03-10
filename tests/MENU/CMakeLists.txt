set(_MENU_ASM_DIR ${CMAKE_SOURCE_DIR}/LIB386/MENU)
set(_MENU_ASM_INC ${CMAKE_SOURCE_DIR}/LIB386/H)

# NOTE: SORT.CPP contains only commented ASM — no C implementation exists.
# This test can only run in ASM mode. For now, a placeholder verifies linkage.
add_executable(test_sort test_sort.cpp)
target_include_directories(test_sort PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(test_sort PRIVATE m)
add_test(NAME test_sort COMMAND test_sort)
