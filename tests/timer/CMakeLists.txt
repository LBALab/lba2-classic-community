add_executable(test_timer_lock_window test_timer_lock_window.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/TIMER.CPP)

target_include_directories(test_timer_lock_window PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_timer_lock_window PRIVATE SDL3::SDL3)

set_target_properties(test_timer_lock_window PROPERTIES CXX_STANDARD 11)

add_test(NAME test_timer_lock_window COMMAND test_timer_lock_window)

register_host_test(test_timer_lock_window)

# Fixed simulation-timestep scheduler (issue #358).
add_executable(test_fixed_step test_fixed_step.cpp
    ${CMAKE_SOURCE_DIR}/LIB386/SYSTEM/TIMER.CPP)

target_include_directories(test_fixed_step PRIVATE
    ${CMAKE_SOURCE_DIR}/LIB386/H
)

target_link_libraries(test_fixed_step PRIVATE SDL3::SDL3)

set_target_properties(test_fixed_step PROPERTIES CXX_STANDARD 11)

add_test(NAME test_fixed_step COMMAND test_fixed_step)

register_host_test(test_fixed_step)
