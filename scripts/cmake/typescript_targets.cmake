find_program(BUN_EXECUTABLE bun REQUIRED)
set(TS_ROOT    "${MILLENNIUM_BASE}/src/typescript")
set(TS_STAMPS  "${CMAKE_BINARY_DIR}/ts_stamps")
set(TS_BUILDER "${MILLENNIUM_BASE}/scripts/cmake/typescript_build.cmake")
file(MAKE_DIRECTORY "${TS_STAMPS}")

add_custom_command(
    OUTPUT  "${TS_STAMPS}/install.stamp"
    COMMAND ${CMAKE_COMMAND}
        -D "BUN=${BUN_EXECUTABLE}"
        -D "PKG=install"
        -D "DIR=${TS_ROOT}"
        -D "RUN_INSTALL=1"
        -P "${TS_BUILDER}"
    COMMAND ${CMAKE_COMMAND} -E touch "${TS_STAMPS}/install.stamp"
    COMMENT "TypeScript: bun install"
    VERBATIM
)
add_custom_target(ts_install DEPENDS "${TS_STAMPS}/install.stamp")

file(GLOB_RECURSE _src_ttc    CONFIGURE_DEPENDS "${TS_ROOT}/ttc/src/*.ts"    "${TS_ROOT}/ttc/src/*.tsx")
file(GLOB_RECURSE _src_client CONFIGURE_DEPENDS "${TS_ROOT}/sdk/packages/client/src/*.ts"  "${TS_ROOT}/sdk/packages/client/src/*.tsx")
file(GLOB_RECURSE _src_webkit CONFIGURE_DEPENDS "${TS_ROOT}/sdk/packages/browser/src/*.ts" "${TS_ROOT}/sdk/packages/browser/src/*.tsx")
file(GLOB_RECURSE _src_api    CONFIGURE_DEPENDS "${TS_ROOT}/sdk/packages/loader/src/*.ts"  "${TS_ROOT}/sdk/packages/loader/src/*.tsx")
file(GLOB_RECURSE _src_cdp_isolated_ctx CONFIGURE_DEPENDS "${TS_ROOT}/sdk/packages/cdp-isolated-ctx/src/*.ts" "${TS_ROOT}/sdk/packages/cdp-isolated-ctx/src/*.tsx")
file(GLOB_RECURSE _src_core   CONFIGURE_DEPENDS
    "${TS_ROOT}/frontend/*.ts"
    "${TS_ROOT}/frontend/*.tsx"
)
file(GLOB _src_locales CONFIGURE_DEPENDS
    "${MILLENNIUM_BASE}/src/locales/*.json"
)

macro(_ts_package _name _dir)
  set(_stamp "${TS_STAMPS}/${_name}.stamp")
  file(RELATIVE_PATH _reldir "${MILLENNIUM_BASE}" "${_dir}")
  add_custom_command(
        OUTPUT  "${_stamp}"
        COMMAND ${CMAKE_COMMAND}
            -D "BUN=${BUN_EXECUTABLE}"
            -D "PKG=${_name}"
            -D "DIR=${_dir}"
            -P "${TS_BUILDER}"
        COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
        DEPENDS
            "${TS_STAMPS}/install.stamp"
            ${ARGN}
        COMMENT "Building TS library ${_reldir}"
        VERBATIM
    )
  add_custom_target(ts_${_name} DEPENDS "${_stamp}")
  add_dependencies(ts_${_name} ts_install)
endmacro()

_ts_package(ttc "${TS_ROOT}/ttc"
    "${TS_ROOT}/ttc/package.json"
    "${TS_ROOT}/ttc/rollup.config.js"
    ${_src_ttc}
)

_ts_package(client "${TS_ROOT}/sdk/packages/client"
    "${TS_STAMPS}/ttc.stamp"
    "${TS_ROOT}/sdk/packages/client/package.json"
    "${TS_ROOT}/sdk/packages/client/tsconfig.json"
    ${_src_client}
)
add_dependencies(ts_client ts_ttc)

_ts_package(webkit "${TS_ROOT}/sdk/packages/browser"
    "${TS_STAMPS}/ttc.stamp"
    "${TS_ROOT}/sdk/packages/browser/package.json"
    "${TS_ROOT}/sdk/packages/browser/tsconfig.json"
    ${_src_webkit}
)
add_dependencies(ts_webkit ts_ttc)

_ts_package(api "${TS_ROOT}/sdk/packages/loader"
    "${TS_STAMPS}/client.stamp"
    "${TS_ROOT}/sdk/packages/loader/package.json"
    "${TS_ROOT}/sdk/packages/loader/rollup.config.js"
    "${TS_ROOT}/sdk/packages/loader/tsconfig.json"
    ${_src_api}
)
add_dependencies(ts_api ts_client)

_ts_package(cdp_isolated_ctx "${TS_ROOT}/sdk/packages/cdp-isolated-ctx"
    "${TS_ROOT}/sdk/packages/cdp-isolated-ctx/package.json"
    "${TS_ROOT}/sdk/packages/cdp-isolated-ctx/rollup.config.js"
    "${TS_ROOT}/sdk/packages/cdp-isolated-ctx/tsconfig.json"
    ${_src_cdp_isolated_ctx}
)

_ts_package(core "${TS_ROOT}/frontend"
    "${TS_STAMPS}/client.stamp"
    "${TS_STAMPS}/webkit.stamp"
    "${TS_ROOT}/frontend/package.json"
    ${_src_core}
    ${_src_locales}
)
add_dependencies(ts_core ts_client ts_webkit)

set(_virtfs_h "${MILLENNIUM_BASE}/src/include/millennium/virtfs.h")
if(WIN32 AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
  set(_virtfs_outputs "${_virtfs_h}" "${MILLENNIUM_BASE}/scripts/resources.rc")
else()
  set(_virtfs_outputs "${_virtfs_h}")
endif()
add_custom_command(
    OUTPUT  ${_virtfs_outputs}
    COMMAND ${CMAKE_COMMAND}
        -D "MILLENNIUM_BASE=${MILLENNIUM_BASE}"
        -D "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        -P "${MILLENNIUM_BASE}/scripts/cmake/generate_virtfs.cmake"
    DEPENDS
        "${TS_STAMPS}/api.stamp"
        "${TS_STAMPS}/core.stamp"
        "${TS_STAMPS}/cdp_isolated_ctx.stamp"
    COMMENT "Generating virtfs.h"
    VERBATIM
)
add_custom_target(virtfs_header DEPENDS "${_virtfs_h}")
add_dependencies(virtfs_header ts_api ts_core ts_cdp_isolated_ctx)
