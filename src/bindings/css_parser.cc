/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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

#include "head/css_parser.h"

#include <fstream>
#include <fmt/core.h>
#include <regex>
#include <sstream>

std::string head::css_parser::trim(const std::string& str)
{
    const std::string whitespace = " \t\n\r";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

std::optional<std::string> head::css_parser::convert_from_hex(const std::string& color, color_type type)
{
    if (type == color_type::Unknown) {
        return std::nullopt;
    }

    std::string hex = (color[0] == '#') ? color.substr(1) : color;

    const int r = std::stoi(hex.substr(0, 2), nullptr, 16);
    const int g = std::stoi(hex.substr(2, 2), nullptr, 16);
    const int b = std::stoi(hex.substr(4, 2), nullptr, 16);

    switch (type) {
        case color_type::RawRGB:
            return fmt::format("{}, {}, {}", r, g, b);

        case color_type::RGB:
            return fmt::format("rgb({}, {}, {})", r, g, b);

        case color_type::RawRGBA:
        case color_type::RGBA:
        {
            const double a = (hex.size() == 8) ? std::stoi(hex.substr(6, 2), nullptr, 16) / 255.0 : 1.0;

            if (type == color_type::RawRGBA) {
                return fmt::format("{}, {}, {}, {:.2f}", r, g, b, a);
            }
            return fmt::format("rgba({}, {}, {}, {:.2f})", r, g, b, a);
        }

        default:
            return "#" + hex;
    }
}

std::optional<std::string> head::css_parser::convert_to_hex(const std::string& color, color_type type)
{
    if (type == color_type::Hex) {
        return color;
    }

    std::string valueString = color;
    if (type == color_type::RGB || type == color_type::RGBA) {
        const auto start = color.find('(') + 1;
        const auto end = color.find(')');
        valueString = color.substr(start, end - start);
    }

    std::vector<int> values;
    std::istringstream ss(valueString);
    std::string token;
    while (std::getline(ss, token, ',')) {
        values.push_back(std::stoi(token));
    }

    if (type == color_type::RawRGB || type == color_type::RGB) {
        return fmt::format("#{:02x}{:02x}{:02x}", static_cast<int>(values[0]), static_cast<int>(values[1]), static_cast<int>(values[2]));
    }

    if (type == color_type::RawRGBA || type == color_type::RGBA) {
        const int alpha = static_cast<int>(values[3] * 255.0);
        return fmt::format("#{:02x}{:02x}{:02x}{:02x}", static_cast<int>(values[0]), static_cast<int>(values[1]), static_cast<int>(values[2]), alpha);
    }

    return std::nullopt;
}

std::string head::css_parser::expand_shorthand_hex(const std::string& shortHex)
{
    if (shortHex.size() == 4 && shortHex[0] == '#') {
        std::ostringstream out;
        out << '#';
        for (size_t i = 1; i < 4; ++i)
            out << shortHex[i] << shortHex[i];
        return out.str();
    }
    return shortHex;
}

head::color_type head::css_parser::try_raw_parse(const std::string& color)
{
    if (color.find(", ") == std::string::npos) return color_type::Unknown;
    std::istringstream ss(color);
    std::string token;
    int count = 0;
    while (std::getline(ss, token, ','))
        count++;
    return count == 3 ? color_type::RawRGB : (count == 4 ? color_type::RawRGBA : color_type::Unknown);
}

head::color_type head::css_parser::parse_color(const std::string& color)
{
    if (color.rfind("rgb(", 0) == 0) return color_type::RGB;
    if (color.rfind("rgba(", 0) == 0) return color_type::RGBA;
    if (color.rfind("#", 0) == 0) return color_type::Hex;
    return try_raw_parse(color);
}

json head::css_parser::generate_color_metadata(const std::map<std::string, std::string>& properties, const std::map<std::string, std::pair<std::string, std::string>>& propertyMap)
{
    nlohmann::json result = nlohmann::json::array();

    for (const auto& [prop, value] : properties) {
        std::string expanded = expand_shorthand_hex(value);
        color_type type = parse_color(expanded);
        if (type == color_type::Unknown) continue;

        auto it = propertyMap.find(prop);
        nlohmann::json name = (it != propertyMap.end() && !it->second.first.empty()) ? nlohmann::json(it->second.first) : nlohmann::json(nullptr);
        nlohmann::json description = (it != propertyMap.end() && !it->second.second.empty()) ? nlohmann::json(it->second.second) : nlohmann::json(nullptr);

        auto hex = convert_to_hex(expanded, type).value_or("");

        result.push_back({
            { "color",        prop                   },
            { "name",         name                   },
            { "description",  description            },
            { "type",         static_cast<int>(type) },
            { "defaultColor", hex                    }
        });
    }

    return result;
}
std::string head::css_parser::extract_root_block(const std::string& fileContent)
{
    std::regex rootRegex(R"(:root\s*\{([\s\S]*)\})", std::regex::ECMAScript);
    std::smatch match;
    if (std::regex_search(fileContent, match, rootRegex)) return match[1].str();
    return "";
}

void head::css_parser::parse_properties(const std::string& block, std::map<std::string, std::string>& properties,
                                        std::map<std::string, std::pair<std::string, std::string>>& propertyMap)
{
    std::istringstream ss(block);
    std::string line;
    std::string lastComment;
    bool inComment = false;

    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line.rfind("/*", 0) == 0) {
            lastComment.clear();
            inComment = true;
        }

        if (inComment) {
            lastComment += line + "\n";
            if (line.find("*/") != std::string::npos) inComment = false;
            continue;
        }

        std::regex propRegex(R"(([\w-]+)\s*:\s*([^;]+);)");
        std::smatch match;
        if (std::regex_match(line, match, propRegex)) {
            std::string propName = match[1].str();
            std::string propValue = trim(match[2].str());
            properties[propName] = propValue;

            std::string name, description;
            if (!lastComment.empty()) {
                std::regex nameRegex(R"(@name\s+([^\*]+))");
                std::regex descRegex(R"(@description\s+([^\*]+))");
                std::smatch nameMatch, descMatch;
                if (std::regex_search(lastComment, nameMatch, nameRegex)) name = trim(nameMatch[1].str());
                if (std::regex_search(lastComment, descMatch, descRegex)) description = trim(descMatch[1].str());
                lastComment.clear();
            }

            propertyMap[propName] = { name, description };
        }
    }
}

nlohmann::json head::css_parser::parse_root_colors(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) return nlohmann::json::array();

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string rootBlock = extract_root_block(content);
    if (rootBlock.empty()) return nlohmann::json::array();

    std::map<std::string, std::string> properties;
    std::map<std::string, std::pair<std::string, std::string>> propertyMap;
    parse_properties(rootBlock, properties, propertyMap);

    return generate_color_metadata(properties, propertyMap);
}
