if(WIN32 AND NOT GITHUB_ACTION_BUILD)
    execute_process(COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath" RESULT_VARIABLE result OUTPUT_VARIABLE steam_path ERROR_VARIABLE reg_error)
    if(result EQUAL 0)
        string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" out_steam_path "${steam_path}")
        string(REPLACE "\n" "" out_steam_path "${out_steam_path}")
        message(STATUS "[Millennium] Target build path: ${out_steam_path}")

        set(ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
        set(LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
        set(PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb")
        set(COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb")  
        
        if(MILLENNIUM_32BIT)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${out_steam_path})
        else()
            file(MAKE_DIRECTORY "${out_steam_path}/ext/compat64")
            # build 64 bit compatibility libraries to steam/ext/compat64
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${out_steam_path}/ext/compat64)
        endif()

    else()
        message(WARNING "[Millennium] Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
    endif()
endif()
