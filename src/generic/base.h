#pragma once
#include <string>
#include "stream_parser.h"

static constexpr const char* g_mversion = "1.1.5 (alpha)";
static constexpr const char* modules_repo = "https://github.com/SteamClientHomebrew/__builtins__.git";

static std::string millennium_modules_path = (stream_buffer::steam_path() / "steamui" / "plugins" / "__millennium__").string();

static std::filesystem::path modules_basedir = stream_buffer::steam_path() / ".millennium" / "@modules";
static std::filesystem::path python_bootzip = modules_basedir / "python.zip";

static const auto pythonPath = (modules_basedir).generic_string();
static const auto pythonLibs = (modules_basedir / "python311.zip").generic_string();

#ifdef _WIN32
	static const std::filesystem::path logs_dir = ".millennium/debug.log";
#elif __linux__
    static const std::filesystem::path logs_dir =  fmt::format("{}/.steam/steam/.millennium/debug.log", std::getenv("HOME"));
#endif