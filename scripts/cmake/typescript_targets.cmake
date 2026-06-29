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

file(GLOB_RECURSE _src_ttc  CONFIGURE_DEPENDS "${TS_ROOT}/ttc/src/*.ts" "${TS_ROOT}/ttc/src/*.tsx")
file(GLOB_RECURSE _src_sdk  CONFIGURE_DEPENDS "${TS_ROOT}/sdk/src/*.ts" "${TS_ROOT}/sdk/src/*.tsx")
file(GLOB_RECURSE _src_core CONFIGURE_DEPENDS
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

_ts_package(sdk "${TS_ROOT}/sdk"
    "${TS_STAMPS}/ttc.stamp"
    "${TS_ROOT}/sdk/package.json"
    "${TS_ROOT}/sdk/rollup.config.js"
    "${TS_ROOT}/sdk/tsconfig.json"
    ${_src_sdk}
)
add_dependencies(ts_sdk ts_ttc)

_ts_package(core "${TS_ROOT}/frontend"
    "${TS_STAMPS}/sdk.stamp"
    "${TS_ROOT}/frontend/package.json"
    ${_src_core}
    ${_src_locales}
)
add_dependencies(ts_core ts_sdk)

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
        "${TS_STAMPS}/sdk.stamp"
        "${TS_STAMPS}/core.stamp"
    COMMENT "Generating virtfs.h"
    VERBATIM
)
add_custom_target(virtfs_header DEPENDS "${_virtfs_h}")
add_dependencies(virtfs_header ts_sdk ts_core)
