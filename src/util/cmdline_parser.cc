/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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
#include "millennium/cmdline_parser.h"
#include <algorithm>

command::command(const char* full_cmd)
{
    parse(full_cmd);
}

std::string_view command::executable() const
{
    auto slash = exec.rfind('/');
    auto backslash = exec.rfind('\\');
    size_t sep = std::string::npos;

    if (slash != std::string::npos && backslash != std::string::npos)
        sep = std::max(slash, backslash);
    else if (slash != std::string::npos)
        sep = slash;
    else if (backslash != std::string::npos)
        sep = backslash;

    return sep != std::string::npos ? std::string_view(exec).substr(sep + 1) : std::string_view(exec);
}

void command::ensure_param(std::string_view key, const char* value)
{
    std::string entry = value ? std::string(key) + "=" + value : std::string(key);

    for (auto& p : params) {
        if (p == key || (p.size() > key.size() && p.compare(0, key.size(), key) == 0 && p[key.size()] == '=')) {
            p = entry;
            return;
        }
    }

    params.insert(params.begin(), std::move(entry));
}

void command::remove_param(std::string_view key)
{
    params.erase(std::remove_if(params.begin(), params.end(),
                                [&](const std::string& p)
    {
        return p == key || (p.size() > key.size() && p.compare(0, key.size(), key) == 0 && p[key.size()] == '=');
    }),
                 params.end());
}

std::string command::build() const
{
    std::string result;
    result += quote_token(exec);
    for (const auto& p : params) {
        result += ' ';
        result += quote_token(p);
    }
    return result;
}

bool command::is_quote_char(char c)
{
#ifdef _WIN32
    return c == '"';
#else
    return c == '"' || c == '\'';
#endif
}

std::string command::quote_token(const std::string& s)
{
#ifdef _WIN32
    if (s.find_first_of(" =") != std::string::npos) return '"' + s + '"';
    return s;
#else
    if (s.find_first_of(" \t\"'") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + '"';
#endif
}

void command::parse(const char* full_cmd)
{
    std::string token;
    bool in_quotes = false;
    char quote_char = 0;
    bool first = true;

    for (const char* p = full_cmd; *p; ++p) {
        if (is_quote_char(*p) && (!in_quotes || *p == quote_char)) {
            quote_char = in_quotes ? 0 : *p;
            in_quotes = !in_quotes;
            continue;
        }
        if (*p == ' ' && !in_quotes) {
            if (!token.empty()) flush(token, first);
            continue;
        }
        token += *p;
    }
    if (!token.empty()) flush(token, first);
}

void command::flush(std::string& token, bool& first)
{
    if (first) {
        exec = std::move(token);
        first = false;
    } else {
        params.push_back(std::move(token));
    }
    token.clear();
}
