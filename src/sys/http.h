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

            while (true) 
            {
                res = curl_easy_perform(curl);

                if (!retry || res == CURLE_OK) 
                {
                    break;
                }

                switch (res) 
                {
                    case CURLE_OPERATION_TIMEDOUT: LOG_ERROR("Operation timed out: {}", curl_easy_strerror(res)); break;
                    case CURLE_COULDNT_CONNECT: LOG_ERROR("Couldn't connect to server: {}", curl_easy_strerror(res)); break;
                    case CURLE_SSL_CONNECT_ERROR: LOG_ERROR("SSL connection error: {}", curl_easy_strerror(res)); break;
                    case CURLE_PEER_FAILED_VERIFICATION: LOG_ERROR("Peer failed verification: {}", curl_easy_strerror(res)); break;
                    case CURLE_GOT_NOTHING: LOG_ERROR("Got nothing: {}", curl_easy_strerror(res)); break;
                    default: LOG_ERROR("curl_easy_perform() failed: {}", curl_easy_strerror(res)); break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }
            curl_easy_cleanup(curl);
        }
        return response;
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
