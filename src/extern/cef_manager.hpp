namespace steam_socket
{
    enum response {
        attached_to_target = 420,
        script_inject_evaluate = 69,
        received_cef_details = 69420
    };
}

struct steam_cef_manager
{
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
    inline void push_to_socket(boost::beast::websocket::stream<tcp::socket>& socket, const char* raw_script)
    {
        nlohmann::json evaluate_script = {
            {"id", steam_socket::response::script_inject_evaluate},
            {"method", "Runtime.evaluate"},
            {"params", {
                {"expression", raw_script}, {"userGesture", true}, {"awaitPromise", false} }
            }
        };
        socket.write(boost::asio::buffer(evaluate_script.dump(4)));
    }
    inline void push_to_socket_session(boost::beast::websocket::stream<tcp::socket>& socket, const char* raw_script, std::string sessionId)
    {
        nlohmann::json evaluate_script = {
            {"id", steam_socket::response::script_inject_evaluate},
            {"method", "Runtime.evaluate"},
            {"sessionId", sessionId},
            {"params", {
                {"expression", raw_script}, {"userGesture", true}, {"awaitPromise", false} }
            }
        };
        socket.write(boost::asio::buffer(evaluate_script.dump(4)));
    }
} steam_interface;