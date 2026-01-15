/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "head/sys_accent_col.h"
#include "fmt/format.h"

/** piss off C */
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

struct AccentColors
{
    /** based on windows accent colors, posix is kinda an after-thought */
    unsigned long baseAccent, lightAccent1, lightAccent2, lightAccent3, darkAccent1, darkAccent2, darkAccent3;
};

#define UX_THEME_DLL "uxtheme.dll"

/**
 * Retrieves the current Windows accent colors using undocumented WinRT APIs.
 * This should be working on all windows versions that support accent colors (10, 11).
 *
 * It was tested on Windows 10 and 11, however not all intermediary versions.
 */
AccentColors GetAccentColors()
{
    AccentColors colors = {};
#ifdef _WIN32
    typedef int(WINAPI * GetImmersiveUserColorSetPreference_t)(int forceCheckRegistry, int skipCheckOnFail);
    typedef unsigned long(WINAPI * GetImmersiveColorFromColorSetEx_t)(unsigned int colorSet, unsigned int colorType, int ignoreHighContrast, int dwHighContrastCacheMode);
    typedef int(WINAPI * GetImmersiveColorTypeFromName_t)(const wchar_t* name);

    HMODULE hUxTheme = LoadLibraryW(L"" UX_THEME_DLL);
    if (!hUxTheme) {
        LOG_ERROR("Failed to load " UX_THEME_DLL);
        return colors;
    }

    auto GetColorSet = reinterpret_cast<GetImmersiveUserColorSetPreference_t>((void*)GetProcAddress(hUxTheme, "GetImmersiveUserColorSetPreference"));
    auto GetColorFromSet = reinterpret_cast<GetImmersiveColorFromColorSetEx_t>((void*)GetProcAddress(hUxTheme, "GetImmersiveColorFromColorSetEx"));
    /** GetImmersiveColorTypeFromName is exported in without a name. its consistently ordinal 96 on windows 10/11 though */
    auto GetColorTypeFromName = reinterpret_cast<GetImmersiveColorTypeFromName_t>((void*)GetProcAddress(hUxTheme, ((LPSTR)((ULONG_PTR)((WORD)(96))))));

    if (!GetColorSet || !GetColorFromSet || !GetColorTypeFromName) {
        Logger.Warn("Failed to get function addresses from " UX_THEME_DLL);
        FreeLibrary(hUxTheme);
        return colors;
    }

    std::array<const wchar_t*, 7> names = { L"ImmersiveSystemAccent",      L"ImmersiveSystemAccentLight1", L"ImmersiveSystemAccentLight2", L"ImmersiveSystemAccentLight3",
                                            L"ImmersiveSystemAccentDark1", L"ImmersiveSystemAccentDark2",  L"ImmersiveSystemAccentDark3" };

    try {
        for (size_t i = 0; i < names.size(); ++i) {
            int colorType = GetColorTypeFromName(names[i]);
            if (colorType != -1) {
                (&colors.baseAccent)[i] = GetColorFromSet(GetColorSet(FALSE, FALSE), colorType, FALSE, 0);
            }
        }
    } catch (...) {
        LOG_ERROR("Exception occurred while retrieving accent colors");
    }

    FreeLibrary(hUxTheme);
#endif
    return colors;
}

#define R(c) ((c) & 0xFF)
#define G(c) (((c) >> 8) & 0xFF)
#define B(c) (((c) >> 16) & 0xFF)

/**
 * Converts a DWORD color to a hex string "#RRGGBB"
 */
std::string CastToHex(unsigned long color)
{
    return fmt::format("#{0:02x}{1:02x}{2:02x}", R(color), G(color), B(color));
}

/**
 * Converts a DWORD color to an RGB string "r, g, b"
 */
std::string CastToRgb(unsigned long color)
{
    return fmt::format("{}, {}, {}", R(color), G(color), B(color));
}

/**
 * Gets Windows accent colors using WinRT APIs
 */
nlohmann::json Colors::GetAccentColorWin32()
{
#ifdef _WIN32
    try {
        AccentColors colors = GetAccentColors();

        return {
            { "accent",         CastToHex(colors.baseAccent)   },
            { "accentRgb",      CastToRgb(colors.baseAccent)   },
            { "light1",         CastToHex(colors.lightAccent1) },
            { "light1Rgb",      CastToRgb(colors.lightAccent1) },
            { "light2",         CastToHex(colors.lightAccent2) },
            { "light2Rgb",      CastToRgb(colors.lightAccent2) },
            { "light3",         CastToHex(colors.lightAccent3) },
            { "light3Rgb",      CastToRgb(colors.lightAccent3) },
            { "dark1",          CastToHex(colors.darkAccent1)  },
            { "dark1Rgb",       CastToRgb(colors.darkAccent1)  },
            { "dark2",          CastToHex(colors.darkAccent2)  },
            { "dark2Rgb",       CastToRgb(colors.darkAccent2)  },
            { "dark3",          CastToHex(colors.darkAccent3)  },
            { "dark3Rgb",       CastToRgb(colors.darkAccent3)  },
            { "originalAccent", CastToHex(colors.baseAccent)   }
        };
    } catch (...) {
        LOG_ERROR("Failed to get Windows accent colors");
        return {};
    }
#endif
    return {};
}

