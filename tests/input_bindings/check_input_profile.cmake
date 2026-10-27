cmake_minimum_required(VERSION 3.23)
include("${MODULE}")
if(DEFINED ACTION_HEADER)
    lba2_read_input_actions("${ACTION_HEADER}")
    return()
endif()
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

file(WRITE "${OUTPUT}/named.cmake" [=[
set(INPUT_KEYBOARD_PINGOUIN K_F12 K_Z)
set(INPUT_GAMEPAD_FOUDRE K_GAMEPAD_GUIDE 0)
set(INPUT_DIRECT_TOUCH_ACTION RETURN)
set(INPUT_INDIRECT_TOUCH_ACTION NONE)
set(INPUT_MODAL_REMAPS K_GAMEPAD_B,ESQUIVE,RETURN)
]=])
lba2_configure_input_profile("${OUTPUT}/named.cmake" "${OUTPUT}/named")
file(READ "${OUTPUT}/named/INPUT_PROFILE_CONFIG.H" named)
if(NOT named MATCHES "\\{K_F12, K_Z\\}, \\{K_J, 0\\}, \\{K_C, 0\\}, \\{K_F, 0\\}" OR
   NOT named MATCHES "\\{K_GAMEPAD_DPAD_RIGHT, 0\\}, \\{K_GAMEPAD_DPAD_DOWN, 0\\}, \\{K_GAMEPAD_DPAD_UP, 0\\}, \\{K_GAMEPAD_GUIDE, 0\\}" OR
   NOT named MATCHES "LBA2_INPUT_DIRECT_TOUCH_SLOT 9" OR
   NOT named MATCHES "LBA2_INPUT_INDIRECT_TOUCH_SLOT -1" OR
   NOT named MATCHES "\\{K_GAMEPAD_B, 13, 9\\}")
    message(FATAL_ERROR "Named actions must preserve binding, touch and modal slot indexes")
endif()
get_property(dependencies DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)
if(NOT "${ROOT}/cmake/../SOURCES/INPUT_ACTIONS.H" IN_LIST dependencies)
    message(FATAL_ERROR "Input action header must trigger CMake reconfiguration")
endif()

reject(deadzone "set(INPUT_DEADZONE 32768)" "deadzone must be")
reject(touch "set(INPUT_DIRECT_TOUCH_ACTION PINGOUIN)" "belongs to Input2")
reject(rear_touch "set(INPUT_INDIRECT_TOUCH_ACTION FOUDRE)" "belongs to Input2")
reject(unknown_touch "set(INPUT_DIRECT_TOUCH_ACTION DODGE)" "Unknown input profile action")
reject(numeric_touch "set(INPUT_DIRECT_TOUCH_ACTION 9)" "Unknown input profile action")
reject(legacy_touch "set(INPUT_DIRECT_TOUCH_SLOT 9)" "not numeric touch slots")
reject(legacy_rear_touch "set(INPUT_INDIRECT_TOUCH_SLOT -1)" "not numeric touch slots")
reject(overlay_source "set(INPUT_OVERLAY_TOUCH_SOURCE unknown)" "overlay source must be")
reject(overlay_geometry "input_touch_button(KEY K_ENTER USE 0.1 0.2 0.0 0.1)" "radius must be positive")
reject(overlay_label "input_touch_button(KEY K_ENTER bad-label 0.1 0.2 0.1 0.1)" "Invalid input profile touch button")
reject(overlay_bounds "input_touch_button(KEY K_ENTER USE 1.1 0.2 0.1 0.1)" "geometry must be")
reject(overlay_hide "input_touch_button(HIDE K_ENTER HIDE 0.1 0.2 0.1 0.1)" "HIDE button cannot")
reject(overlay_conflict "set(INPUT_DIRECT_TOUCH_ACTION RETURN)\ninput_touch_button(KEY K_ENTER USE 0.1 0.2 0.1 0.1)" "cannot use both")
reject(keyboard "set(INPUT_KEYBOARD_UP K_UP)" "must have two keys")
reject(binding "set(INPUT_GAMEPAD_UP K_GAMEPAD_A)" "must have two keys")
reject(key "set(INPUT_GAMEPAD_UP 99999 0)" "Invalid input profile key")
reject(unknown_keyboard "set(INPUT_KEYBOARD_DODGE K_X 0)" "Unknown input profile action")
reject(unknown_gamepad "set(INPUT_GAMEPAD_DODGE K_GAMEPAD_B 0)" "Unknown input profile action")
reject(numeric_keyboard "set(INPUT_KEYBOARD_0 K_UP 0)" "Unknown input profile action")
reject(numeric_gamepad "set(INPUT_GAMEPAD_13 K_GAMEPAD_B 0)" "Unknown input profile action")
reject(none_keyboard "set(INPUT_KEYBOARD_NONE 0 0)" "NONE is only valid")
reject(none_gamepad "set(INPUT_GAMEPAD_NONE 0 0)" "NONE is only valid")
reject(remap_source "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,PINGOUIN,RETURN)" "belongs to Input2")
reject(remap_destination "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,RETURN,FOUDRE)" "belongs to Input2")
reject(remap_unknown "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,DODGE,RETURN)" "Unknown input profile action")
reject(remap_unknown_destination "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,RETURN,DODGE)" "Unknown input profile action")
reject(remap_none "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,NONE,RETURN)" "NONE is only valid")
reject(remap_none_destination "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,RETURN,NONE)" "NONE is only valid")
reject(remap_numeric "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,8,9)" "gameplay_action,modal_action")
reject(duplicate "set(INPUT_MODAL_REMAPS K_GAMEPAD_A,ACTION_ALWAYS,ACTION_M K_GAMEPAD_A,ACTION_ALWAYS,RETURN)" "Duplicate input profile modal remap")
reject(remap_limit "set(INPUT_MODAL_REMAPS)\nforeach(slot RANGE 0 32)\nlist(APPEND INPUT_MODAL_REMAPS K_GAMEPAD_A,ACTION_ALWAYS,RETURN)\nendforeach()" "at most 32 modal remaps")

