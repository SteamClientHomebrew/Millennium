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

#pragma once
#include <curl/curl.h>
#include <filesystem>
#include <functional>
#include <string>
#include <tuple>

#define MILLENNIUM_USERAGENT "Millennium/" MILLENNIUM_VERSION

struct DownloadData
{
    FILE* fp;
    size_t downloaded;
    std::function<void(size_t, size_t)> progressCallback;
    size_t totalSize;
};

class HttpError : public std::runtime_error
{
  public:
    HttpError(const std::string& message) : std::runtime_error(message)
    {
    }
};

namespace Http
{
std::string Get(const char* url, bool retry = true, const long timeout = 10L);
std::string Post(const char* url, const std::string& postData, bool retry = true);
void DownloadWithProgress(const std::tuple<std::string, size_t>& download_info, const std::filesystem::path& destPath, std::function<void(size_t, size_t)> progressCallback);
} // namespace Http
