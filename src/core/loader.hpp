#pragma once
#ifdef _WIN32
#undef _WINSOCKAPI_
#include <winsock2.h>
#endif
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <string>
#include <nlohmann/json.hpp>
#include <sys/locals.h>
#include <socket/await_pipe.h>

class PluginLoader {
public:

	PluginLoader(std::chrono::system_clock::time_point startTime);

	const void StartBackEnds();
	const void StartFrontEnds();

private:
	const void PrintActivePlugins();

	const std::thread ConnectSharedJSContext(void* sharedJsHandler, SocketHelpers* socketHelpers);
	const std::thread ConnectCEFBrowser(void* cefBrowserHandler, SocketHelpers* socketHelpers);

	std::unique_ptr<SettingsStore> m_settingsStorePtr;
	std::shared_ptr<std::vector<SettingsStore::PluginTypeSchema>> m_pluginsPtr;
	std::chrono::system_clock::time_point m_startTime;
};

namespace Sockets {
	bool PostShared(nlohmann::json data);
	bool PostGlobal(nlohmann::json data);
}