#pragma once
#include <extern/injector/inject.hpp>

struct endpoints
{
public:
    //used for serving steam/steamui contents, hosted by the steam client
    boost::network::uri::uri steam_resources = 
        boost::network::uri::uri("https://steamloopback.host");

    //steam cef remote debugging endpoint
    boost::network::uri::uri debugger = 
        boost::network::uri::uri("http://127.0.0.1:8080");
}endpoints;

/// <summary>
/// javascript helpers when interacting with the DOM
/// </summary>
class cef_dom {
private:
    class dom_head {
    public:
        virtual const std::basic_string<char, std::char_traits<char>, std::allocator<char>> add(std::string) {
            return std::string();
        }
    };

    /// <summary>
    /// evaluate scripts from a url host on a js vm
    /// </summary>
    class runtime {
    public: 
        const std::string evaluate(std::string remote_url) noexcept {
            return std::format("(() => fetch('{}').then((response) => response.text()).then((data) => eval(data)))()", remote_url);
        }
    };

    class stylesheet : public dom_head {
    public:
        const std::basic_string<char, std::char_traits<char>, std::allocator<char>> add(std::string filename) override {
            return std::format("!document.querySelectorAll(`link[href='{}']`).length && document.head.appendChild(Object.assign(document.createElement('link'), {{ rel: 'stylesheet', href: '{}' }}));", filename, filename);
        }
    };

    class javascript : public dom_head {
    public:
        const std::basic_string<char, std::char_traits<char>, std::allocator<char>> add(std::string filename) override {
            return std::format("!document.querySelectorAll(`script[src='{}'][type='module']`).length && document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module' }}));", filename, filename);
        }
    };

public:
    /// <summary>
    /// uses a singleton class on the dom for ease of access
    /// </summary>
    /// <returns>cef_dom instance, statically typed</returns>
    static cef_dom& get() {
        static cef_dom instance;
        return instance;
    }

    runtime runtime_handler;

    javascript javascript_handler;
    stylesheet stylesheet_handler;

private:
    cef_dom() {}
    cef_dom(const cef_dom&) = delete;
    cef_dom& operator=(const cef_dom&) = delete;
};

struct steam_cef_manager
{
public:
    /// <summary>
    /// response types that are handled by millennium in the client interface handler
    /// </summary>
    static enum response
    {
        attached_to_target = 420,
        script_inject_evaluate = 69,
        received_cef_details = 69420,
        get_document = 9898
    };

    enum script_type { javascript, stylesheet };
private:
    skin_config config;

    /// <summary>
    /// https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#method-evaluate
    /// create a javascript pipe to the dom that runs on a vm, sending too much js causes socket crash, use cef_dom runtime eval 
    /// </summary>
    /// <param name="socket">steam cef socket, unique to each page</param>
    /// <param name="raw_script">javascript to be executed</param>
    /// <param name="sessionId">socket identifier, if needed (not required for remote webpages)</param>
    inline void push_to_socket(boost::beast::websocket::stream<tcp::socket>& socket, std::string raw_script, std::string sessionId = std::string())
    {
        const std::string millennium_functions = R"(
            class millennium{static sharedjscontext_exec(c){const t=new WebSocket(atob('d3M6Ly9sb2NhbGhvc3Q6MzI0Mg==')),r=()=>t.send(JSON.stringify({type:6,content:c}));return new Promise((e,n)=>{t.onopen=r,t.onmessage=t=>(e(JSON.parse(t.data)),t.target.close()),t.onerror=n,t.onclose=()=>n(new Error)})}}window.millennium=millennium;
        )";

        nlohmann::json evaluate_script = {
            {"id", response::script_inject_evaluate},
            {"method", "Runtime.evaluate"},
            {"params", {{"expression", std::string()}}}
        };

        if (!sessionId.empty())
            evaluate_script["sessionId"] = sessionId;

        evaluate_script["params"]["expression"] = raw_script;
        socket.write(boost::asio::buffer(evaluate_script.dump(4)));

        evaluate_script["params"]["expression"] = millennium_functions;
        socket.write(boost::asio::buffer(evaluate_script.dump(4)));
    }

