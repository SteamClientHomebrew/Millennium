include_guard(GLOBAL)

set(MILLENNIUM_LINUX_HOST_LIBRARY_PATHS
    "/run/host/usr/lib64;/run/host/usr/lib;/run/host/usr/lib/x86_64-linux-gnu"
    CACHE STRING "Host library directories visible inside SteamRT3 (empty disables host RPATH)")

function(millennium_configure_linux_host_rpath target)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR DISTRO_NIX OR MILLENNIUM_NIX_64BIT OR NOT MILLENNIUM_LINUX_HOST_LIBRARY_PATHS)
        return()
    endif()

    set_property(TARGET "${target}" APPEND PROPERTY BUILD_RPATH "${MILLENNIUM_LINUX_HOST_LIBRARY_PATHS}")
    set_property(TARGET "${target}" APPEND PROPERTY INSTALL_RPATH "${MILLENNIUM_LINUX_HOST_LIBRARY_PATHS}")
    target_link_options("${target}" PRIVATE "LINKER:--disable-new-dtags")
endfunction()
