#pragma once
#include <string>
#include <vector>

namespace ipc_handler
{
	struct cor_response {
		std::string content;
		bool success;
	};

	const cor_response cor_request(std::string uri,
		std::string user_agent);
}