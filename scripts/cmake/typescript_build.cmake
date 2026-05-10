if(RUN_INSTALL)
    set(_cmd install)
else()
    set(_cmd run build)
endif()

execute_process(
    COMMAND ${BUN} ${_cmd}
    WORKING_DIRECTORY "${DIR}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE  _err
)

if(NOT _result EQUAL 0)
    if(_out)
        message("${_out}")
    endif()
    if(_err)
        message("${_err}")
    endif()
    message(FATAL_ERROR "TypeScript: ${PKG} failed")
endif()
