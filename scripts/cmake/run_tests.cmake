execute_process(
  COMMAND "${CTEST}" --output-on-failure -C "${CONFIG}"
  WORKING_DIRECTORY "${WORKING_DIR}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err
)

if(NOT _rc EQUAL 0)
  message("${_out}")
  message("${_err}")
  message(FATAL_ERROR " FAIL")
endif()

message(" OK")
