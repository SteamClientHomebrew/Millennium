#include <window/core/colors.hpp>
#include <window/imgui/imgui_internal.h>
#include <stdafx.h>

namespace colors
{
	ImVec4 HexToImVec4(std::string inputColor) {

		const char* hexColor = inputColor.c_str();
		ImVec4 result(0.0f, 0.0f, 0.0f, 1.0f);

		if (hexColor[0] == '#' && (strlen(hexColor) == 7 || strlen(hexColor) == 9)) {
			unsigned int hexValue = 0;
			int numParsed = sscanf(hexColor + 1, "%x", &hexValue);

			if (numParsed == 1) {
				if (strlen(hexColor) == 7) {
					result.x = static_cast<float>((hexValue >> 16) & 0xFF) / 255.0f;
					result.y = static_cast<float>((hexValue >> 8) & 0xFF) / 255.0f;
					result.z = static_cast<float>(hexValue & 0xFF) / 255.0f;
				}
				else if (strlen(hexColor) == 9) {
					result.x = static_cast<float>((hexValue >> 24) & 0xFF) / 255.0f;
					result.y = static_cast<float>((hexValue >> 16) & 0xFF) / 255.0f;
					result.z = static_cast<float>((hexValue >> 8) & 0xFF) / 255.0f;
					result.w = static_cast<float>(hexValue & 0xFF) / 255.0f;
				}
			}
		}

		return result;
	}

	std::string ImVec4ToHex(const ImVec4& color) {

		ImVec4 clampedColor;
		clampedColor.x = ImClamp(color.x, 0.0f, 1.0f);
		clampedColor.y = ImClamp(color.y, 0.0f, 1.0f);
		clampedColor.z = ImClamp(color.z, 0.0f, 1.0f);
		clampedColor.w = ImClamp(color.w, 0.0f, 1.0f);

		unsigned char r = static_cast<unsigned char>(clampedColor.x * 255.0f);
		unsigned char g = static_cast<unsigned char>(clampedColor.y * 255.0f);
		unsigned char b = static_cast<unsigned char>(clampedColor.z * 255.0f);
		unsigned char a = static_cast<unsigned char>(clampedColor.w * 255.0f);

		std::stringstream ss;
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(r);
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(g);
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);

		if (a < 255) {
			ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(a);
		}

		return ss.str();
	}

}
