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

#pragma once

#include <string>
#include <nlohmann/json.hpp>

/**
 * Colors utility class for managing Windows accent colors and theme colors
 */
class Colors
{
  public:
    /**
     * Gets Windows accent colors using WinRT APIs
     * @return JSON string containing accent color variations
     */
    static nlohmann::json GetAccentColorWin32();

    /**
     * Gets default black colors for POSIX systems
     * @return JSON string containing default black color variations
     */
    static nlohmann::json GetAccentColorPosix();

    /**
     * Extrapolates color variations from a custom accent color
     * @param accent_color Hex color string (e.g., "#FF5733")
     * @return JSON string containing color variations
     */
    static nlohmann::json ExtrapolateCustomColor(const std::string& accent_color);

    /**
     * Gets accent color based on input parameter
     * @param accent_color Either "DEFAULT_ACCENT_COLOR" or a hex color string
     * @return JSON string containing accent color data
     */
    static nlohmann::json GetAccentColor(const std::string& accent_color);

  private:
    /**
     * Adjusts a hex color by a percentage (lighter/darker)
     * @param hex_color Input hex color string
     * @param percent Adjustment percentage (positive=lighter, negative=darker)
     * @return Adjusted hex color string
     */
    static std::string AdjustColorIntensity(const std::string& hex_color, int percent);

    /**
     * Converts hex color to RGB string format
     * @param hex_color Hex color string
     * @return RGB string in format "r, g, b"
     */
    static std::string Hex2Rgb(const std::string& hex_color);
};
