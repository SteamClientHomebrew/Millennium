struct steam_cef_manager
{
public:
    static enum response
    {
        attached_to_target = 420,
        script_inject_evaluate = 69,
        received_cef_details = 69420,
        get_document = 9898
    };

    enum script_type
    {
        javascript,
        stylesheet
    };
private:
    skin_config config;

    inline void push_to_socket(boost::beast::websocket::stream<tcp::socket>& socket, std::string raw_script, std::string sessionId = "")
    {
        nlohmann::json evaluate_script = {
            {"id", response::script_inject_evaluate},
            {"method", "Runtime.evaluate"},
            {"params", {
                {"expression", raw_script}, {"userGesture", true}, {"awaitPromise", false} }
            }
        };

        if (!sessionId.empty())
            evaluate_script["sessionId"] = sessionId;

        socket.write(boost::asio::buffer(evaluate_script.dump(4)));
    }
    bool javascript_allowed() {
        return json::read_json_file(std::format("{}/settings.json", config.get_steam_skin_path()))["allow-javascript"].get<bool>();
    }

    std::string append_css(std::string filename) {
        return R"((()=>{const h=')" + filename + R"('; if (!document.querySelectorAll(`link[href = "${h}"]`).length) { const l = document.createElement('link'); l.rel = 'stylesheet', l.href = h, document.head.appendChild(l); } })())";
    }

    std::string append_js(std::string filename) {
        return "!document.querySelectorAll(`script[src='" + filename + "']`).length && document.head.appendChild(Object.assign(document.createElement('script'), { src: '" + filename + "' }));";
    }

public:
    inline std::string discover(std::basic_string<char, std::char_traits<char>, std::allocator<char>> remote_endpoint)
    {
        HINTERNET hInternet = InternetOpenA("millennium.patcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        HINTERNET hUrl = InternetOpenUrlA(hInternet, remote_endpoint.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

        char buffer[1024];
        DWORD total_bytes_read = 0;
        std::string discovery_result;

        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read) discovery_result.append(buffer, total_bytes_read);
        InternetCloseHandle(hUrl); InternetCloseHandle(hInternet);

        return discovery_result;
    }

    __declspec(noinline) void inject_millennium(boost::beast::websocket::stream<tcp::socket>& socket, nlohmann::basic_json<>& socket_response)
    {
        config.append_skins_to_settings();

        std::string javascript = "https://raw.githack.com/ShadowMonster99/millennium-steam-patcher/main/settings_modal/index.js";
        //std::string javascript = "https://steamloopback.host/skins/index.js";

        push_to_socket(socket, append_js(javascript), socket_response["sessionId"].get<std::string>());
    }

    void evaluate(boost::beast::websocket::stream<tcp::socket>& socket, std::string file, script_type type, nlohmann::basic_json<> socket_response = NULL)
    {
        if (type == script_type::javascript && !javascript_allowed())
        {
            return;
        }

        std::string file_address = std::format("https://steamloopback.host/skins/{}/{}", std::string(config.get_settings_config()["active-skin"]), file);

        std::optional<std::string> sessionId;

        if (!socket_response.is_null() && socket_response.contains("sessionId")) {
            sessionId = socket_response["sessionId"].get<std::string>();
        }

        steam_interface.push_to_socket(socket, type == script_type::javascript ? append_js(file_address) : append_css(file_address), sessionId.value_or(std::string()));
    }

} steam_interface;