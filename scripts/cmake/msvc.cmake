# Reverse the MSVC platform from the compiler path cmake set.
get_filename_component(COMPILER_DIR   ${CMAKE_CXX_COMPILER} DIRECTORY)
get_filename_component(COMPILER_DIR   ${COMPILER_DIR}       DIRECTORY)
get_filename_component(COMPILER_DIR   ${COMPILER_DIR}       DIRECTORY)
get_filename_component(COMPILER_DIR   ${COMPILER_DIR}       DIRECTORY)

# Get the base directory of the MSVC installation
get_filename_component(BASE_DIRECTORY ${COMPILER_DIR}       DIRECTORY)
get_filename_component(BASE_DIRECTORY ${BASE_DIRECTORY}     DIRECTORY)
get_filename_component(BASE_DIRECTORY ${BASE_DIRECTORY}     DIRECTORY)
get_filename_component(BASE_DIRECTORY ${BASE_DIRECTORY}     DIRECTORY)

set(USER_CMAKE_MAKE_PROGRAM "${BASE_DIRECTORY}/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe")

# MSVC-specific toolchain args for building Millennium in 32-bit mode
set(X86_TOOLCHAIN_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/x86_msvc_toolchain.cmake
    -DMSVC_BASE_PATH=${COMPILER_DIR}
    -DMSVC_SDK_VERSION=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}
    -DMSVC_TOOLSET_VERSION=${MSVC_TOOLSET_VERSION}
)
