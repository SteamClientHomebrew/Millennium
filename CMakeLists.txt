cmake_minimum_required(VERSION 3.14)
project(millennium LANGUAGES CXX)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wno-format-security)
endif()

# Specify C++ standard globally
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories for all configurations
include_directories(
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/vendor/fmt/include"
    "${CMAKE_SOURCE_DIR}/src/deps/imgui"
    "${CMAKE_SOURCE_DIR}/vendor/glfw"
    "${CMAKE_SOURCE_DIR}/vendor/asio/asio/include"
    "${CMAKE_SOURCE_DIR}/vendor/nlohmann/include"
    "${CMAKE_SOURCE_DIR}/vendor/websocketpp"
    "${CMAKE_SOURCE_DIR}/vendor/directx/Include"
)

# Library directories, adjust paths as necessary
link_directories(
    "${CMAKE_SOURCE_DIR}/vendor/libcurl/lib"
    "${CMAKE_SOURCE_DIR}/src/deps/glfw"
    "${CMAKE_SOURCE_DIR}/vendor/directx/Lib/x86"
)

# Set up configurations
set(CMAKE_CONFIGURATION_TYPES Debug Release)

# Define preprocessor definitions
add_compile_definitions(
    FMT_HEADER_ONLY
    _WEBSOCKETPP_CPP11_THREAD_
    CURL_STATICLIB
    ASIO_HAS_STD_INVOKE_RESULT
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_
    _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
    ASIO_STANDALONE
    _CRT_SECURE_NO_WARNINGS
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deps/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/asio/asio/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/websocketpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/websocketpp/websocketpp
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/your_additional_include_path
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libcurl/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deps/glfw
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/your_additional_library_path
)

# Source and header files
set(SOURCE_FILES
    src/core/injector/conditions/conditionals.cpp
    src/core/injector/event_handler.cpp
    src/core/injector/startup/bootstrap.cpp
    src/core/injector/startup/welcome_modal.cpp
    src/core/ipc/handlers/cor.cpp
    src/core/ipc/ipc_main.cpp
    src/core/steam/cef_manager.cpp
    src/core/steam/window/manager.cpp
    src/deps/imgui/imgui.cpp
    src/deps/imgui/imgui_demo.cpp
    src/deps/imgui/imgui_draw.cpp
    src/deps/imgui/imgui_impl_glfw.cpp
    src/deps/imgui/imgui_impl_opengl3.cpp
    src/deps/imgui/imgui_tables.cpp
    src/deps/imgui/imgui_widgets.cpp
    src/utils/io/metrics.cpp
    src/scripts/sdk/sdk.cpp
    src/stdafx.cpp
    src/utils/clr/platform.cpp
    src/utils/io/input-output.cpp
    src/utils/thread/thread_handler.cpp
    src/utils/updater/update.cpp
    src/window/api/installer.cpp
    src/window/core/aptos.cpp
    src/window/core/colors.cpp
    src/window/interface_v2/editor.cpp
    src/window/core/window.cpp
    src/window/interface/globals.cpp
    src/window/interface_v2/settings.cpp
    src/window/win_handler.cpp
    src/main.cpp
    src/utils/config/config.cpp
    src/utils/http/http_client.cpp
    src/utils/cout/logger.cpp
)

set(HEADER_FILES
    src/core/injector/conditions/conditionals.hpp
    src/core/injector/event_handler.hpp
    src/core/injector/startup/bootstrap.hpp
    src/core/injector/startup/welcome_modal.hpp
    src/core/ipc/handlers/types.hpp
    src/core/ipc/ipc_main.hpp
    src/core/steam/application.hpp
    src/core/steam/cef_manager.hpp
    src/core/steam/colors/accent_colors.hpp
    src/core/steam/window/manager.hpp
    src/core/steam/window/types/accent_api.hpp
    src/core/steam/window/types/corners.hpp
    src/core/steam/window/types/system_backdrop_type.hpp
    src/deps/imgui/imconfig.h
    src/deps/imgui/imgui.h
    src/deps/imgui/imgui_impl_glfw.h
    src/deps/imgui/imgui_impl_opengl3.h
    src/deps/imgui/imgui_impl_opengl3_loader.h
    src/deps/imgui/imgui_internal.h
    src/deps/imgui/imstb_rectpack.h
    src/deps/imgui/imstb_textedit.h
    src/deps/imgui/imstb_truetype.h
    src/utils/io/metrics.hpp
    src/scripts/sdk/sdk.hpp
    src/utils/base64.hpp
    src/utils/clr/platform.hpp
    src/utils/io/input-output.hpp
    src/utils/thread/thread_handler.hpp
    src/utils/updater/update.hpp
    src/window/api/installer.hpp
    src/window/core/colors.hpp
    src/window/interface_v2/editor.hpp
    src/window/core/memory.h
    src/window/core/window.hpp
    src/window/interface/globals.h
    src/window/interface_v2/settings.hpp
    src/window/win_handler.hpp
    src/scripts/resource.h
    src/stdafx.h
    src/utils/config/config.hpp
    src/utils/http/http_client.hpp
    src/utils/json.hpp
    src/utils/cout/logger.hpp
)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

# Find GTKmm package
find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0)

# Include GTKmm headers
include_directories(${GTKMM_INCLUDE_DIRS})

# Define the executable or library
#add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES})
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

#set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
#set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "libxcb")
#set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".so.1")

find_package(cpr CONFIG REQUIRED)
#target_link_libraries(millennium PRIVATE cpr::cpr)

find_package(OpenGL REQUIRED) # Finds the OpenGL library
find_package(glfw3 REQUIRED) # Assumes you have installed GLFW on your system

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OPENGL_LIBRARIES}
    ${GTK3_LIBRARIES}
    ${GTKMM_LIBRARIES}
    glfw
    cpr::cpr
)
# Set output directory for binaries
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/${CMAKE_SYSTEM_NAME}/${PROJECT_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "~/.local/share/Steam/ubuntu12_32/"
)

# Additional configurations for include directories and definitions can be added here as needed.