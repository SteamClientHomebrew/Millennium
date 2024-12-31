#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <bits/this_thread_sleep.h>

size_t write_file_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* file = static_cast<std::ofstream*>(userp);
    size_t totalSize = size * nmemb;
    file->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

bool download_file(const std::string& url, const std::string& outputFilePath) {
    CURL* curl;
    CURLcode res;
    std::ofstream outputFile;

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL." << std::endl;
        return false;
    }

    outputFile.open(outputFilePath, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open file: " << outputFilePath << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outputFile);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Millennium/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        outputFile.close();
        curl_easy_cleanup(curl);
        return false;
    }

    // Clean up
    outputFile.close();
    curl_easy_cleanup(curl);
    return true;
}

const std::string get(const char* url, bool retry = true) {
    CURL* curl;
    CURLcode res;
    std::string response;
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Millennium/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

        while (true) {
            res = curl_easy_perform(curl);

            if (!retry || res == CURLE_OK) {   
                break;
            }

            std::cerr << "res: " << res << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
        curl_easy_cleanup(curl);
    }
    return response;
}