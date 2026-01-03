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
#include <vector>
#include <nlohmann/json.hpp>

#include "millennium/singleton.h"

namespace Millennium
{
int AddBrowserCss(const std::string& path, const std::string& regex);
int AddBrowserJs(const std::string& path, const std::string& regex);
bool RemoveBrowserModule(int id);
} // namespace Millennium

class WebkitHookStore : Singleton<WebkitHookStore>
{
    friend class Singleton<WebkitHookStore>;

  public:

    void Push(int moduleId);
    void UnregisterAll();

  private:
    std::vector<int> stack;
};

struct WebkitItem
{
    std::string matchString;
    std::string targetPath;
    std::string fileType; // "TargetCss" or "TargetJs"
};

std::vector<WebkitItem> ParseConditionalPatches(const nlohmann::json& conditional_patches, const std::string& theme_name);

int AddBrowserCss(const std::string& css_path, const std::string& regex = ".*");
int AddBrowserJs(const std::string& js_path, const std::string& regex = ".*");

void AddConditionalData(const std::string& path, const nlohmann::json& data, const std::string& theme_name);
