#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <bits/this_thread_sleep.h>
#include <shimlogger.h>
#include <http.h>   

size_t WriteFileCallback(void* contents, size_t size, size_t nMemoryBytes, void* userPtr) 
{
    std::ofstream* file = static_cast<std::ofstream*>(userPtr);
    size_t totalSize = size * nMemoryBytes;
    file->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t WriteByteCallback(char* charPtr, size_t size, size_t nMemoryBytes, std::string* data) 
{
    data->append(charPtr, size * nMemoryBytes);
    return size * nMemoryBytes;
}

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