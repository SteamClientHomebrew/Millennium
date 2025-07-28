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
#include <string>
#include <chrono>
#include <thread>
#include "internal_logger.h"
#include <curl/curl.h>

static size_t WriteByteCallback(char* ptr, size_t size, size_t nmemb, std::string* data) 
{
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

class HttpError : public std::exception 
{
public:
    HttpError(const std::string& message) : message(message) {}

    const std::string& GetMessage() const { return message; }

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
        if (curl) 
        {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteByteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, fmt::format("Millennium/{}", MILLENNIUM_VERSION).c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

            while (true) 
            {
                res = curl_easy_perform(curl);

                if (!retry || res == CURLE_OK) 
                {
                    break;
                }

                #if defined(_WIN32)
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                #elif defined(__linux__) || defined(__APPLE__)

                if (g_threadTerminateFlag->flag.load())
                {
                    throw HttpError("Thread termination flag is set, aborting HTTP request.");
                }

                /** Calling the server to quickly seems to accident DoS it on unix. */
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                #endif
            }
            curl_easy_cleanup(curl);
        }
        return response;
    }
}
