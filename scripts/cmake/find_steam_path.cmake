function (find_steam_path project_name)
    if(WIN32 AND NOT GITHUB_ACTION_BUILD)
        execute_process(COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath" RESULT_VARIABLE result OUTPUT_VARIABLE steam_path ERROR_VARIABLE reg_error)
        if(result EQUAL 0)
            string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" out_steam_path "${steam_path}")
            string(REPLACE "\n" "" out_steam_path "${out_steam_path}")
            execute_process(
                COMMAND powershell -NoProfile -Command "(New-Object -ComObject Scripting.FileSystemObject).GetFolder('${out_steam_path}').Path"
                OUTPUT_VARIABLE out_steam_path OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            set(out_steam_path "${out_steam_path}" PARENT_SCOPE)

            # Determine output dir based on target type:
            #   shared libraries  -> <steam>/millennium/lib/
            #   executables       -> <steam>/millennium/bin/
            get_target_property(_target_type ${project_name} TYPE)
            if(_target_type STREQUAL "SHARED_LIBRARY" OR _target_type STREQUAL "MODULE_LIBRARY")
                set(_out_dir "${out_steam_path}/millennium/lib")
            else()
                set(_out_dir "${out_steam_path}/millennium/bin")
            endif()

            set_target_properties(${project_name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${_out_dir}"
                LIBRARY_OUTPUT_DIRECTORY "${_out_dir}"
            )

            foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
                file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/pdb/${cfg})

                set_target_properties(${project_name} PROPERTIES
                    COMPILE_PDB_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/pdb/${cfg}
                    PDB_OUTPUT_DIRECTORY_${cfg}         ${CMAKE_BINARY_DIR}/pdb/${cfg}
                )
            endforeach()

            target_link_options(${project_name} PRIVATE "/INCREMENTAL" "/ILK:${CMAKE_BINARY_DIR}/pdb/${project_name}.ilk")
        else()
            message(WARNING "[MLNM] Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
        endif()
    endif()
endfunction()
