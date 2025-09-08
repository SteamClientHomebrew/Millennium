cmake_minimum_required(VERSION 3.10...3.21)
project(Millennium LANGUAGES C CXX)

# get the millennium version from version file
file(STRINGS "${CMAKE_SOURCE_DIR}/version" VERSION_LINES LIMIT_COUNT 2)
list(GET VERSION_LINES 1 MILLENNIUM_VERSION)
set(MILLENNIUM_VERSION "${MILLENNIUM_VERSION}")

configure_file(
  ${CMAKE_SOURCE_DIR}/version.h.in  # Input template file
  ${CMAKE_BINARY_DIR}/version.h     # Output header file
)

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Define it as a preprocessor macro
add_compile_definitions(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
message(STATUS "Git commit hash: ${GIT_COMMIT_HASH}")

# Add project root folder to preprocessor definitions
add_compile_definitions(MILLENNIUM_ROOT="${CMAKE_SOURCE_DIR}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS "${CMAKE_SOURCE_DIR}/sdk/typescript-packages/loader/build")
  set(MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS "${CMAKE_SOURCE_DIR}/assets")

  add_compile_definitions(MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS}")
  add_compile_definitions(MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS}")
endif()

if(DEFINED ENV{NIX_OS})
  message(STATUS "Building Millennium for Nix")
  set(NIX_BUILD ON)
endif()

message(STATUS "Millennium Version: ${MILLENNIUM_VERSION}")
set(BUILD_SHARED_LIBS OFF)

# set c++ directives
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT APPLE)
  # set 32-bit build
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -m32")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

if (APPLE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
endif()

if(MINGW)
  # Hide all symbols by default
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_C_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
  
  # Exclude symbols from static libraries
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,ALL")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--exclude-libs,ALL")
endif()

if(NOT UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

# Strip binary on release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
endif()

if(UNIX AND NOT APPLE AND NOT NIX_BUILD)
  add_subdirectory(cli)
  add_subdirectory(unix-hooks)
elseif(UNIX AND APPLE)
  add_subdirectory(darwin)
endif()

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "LSB Release ID: ${LSB_RELEASE_ID_SHORT}")

# Check Python version in 32 bit section
# Just include Python headers for Windows and Apple platforms
if(WIN32)
  include_directories(${CMAKE_SOURCE_DIR}/vendor/python/win32 ${CMAKE_SOURCE_DIR}/vendor/lua/src)
elseif(UNIX)
  if(APPLE)
    include_directories("$ENV{HOME}/.pyenv/versions/3.11.8/include/python3.11")

    set(MILLENNIUM__PYTHON_ENV "$ENV{HOME}/.pyenv/versions/3.11.8")
    set(LIBPYTHON_RUNTIME_PATH "$ENV{HOME}/.pyenv/versions/3.11.8/lib/libpython3.11.dylib")
  else()
    if(NIX_BUILD)
      add_compile_definitions(_NIX_OS=ON)
      add_compile_definitions(__NIX_SELF_PATH="$ENV{out}") 
      add_compile_definitions(__NIX_SHIMS_PATH="$ENV{shims}")
      add_compile_definitions(__NIX_ASSETS_PATH="$ENV{assets}")
    endif()

    # Try to find required version of Python
    # Python guarantee to have API and ABI compatible within a same major and minor versions
    # Harden version range to to find only 3.11 python
    find_package(Python 3.11 EXACT COMPONENTS Development)

    if(PYTHON_FOUND)
      # Run simple test to check if python is working with current flags
      try_compile(PYTHON_TEST_RESULT
        "${CMAKE_BINARY_DIR}"
        SOURCES "${CMAKE_CURRENT_LIST_DIR}/tests/FindPython_test.cc"
        LINK_LIBRARIES Python::Module)

      if(PYTHON_TEST_RESULT)
        message(STATUS "Found suitable Python version ${Python_VERSION}")
        set(LIBPYTHON_RUNTIME_PATH ${Python_LIBRARIES})
        if(NOT Python_ROOT_DIR)
          cmake_path(GET Python_LIBRARY_DIRS PARENT_PATH Python_ROOT_DIR)
        endif()
        set(MILLENNIUM__PYTHON_ENV ${Python_ROOT_DIR})
      else()
        message(STATUS "Python ABI mismatch, rolling back to default one")
      endif()
    else()
      # Use this var to check if the package been found and it's 32bit
      set(PYTHON_TEST_RESULT FALSE)
      message(STATUS "No Python package found, rolling back to default one")
    endif()

    if(NOT ${PYTHON_TEST_RESULT})
    
      set(MILLENNIUM__PYTHON_ENV "/opt/python-i686-3.11.8") 
      set(LIBPYTHON_RUNTIME_PATH "/opt/python-i686-3.11.8/lib/libpython-3.11.8.so")

      if(DISTRO_ARCH OR LSB_RELEASE_ID_SHORT STREQUAL "Arch")
        include_directories("/opt/python-i686-3.11.8/include/python3.11/")

        # Function to check if a program exists in PATH
        function(check_program_exists program_name result_var)
          find_program(${program_name}_EXECUTABLE ${program_name})
          if(${program_name}_EXECUTABLE)
              set(${result_var} TRUE PARENT_SCOPE)
          else()
              set(${result_var} FALSE PARENT_SCOPE)
          endif()
        endfunction()

        # List of common AUR helpers with their update command syntax for "millennium" package
        set(AUR_HELPERS
          "yay"
          "paru"
          "aurman"
          "pikaur"
          "pamac"
          "trizen"
          "pacaur"
          "aura"
        )

        # Map AUR helpers to their respective update commands
        set(yay_UPDATE_COMMAND "yay -Syu millennium")
        set(paru_UPDATE_COMMAND "paru -Syu millennium")
        set(aurman_UPDATE_COMMAND "aurman -Syu millennium")
        set(pikaur_UPDATE_COMMAND "pikaur -Syu millennium")
        set(pamac_UPDATE_COMMAND "pamac upgrade millennium")
        set(trizen_UPDATE_COMMAND "trizen -Syu millennium")
        set(pacaur_UPDATE_COMMAND "pacaur -Syu millennium")
        set(aura_UPDATE_COMMAND "aura -Ayu millennium")

        # Default fallback for plain pacman (though it won't work for AUR packages directly)
        set(pacman_UPDATE_COMMAND "sudo pacman -Syu millennium")

        find_program(PACMAN_EXECUTABLE pacman)
        if(NOT PACMAN_EXECUTABLE)
          message(STATUS "Not running on an Arch-based system (pacman not found)")
          set(AUR_HELPER "none")
          set(UPDATE_COMMAND "")
          else()
          message(STATUS "Arch-based system detected")

          set(AUR_HELPER "none")
          foreach(helper ${AUR_HELPERS})
              check_program_exists(${helper} HAS_${helper})
              if(HAS_${helper})
                  set(AUR_HELPER ${helper})
                  set(UPDATE_COMMAND ${${helper}_UPDATE_COMMAND})
                  break()
              endif()
          endforeach()

          if(AUR_HELPER STREQUAL "none")
              message(STATUS "No AUR helper found. User likely uses plain pacman.")
              message(STATUS "Note: Plain pacman cannot directly install AUR packages.")
              set(UPDATE_COMMAND ${pacman_UPDATE_COMMAND})
              message(STATUS "Fallback command: ${UPDATE_COMMAND}")
          else()
              message(STATUS "AUR helper found: ${AUR_HELPER}")
              message(STATUS "Update command: ${UPDATE_COMMAND}")
          endif()
        endif()

        set(AUR_HELPER ${AUR_HELPER} CACHE STRING "Detected AUR helper")
        set(UPDATE_COMMAND ${UPDATE_COMMAND} CACHE STRING "Command to update millennium package")

        if(NOT AUR_HELPER STREQUAL "none")
          message(STATUS "Using ${UPDATE_COMMAND} as update script to update Millennium.")
          set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "${UPDATE_COMMAND}")
        else()
          message(STATUS "No AUR helper found. Please update Millennium manually.")
          set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "Couldn't find AUR helper. Please update Millennium manually.")
        endif()

      else()
        include_directories("${CMAKE_SOURCE_DIR}/vendor/python/posix")

        set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "curl -fsSL 'https://raw.githubusercontent.com/SteamClientHomebrew/Millennium/refs/heads/main/scripts/install.sh' | sh")
      endif()
    endif()
  endif()
