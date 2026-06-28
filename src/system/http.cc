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

#include "millennium/http.h"
#include "millennium/millennium_lifecycle.h"
#include "millennium/config.h"
#include "millennium/crypto.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

static size_t WriteByteCallback(char* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

static size_t WriteFileCallback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* data = static_cast<DownloadData*>(userdata);
    size_t written = fwrite(ptr, size, nmemb, data->fp);
    data->downloaded += written * size;
    if (data->progressCallback) {
        data->progressCallback(data->downloaded, data->totalSize);
    }
    return written;
}

static std::string get_proxy_url()
{
    return CONFIG.get({ "network", "proxy" }, "").get<std::string>();
}

static void apply_proxy(CURL* curl)
{
    const std::string proxy = get_proxy_url();
    if (proxy.empty()) {
        return;
    }

    curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

    const std::string username = CONFIG.get({ "network", "proxyUsername" }, "").get<std::string>();
    const std::string stored_pw = CONFIG.get({ "network", "proxyPassword" }, "").get<std::string>();

    if (!username.empty()) {
        const std::string password = Crypto::is_encrypted(stored_pw) ? Crypto::decrypt(stored_pw) : stored_pw;
        const std::string userpwd  = username + ":" + password;
        curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, userpwd.c_str());
    }
}

namespace Http
{

std::string Get(const char* url, bool retry, const long timeout)
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
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        apply_proxy(curl);

        while (true) {
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                break;
            }
            if (!retry) {
                curl_easy_cleanup(curl);
                throw HttpError(std::string("curl GET failed: ") + curl_easy_strerror(res));
            }
            if (millennium_lifecycle::get().terminate.flag.load()) {
                throw HttpError("Thread termination flag is set, aborting HTTP request.");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

std::string Post(const char* url, const std::string& postData, bool retry)
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
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        apply_proxy(curl);

        while (true) {
            res = curl_easy_perform(curl);
            if (!retry || res == CURLE_OK) {
                break;
            }

            if (millennium_lifecycle::get().terminate.flag.load()) {
                throw HttpError("Thread termination flag is set, aborting HTTP request.");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

void DownloadWithProgress(const std::tuple<std::string, size_t>& download_info, const std::filesystem::path& destPath, std::function<void(size_t, size_t)> progressCallback)
{
    const auto& [url, expectedSize] = download_info;

    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize curl");

    FILE* fp = fopen(destPath.string().c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to open file: " + destPath.string());
    }

    CURLcode res;
    DownloadData downloadData = { fp, 0, progressCallback, expectedSize };

    if (!expectedSize) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        apply_proxy(curl);

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_off_t contentLength;
            if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength) == CURLE_OK && contentLength > 0) {
                downloadData.totalSize = static_cast<size_t>(contentLength);
            }
        }
    }

    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloadData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, MILLENNIUM_USERAGENT);
    apply_proxy(curl);

    res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::filesystem::remove(destPath);
        throw std::runtime_error("Download failed: " + std::string(curl_easy_strerror(res)));
    }

    if (response_code < 200 || response_code >= 300) {
        std::filesystem::remove(destPath);
        throw std::runtime_error("Download failed: HTTP " + std::to_string(response_code));
    }

    if (progressCallback) {
        progressCallback(downloadData.downloaded, downloadData.totalSize);
    }
}

} // namespace Http
