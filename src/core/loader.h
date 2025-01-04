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
#include <core/py_controller/co_spawn.h>

extern std::shared_ptr<InterpreterMutex> g_threadTerminateFlag;

class PluginLoader {
public:

	PluginLoader(std::chrono::system_clock::time_point startTime, uint16_t ftpPort);
	// ~PluginLoader();

	const void StartBackEnds(PythonManager& manager);
	const void StartFrontEnds();
	const void InjectWebkitShims();

private:
	const void Initialize();

	const void PrintActivePlugins();
	const std::thread ConnectCEFBrowser(void* cefBrowserHandler, SocketHelpers* socketHelpers);

	std::unique_ptr<SettingsStore> m_settingsStorePtr;
	std::shared_ptr<std::vector<SettingsStore::PluginTypeSchema>> m_pluginsPtr, m_enabledPluginsPtr;
	std::chrono::system_clock::time_point m_startTime;
	uint16_t m_ftpPort, m_ipcPort;

	std::vector<std::thread> m_threadPool;
};

namespace Sockets {
	bool PostShared(nlohmann::json data);
	bool PostGlobal(nlohmann::json data);
	void Shutdown();
}