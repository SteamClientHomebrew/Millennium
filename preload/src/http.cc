#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <bits/this_thread_sleep.h>
#include <shimlogger.h>
#include <http.h>   

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