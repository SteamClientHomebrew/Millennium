cmake_minimum_required(VERSION 3.16)
project(ProcLaunchHook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

# Add include directory
include_directories(include)

# Add sources
set(SOURCES
    src/main.mm
    src/watchdog.mm
    src/exec.mm
)

set_source_files_properties(
    src/watchdog.mm
    PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

add_executable(SteamSniper ${SOURCES})

# Link only necessary frameworks - avoid full Cocoa
find_library(FOUNDATION_FRAMEWORK Foundation)
find_library(APPKIT_FRAMEWORK AppKit)
target_link_libraries(SteamSniper ${FOUNDATION_FRAMEWORK} ${APPKIT_FRAMEWORK})

set_target_properties(SteamSniper PROPERTIES
    OUTPUT_NAME "Millennium"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Millennium"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_IDENTIFIER "com.millennium.steamsniper"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.xml"
)

add_custom_command(TARGET SteamSniper POST_BUILD
    COMMAND /usr/libexec/PlistBuddy -c "Add :LSUIElement bool true" 
            "$<TARGET_BUNDLE_DIR:SteamSniper>/Contents/Info.plist" || true
    COMMENT "Making app headless"
)

if (EXISTS "${CMAKE_SOURCE_DIR}/darwin/AppIcon.icns")
    set_source_files_properties("${CMAKE_SOURCE_DIR}/darwin/AppIcon.icns" 
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(SteamSniper PRIVATE "${CMAKE_SOURCE_DIR}/darwin/AppIcon.icns")

    add_custom_command(TARGET SteamSniper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${CMAKE_SOURCE_DIR}/darwin/AppIcon.icns" 
                "$<TARGET_BUNDLE_DIR:SteamSniper>/Contents/Resources/AppIcon.icns"
        COMMENT "Copying app icon to Resources folder"
    )
endif()


add_custom_command(TARGET SteamSniper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/Millennium" "$<TARGET_BUNDLE_DIR:SteamSniper>/Contents/MacOS/Millennium.Patcher"
    COMMENT "Moving executable to MacOS folder"
)