#include <include/json.hpp>
#include <include/config.hpp>

using tcp = boost::asio::ip::tcp;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace websocket = boost::beast::websocket;

class functions
{
private:
    skin_config skinConfig;
public:
    inline void restart_steam()
    {
        if (std::string(GetCommandLineA()).find("-dev") == std::string::npos)
        {
            int message_box_result = MessageBoxA(GetForegroundWindow(), "Steam requires a restart to apply new changes. Do you want to restart steam now?", "Millennium", MB_YESNO | MB_ICONINFORMATION);

            if (message_box_result == IDYES) {
                ShellExecuteA(NULL, "open", "restart.bat", NULL, NULL, SW_HIDE);
                exit(0);
            }
        }
    }

    inline void update_skin(std::string skin_update_name)
    {
        std::string disk_path = std::format("{}/{}", skinConfig.get_steam_skin_path(), skin_update_name);

        if (skin_update_name == "default" || std::filesystem::exists(disk_path)) {

            nlohmann::json settings = json::read_json_file(std::format("{}/settings.json", skinConfig.get_steam_skin_path()));
            settings["active-skin"] = skin_update_name;
            (void)json::write_json_file(std::format("{}/settings.json", skinConfig.get_steam_skin_path()), settings);

            restart_steam();
        }
        else {
            int result = MessageBoxA(GetForegroundWindow(), "Millennium cant seem to find the selected skin?\ncheck the names of the skin folders", "Millennium", MB_ICONINFORMATION);
        }
    }

    inline void open_skin_folder()
    {
        ShellExecute(NULL, "open", std::string(skinConfig.get_steam_skin_path()).c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    inline void allow_javascript(bool value)
    {
        nlohmann::json settings = json::read_json_file(std::format("{}/settings.json", skinConfig.get_steam_skin_path()));
        settings["allow-javascript"] = value;
        (void)json::write_json_file(std::format("{}/settings.json", skinConfig.get_steam_skin_path()), settings);

        restart_steam();
    }

    inline void change_console(bool value)
    {
        nlohmann::json settings = json::read_json_file(std::format("{}/settings.json", skinConfig.get_steam_skin_path()));
        settings["enable-console"] = value;
        (void)json::write_json_file(std::format("{}/settings.json", skinConfig.get_steam_skin_path()), settings);

        restart_steam();
    }
};

class millennium_ipc_listener {
public:
    millennium_ipc_listener(boost::asio::io_context& ioc) :
        acceptor_(ioc, tcp::endpoint(tcp::v4(), 3242)),
        socket_(ioc) {
        ipc_do_accept();
    }

private:
    void ipc_do_accept() {
        acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<millennium_ipc_session>(std::move(socket_))->ipc_initialize();
            }
            ipc_do_accept();
            });
    }

    class millennium_ipc_session : public std::enable_shared_from_this<millennium_ipc_session> {
    public:
        millennium_ipc_session(tcp::socket socket) : ws_(std::move(socket)) {}

        void ipc_initialize() {
            ws_.async_accept([self = shared_from_this()](boost::system::error_code ec) {
                if (!ec) {
                    self->read_request_payload();
                }
            });
        }

        void read_request_payload() 
        {
            ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code read_error, std::size_t bytes_transferred) 
            {
                if (read_error) {
                    return;
                }

                functions* skin_helpers = new functions;
                nlohmann::basic_json<> msg = nlohmann::json::parse(boost::beast::buffers_to_string(self->buffer_.data()));
                ipc_types ipc_message = static_cast<ipc_types>(msg["type"].get<int>());

                switch (ipc_message)
                {
                    case ipc_types::open_skins_folder: skin_helpers->open_skin_folder(); break;
                    case ipc_types::open_url: ShellExecute(NULL, "open", msg["content"].get<std::string>().c_str(), 0, 0, 1); break;
                    case ipc_types::skin_update: skin_helpers->update_skin(msg["content"]); break;
                    case ipc_types::change_javascript: skin_helpers->allow_javascript(msg["content"].get<bool>()); break;
                    case ipc_types::change_console: skin_helpers->change_console(msg["content"].get<bool>()); break;
                }

                delete skin_helpers;
                self->buffer_.consume(bytes_transferred);
                self->read_request_payload();
            });
        }

    private:
        enum ipc_types {
            open_skins_folder, skin_update, open_url, change_javascript, change_console
        };

        websocket::stream<tcp::socket> ws_;
        boost::beast::multi_buffer buffer_;
    };

    tcp::acceptor acceptor_;
    tcp::socket socket_;
};
