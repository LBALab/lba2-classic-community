file(MAKE_DIRECTORY "${RUN_DIR}/data" "${RUN_DIR}/user")
execute_process(COMMAND "${GAME}" --help RESULT_VARIABLE result OUTPUT_VARIABLE help)
if(NOT result EQUAL 0 OR NOT help MATCHES "Usage:")
    message(FATAL_ERROR "SDK-free game help failed (${result}): ${help}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=LBA2_GAME_DIR --unset=LBA2_USER_DIR --unset=LBA2_PROFILE
        "${GAME}" --headless --no-autosave --game-dir "${RUN_DIR}/data" --user-dir "${RUN_DIR}/user"
    WORKING_DIRECTORY "${RUN_DIR}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
if(NOT result EQUAL 1 OR NOT error MATCHES "Game data not found")
    message(FATAL_ERROR "SDK-free boot did not report missing data (${result}): ${output}\n${error}")
endif()
if(NOT EXISTS "${RUN_DIR}/user/adeline.log")
    message(FATAL_ERROR "SDK-free boot did not initialize its owned log")
endif()
