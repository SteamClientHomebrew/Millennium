#pragma once
#include <string.h>
#include <chrono>
#include <thread>
#include <cpr/cpr.h>
#include <utilities/log.hpp>

static std::string GetRequest(const char* url) 
{
    cpr::Response response = cpr::Get(cpr::Url{url}, cpr::UserAgent{"millennium.patcher/1.0"});

    if (response.error) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return GetRequest(url);
    }

    return response.text;
}