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

#include "millennium/argp_win32.h"
#include "millennium/plat_msg.h"
#include "millennium/http.h"
#include "millennium/logger.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <filesystem>
#include <fstream>
#include <fmt/format.h>

#ifdef __APPLE__
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <psapi.h>
#include <iphlpapi.h>
#endif

static bool bHasCheckedConnection = false;

class SocketHelpers
{
  private:
    u_short debuggerPort;

#ifdef __APPLE__
    std::string GetPrecomputedBrowserContext()
    {
        const char* browserContextPath = std::getenv("MILLENNIUM_BROWSER_CONTEXT_PATH");
        if (browserContextPath && browserContextPath[0] != '\0') {
            for (size_t attempt = 1; attempt <= 120; ++attempt) {
                if (g_shouldTerminateMillennium->flag.load()) {
                    throw HttpError("Thread termination flag is set, aborting HTTP request.");
                }

                const int browserContextFile = open(browserContextPath, O_RDONLY);
                if (browserContextFile >= 0) {
                    char buffer[512];
                    const ssize_t bytesRead = read(browserContextFile, buffer, sizeof(buffer));
                    close(browserContextFile);

                    if (bytesRead > 0) {
                        unlink(browserContextPath);
                        return std::string(buffer, static_cast<size_t>(bytesRead));
                    }
                }

                usleep(250000);
            }
        }

        return {};
    }
#endif

    u_short GetDebuggerPort()
    {
        return CommandLineArguments::GetRemoteDebuggerPort();
    }

    const std::string GetDebuggerUrl()
    {
        return fmt::format("http://127.0.0.1:{}", debuggerPort);
    }

    struct SteamConnectionProps
    {
        bool hasConnection;
        std::string processName = {};
    };

#ifdef _WIN32
    std::string GetProcessName(DWORD processID)
    {
        char Buffer[MAX_PATH];
        HANDLE Handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

        if (Handle) {
            if (GetModuleFileNameExA(Handle, 0, Buffer, MAX_PATH)) {
                return std::string(Buffer);
            }
            CloseHandle(Handle);
        }

        return {};
    }
#endif

