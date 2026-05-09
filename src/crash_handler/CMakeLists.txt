cmake_minimum_required(VERSION 3.21)
project(millennium_crashhandler CXX)

include(${MILLENNIUM_BASE}/scripts/cmake/millennium_version.cmake)

set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew 2025")
set(VERSION_TRADEMARK1   "All Rights Reserved")
set(VERSION_DESCRIPTION  "Crash handler watchdog for Millennium")
set(VERSION_FILENAME     "${MILLENNIUM_CRASH_HANDLER_NAME}.exe")
set(VERSION_PRODUCT      "Millennium Crash Handler")
set(VERSION_FILETYPE     "VFT_APP")

set(VERSION_FILE "${CMAKE_BINARY_DIR}/${MILLENNIUM_CRASH_HANDLER_NAME}.rc")
configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${VERSION_FILE} @ONLY)

set(CRASH_HANDLER_SOURCES
    main.cc
    ${MILLENNIUM_BASE}/src/shared/crash_report.cc
    ${VERSION_FILE}
)

add_executable(${MILLENNIUM_CRASH_HANDLER_NAME} ${CRASH_HANDLER_SOURCES})

target_include_directories(${MILLENNIUM_CRASH_HANDLER_NAME} PRIVATE
    ${MILLENNIUM_BASE}/src
    ${MILLENNIUM_BASE}/src/include
)

target_link_libraries(${MILLENNIUM_CRASH_HANDLER_NAME} dbghelp)

# always ship with debug info
target_compile_options(${MILLENNIUM_CRASH_HANDLER_NAME} PRIVATE /Z7)
target_link_options(${MILLENNIUM_CRASH_HANDLER_NAME} PRIVATE /DEBUG /ENTRY:mainCRTStartup
    "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
)

set_target_properties(${MILLENNIUM_CRASH_HANDLER_NAME} PROPERTIES
    OUTPUT_NAME "${MILLENNIUM_CRASH_HANDLER_NAME}"
    WIN32_EXECUTABLE TRUE
)

# In local dev builds, put the exe in the Steam directory alongside millennium.dll.
# In CI (GITHUB_ACTION_BUILD), find_steam_path is a no-op and CMAKE_RUNTIME_OUTPUT_DIRECTORY handles it.
if(NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
    find_steam_path(${MILLENNIUM_CRASH_HANDLER_NAME})
endif()