function(reject_header name contents diagnostic)
    set(header "${OUTPUT}/${name}.h")
    file(WRITE "${header}" "${contents}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DMODULE=${MODULE}" "-DACTION_HEADER=${header}"
        -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "${diagnostic}")
        message(FATAL_ERROR "${name}: expected header error '${diagnostic}', got ${result}: ${output}${error}")
    endif()
endfunction()

file(READ "${ROOT}/SOURCES/INPUT_ACTIONS.H" actions)
reject_header(duplicate_name "${actions}\n#define INPUT_SLOT_UP 0" "Duplicate input action")
reject_header(duplicate_slot "${actions}\n#define INPUT_SLOT_OTHER 0" "Duplicate input action")
string(REPLACE "#define INPUT_SLOT_UP 0" "#define INPUT_SLOT_UP (0)" invalid "${actions}")
reject_header(expression "${invalid}" "declarations must be")
string(REPLACE "#define INPUT_SLOT_UP 0" "#define INPUT_SLOT_UP 36" invalid "${actions}")
reject_header(bounds "${invalid}" "binding slots must be")
string(REPLACE "#define INPUT_SLOT_NONE -1" "#define INPUT_SLOT_NONE 36" invalid "${actions}")
reject_header(none_value "${invalid}" "NONE must be -1")
string(REPLACE "#define INPUT_SLOT_NONE -1\n" "" invalid "${actions}")
reject_header(missing_none "${invalid}" "must declare NONE")
string(REPLACE "#define INPUT_SLOT_UP 0\n" "" invalid "${actions}")
reject_header(missing_slot "${invalid}" "all 36 binding slots")
string(REPLACE "#define INPUT_SLOT_UP 0" "#define INPUT_SLOT_UP -1" invalid "${actions}")
reject_header(negative_slot "${invalid}" "Duplicate input action|binding slots must be")
string(REPLACE "#define INPUT_SLOT_PINGOUIN 32" "#define INPUT_SLOT_PINGOUIN 31" invalid "${actions}")
string(REPLACE "#define INPUT_SLOT_WEAPON_7 31" "#define INPUT_SLOT_WEAPON_7 32" invalid "${invalid}")
reject_header(word_boundary "${invalid}" "Input2 must start at PINGOUIN")
message(STATUS "Input profile generation and invalid configurations checked")
