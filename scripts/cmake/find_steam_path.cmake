function (find_steam_path project_name)
    if(WIN32 AND NOT GITHUB_ACTION_BUILD)
        # resolve %LOCALAPPDATA%/Millennium as the dev output root
        set(millennium_data_path "$ENV{LOCALAPPDATA}/Millennium")
        message(STATUS "[Millennium] Dev output root: ${millennium_data_path}")

        # libraries go to lib/, executables go to bin/
        get_target_property(target_type ${project_name} TYPE)
        if(target_type STREQUAL "SHARED_LIBRARY")
            set(out_dir "${millennium_data_path}/lib")
        else()
            set(out_dir "${millennium_data_path}/bin")
        endif()

        message(STATUS "[Millennium] Target '${project_name}' -> ${out_dir}")

        set_target_properties(${project_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${out_dir}"
            LIBRARY_OUTPUT_DIRECTORY "${out_dir}"
        )

        # expose the steam path for targets that need it (e.g. hardlinks)
        execute_process(COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"
                        RESULT_VARIABLE result OUTPUT_VARIABLE steam_path ERROR_VARIABLE reg_error)
        if(result EQUAL 0)
            string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" out_steam_path "${steam_path}")
            string(REPLACE "\n" "" out_steam_path "${out_steam_path}")
            set(out_steam_path "${out_steam_path}" PARENT_SCOPE)
        endif()

        foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/pdb/${cfg})

            set_target_properties(${project_name} PROPERTIES
                COMPILE_PDB_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/pdb/${cfg}
                PDB_OUTPUT_DIRECTORY_${cfg}         ${CMAKE_BINARY_DIR}/pdb/${cfg}
            )
        endforeach()

        target_link_options(${project_name} PRIVATE "/INCREMENTAL" "/ILK:${CMAKE_BINARY_DIR}/pdb/${project_name}.ilk")
    endif()
endfunction()
