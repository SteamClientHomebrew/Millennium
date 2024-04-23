#pragma once
#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
    
    struct url_data {
        size_t size;
        char* data;
    };

    static size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {
        size_t index = data->size;
        size_t n = (size * nmemb);
        char* tmp;

        data->size += (size * nmemb);

        tmp = (char*)realloc(data->data, data->size + 1); /* +1 for '\0' */

        if(tmp) {
            data->data = tmp;
        } else {
            if(data->data) {
                free(data->data);
            }
            fprintf(stderr, "Failed to allocate memory.\n");
            return 0;
        }

        memcpy((data->data + index), ptr, n);
        data->data[data->size] = '\0';

        return size * nmemb;
    }

    static char *http_get(const char* url) {
        CURL *curl;

        struct url_data data;
        data.size = 0;
        data.data = (char*)malloc(4096);
        if(NULL == data.data) {
            fprintf(stderr, "Failed to allocate memory.\n");
            return NULL;
        }

        data.data[0] = '\0';

        CURLcode res;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "millennium.patcher/1.0");
            res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                    fprintf(stderr, "curl_easy_perform() failed: %s\n",  
                            curl_easy_strerror(res));
            }

            curl_easy_cleanup(curl);

        }
        return data.data;
    }

    // Callback function to write received data to a file
    static size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
        return fwrite(ptr, size, nmemb, stream);
    }

    // Function to download a file from a URL to a specified path
    static int download_file(const char *url, const char *output_path) {
        CURL *curl;
        FILE *fp;
        CURLcode res;

        // Initialize libcurl
        curl = curl_easy_init();
        if (curl) {
            // Open file for writing
            fp = fopen(output_path, "wb");
            if (fp == NULL) {
                fprintf(stderr, "Failed to open file for writing\n");
                return 1;
            }

            // Set URL to download
            curl_easy_setopt(curl, CURLOPT_URL, url);

            // Set callback function to write data to file
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

            // Set file stream as data to write to
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            // Perform the request
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                fclose(fp);
                curl_easy_cleanup(curl);
                return 1;
            }

            // Cleanup
            curl_easy_cleanup(curl);
            fclose(fp);
        } else {
            fprintf(stderr, "Failed to initialize libcurl\n");
            return 1;
        }

        return 0;
    }
}