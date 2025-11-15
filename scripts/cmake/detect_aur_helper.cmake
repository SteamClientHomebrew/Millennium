find_program(LSB_RELEASE_EXEC lsb_release)
if(LSB_RELEASE_EXEC)
    execute_process(
        COMMAND ${LSB_RELEASE_EXEC} -is
        OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "LSB Release ID: ${LSB_RELEASE_ID_SHORT}")
endif()

function(detect_aur_helper result_var)
    set(AUR_HELPERS yay paru aurman pikaur pamac trizen pacaur aura)

    find_program(PACMAN_EXECUTABLE pacman)
    if(NOT PACMAN_EXECUTABLE)
        message(STATUS "Not running on an Arch-based system (pacman not found)")
        set(${result_var} "Couldn't find AUR helper. Please update Millennium manually." PARENT_SCOPE)
        return()
    endif()
    message(STATUS "Arch-based system detected")
    foreach(helper ${AUR_HELPERS})
        find_program(${helper}_EXECUTABLE ${helper})
        if(${helper}_EXECUTABLE)
            if(helper STREQUAL "pamac")
                set(${result_var} "pamac upgrade millennium" PARENT_SCOPE)
            elseif(helper STREQUAL "aura")
                set(${result_var} "aura -Ayu millennium" PARENT_SCOPE)
            else()
                set(${result_var} "${helper} -Syu millennium" PARENT_SCOPE)
            endif()
            message(STATUS "AUR helper found: ${helper}")
            return()
        endif()
    endforeach()

    message(STATUS "No AUR helper found. Please update Millennium manually.")
    set(${result_var} "Couldn't find AUR helper. Please update Millennium manually." PARENT_SCOPE)
endfunction()