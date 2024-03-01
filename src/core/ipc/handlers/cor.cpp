#include "types.hpp"
#include <utils/http/http_client.hpp>
#include <utils/cout/logger.hpp>
#include <iostream>

namespace ipc_handler
{
	__declspec(noinline) const cor_response cor_request(std::string uri,
        std::string user_agent)
	{
        std::cout << uri << std::endl;
        std::cout << user_agent << std::endl;

        bool success = false;
        std::string discovery_result;

        HINTERNET hInternet = InternetOpenA(user_agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        HINTERNET hUrl = InternetOpenUrlA(hInternet, uri.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

        success = hUrl && hInternet;

        char buffer[1024];
        unsigned long bytes_read = 0;

        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read)
        {
            discovery_result.append(buffer, bytes_read);
        }

        http::close(hUrl, hInternet);

        return {
            discovery_result,
            success
        };
	}
}