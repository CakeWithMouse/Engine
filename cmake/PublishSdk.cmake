execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BINARY}"
    --prefix "${DESTINATION}" --config "${CONFIGURATION}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
file(WRITE "${BINARY}/publish-sdk-${CONFIGURATION}.log" "${output}\n${error}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SDK publication failed: ${error}\nSee ${BINARY}/publish-sdk-${CONFIGURATION}.log")
endif()
message(STATUS "CMake SDK: ${DESTINATION}")
