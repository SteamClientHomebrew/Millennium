#pragma once
#include <string>
#include "stream_parser.h"

static constexpr const char* g_mversion = "1.1.5 (alpha)";
static std::string millennium_modules_path = (stream_buffer::steam_path() / "steamui" / ".millennium_modules").string();