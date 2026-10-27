cmake_minimum_required(VERSION 3.23)
include("${MODULE}")
if(DEFINED PROFILE)
    lba2_configure_input_profile("${PROFILE}" "${OUTPUT}")
    return()
endif()

file(MAKE_DIRECTORY "${OUTPUT}")
foreach(profile default vita android)
    lba2_configure_input_profile("${ROOT}/cmake/input/${profile}.cmake" "${OUTPUT}/${profile}")
endforeach()
file(READ "${OUTPUT}/default/INPUT_PROFILE_CONFIG.H" defaults)
file(READ "${OUTPUT}/vita/INPUT_PROFILE_CONFIG.H" vita)
file(READ "${OUTPUT}/android/INPUT_PROFILE_CONFIG.H" android)
if(NOT defaults MATCHES "LBA2_GAMEPAD_DEADZONE_DEFAULT 8000" OR
   NOT defaults MATCHES "LBA2_INPUT_DIRECT_TOUCH_SLOT -1" OR
   NOT vita MATCHES "LBA2_GAMEPAD_DEADZONE_DEFAULT 13107" OR
   NOT vita MATCHES "LBA2_INPUT_DIRECT_TOUCH_SLOT 9" OR
   NOT vita MATCHES "\\{K_GAMEPAD_B, 13, 9\\}" OR
   NOT android MATCHES "LBA2_INPUT_GAMEPAD_DEFAULTS \\{K_GAMEPAD_RTRIGGER, 0\\}" OR
   NOT android MATCHES "LBA2_INPUT_KEYBOARD_DEFAULTS \\{K_GRAY_UP, K_NUMPAD_8\\}" OR
   NOT android MATCHES "\\{0, \"ESC\", 0.065f" OR
   NOT android MATCHES "LBA2_INPUT_OVERLAY_DIRECT 1" OR
   NOT android MATCHES "LBA2_INPUT_OVERLAY_INDIRECT 1")
    message(FATAL_ERROR "Generated platform input profiles do not match their configuration")
endif()

function(reject name contents diagnostic)
    set(profile "${OUTPUT}/${name}.cmake")
    file(WRITE "${profile}" "${contents}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DMODULE=${MODULE}" "-DPROFILE=${profile}" "-DOUTPUT=${OUTPUT}/${name}"
        -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "${diagnostic}")
        message(FATAL_ERROR "${name}: expected profile error '${diagnostic}', got ${result}: ${output}${error}")
    endif()
endfunction()

reject(deadzone "set(INPUT_DEADZONE 32768)" "deadzone must be")
reject(touch "set(INPUT_DIRECT_TOUCH_SLOT 32)" "DIRECT-touch slot must be")
reject(rear_touch "set(INPUT_INDIRECT_TOUCH_SLOT 32)" "INDIRECT-touch slot must be")
reject(overlay_source "set(INPUT_OVERLAY_TOUCH_SOURCE unknown)" "overlay source must be")
reject(overlay_geometry "input_touch_button(KEY K_ENTER USE 0.1 0.2 0.0 0.1)" "radius must be positive")
reject(overlay_label "input_touch_button(KEY K_ENTER bad-label 0.1 0.2 0.1 0.1)" "Invalid input profile touch button")
reject(overlay_bounds "input_touch_button(KEY K_ENTER USE 1.1 0.2 0.1 0.1)" "geometry must be")
reject(overlay_hide "input_touch_button(HIDE K_ENTER HIDE 0.1 0.2 0.1 0.1)" "HIDE button cannot")
reject(overlay_conflict "set(INPUT_DIRECT_TOUCH_SLOT 9)\ninput_touch_button(KEY K_ENTER USE 0.1 0.2 0.1 0.1)" "cannot use both")
reject(keyboard "set(INPUT_KEYBOARD_0 K_UP)" "must have two keys")
reject(binding "set(INPUT_GAMEPAD_0 K_GAMEPAD_A)" "must have two keys")
reject(key "set(INPUT_GAMEPAD_0 99999 0)" "Invalid input profile key")
reject(remap "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,8,32)" "remap slots must be")
reject(duplicate "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,8,7 K_GAMEPAD_A,8,9)" "Duplicate input profile modal remap")
reject(remap_limit "set(INPUT_MODAL_REMAPS)\nforeach(slot RANGE 0 32)\nlist(APPEND INPUT_MODAL_REMAPS K_GAMEPAD_A,\${slot},0)\nendforeach()" "at most 32 modal remaps")
message(STATUS "Input profile generation and invalid configurations checked")
