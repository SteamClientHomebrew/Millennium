#ifdef _WIN32 
#include <winsock2.h> 
#include <windows.h> 
#include <ws2tcpip.h> 
#endif

#include <stdafx.h>
#include <utils/config/config.hpp>
#include <format>
#include <cpr/include/cpr.h>

static constexpr const std::string_view user_agent = "millennium.patcher";

const bool valid_request(std::string url)
{
    bool isDevToolsRequest = url.find("127.0.0.1:8080") != std::string::npos;
    bool isSteamLoopBack = url.find("steamloopback.host") != std::string::npos;

    return !Settings::Get<bool>("allow-store-load") ? (isDevToolsRequest || isSteamLoopBack) : true;
}

const std::string http::get(std::string remote_endpoint)
{
    if (!valid_request(remote_endpoint)) {
        console.err(std::format("un-allowed to make requests to outside of local context"));
        throw http_error(http_error::errors::not_allowed);
    }

    cpr::Header header;
    header["User-Agent"] = user_agent.data();

    cpr::Response r = cpr::Get(cpr::Url{ remote_endpoint }, header);
    return r.text;
}

const nlohmann::json http::getJson(std::string remote_endpoint)
{
    return nlohmann::json::parse(http::get(remote_endpoint));
}

using json = nlohmann::json;

const std::string http::post(std::string path, std::string data)
{
    cpr::Response r = cpr::Post(cpr::Url{ "http://millennium.web.app" + path },
        cpr::Body{ data },
        cpr::Header{ {"Content-Type", "application/json"} });

    return r.text;
}

const bool http::to_disk(const char* url, const char* path)
{
    if (!valid_request(url))
    {
        console.err(std::format("un-allowed to make requests to outside of local context"));
        throw http_error(http_error::errors::not_allowed);
    }

    cpr::Header header;
    header["User-Agent"] = user_agent.data();

    cpr::Response r = cpr::Get(cpr::Url{ url }, header);

    if (r.error || r.status_code != 200) {
        return false;
    }

    std::ofstream output_file(path, std::ios::binary);
    output_file.write(r.text.c_str(), r.text.length());
    output_file.close();

    return true;
}