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

#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <bits/this_thread_sleep.h>
#include <shimlogger.h>
#include <phttp.h>   

/**
 * @brief Write a file to a callback.
 * @param contents The contents of the file.
 * @param size The size of the memory.
 * @param nMemoryBytes The number of memory bytes.
 * @param userPtr The user pointer.
 * @return The size of the memory bytes.
 */
size_t WriteFileCallback(void* contents, size_t size, size_t nMemoryBytes, void* userPtr) 
{
    std::ofstream* file = static_cast<std::ofstream*>(userPtr);
    size_t totalSize = size * nMemoryBytes;
    file->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

/**
 * @brief Write a byte to a string.
 * @param charPtr The pointer to the character.
 * @param size The size of the memory.
 * @param nMemoryBytes The number of memory bytes.
 * @param data The string to write to.
 * @return The size of the memory bytes.
 */
size_t WriteByteCallback(char* charPtr, size_t size, size_t nMemoryBytes, std::string* data) 
{
    data->append(charPtr, size * nMemoryBytes);
    return size * nMemoryBytes;
}

/**
 * @brief Download a resource from a URL and save it to a file.
 * @param url The URL to download the resource from.
 * @param outputFilePath The path to save the downloaded resource to.
 * @return True if the resource was downloaded successfully, false otherwise.
 */
bool DownloadResource(const std::string& url, const std::string& outputFilePath) 
{
    CURL* curl;
    CURLcode resultCode;
    std::ofstream outputFile;

    curl = curl_easy_init();
    if (!curl) 
    {
        Print("Failed to initialize CURL.");
        return false;
    }

    outputFile.open(outputFilePath, std::ios::binary);
    if (!outputFile.is_open()) 
    {
        Print("Failed to open file: {}", outputFilePath);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outputFile);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Millennium/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    resultCode = curl_easy_perform(curl);
    if (resultCode != CURLE_OK) 
    {
        Print("Failed to download resource: {} {}", url, curl_easy_strerror(resultCode));

        outputFile.close();
        curl_easy_cleanup(curl);
        return false;
    }

    outputFile.close();
    curl_easy_cleanup(curl);
    return true;
}

/**
 * @brief Fetch a URL and return the response, Optionally retry fetching the URL until it succeeds.
 * @note This function will automatically follow redirects.
 * 
 * @param url The URL to fetch.
 * @param shouldRetryUntilSuccess Whether to retry fetching the URL until it succeeds.
 * @return The response from the URL.
 */
const std::string Fetch(const char* url, bool shouldRetryUntilSuccess) 
{
    CURL* curl;
    CURLcode curlResult;
    std::string strResponse;
    curl = curl_easy_init();

    if (curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteByteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &strResponse);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Millennium/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

        while (true) 
        {
            curlResult = curl_easy_perform(curl);
    
            if (!shouldRetryUntilSuccess || curlResult == CURLE_OK) {   
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        curl_easy_cleanup(curl);
    }
    return strResponse;
}