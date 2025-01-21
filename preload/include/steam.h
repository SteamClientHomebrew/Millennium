#pragma once
#include <string>

/** If no steam path can be retrieved from the registry, this is the fallback */
static const char* FALLBACK_STEAM_PATH = "C:/Program Files (x86)/Steam";

std::string GetSteamPath();