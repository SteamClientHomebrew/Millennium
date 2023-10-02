#pragma once
#include <window/imgui/imgui.h>
#include <string>

namespace colors
{
	ImVec4 HexToImVec4(std::string hexColor);

	std::string ImVec4ToHex(const ImVec4& color);
}