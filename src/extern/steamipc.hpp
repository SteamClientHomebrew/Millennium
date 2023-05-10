#include <include/json.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

class functions
{
private:
    SkinConfig skinConfig;
public:

    inline void uninstall()
    {
        std::ofstream outfile("uninstall.bat");
        outfile << R"(
            @echo off
            setlocal
            echo [Millennium] uninstalling millennium from steam...
            :LOOP
            tasklist /FI "IMAGENAME eq steam.exe" 2>NUL | find /I /N "steam.exe">NUL
            if "%ERRORLEVEL%"=="0" (
                echo [Millennium] steam is running, waiting for it to close...
                ping -n 2 127.0.0.1 >NUL
                goto LOOP
            )
            echo [Millennium] deleting file User32.dll...
            del /F User32.dll
            echo [Millennium] deleting file User32.pdb...
            del /F User32.pdb
            echo [Millennium] deleting file libcrypto-3.dll...
            del /F libcrypto-3.dll
            echo [Millennium] uninstall successful. thanks for being a part of Millennium!
            echo [Millennium] you can now close this window...
            pause
            )";
        outfile.close();

        ShellExecuteA(NULL, "open", "uninstall.bat", NULL, NULL, SW_SHOW);
        exit(0);
    }

    inline void restart_steam()
    {
        int message_box_result = MessageBoxA(GetForegroundWindow(), "Steam requires a restart to apply new changes. Do you want to restart steam now?", "Millennium", MB_YESNO | MB_ICONINFORMATION);

        if (message_box_result == IDYES) {
            ShellExecuteA(NULL, "open", "restart.bat", NULL, NULL, SW_HIDE);
            exit(0);
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

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

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
                //std::cout << "New WebSocket connection accepted\n";
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

        void read_request_payload() {
            ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred) 
            {
                if (!ec)  {

                    functions* skin_helpers = new functions;
                    nlohmann::basic_json<> incoming_payload = nlohmann::json::parse(boost::beast::buffers_to_string(self->buffer_.data()));
                    ipc_types ipc_message = static_cast<ipc_types>(incoming_payload["type"].get<int>());

                    if (ipc_message == ipc_types::open_skins_folder) 
                        skin_helpers->open_skin_folder();
                    
                    if (ipc_message == ipc_types::open_url)
                        ShellExecute(NULL, "open", incoming_payload["content"].get<std::string>().c_str(), NULL, NULL, SW_SHOWNORMAL);

                    if (ipc_message == ipc_types::skin_update) 
                        skin_helpers->update_skin(incoming_payload["content"]);
                    
                    if (ipc_message == ipc_types::change_javascript) 
                        skin_helpers->allow_javascript(incoming_payload["content"].get<bool>());
                    
                    if (ipc_message == ipc_types::change_console)
                        skin_helpers->change_console(incoming_payload["content"].get<bool>()); 
                    
                    if (ipc_message == ipc_types::uninstall)
                        skin_helpers->uninstall();

                    if (ipc_message == ipc_types::message_box)
                    {
                        MessageBoxA(GetForegroundWindow(), incoming_payload["content"].get<std::string>().c_str(), "Millennium", MB_ICONINFORMATION);
                    }
             

                    delete skin_helpers;
                    self->buffer_.consume(bytes_transferred);
                    self->read_request_payload();
                }
            });
        }

    private:
        enum ipc_types
        {
            open_skins_folder = 0,
            skin_update = 1,
            open_url = 2,
            change_javascript = 3,
            change_console = 4,
            uninstall = 5,
            message_box = 6
        };

        websocket::stream<tcp::socket> ws_;
        boost::beast::multi_buffer buffer_;
    };

    tcp::acceptor acceptor_;
    tcp::socket socket_;
};
