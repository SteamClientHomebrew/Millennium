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

    static nlohmann::json ParseHeaders(const std::string& headers) 
    {
        nlohmann::json jsonHeaders = nlohmann::json::array();
        std::istringstream stream(headers);
        std::string line;
        
        while (std::getline(stream, line)) 
        {
            if (line.find(':') != std::string::npos) 
            {
                std::string name = line.substr(0, line.find(':'));
                std::string value = line.substr(line.find(':') + 1);
                
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                jsonHeaders.push_back({{"name", name}, {"value", value}});
            }
        }
        return jsonHeaders;
    }

    static std::pair<std::string, long> GetWithHeaders(const char* url, nlohmann::json headers, std::string userAgent, nlohmann::json& responseHeadersJson) 
    {
        struct curl_slist* headersList = NULL;

        for (auto& [key, value] : headers.items()) 
        {
            headersList = curl_slist_append(headersList, fmt::format("{}: {}", key, value.get<std::string>()).c_str());
        }

        CURL* curl;
        CURLcode res;
        std::string response;
        std::string responseHeaders;
        long statusCode = 0;

        curl = curl_easy_init();
        if (curl) 
        {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, +[](char* buffer, size_t size, size_t nitems, std::string* userdata) -> size_t {
                userdata->append(buffer, size * nitems);
                return size * nitems;
            });
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
            res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
            responseHeadersJson = ParseHeaders(responseHeaders);
            curl_easy_cleanup(curl);
        }
        curl_slist_free_all(headersList);

        return {response, statusCode};
    }

    static bool DownloadFile(const char* url, const char* filename) 
    {
        CURL* curl;
        CURLcode res;
        FILE* file;

        curl = curl_easy_init();
        if (curl) 
        {
            file = fopen(filename, "wb");
            if (file) 
            {
                curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, fmt::format("Millennium/{}", MILLENNIUM_VERSION).c_str());

                res = curl_easy_perform(curl);
                fclose(file);

                if (res != CURLE_OK) 
                {
                    LOG_ERROR("curl_easy_perform() failed: {}", curl_easy_strerror(res));
                    return false;
                }
            }
            else 
            {
                LOG_ERROR("Failed to open file: {}", filename);
                return false;
            }
            curl_easy_cleanup(curl);
        }
        return true;
    }
}
