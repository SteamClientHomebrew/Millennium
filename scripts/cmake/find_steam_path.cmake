function (find_steam_path project_name) 
    if(WIN32 AND NOT GITHUB_ACTION_BUILD)
        execute_process(COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath" RESULT_VARIABLE result OUTPUT_VARIABLE steam_path ERROR_VARIABLE reg_error)
        if(result EQUAL 0)
            string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" out_steam_path "${steam_path}")
            string(REPLACE "\n" "" out_steam_path "${out_steam_path}")
            message(STATUS "[Millennium] Target build path: ${out_steam_path}")

            set(out_steam_path "${out_steam_path}" PARENT_SCOPE)

            set_target_properties(${project_name} PROPERTIES 
                RUNTIME_OUTPUT_DIRECTORY "${out_steam_path}"
                LIBRARY_OUTPUT_DIRECTORY "${out_steam_path}"
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
            message(WARNING "[Millennium] Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
        endif()
    endif()
endfunction()