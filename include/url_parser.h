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

#include <string>
#include <sstream>
#include <iostream>
#include <string>
#include <iomanip>

static std::string UrlEncode(const std::string &value) 
{
    std::ostringstream encoded;

    for (unsigned char c : value) 
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')   
            encoded << c;
        else if (c == ' ')   
            encoded << '+';
        else 
            encoded << '%' << std::uppercase << std::hex << int(c);    
    }
    return encoded.str();
}

static const std::string UrlDecode(const std::string &url) 
{
    std::string decoded;
    char ch;
    int hexValue;

    for (size_t i = 0; i < url.length(); ++i) 
    {
        if (url[i] == '%') 
        {
            if (i + 2 < url.length() && std::isxdigit(url[i + 1]) && std::isxdigit(url[i + 2])) 
            {
                std::stringstream ss;
                ss << std::hex << url.substr(i + 1, 2);
                ss >> hexValue;
                ch = static_cast<char>(hexValue);
                decoded += ch;
                i += 2; 
            }
        } 
        else if (url[i] == '+') { decoded += ' ';    } 
        else                    { decoded += url[i]; }
    }
    return decoded;
}

static const std::string UrlFromPath(const std::string baseAddress, const std::string path) 
{
    #if defined(__linux__) || defined(__APPLE__)
    {
        return baseAddress + UrlEncode(path.substr(1));
    }
    #elif _WIN32
    {
        return baseAddress + UrlEncode(path);
    }
    #endif
}

static const std::string PathFromUrl(const std::string& path)
{
    // Remove query parameters for file path
    std::string cleanPath = path;
    size_t queryPos = cleanPath.find('?');
    if (queryPos != std::string::npos) {
        cleanPath = cleanPath.substr(0, queryPos);
    }

    #if defined(__linux__) || defined(__APPLE__)
    {
        return "/" + UrlDecode(cleanPath);
    }
    #elif _WIN32
    {
        return UrlDecode(cleanPath);
    }
    #endif
}