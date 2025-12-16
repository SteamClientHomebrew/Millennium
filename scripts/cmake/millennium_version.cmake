# Read the Millennium version from the version file
file(STRINGS "${MILLENNIUM_BASE}/scripts/version" VERSION_LINES LIMIT_COUNT 2)
list(GET VERSION_LINES 1 MILLENNIUM_VERSION)
string(STRIP "${MILLENNIUM_VERSION}" MILLENNIUM_VERSION)

# Make cmake re-run if the version file changes
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MILLENNIUM_BASE}/scripts/version")

# Generate version.h from version.h.in
configure_file(${MILLENNIUM_BASE}/src/include/millennium/version.h.in ${CMAKE_BINARY_DIR}/version.h)
# Get the current git commit hash
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${MILLENNIUM_BASE} OUTPUT_VARIABLE GIT_COMMIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "[Millennium] Version: ${MILLENNIUM_VERSION}")
message(STATUS "[Millennium] Git commit hash: ${GIT_COMMIT_HASH}")

add_compile_definitions(
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
    MILLENNIUM_ROOT="${MILLENNIUM_BASE}"
    MILLENNIUM_VERSION="${MILLENNIUM_VERSION}"
)
