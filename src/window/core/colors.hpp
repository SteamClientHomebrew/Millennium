#pragma once
#include <string>

struct ImVec4
{
    float                                                     x, y, z, w;
    constexpr ImVec4()                                        : x(0.0f), y(0.0f), z(0.0f), w(0.0f) { }
    constexpr ImVec4(float _x, float _y, float _z, float _w)  : x(_x), y(_y), z(_z), w(_w) { }
};

namespace colors
{
	ImVec4 HexToImVec4(std::string hexColor);

	std::string ImVec4ToHex(const ImVec4& color);
}
