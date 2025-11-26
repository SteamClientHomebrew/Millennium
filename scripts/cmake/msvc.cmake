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
set(X86_TOOLCHAIN_ARGS "")
