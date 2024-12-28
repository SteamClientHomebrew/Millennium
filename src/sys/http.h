#pragma once
#include <string>
#include <chrono>
#include <thread>
#include <sys/log.h>
#include <curl/curl.h>

static size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) 
{
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

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
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
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

                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }
            curl_easy_cleanup(curl);
        }
        return response;
    }
}
