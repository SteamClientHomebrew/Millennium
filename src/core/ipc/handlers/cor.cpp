#include "types.hpp"
#include <utils/http/http_client.hpp>
#include <utils/cout/logger.hpp>
#include <iostream>

/// <summary>
/// Proof of concept displaying how COR headers can be bypassed by using the backend. 
/// </summary>
namespace ipc_handler
{
	/// <summary>
	/// create a get request to the server with a custom user_agent and nullptr origin. 
    /// this means requests can be created from inside the JS context without having to worry about CORS
	/// </summary>
	/// <param name="uri"></param>
	/// <param name="user_agent"></param>
	/// <returns></returns>
	const cor_response cor_request(std::string uri,
        std::string user_agent)
	{
        //bool success = false;
        //std::string discovery_result;

        //HINTERNET hInternet = InternetOpenA(user_agent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        //HINTERNET hUrl = InternetOpenUrlA(hInternet, uri.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

        //// ensure they both aren't nullptr
        //success = hUrl && hInternet;

        //char buffer[1024];
        //unsigned long bytes_read = 0;

        //while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read)
        //{
        //    discovery_result.append(buffer, bytes_read);
        //}

        //http::close(hUrl, hInternet);

        return {
            "",
            true
        };
	}
}