endif()

message(STATUS "Set Python runtime library to ${LIBPYTHON_RUNTIME_PATH}")

if(WIN32 AND NOT GITHUB_ACTION_BUILD)
  execute_process(
    COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE steam_path
    ERROR_VARIABLE reg_error
  )

  if(result EQUAL 0)
    string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" extracted_path "${steam_path}")
    string(REPLACE "\n" "" extracted_path "${extracted_path}")

    message(STATUS "Build Steam Path: ${extracted_path}")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${extracted_path})
    set(LIBRARY_OUTPUT_DIRECTORY ${extracted_path})
  else()
    message(WARNING "Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
  endif()
endif()

# Set version information
add_compile_definitions(MILLENNIUM_VERSION="${MILLENNIUM_VERSION}")

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/vendor/fmt/include
  ${CMAKE_SOURCE_DIR}/vendor/asio/asio/include
  ${CMAKE_SOURCE_DIR}/vendor/nlohmann/include
  ${CMAKE_SOURCE_DIR}/vendor/websocketpp
  ${CMAKE_SOURCE_DIR}/vendor/crow/include
  ${CMAKE_SOURCE_DIR}/vendor/ini/src
)

add_compile_definitions(
  "WINRT_NO_COROUTINES"
  "LJ_TARGET_X86"
  "LJ_HASJIT"
  "CURL_STATICLIB"
  "_WEBSOCKETPP_CPP11_THREAD_"
  "_WEBSOCKETPP_CPP11_TYPE_TRAITS_"
  "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_"
  "ASIO_STANDALONE"
  "ASIO_HAS_STD_INVOKE_RESULT"
  "FMT_HEADER_ONLY"
  "_CRT_SECURE_NO_WARNINGS"
)