    SteamConnectionProps GetSteamConnectionProps()
    {
#ifdef _WIN32
        // Windows-specific code
        DWORD size = 0;
        PMIB_TCPTABLE2 tcpTable;
        GetTcpTable2(nullptr, &size, TRUE);
        tcpTable = (PMIB_TCPTABLE2)malloc(size);

        if (GetTcpTable2(tcpTable, &size, TRUE) != NO_ERROR) {
            free(tcpTable);
            return { false, "Error getting TCP table" };
        }

        for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
            if (ntohs((u_short)tcpTable->table[i].dwLocalPort) == debuggerPort) {
                const auto targetProcess = std::filesystem::path(GetProcessName(tcpTable->table[i].dwOwningPid));

                if (targetProcess.string().empty()) {
                    // Skip if we can't get the process name
                    continue;
                }

                return { targetProcess.filename().string() == "steamwebhelper.exe", targetProcess.string() };
            }
        }
#endif
        return { true };
    }

  public:
    struct ConnectSocketProps
    {
        std::string commonName;
        std::function<std::string()> fetchSocketUrl;

        std::function<void(websocketpp::client<websocketpp::config::asio_client>*, websocketpp::connection_hdl)> onConnect;

        std::function<void(websocketpp::client<websocketpp::config::asio_client>*, websocketpp::connection_hdl, std::shared_ptr<websocketpp::config::core_client::message_type>)>
            onMessage;
    };

    /**
     * @brief Verify the socket connection was opened by the Steam Client
     * (i.e owned by Steam), and if not report a faulty connection.
     */
    void VerifySteamConnection()
    {
        if (bHasCheckedConnection) {
            return;
        }

        auto [canConnect, processName] = this->GetSteamConnectionProps();
        if (!canConnect) {
            const std::string message =
                fmt::format("Millennium can't connect to Steam because the target port '{}' is currently being used by '{}'.\n"
                            "To address this you must uninstall/close the conflicting app, change the port it uses (assuming its possible), or uninstall Millennium.\n\n"
                            "Millennium & Steam will now close until further action is taken.",
                            debuggerPort, processName);

            Plat_ShowMessageBox("Fatal Error", message.c_str(), MESSAGEBOX_ERROR);
            Logger.Warn(message);
            std::exit(1);
        }

        bHasCheckedConnection = true;
    }

    SocketHelpers() : debuggerPort(GetDebuggerPort())
    {
        Logger.Log("Opting to use '{}' for SteamDBG port", debuggerPort);
        this->VerifySteamConnection();
    }

    /**
     * @brief Get the Steam browser context.
     * It can locally be accessed at localhost:%PORT%/json/version
     *
     * @return std::string The Steam browser context.
     */
    const std::string GetSteamBrowserContext()
    {
        const std::string browserUrl = fmt::format("{}/json/version", this->GetDebuggerUrl());

#ifdef __APPLE__
        const std::string precomputedContext = GetPrecomputedBrowserContext();
        if (!precomputedContext.empty()) {
            Logger.Log("Resolved precomputed Steam browser websocket URL: {}", precomputedContext);
            return precomputedContext;
        }
#endif

        Logger.Log("Waiting for Steam browser context at {}", browserUrl);

        for (size_t attempt = 1;; ++attempt) {
            if (g_shouldTerminateMillennium->flag.load()) {
                throw HttpError("Thread termination flag is set, aborting HTTP request.");
            }

            try {
                const std::string response = Http::Get(browserUrl.c_str(), false, 1L);
                if (response.empty()) {
                    if (attempt == 1 || attempt % 10 == 0) {
                        Logger.Log("Steam browser context is not ready yet (attempt {}).", attempt);
                    }
                } else {
                    nlohmann::basic_json<> instance = nlohmann::json::parse(response);
                    if (instance.contains("webSocketDebuggerUrl") && instance["webSocketDebuggerUrl"].is_string()) {
                        const std::string socketUrl = instance["webSocketDebuggerUrl"];
                        Logger.Log("Resolved Steam browser websocket URL: {}", socketUrl);
                        return socketUrl;
                    }

                    Logger.Warn("Steam browser context response is missing webSocketDebuggerUrl (attempt {}).", attempt);
                }
            } catch (nlohmann::detail::exception& exception) {
                if (attempt == 1 || attempt % 10 == 0) {
                    Logger.Warn("Steam browser context parse failed on attempt {}: {}", attempt, exception.what());
                }
            }

#ifdef __APPLE__
            usleep(250000);
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
#endif
        }
    }

    void ConnectSocket(std::shared_ptr<ConnectSocketProps> socketProps)
    {
        std::string socketUrl;
        const std::string commonName = socketProps->commonName;
        const auto fetchSocketUrl = socketProps->fetchSocketUrl;
        const auto onConnect = socketProps->onConnect;
        const auto onMessage = socketProps->onMessage;

        try {
            Logger.Log("[{}] Resolving websocket endpoint...", commonName);
            socketUrl = fetchSocketUrl();
        }
        /** The request was broke early before it was received. Likely because Millennium is shutting down. */
        catch (HttpError& exception) {
            Logger.Warn("Failed to get Steam browser context: {}", exception.GetMessage());
            return;
        }

        if (socketUrl.empty()) {
            LOG_ERROR("[{}] Socket URL is empty. Aborting connection.", commonName);
            return;
        }

        websocketpp::client<websocketpp::config::asio_client> socketClient;

        try {
            socketClient.set_access_channels(websocketpp::log::alevel::none);
            socketClient.clear_error_channels(websocketpp::log::elevel::none);
            socketClient.set_error_channels(websocketpp::log::elevel::all);

            socketClient.init_asio();

            // Validate handlers before binding
            if (!onConnect || !onMessage) {
                LOG_ERROR("[{}] Invalid event handlers. Connection aborted.", commonName);
                return;
            }

            socketClient.set_open_handler([&socketClient, &commonName, &onConnect](websocketpp::connection_hdl hdl)
            {
                Logger.Log("[{}] WebSocket handshake completed.", commonName);
                onConnect(&socketClient, hdl);
            });
            socketClient.set_message_handler(bind(onMessage, &socketClient, std::placeholders::_1, std::placeholders::_2));
            socketClient.set_fail_handler([&socketClient, &commonName](websocketpp::connection_hdl hdl)
            {
                auto connection = socketClient.get_con_from_hdl(hdl);
                LOG_ERROR("[{}] WebSocket handshake failed: {} [{}]", commonName, connection->get_ec().message(), connection->get_ec().value());
            });
            socketClient.set_close_handler([&socketClient, &commonName](websocketpp::connection_hdl hdl)
            {
                auto connection = socketClient.get_con_from_hdl(hdl);
                Logger.Warn("[{}] WebSocket closed with code {} ({})", commonName, connection->get_remote_close_code(),
                            connection->get_remote_close_reason().empty() ? "no reason" : connection->get_remote_close_reason());
            });

            websocketpp::lib::error_code errorCode;
            auto con = socketClient.get_connection(socketUrl, errorCode);

            if (errorCode) {
                LOG_ERROR("[{}] Failed to establish connection: {} [{}]", commonName, errorCode.message(), errorCode.value());
                return;
            }

            Logger.Log("[{}] Connecting to {}", commonName, socketUrl);
            socketClient.connect(con);
            socketClient.run();
        } catch (const websocketpp::exception& ex) {
            LOG_ERROR("[{}] WebSocket exception thrown -> {}", commonName, ex.what());
        } catch (const std::exception& ex) {
            LOG_ERROR("[{}] Standard exception caught -> {}", commonName, ex.what());
        } catch (...) {
            LOG_ERROR("[{}] Unknown exception caught.", commonName);
        }

        Logger.Log("Disconnected from [{}] module...", commonName);
    }
};
