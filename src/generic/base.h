#pragma once
#include <string>
#include "stream_parser.h"

static constexpr const char* g_millenniumVersion = "1.1.5 (alpha)";
static constexpr const char* builtinsRepository = "https://github.com/SteamClientHomebrew/__builtins__.git";
static constexpr const char* pythonModulesRepository =  "https://github.com/SteamClientHomebrew/Packages.git";

static std::string builtinsModulesAbsolutePath = (SystemIO::GetSteamPath() / "steamui" / "plugins" / "__millennium__").string();
static std::filesystem::path pythonModulesBaseDir = SystemIO::GetSteamPath() / ".millennium" / "@modules";

static const auto pythonPath = (pythonModulesBaseDir).generic_string();
static const auto pythonLibs = (pythonModulesBaseDir / "python311.zip").generic_string();

static const std::filesystem::path logsDirectory = SystemIO::GetSteamPath() / ".millennium/debug.log";