if(WIN32)
  add_subdirectory(boot/win32)
endif()

if(NOT USE_INTERNAL_FPCONV)
    # Use libc number conversion routines (strtod(), sprintf())
    set(FPCONV_SOURCES fpconv.c)
else()
    # Use internal number conversion routines
    add_definitions(-DUSE_INTERNAL_FPCONV)
    set(FPCONV_SOURCES g_fmt.c dtoa.c)

    include(TestBigEndian)
    TEST_BIG_ENDIAN(IEEE_BIG_ENDIAN)
    if(IEEE_BIG_ENDIAN)
        add_definitions(-DIEEE_BIG_ENDIAN)
    endif()

    if(MULTIPLE_THREADS)
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        find_package(Threads REQUIRED)
        if(NOT CMAKE_USE_PTHREADS_INIT)
            message(FATAL_ERROR
                    "Pthreads not found - required by MULTIPLE_THREADS option")
        endif()
        add_definitions(-DMULTIPLE_THREADS)
    endif()
endif()

# Handle platforms missing isinf() macro (Eg, some Solaris systems).
include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(isinf math.h HAVE_ISINF)
if(NOT HAVE_ISINF)
    add_definitions(-DUSE_INTERNAL_ISINF)
endif()

file(GLOB_RECURSE BUILTINS_SRC "src/*.cc")
file(GLOB_RECURSE EXCLUDE_SYS_SRC "src/sys/*.cc")
list(REMOVE_ITEM BUILTINS_SRC ${EXCLUDE_SYS_SRC})
set(SOURCE_FILES ${BUILTINS_SRC})

