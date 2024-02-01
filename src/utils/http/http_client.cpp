#include <stdafx.h>
#include <utils/config/config.hpp>
#include <boost/network/uri.hpp>
#include <format>

static constexpr const std::string_view user_agent = "millennium.patcher";

const void http::close(HINTERNET sentinel, ...)
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

const bool valid_request(std::string url)
{
    bool isDevToolsRequest = url.find("127.0.0.1:8080") != std::string::npos;
    bool isSteamLoopBack = url.find("steamloopback.host") != std::string::npos;

    return !Settings::Get<bool>("allow-store-load") ? (isDevToolsRequest || isSteamLoopBack) : true;
}

const std::string http::get(std::string remote_endpoint)
{
    //console.log(std::format("making GET request to {}", remote_endpoint));

    if (!valid_request(remote_endpoint))
    {
        console.err(std::format("un-allowed to make requests to outside of local context"));
        throw http_error(http_error::errors::not_allowed);
    }

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

    char buffer[1024];
    unsigned long bytes_read = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read)
    {
        discovery_result.append(buffer, bytes_read);
    }

    http::close(hUrl, hInternet);

    return discovery_result;
}

const nlohmann::json http::getJson(std::string remote_endpoint)
{
    return nlohmann::json::parse(http::get(remote_endpoint));
}

using json = nlohmann::json;


const std::string http::post(std::string path, std::string data)
{
    static CHAR hdrs[] = "Content-Type: application/json";

    HINTERNET hSession = InternetOpenA("MyAgent", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    HINTERNET hConnect = InternetConnect(hSession, "us-central1-bettersteam-87b59.cloudfunctions.net", 443, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE, 1);
    HttpSendRequestA(hRequest, hdrs, strlen(hdrs), const_cast<char*>(data.c_str()), strlen(data.c_str()));

    DWORD dwContentLen;
    DWORD dwBufLen = sizeof(dwContentLen);
    if (HttpQueryInfo(hRequest,
        HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
        (LPVOID)&dwContentLen,
        &dwBufLen,
        0))
    {
        char* pData = (char*)GlobalAlloc(GMEM_FIXED, dwContentLen + 1);
        DWORD dwReadSize = dwContentLen / 10;   // We will read 10% of data
        // with each read.

        DWORD cReadCount;
        DWORD dwBytesRead;
        char* pCopyPtr = pData;
        for (cReadCount = 0; cReadCount < 10; cReadCount++)
        {
            InternetReadFile(hRequest, pCopyPtr, dwReadSize, &dwBytesRead);
            pCopyPtr = pCopyPtr + dwBytesRead;
        }
        // extra read to account for integer division round off
        InternetReadFile(hRequest,
            pCopyPtr,
            dwContentLen - (pCopyPtr - pData),
            &dwBytesRead);
        // Null terminate data
        pData[dwContentLen] = 0;

        return pData;
    }

    return std::string();
}

const std::string http::replicateGet(std::string remote_endpoint, std::string userAgent, nlohmann::json& headers)
{
    std::string response;
    boost::network::uri::uri remoteAddress = boost::network::uri::uri(remote_endpoint);

    HINTERNET hInternet = InternetOpen(userAgent.c_str(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hConnect = InternetConnect(hInternet, remoteAddress.host().c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (hConnect) {
            HINTERNET hRequest = HttpOpenRequest(hConnect, "GET", remote_endpoint.c_str(), NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
            if (hRequest) {
                std::string headersStr;
                for (auto it = headers.begin(); it != headers.end(); ++it) {
                    headersStr += it.key() + ": " + it.value().get<std::string>() + "\r\n";
                }

                HttpAddRequestHeaders(hRequest, headersStr.c_str(), headersStr.length(), HTTP_ADDREQ_FLAG_REPLACE);

                BOOL sendRequest = HttpSendRequest(hRequest, NULL, 0, NULL, 0);
                if (sendRequest) {
                    CHAR buffer[4096];
                    DWORD bytesRead = 0;

                    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                        response.append(buffer, bytesRead);
                    }
                }
                InternetCloseHandle(hRequest);
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }

    return response;
}

const std::vector<unsigned char> http::get_bytes(const char* url)
{
    //console.log(std::format("making GET request to retrieve bytes from {}", url));

    if (!valid_request(url))
    {
        console.err(std::format("un-allowed to make requests to outside of local context"));
        throw http_error(http_error::errors::not_allowed);
    }

	std::vector<unsigned char> image_bytes;

    HINTERNET hInternet = InternetOpenA(user_agent.data(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);

    if (!hUrl || !hInternet)
    {
        console.err("error making request to remote uri");

        switch (GetLastError())
        {
            case ERROR_INTERNET_CANNOT_CONNECT: throw http_error(http_error::errors::couldnt_connect); break;
            default: throw http_error(http_error::errors::miscellaneous);
        }
    }

    unsigned char buffer[1024];
    unsigned long bytes_read;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0)
    {
        image_bytes.insert(image_bytes.end(), buffer, buffer + bytes_read);
    }

    InternetCloseHandle(hUrl); InternetCloseHandle(hInternet);

	return image_bytes;
}