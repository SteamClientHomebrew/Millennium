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

#pragma once
#include "millennium/backend_mgr.h"
#include "millennium/logger.h"

#include <atomic>
#include <chrono>
#include <curl/curl.h>
#include <memory>
#include <string>
#include <thread>

#define MILLENNIUM_USERAGENT "Millennium/" MILLENNIUM_VERSION

extern std::shared_ptr<InterpreterMutex> g_shouldTerminateMillennium;

static size_t WriteByteCallback(char* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

struct DownloadData
{
    FILE* fp;
    size_t downloaded;
    std::function<void(size_t, size_t)> progressCallback;
    size_t totalSize;
};

static size_t WriteFileCallback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* data = static_cast<DownloadData*>(userdata);
    size_t written = fwrite(ptr, size, nmemb, data->fp);

    data->downloaded += written;

    if (data->progressCallback) {
        data->progressCallback(data->downloaded, data->totalSize);
    }

    return written;
}

class HttpError : public std::exception
{
  public:
    HttpError(const std::string& message) : message(message)
    {
    }

    const std::string& GetMessage() const
    {
        return message;
    }

  private:
    std::string message;
};

namespace Http
{
static std::string Get(const char* url, bool retry = true)
{
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteByteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, MILLENNIUM_USERAGENT);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

        while (true) {
            res = curl_easy_perform(curl);

            if (!retry || res == CURLE_OK) {
                break;
            }

            if (g_shouldTerminateMillennium->flag.load()) {
                throw HttpError("Thread termination flag is set, aborting HTTP request.");
            }

            /** Calling the server to quickly seems to accident DoS. */
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        curl_easy_cleanup(curl);
    }
    return response;
}
static std::string Post(const char* url, const std::string& postData, bool retry = true)
{
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postData.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteByteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, MILLENNIUM_USERAGENT);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        while (true) {
            res = curl_easy_perform(curl);

            if (!retry || res == CURLE_OK) {
                break;
            }

            if (g_shouldTerminateMillennium->flag.load()) {
                throw HttpError("Thread termination flag is set, aborting HTTP request.");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

static void DownloadWithProgress(const std::string& url, const std::filesystem::path& destPath, std::function<void(size_t, size_t)> progressCallback)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize curl");

    FILE* fp = fopen(destPath.string().c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to open file: " + destPath.string());
    }

    DownloadData downloadData = { fp, 0, progressCallback, 0 };

    /* HEAD the request to get content length */
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_off_t contentLength;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
        downloadData.totalSize = static_cast<size_t>(contentLength);
    }

    /** Second request: GET to download the file */
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloadData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, MILLENNIUM_USERAGENT);

    res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::filesystem::remove(destPath);
        throw std::runtime_error("Download failed: " + std::string(curl_easy_strerror(res)));
    }

    if (progressCallback) {
        progressCallback(downloadData.downloaded, downloadData.downloaded);
    }
}
} // namespace Http
