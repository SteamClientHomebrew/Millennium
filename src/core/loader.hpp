#pragma once
#ifdef _WIN32
#undef _WINSOCKAPI_
#include <winsock2.h>
#endif
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <string>
#include <nlohmann/json.hpp>
#include <generic/stream_parser.h>

static std::string sessionId;

struct shared {
	websocketpp::client
		<websocketpp::config::asio_client>* client;
	websocketpp::connection_hdl handle;
};

class plugin {
public:

	static plugin get() {
		static plugin _p;
		return _p;
	}

	void bootstrap();
};

namespace tunnel {
	bool post_shared(nlohmann::json data);
	bool post_global(nlohmann::json data);
}