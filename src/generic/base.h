#pragma once
#include <string>
#include "stream_parser.h"

static constexpr const char* g_mversion = "1.1.5 (alpha)";
static constexpr const char* modules_repo = "https://github.com/SteamClientHomebrew/__builtins__.git";

static std::string millennium_modules_path = (stream_buffer::steam_path() / "steamui" / "plugins" / "__millennium__").string();
