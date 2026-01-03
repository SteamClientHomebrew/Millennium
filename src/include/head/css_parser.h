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

#include <map>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace Millennium
{
enum class ColorTypes
{
    RawRGB = 1,
    RGB = 2,
    RawRGBA = 3,
    RGBA = 4,
    Hex = 5,
    Unknown = 6
};

class CSSParser
{
  public:
    [[nodiscard]] static std::optional<std::string> ConvertFromHex(const std::string& color, ColorTypes type);
    [[nodiscard]] static std::optional<std::string> ConvertToHex(const std::string& color, ColorTypes type);
    [[nodiscard]] static std::string ExpandShorthandHexColor(const std::string& shortHex);
    [[nodiscard]] static ColorTypes ParseColor(const std::string& color);
    [[nodiscard]] static ColorTypes TryRawParse(const std::string& color);

    [[nodiscard]] static nlohmann::json ParseRootColors(const std::string& filePath);

  private:
    [[nodiscard]] static std::string ExtractRootBlock(const std::string& fileContent);
    static void ParseProperties(const std::string& block, std::map<std::string, std::string>& properties, std::map<std::string, std::pair<std::string, std::string>>& propertyMap);
    [[nodiscard]] static std::string Trim(const std::string& str);

    [[nodiscard]] static nlohmann::json GenerateColorMetadata(const std::map<std::string, std::string>& properties,
                                                const std::map<std::string, std::pair<std::string, std::string>>& propertyMap);
};
} // namespace Millennium
