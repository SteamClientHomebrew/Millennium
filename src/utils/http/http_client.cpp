#include <format>
#include <utils/http/http_client.hpp>

const std::string http::get(std::string remote_endpoint)
{
    HINTERNET hInternet = InternetOpenA("millennium.patcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlA(hInternet, remote_endpoint.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

    if (!hUrl || !hInternet)
    {
        throw std::runtime_error(std::format("the requested operation [{}()] failed. reason: InternetOpen*A was nullptr", __func__));
    }

    char buffer[1024];
    DWORD total_bytes_read = 0;
    std::string discovery_result;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read) discovery_result.append(buffer, total_bytes_read);
    InternetCloseHandle(hUrl); InternetCloseHandle(hInternet);

    return discovery_result;
}