/**
 * TODO: Maybe find a better way to do this. Gnome/KDE/Macos all have different standards.
 * Currently just returns black colors for POSIX systems.
 */
nlohmann::json Colors::GetAccentColorPosix()
{
    nlohmann::json color_dictionary = {
        { "accent",         "#000"    },
        { "light1",         "#000"    },
        { "light2",         "#000"    },
        { "light3",         "#000"    },
        { "dark1",          "#000"    },
        { "dark2",          "#000"    },
        { "dark3",          "#000"    },
        { "accentRgb",      "0, 0, 0" },
        { "light1Rgb",      "0, 0, 0" },
        { "light2Rgb",      "0, 0, 0" },
        { "light3Rgb",      "0, 0, 0" },
        { "dark1Rgb",       "0, 0, 0" },
        { "dark2Rgb",       "0, 0, 0" },
        { "dark3Rgb",       "0, 0, 0" },
        { "originalAccent", "#000"    }
    };

    return color_dictionary;
}

/**
 * Adjusts a hex color by a percentage (lighter/darker)
 * @param hex_color Input hex color string
 * @param percent Adjustment percentage (positive=lighter, negative=darker)
 */
std::string Colors::AdjustColorIntensity(const std::string& hex_color, int percent)
{
    auto clamp = [](int v) { return std::max(0, std::min(255, v)); };
    std::string color = (hex_color[0] == '#') ? hex_color.substr(1) : hex_color;
    if (color.length() != 6) return hex_color;

    int r = std::stoi(color.substr(0, 2), nullptr, 16);
    int g = std::stoi(color.substr(2, 2), nullptr, 16);
    int b = std::stoi(color.substr(4, 2), nullptr, 16);

    auto adjust = [&](int c) { return clamp(percent < 0 ? static_cast<int>(c * (1.0 + percent / 100.0)) : static_cast<int>(c + (255 - c) * (percent / 100.0))); };

    r = adjust(r);
    g = adjust(g);
    b = adjust(b);

    return fmt::format("#{0:02x}{1:02x}{2:02x}", r, g, b);
}

/**
 * Converts hex color to RGB string format
 * @param hex_color Hex color string
 * @return RGB string in format "r, g, b"
 */
std::string Colors::Hex2Rgb(const std::string& hex_color)
{
    std::string color = (hex_color[0] == '#') ? hex_color.substr(1) : hex_color;
    if (color.length() != 6) {
        return "0, 0, 0";
    }

    int r = std::stoi(color.substr(0, 2), nullptr, 16);
    int g = std::stoi(color.substr(2, 2), nullptr, 16);
    int b = std::stoi(color.substr(4, 2), nullptr, 16);

    return fmt::format("{}, {}, {}", r, g, b);
}

/**
 * Extrapolates color variations from a custom accent color. Windows has 7 variations of the accent color.
 * So we need to emulate their approach so themes don't break.
 *
 * TODO: This is a very naive approach and probably doesn't look good with all colors.
 *
 * @param accentColor Hex color string (e.g., "#FF5733")
 * @return JSON string containing color variations
 */
nlohmann::json Colors::ExtrapolateCustomColor(const std::string& accentColor)
{
#ifdef _WIN32
    std::string originalAccent = GetAccentColorWin32()["originalAccent"];
#else
    std::string originalAccent = GetAccentColorPosix()["originalAccent"];
#endif

    nlohmann::json result = {
        { "accent", accentColor },
        { "light1", AdjustColorIntensity(accentColor, 15) },
        { "light2", AdjustColorIntensity(accentColor, 30) },
        { "light3", AdjustColorIntensity(accentColor, 45) },
        { "dark1", AdjustColorIntensity(accentColor, -15) },
        { "dark2", AdjustColorIntensity(accentColor, -30) },
        { "dark3", AdjustColorIntensity(accentColor, -45) },
        { "accentRgb", Hex2Rgb(accentColor) },
        { "light1Rgb", Hex2Rgb(AdjustColorIntensity(accentColor, 15)) },
        { "light2Rgb", Hex2Rgb(AdjustColorIntensity(accentColor, 30)) },
        { "light3Rgb", Hex2Rgb(AdjustColorIntensity(accentColor, 45)) },
        { "dark1Rgb", Hex2Rgb(AdjustColorIntensity(accentColor, -15)) },
        { "dark2Rgb", Hex2Rgb(AdjustColorIntensity(accentColor, -30)) },
        { "dark3Rgb", Hex2Rgb(AdjustColorIntensity(accentColor, -45)) },
        { "originalAccent", originalAccent }
    };

    return result;
}

nlohmann::json Colors::GetAccentColor(const std::string& accentColor)
{
    if (accentColor == "DEFAULT_ACCENT_COLOR") {
#ifdef _WIN32
        return GetAccentColorWin32();
#else
        return GetAccentColorPosix();
#endif
    } else {
        return ExtrapolateCustomColor(accentColor);
    }
}