if(MINGW)
  execute_process(
    COMMAND bash -c "cd '${CMAKE_SOURCE_DIR}/vendor/lua' && mingw32-make BUILDMODE=static"
    RESULT_VARIABLE LUAJIT_BUILD_RESULT
    OUTPUT_VARIABLE LUAJIT_BUILD_OUTPUT
    ERROR_VARIABLE LUAJIT_BUILD_ERROR
  )
  if(NOT LUAJIT_BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to build LuaJIT static library:\n${LUAJIT_BUILD_ERROR}")
  else()
    message(STATUS "LuaJIT static library built successfully.")
  endif()
elseif(UNIX)
  execute_process(
    COMMAND make -C "${CMAKE_SOURCE_DIR}/vendor/lua" CC="gcc -m32" CFLAGS="-m32 -O2 -fPIC" BUILDMODE=static
    RESULT_VARIABLE LUAJIT_BUILD_RESULT
    OUTPUT_VARIABLE LUAJIT_BUILD_OUTPUT
    ERROR_VARIABLE LUAJIT_BUILD_ERROR
  )
  if(NOT LUAJIT_BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to build LuaJIT static library:\n${LUAJIT_BUILD_ERROR}")
  else()
    message(STATUS "LuaJIT static library built successfully.")
  endif()
endif()

set(LUAJIT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua/src)
set(LUAJIT_STATIC ${LUAJIT_DIR}/libluajit.a)

list(APPEND SOURCE_FILES 
  "vendor/lua-cjson/lua_cjson.c" 
  "vendor/lua-cjson/strbuf.c"
  "vendor/lua-cjson/${FPCONV_SOURCES}"
)

if(WIN32)
  list(APPEND SOURCE_FILES "src/plat/init_win32.cc")
elseif(UNIX)
  list(APPEND SOURCE_FILES "src/plat/init_posix.cc")
endif()

if(WIN32)
  add_library(Millennium SHARED "${SOURCE_FILES}")
elseif(UNIX)
  # add_executable(Millennium "${SOURCE_FILES}")
  # add_compile_definitions(MILLENNIUM_EXECUTABLE)
  if (APPLE)
    add_executable(Millennium "${SOURCE_FILES}")
  else()
    add_library(Millennium SHARED "${SOURCE_FILES}")
  endif()
  add_compile_definitions(MILLENNIUM_SHARED)

  target_compile_definitions(Millennium PRIVATE MILLENNIUM__PYTHON_ENV="${MILLENNIUM__PYTHON_ENV}")
  target_compile_definitions(Millennium PRIVATE LIBPYTHON_RUNTIME_PATH="${LIBPYTHON_RUNTIME_PATH}")
  target_compile_definitions(Millennium PRIVATE MILLENNIUM__UPDATE_SCRIPT_PROMPT="${MILLENNIUM__UPDATE_SCRIPT_PROMPT}")
endif()

if(NOT APPLE)
  set_target_properties(Millennium PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
  target_compile_options(Millennium PRIVATE -m32)
endif()

if(WIN32)
  set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
  set_target_properties(Millennium PROPERTIES PREFIX "")
  set_target_properties(Millennium PROPERTIES NO_EXPORT TRUE)
elseif(UNIX AND NOT APPLE)
  set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
  set_target_properties(Millennium PROPERTIES PREFIX "lib")
  set_target_properties(Millennium PROPERTIES SUFFIX "_x86.so")
endif()

if(MSVC)
  # prevent MSVC from generating .lib and .exp archives
  set_target_properties(Millennium PROPERTIES ARCHIVE_OUTPUT_NAME "" LINK_FLAGS "/NOEXP")
endif()

find_program(WINDRES windres)

if(WINDRES)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/version.o
    COMMAND ${WINDRES} -i ${CMAKE_SOURCE_DIR}/scripts/version.rc -o ${CMAKE_BINARY_DIR}/version.o
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/version.rc
  )

  add_custom_target(resource DEPENDS ${CMAKE_BINARY_DIR}/version.o)
  add_dependencies(Millennium resource)
  target_link_libraries(Millennium ${CMAKE_BINARY_DIR}/version.o)
endif()

if(NIX_BUILD)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(Millennium OpenSSL::SSL)
endif()

set_target_properties(Millennium PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_compile_features(Millennium PUBLIC cxx_std_17)

find_package(unofficial-minizip CONFIG REQUIRED) # used for extracting zip files
find_package(CURL REQUIRED) # used for web requests
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBGIT2 REQUIRED libgit2)

include_directories(${LIBGIT2_INCLUDE_DIRS})
link_directories(${LIBGIT2_LIBRARY_DIRS})
add_definitions(${LIBGIT2_CFLAGS_OTHER})

if (WIN32)
set(LIBGIT2_LIBRARIES
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libgit2.a
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libhttp_parser.a
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libpcre.a
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libzlib.a
)
elseif(UNIX)
set(LIBGIT2_LIBRARIES
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libgit2.a
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libhttp_parser.a
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libpcre.a
  ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libz.a
)
endif()

if(WIN32)
  if(GITHUB_ACTION_BUILD)
    set(PYTHON_LIB "${CMAKE_SOURCE_DIR}/build/python/python311.lib")
  else()
    set(PYTHON_LIB ${CMAKE_SOURCE_DIR}/vendor/python/python311.lib ${CMAKE_SOURCE_DIR}/vendor/python/python311_d.lib)
  endif()

  find_package(minhook CONFIG REQUIRED) # used for hooking functions in steam 

  target_link_libraries(Millennium
    ${LIBGIT2_LIBRARIES}
    wsock32
    Iphlpapi
    DbgHelp
    rpcrt4
    winhttp
    ${PYTHON_LIB}
    minhook::minhook
  )

elseif(UNIX)
  if(APPLE)
    target_link_libraries(Millennium "$ENV{HOME}/.pyenv/versions/3.11.8/lib/libpython3.11.dylib")
  else()
    if(PYTHON_TEST_RESULT)
      target_link_libraries(Millennium Python::Module)
    else()
      target_link_libraries(Millennium "/opt/python-i686-3.11.8/lib/libpython-3.11.8.so")
    endif()
  endif()
endif()

# Link required libraries for all platforms
target_link_libraries(Millennium 
  unofficial::minizip::minizip 
  CURL::libcurl 
  ${LUAJIT_STATIC}
  ${LIBGIT2_LIBRARIES}
)