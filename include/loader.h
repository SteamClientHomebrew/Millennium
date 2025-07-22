/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once
#ifdef _WIN32
#undef _WINSOCKAPI_
#include <winsock2.h>
#endif
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <string>
#include <nlohmann/json.hpp>
#include "locals.h"
#include "co_spawn.h"

extern std::shared_ptr<InterpreterMutex> g_threadTerminateFlag;
#include "await_pipe.h"

class PluginLoader {
public:

	PluginLoader(std::chrono::system_clock::time_point startTime);
	// ~PluginLoader();

	const void StartBackEnds(PythonManager& manager);
	const void StartFrontEnds();
	const void InjectWebkitShims();

private:
	const void Initialize();

	const void PrintActivePlugins();
	std::shared_ptr<std::thread> ConnectCEFBrowser(void* cefBrowserHandler, SocketHelpers* socketHelpers);

	std::unique_ptr<SettingsStore> m_settingsStorePtr;
	std::shared_ptr<std::vector<SettingsStore::PluginTypeSchema>> m_pluginsPtr, m_enabledPluginsPtr;
	std::chrono::system_clock::time_point m_startTime;

	std::vector<std::thread> m_threadPool;
};

namespace Sockets {
	bool PostShared(nlohmann::json data);
	bool PostGlobal(nlohmann::json data);
	void Shutdown();
}