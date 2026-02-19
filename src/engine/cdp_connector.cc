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

#include "millennium/cmdline_parse.h"
#include "millennium/cdp_api.h"
#include "millennium/http.h"
#include "millennium/logger.h"
#include "millennium/cdp_connector.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <fmt/format.h>

socket_utils::socket_utils() : m_debugger_port(get_steam_debugger_port())
{
    logger.log("Opting to use '{}' for SteamDBG port", m_debugger_port);
}

const std::string socket_utils::get_steam_browser_context()
{
    try {
        std::string browser_url = fmt::format("{}/json/version", this->get_steam_debugger_url());
        json instance = nlohmann::json::parse(Http::Get(browser_url.c_str(), true, 5L));

        return instance["webSocketDebuggerUrl"];
    } catch (nlohmann::detail::exception& exception) {
        LOG_ERROR("An error occurred while making a connection to Steam browser context. It's likely that the debugger port '{}' is in use by another process. exception -> {}",
                  m_debugger_port, exception.what());
        std::exit(1);
    }
}

void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> socket_props)
{
    std::string socket_url;
    std::string name = socket_props->name;
    auto fetch_socket_url = socket_props->fetch_socket_url;
    auto on_connect_cb = socket_props->on_connect;

    try {
        socket_url = fetch_socket_url();
    } catch (HttpError& exception) {
        logger.warn("Failed to get Steam browser context: {}", exception.GetMessage());
        return;
    }

    if (socket_url.empty()) {
        LOG_ERROR("[{}] Socket URL is empty. Aborting connection.", name);
        return;
    }

    websocketpp::client<websocketpp::config::asio_client> socket_client;
    std::shared_ptr<cdp_client> cdp;

    try {
        socket_client.set_access_channels(websocketpp::log::alevel::none);
        socket_client.clear_error_channels(websocketpp::log::elevel::none);
        socket_client.set_error_channels(websocketpp::log::elevel::none);
        socket_client.init_asio();

        if (!on_connect_cb) {
            LOG_ERROR("[{}] Invalid event handlers. Connection aborted.", name);
            return;
        }

        websocketpp::lib::error_code ec;
        auto con = socket_client.get_connection(socket_url, ec);
        if (ec) {
            LOG_ERROR("[{}] Failed to get_connection: {} [{}]", name, ec.message(), ec.value());
            return;
        }

        cdp = std::make_shared<cdp_client>(con);
        con->set_message_handler([cdp = cdp, name](websocketpp::connection_hdl, auto msg) { cdp->handle_message(msg->get_payload()); });

        con->set_open_handler([cdp = cdp, on_connect_cb, name](websocketpp::connection_hdl h)
        {
            try {
                if (auto locked = h.lock()) {
                    on_connect_cb(cdp);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("[{}] Exception in onConnect: {}", name, e.what());
            } catch (...) {
                LOG_ERROR("[{}] Unknown exception in onConnect", name);
            }
        });

        socket_client.connect(con);
        socket_client.run();

    } catch (const websocketpp::exception& ex) {
        LOG_ERROR("[{}] WebSocket exception thrown -> {}", name, ex.what());
    } catch (const std::exception& ex) {
        LOG_ERROR("[{}] Standard exception caught -> {}", name, ex.what());
    } catch (...) {
        LOG_ERROR("[{}] Unknown exception caught.", name);
    }

    logger.log("[{}] Shutting down CDP client...", name);
    if (cdp) {
        cdp->shutdown();
    }
    logger.log("Disconnected from [{}] module...", name);
}

unsigned short socket_utils::get_steam_debugger_port()
{
    return CommandLineArguments::get_remote_debugger_port();
}

const std::string socket_utils::get_steam_debugger_url()
{
    return fmt::format("http://127.0.0.1:{}", m_debugger_port);
}
