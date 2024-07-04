#pragma once
#include <string>
#include <chrono>
#include <thread>
#include <cpr/cpr.h>
#include <sys/log.h>

static std::string GetRequest(const char* url) 
{
    while (true) 
    {
        cpr::Response response = cpr::Get(cpr::Url{url}, cpr::UserAgent{"millennium.patcher/1.0"});

        if (!response.error) 
        {
            return response.text;
        }
        
        Logger.Log("Couldn't connect to server: [{}]", url);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
