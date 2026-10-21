# LongProjectPointIRound (LIB386/3D/LPROJ3DI.CPP) is lba2cc's, with no 1997
# assembly to compare against, so it gets a host test instead.
add_executable(test_lproj_round test_lproj_round.cpp)
target_include_directories(test_lproj_round PRIVATE ${CMAKE_SOURCE_DIR}/LIB386/H)
target_link_libraries(test_lproj_round PRIVATE 3D sys)
set_target_properties(test_lproj_round PROPERTIES CXX_STANDARD 11)
add_test(NAME test_lproj_round COMMAND test_lproj_round)
register_host_test(test_lproj_round)