public:
    /// <summary>
    /// discover information from remote_endpoint
    /// </summary>
    /// <param name="remote_endpoint">absolute uri</param>
    /// <returns>string value of the result, does not include headers</returns>
    inline std::string discover(std::basic_string<char, std::char_traits<char>, std::allocator<char>> remote_endpoint)
    {
        HINTERNET hInternet = InternetOpenA("millennium.patcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        HINTERNET hUrl = InternetOpenUrlA(hInternet, remote_endpoint.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

        if (!hUrl || !hInternet)
        {
            throw std::runtime_error("InternetOpen*A was nullptr");
        }

        char buffer[1024];
        DWORD total_bytes_read = 0;
        std::string discovery_result;

        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read) discovery_result.append(buffer, total_bytes_read);
        InternetCloseHandle(hUrl); InternetCloseHandle(hInternet);

        return discovery_result;
    }

    /// <summary>
    /// injects millennium into the settings page. its called when the settings page is opened
    /// </summary>
    /// <param name="socket">steam socket</param>
    /// <param name="socket_response">used sessionId to target the setting modal</param>
    /// <returns></returns>
    __declspec(noinline) void __fastcall inject_millennium(boost::beast::websocket::stream<tcp::socket>& socket, nlohmann::basic_json<>& socket_response)
    {
        //std::string javascript = "https://shadowmonster99.github.io/millennium-steam-patcher/root/dependencies/source.js";
        std::string javascript = std::format("{}/skins/index.js", endpoints.steam_resources.string());

        steam_interface.push_to_socket(
            socket, 
            //uses js vm to run the javascript code from a url as its too long to send over socket, steamloopbackhost is allowed
            cef_dom::get().runtime_handler.evaluate(javascript), 
            socket_response["sessionId"].get<std::string>()
        );
    }

    /// <summary>
    /// interface to inject js into a cef instance
    /// </summary>
    /// <param name="socket">cef instance from steam</param>
    /// <param name="file">the remote or local endpoint to a file</param>
    /// <param name="type">javascript or stylesheet injections from script_type</param>
    /// <param name="socket_response">if not null, its assumed to use sessionId (i.e for client handling)</param>
    void evaluate(boost::beast::websocket::stream<tcp::socket>& socket, std::string file, script_type type, nlohmann::basic_json<> socket_response = NULL)
    {
        if (type == script_type::javascript && (registry::get_registry("allow-javascript") == "false")) {
            return;
        }

        //calculate endpoint type, if remote url retain info, if local add loopbackhost
        if (file.compare(0, 4, "http") != 0) {
            file = std::format("{}/skins/{}/{}", endpoints.steam_resources.string(), std::string(registry::get_registry("active-skin")), file);
        }

        std::optional<std::string> sessionId;

        if (!socket_response.is_null() && socket_response.contains("sessionId")) {
            sessionId = socket_response["sessionId"].get<std::string>();
        }

        //pass in sessionId if available
        switch (type)
        {
        case script_type::javascript:
            steam_interface.push_to_socket(socket, cef_dom::get().javascript_handler.add(file).data(), sessionId.value_or(std::string()));
            break;
        case script_type::stylesheet:
            steam_interface.push_to_socket(socket, cef_dom::get().stylesheet_handler.add(file).data(), sessionId.value_or(std::string()));
            break;
        default: 
            throw std::runtime_error("invalid target scripting type");
        }
    }

} steam_interface;

/// <summary>
/// interact with SteamJSContext, which is the host of the steam client, includes functions for the entire steam client under
/// the SteamClient class, all pages are created and destroyed from this instance
/// </summary>
struct steam_js_context {
private:
    boost::asio::io_context io_context;
    boost::beast::websocket::stream<tcp::socket> socket;
    tcp::resolver resolver;

    /// <summary>
    /// connect to the socket of SharedJSContext
    /// </summary>
    /// <param name="callback"></param>
    void establish_socket_handshake(std::function<void()> callback)
    {
        if (callback == nullptr)
            throw std::runtime_error("callback function was nullptr");

        boost::asio::connect(socket.next_layer(), resolver.resolve(endpoints.debugger.host(), endpoints.debugger.port()));

        nlohmann::json instances = nlohmann::json::parse(steam_interface.discover(endpoints.debugger.string() + "/json"));

        //instances contains all pages, filter the correct instance
        auto itr_impl = std::find_if(instances.begin(), instances.end(), [](const nlohmann::json& instance) { 
            return instance["title"].get<std::string>() == "SharedJSContext"; 
        });

        if (itr_impl == instances.end()) {
            throw std::runtime_error("SharedJSContext wasn't instantiated");
        }

        boost::network::uri::uri socket_url((*itr_impl)["webSocketDebuggerUrl"].get<std::string>());
        socket.handshake(endpoints.debugger.host(), socket_url.path());

        //implicit type converge interrupt_handler
        static_cast<void>(callback());
        close_socket();
    }

    //prolonging the socket closure causes socket failure and slow exec times all around
    void close_socket()
    {
        boost::system::error_code error_code;
        socket.close(boost::beast::websocket::close_code::normal, error_code);
    }

public:

    steam_js_context() : socket(io_context), resolver(io_context) {}

    /// <summary>
    /// reload the SteamJSContext, which will reload the steam interface, but not the app
    /// </summary>
    void reload()
    {
        establish_socket_handshake([this]() {
            //https://chromedevtools.github.io/devtools-protocol/tot/Page/#method-reload
            socket.write(boost::asio::buffer(nlohmann::json({ {"id", 89}, {"method", "Page.reload"} }).dump()));
        });
    }

    /// <summary>
    /// restart the app completely, SharedJsContext and the app intself, cmd args are retained
    /// </summary>
    void restart()
    {
        establish_socket_handshake([this]() {
            //not used anymore, but here in case
            socket.write(boost::asio::buffer(nlohmann::json({ {"id", 8987}, {"method", "Runtime.evaluate"}, {"params", {{"expression", "setTimeout(() => {SteamClient.User.StartRestart(true)}, 1000)"}, {"userGesture", true}, {"awaitPromise", false}}} }).dump()));
        });
    }

    /// <summary>
    /// execute js on the SharedJSContext, where you can access the SteamClient in full
    /// </summary>
    /// <param name="javascript">code to execute on the page</param>
    /// <returns></returns>
    std::string exec_command(std::string javascript)
    {
        std::promise<std::string> promise;

        establish_socket_handshake([&]() {
            socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1337}, {"method", "Runtime.evaluate"}, {"params", {{"expression", javascript}, {"awaitPromise", true}}} }).dump()));

            boost::beast::multi_buffer buffer; socket.read(buffer);
            promise.set_value(nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()))["result"].dump());
        });

        return promise.get_future().get();
    }
};