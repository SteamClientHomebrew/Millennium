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

#include "__builtins__/css_analyzer.h"
#include <fstream>
#include <iomanip>
#include <locals.h>
#include <regex>
#include <sstream>
#include <vector>

std::string Millennium::CSSParser::trim(const std::string& str)
{
    const std::string whitespace = " \t\n\r";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

std::optional<std::string> Millennium::CSSParser::convertFromHex(const std::string& color, ColorTypes type)
{
    if (type == ColorTypes::Unknown)
        return std::nullopt;

    std::string hex = color;
    if (hex[0] == '#')
        hex.erase(0, 1);

    int r = std::stoi(hex.substr(0, 2), nullptr, 16);
    int g = std::stoi(hex.substr(2, 2), nullptr, 16);
    int b = std::stoi(hex.substr(4, 2), nullptr, 16);

    std::ostringstream out;
    if (type == ColorTypes::RawRGB)
    {
        out << r << ", " << g << ", " << b;
        return out.str();
    }
    if (type == ColorTypes::RGB)
    {
        out << "rgb(" << r << ", " << g << ", " << b << ")";
        return out.str();
    }

    if (type == ColorTypes::RawRGBA || type == ColorTypes::RGBA)
    {
        double a = 1.0;
        if (hex.size() == 8)
            a = std::stoi(hex.substr(6, 2), nullptr, 16) / 255.0;
        if (type == ColorTypes::RawRGBA)
        {
            out << r << ", " << g << ", " << b << ", " << std::fixed << std::setprecision(2) << a;
            return out.str();
        }
        out << "rgba(" << r << ", " << g << ", " << b << ", " << std::fixed << std::setprecision(2) << a << ")";
        return out.str();
    }

    return "#" + hex;
}

std::optional<std::string> Millennium::CSSParser::convertToHex(const std::string& color, ColorTypes type)
{
    if (type == ColorTypes::Hex)
        return color;

    std::vector<int> values;
    if (type == ColorTypes::RawRGB || type == ColorTypes::RawRGBA)
    {
        std::istringstream ss(color);
        std::string token;
        while (std::getline(ss, token, ','))
            values.push_back(std::stoi(token));
    }
    else if (type == ColorTypes::RGB || type == ColorTypes::RGBA)
    {
        auto start = color.find('(') + 1;
        auto end = color.find(')');
        std::string inner = color.substr(start, end - start);
        std::istringstream ss(inner);
        std::string token;
        while (std::getline(ss, token, ','))
            values.push_back(std::stoi(token));
    }

    std::ostringstream out;
    out << '#';
    out << std::hex << std::setfill('0');
    if (type == ColorTypes::RawRGB || type == ColorTypes::RGB)
    {
        out << std::setw(2) << values[0];
        out << std::setw(2) << values[1];
        out << std::setw(2) << values[2];
        return out.str();
    }

    if (type == ColorTypes::RawRGBA || type == ColorTypes::RGBA)
    {
        int a = static_cast<int>(values[3] * 255.0);
        out << std::setw(2) << values[0];
        out << std::setw(2) << values[1];
        out << std::setw(2) << values[2];
        out << std::setw(2) << a;
        return out.str();
    }

    return std::nullopt;
}

std::string Millennium::CSSParser::expandHexColor(const std::string& shortHex)
{
    if (shortHex.size() == 4 && shortHex[0] == '#')
    {
        std::ostringstream out;
        out << '#';
        for (size_t i = 1; i < 4; ++i)
            out << shortHex[i] << shortHex[i];
        return out.str();
    }
    return shortHex;
}

Millennium::ColorTypes Millennium::CSSParser::tryRawParse(const std::string& color)
{
    if (color.find(", ") == std::string::npos)
        return ColorTypes::Unknown;
    std::istringstream ss(color);
    std::string token;
    int count = 0;
    while (std::getline(ss, token, ','))
        count++;
    return count == 3 ? ColorTypes::RawRGB : (count == 4 ? ColorTypes::RawRGBA : ColorTypes::Unknown);
}

Millennium::ColorTypes Millennium::CSSParser::parseColor(const std::string& color)
{
    if (color.rfind("rgb(", 0) == 0)
        return ColorTypes::RGB;
    if (color.rfind("rgba(", 0) == 0)
        return ColorTypes::RGBA;
    if (color.rfind("#", 0) == 0)
        return ColorTypes::Hex;
    return tryRawParse(color);
}

nlohmann::json Millennium::CSSParser::generateColorMetadata(const std::map<std::string, std::string>& properties,
                                                            const std::map<std::string, std::pair<std::string, std::string>>& propertyMap)
{
    nlohmann::json result = nlohmann::json::array();

    for (const auto& [prop, value] : properties)
    {
        std::string expanded = expandHexColor(value);
        ColorTypes type = parseColor(expanded);
        if (type == ColorTypes::Unknown)
            continue;

        auto it = propertyMap.find(prop);
        nlohmann::json name = (it != propertyMap.end() && !it->second.first.empty()) ? nlohmann::json(it->second.first) : nlohmann::json(nullptr);
        nlohmann::json description = (it != propertyMap.end() && !it->second.second.empty()) ? nlohmann::json(it->second.second) : nlohmann::json(nullptr);

        auto hex = convertToHex(expanded, type).value_or("");

        result.push_back({
            {"color",        prop                  },
            {"name",         name                  },
            {"description",  description           },
            {"type",         static_cast<int>(type)},
            {"defaultColor", hex                   }
        });
    }

    return result;
}
std::string Millennium::CSSParser::extractRootBlock(const std::string& fileContent)
{
    std::regex rootRegex(R"(:root\s*\{([\s\S]*)\})", std::regex::ECMAScript);
    std::smatch match;
    if (std::regex_search(fileContent, match, rootRegex))
        return match[1].str();
    return "";
}

void Millennium::CSSParser::parseProperties(const std::string& block, std::map<std::string, std::string>& properties,
                                            std::map<std::string, std::pair<std::string, std::string>>& propertyMap)
{
    std::istringstream ss(block);
    std::string line;
    std::string lastComment;
    bool inComment = false;

    while (std::getline(ss, line))
    {
        line = trim(line);
        if (line.empty())
            continue;

        if (line.rfind("/*", 0) == 0)
        {
            lastComment.clear();
            inComment = true;
        }

        if (inComment)
        {
            lastComment += line + "\n";
            if (line.find("*/") != std::string::npos)
                inComment = false;
            continue;
        }

        std::regex propRegex(R"(([\w-]+)\s*:\s*([^;]+);)");
        std::smatch match;
        if (std::regex_match(line, match, propRegex))
        {
            std::string propName = match[1].str();
            std::string propValue = trim(match[2].str());
            properties[propName] = propValue;

            std::string name, description;
            if (!lastComment.empty())
            {
                std::regex nameRegex(R"(@name\s+([^\*]+))");
                std::regex descRegex(R"(@description\s+([^\*]+))");
                std::smatch nameMatch, descMatch;
                if (std::regex_search(lastComment, nameMatch, nameRegex))
                    name = trim(nameMatch[1].str());
                if (std::regex_search(lastComment, descMatch, descRegex))
                    description = trim(descMatch[1].str());
                lastComment.clear();
            }

            propertyMap[propName] = {name, description};
        }
    }
}

nlohmann::json Millennium::CSSParser::parseRootColors(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
        return nlohmann::json::array();

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string rootBlock = extractRootBlock(content);
    if (rootBlock.empty())
        return nlohmann::json::array();

    std::map<std::string, std::string> properties;
    std::map<std::string, std::pair<std::string, std::string>> propertyMap;
    parseProperties(rootBlock, properties, propertyMap);

    return generateColorMetadata(properties, propertyMap);
}
