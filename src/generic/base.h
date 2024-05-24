#pragma once
#include <string>
#include "stream_parser.h"

static constexpr const char* g_mversion = "1.1.5 (alpha)";
static constexpr const char* builtins_repo = "https://github.com/SteamClientHomebrew/__builtins__.git";
static constexpr const char* modules_repo =  "https://github.com/SteamClientHomebrew/Packages.git";

static std::string millennium_modules_path = (stream_buffer::steam_path() / "steamui" / "plugins" / "__millennium__").string();
static std::filesystem::path modules_basedir = stream_buffer::steam_path() / ".millennium" / "@modules";

static const auto pythonPath = (modules_basedir).generic_string();
static const auto pythonLibs = (modules_basedir / "python311.zip").generic_string();

static const std::filesystem::path logs_dir = stream_buffer::steam_path() / ".millennium/debug.log";