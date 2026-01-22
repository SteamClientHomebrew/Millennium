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
#include "millennium/types.h"
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace head
{
enum class color_type
{
    RawRGB = 1,
    RGB = 2,
    RawRGBA = 3,
    RGBA = 4,
    Hex = 5,
    Unknown = 6
};

namespace css_parser
{
std::optional<std::string> convert_from_hex(const std::string& color, color_type type);
std::optional<std::string> convert_to_hex(const std::string& color, color_type type);
std::string expand_shorthand_hex(const std::string& shortHex);

color_type parse_color(const std::string& color);
color_type try_raw_parse(const std::string& color);

nlohmann::json parse_root_colors(const std::string& filePath);

std::string extract_root_block(const std::string& fileContent);
void parse_properties(const std::string& block, std::map<std::string, std::string>& properties, std::map<std::string, std::pair<std::string, std::string>>& propertyMap);
std::string trim(const std::string& str);

json generate_color_metadata(const std::map<std::string, std::string>& properties, const std::map<std::string, std::pair<std::string, std::string>>& propertyMap);
}; // namespace css_parser
} // namespace head
