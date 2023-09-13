#include <utils/cout/logger.hpp>
#include "http_client.hpp"

#include <wininet.h>

static constexpr const std::string_view user_agent = "millennium.patcher";

const void close(HINTERNET sentinel, ...)
{
    va_list args;
    va_start(args, sentinel);

    void* handle = va_arg(args, void*);
    while (handle != sentinel)
    {
        InternetCloseHandle(handle);
        handle = va_arg(args, void*);
    }

    va_end(args);
}

const std::string http::get(std::string remote_endpoint)
{
    //console.log(std::format("making GET request to {}", remote_endpoint));
    std::string discovery_result;

    HINTERNET hInternet = InternetOpenA(user_agent.data(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlA(hInternet, remote_endpoint.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

    if (!hUrl || !hInternet)
    {
        console.err("error making request to remote uri");

        switch (GetLastError())
        {
            case ERROR_INTERNET_CANNOT_CONNECT: throw http_error(http_error::errors::couldnt_connect); break;
            default: throw http_error(http_error::errors::miscellaneous);
        }
    }

    char buffer[8096];
    unsigned long bytes_read = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read)
    {
        discovery_result.append(buffer, bytes_read);
    }

    close(hUrl, hInternet);

    return discovery_result;
}
