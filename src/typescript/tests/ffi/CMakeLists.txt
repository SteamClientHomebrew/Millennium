find_program(BUN_EXECUTABLE bun
  HINTS ENV BUN_INSTALL
  PATH_SUFFIXES bin
)
if(NOT BUN_EXECUTABLE)
  message(FATAL_ERROR "bun was not found on PATH.")
endif()

set(_ffi_workspace_root "${CMAKE_SOURCE_DIR}/src/typescript")

add_test(NAME ffi_install
  COMMAND "${BUN_EXECUTABLE}" install --frozen-lockfile
  WORKING_DIRECTORY "${_ffi_workspace_root}"
)
set_tests_properties(ffi_install PROPERTIES FIXTURES_SETUP ffi_deps)

add_test(NAME ffi_contract
  COMMAND "${BUN_EXECUTABLE}" test
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_tests_properties(ffi_contract PROPERTIES FIXTURES_REQUIRED ffi_deps)
