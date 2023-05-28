#include <extern/injector/inject.hpp>
#include <include/config.hpp>
#include <extern/steam/cef_manager.hpp>
#include <extern/ipc/steamipc.hpp>
#include <regex>

namespace websocket = boost::beast::websocket;
using namespace boost;

Console console; skin_config config;
using tcp = boost::asio::ip::tcp;

nlohmann::basic_json<> skin_json_config;

/// <summary>
/// responsible for handling the remote steam pages
/// </summary>
class remote
{
private:
    //socket variables
    boost::asio::io_context io_context;
    boost::beast::websocket::stream<tcp::socket> socket;
    asio::ip::tcp::resolver resolver;

    //initialize the socket from class initializer
    remote() : socket(io_context), resolver(io_context) {}

    /// <summary>
    /// used by the remote interface handler to check if the cef instance needs to be injected into
    /// </summary>
    /// <param name="patch">iteration of the patch from config</param>
    /// <param name="instance">cef instance</param>
    /// <returns>should patch</returns>
    inline bool __cdecl should_patch_interface(nlohmann::json& patch, const nlohmann::json& instance)
    {
        //store a static variable that stores what pages have already been patched to prevent overworking the threads
        static std::vector<std::string> patched = {};

        //check if the current instance was already patched
        for (const std::string& element : patched) {
            if (element == instance["webSocketDebuggerUrl"].get<std::string>()) {
                return false;
            }
        }

        //get the url of the page we are active on 
        std::string steam_page_url_header = instance["url"].get<std::string>();

        if (steam_page_url_header.find(patch["url"]) xor std::string::npos) 
        {
            console.imp("[remote] -> regex match = > " + std::string(steam_page_url_header) + " regex: [" + patch["url"].get<std::string>() + "]");
            //mark that it was successfully patched
            patched.push_back(instance["webSocketDebuggerUrl"].get<std::string>());
        }
        else return false;
    }

    /// <summary>
    /// set page settings using the chrome dev tools protocol
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    const void page_settings(void) noexcept {
        //remove CSP on the remote page to allow cross origin scripting, specifically
        //from the steamloopbackhost
        //https://chromedevtools.github.io/devtools-protocol/
        socket.write(boost::asio::buffer(
            nlohmann::json({ 
                {"id", 6}, 
                {"method", "Page.setBypassCSP"}, 
                {"params", {{"enabled", true}}} 
            }).dump()
        ));
        //enable page event logging
        socket.write(boost::asio::buffer(
            nlohmann::json({ 
                {"id", 1}, 
                {"method", "Page.enable"} 
            }).dump()
        ));
    }

    /// <summary>
    /// handle remote interface, that are not part of the steam client.
    /// </summary>
    /// <param name="page">cef page instance</param>
    /// <param name="css_to_evaluate"></param>
    /// <param name="js_to_evaluate"></param>
    const void patch(const nlohmann::json& page, std::string css_eval, std::string js_eval)
    {
        boost::network::uri::uri socket_url(page["webSocketDebuggerUrl"].get<std::string>());
        boost::asio::connect(socket.next_layer(), resolver.resolve(endpoints.debugger.host(), endpoints.debugger.port()));

        //connect to the socket associated with the page directly instead of using browser instance.
        //this way after the first injection we can inject immediatly into the page because we dont need to wait for the title 
        //of the remote page, we can just inject into it in a friendly way
        socket.handshake(endpoints.debugger.host(), socket_url.path());

        //activiate page settings needed
        page_settings();

        //evaluate javascript and stylesheets
        std::function<void()> evaluate_scripting = ([&]() -> void {
            if (not js_eval.empty()) 
                steam_interface.evaluate(socket, js_eval, steam_interface.script_type::javascript);
            if (not css_eval.empty()) 
                steam_interface.evaluate(socket, css_eval, steam_interface.script_type::stylesheet);
        }); 
        //try to evaluate immediatly in case of slow initial connection times,
        //ie millennium missed the original page events so it misses the first inject
        evaluate_scripting();

        while (true) {
            //read socket response and convert to a json response
            boost::beast::flat_buffer buffer; socket.read(buffer);
            nlohmann::json response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));

            //check response method
            if (response["method"] == "Page.frameResized") continue;

            //run scripting until it succeeds
            while (true) {
                evaluate_scripting();

                //create a new response buffer but on the same socket stream to prevent shared memory usage
                boost::beast::multi_buffer buffer; socket.read(buffer);

                //the js evaluation was successful so we can break.
                if (nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()))
                    ["result"]["exceptionDetails"]["exception"]["className"] not_eq "TypeError") {
                    break;
                }
            }
        }
    }
public:
    //create a singleton instance of the class incase it needs to be refrences elsewhere and
    //only one instance needs to be running
    static remote& get_instance()
    {
        static remote instance;
        return instance;
    }

    remote(const remote&) = delete;
    remote& operator=(const remote&) = delete;

    void handle_interface()
    {
        /// <summary>
        /// check the skin config against every page that exists, and if matching and is a remote url, then patch
        /// </summary>
        std::function<void(nlohmann::json&)> check_page = ([this](nlohmann::json& instances) {
            for (nlohmann::basic_json<>& instance : instances)
            {
                for (nlohmann::basic_json<>& address : skin_json_config["patch"])
                {
                    //check if the patch address is a url (remote) and check if it should be patched
                    if (not (address["url"].get<std::string>().compare(0, 4, "http") == 0) or not should_patch_interface(address, instance))
                        continue;

                    //create a seperate thread to run math because there are multiple remote pages that need handling
                    std::thread thread([&] {
                        try {
                            //use css and js if available
                            patch(instance, address.value("css", std::string()), address.value("js", std::string()));
                        }
                        catch (const boost::system::system_error& ex) {
                            if (ex.code() == boost::asio::error::misc_errors::eof) {
                                console.log("targetted CEF instance was destroyed");
                            }
                        }
                    });
                    //detach the running thread and leave it to be managed by the heap :(
                    thread.detach();
                }
            }
        });
    
        do {
            nlohmann::json instances = nlohmann::json::parse(steam_interface.discover(endpoints.debugger.string() + "/json"));

            //config fail meaning file was deleted, misplaced of the skin doesnt contain valid skin information
            if (skin_json_config["config_fail"]) 
                continue;

            //check all the instances 
            try {
                check_page(instances);
            }
            catch (nlohmann::json::exception& ex) {
                continue;
            }

        } while (true);
    }
};

/// <summary>
/// responsible for handling the client steam pages
/// </summary>
class client
{
private:
    //socket variables
    boost::asio::io_context io_context;
    boost::beast::websocket::stream<tcp::socket> socket;
    asio::ip::tcp::resolver resolver;

    //initialize the socket from class initializer
    client() : socket(io_context), resolver(io_context) {}

    mutable std::string sessionId, page_title;

    nlohmann::json socket_response;

    /// <summary>
    /// socket response errors
    /// </summary>
    class socket_response_error : public std::exception
    {
    public:
        enum errors {
            socket_error_message
        };

        socket_response_error(int errorCode) : errorCode_(errorCode) {}

        int code() const
        {
            return errorCode_;
        }

    private:
        int errorCode_;
    };

    /// <summary>
    /// function responsible for handling css and js injections
    /// </summary>
    /// <param name="title">the title of the cef page to inject into</param>
    void patch(std::string title)
    {
        for (nlohmann::basic_json<>& patch : skin_json_config["patch"])
        {
            bool contains_http = patch["url"].get<std::string>().compare(0, 4, "http") == 0;
            //used regex match instead of regex find or other sorts, make sure you validate your regex 
            bool regex_match = std::regex_match(title, std::regex(patch["url"].get<std::string>()));

            if (contains_http or not regex_match)
                continue;

            console.imp("[client] match => " + std::string(title) + " regex: [" + patch["url"].get<std::string>() + "]");

            if (patch.contains("css")) steam_interface.evaluate(socket, patch["css"], steam_interface.script_type::stylesheet, socket_response);
            if (patch.contains("js"))  steam_interface.evaluate(socket, patch["js"], steam_interface.script_type::javascript, socket_response);
        }
    }

    /// <summary>
    /// cef instance created, captures remote and client based targets, however only local targets are controlled
    /// </summary>
    void target_created()
    {
        socket.write(boost::asio::buffer(nlohmann::json({
            {"id", steam_cef_manager::response::attached_to_target},
            {"method", "Target.attachToTarget"},
            {"params", { {"targetId", socket_response["params"]["targetInfo"]["targetId"]}, {"flatten", true} } }
        }).dump()));
    }

    /// <summary>
    /// cef instance target info changed, happens when something ab the instance changes, i.e the client title or others
    /// </summary>
    void target_info_changed()
    {
        //run a js vm to get cef info
        socket.write(boost::asio::buffer(
            nlohmann::json({
                {"id", steam_cef_manager::response::received_cef_details},
                {"method", "Runtime.evaluate"},
                {"sessionId", sessionId},
                {"params", {
                    //create an event to get the page title and url to use it later since it isnt evaluated properly on
                    //https://chromedevtools.github.io/devtools-protocol/tot/Target/#event-targetInfoChanged
                    {"expression", "(() => { return { title: document.title, url: document.location.href }; })()"},
                    {"returnByValue", true}
                }}
            }).dump()
        ));
    }

    /// <summary>
    /// received cef details from the js query, returns title and url
    /// </summary>
    void received_cef_details()
    {
        if (socket_response.contains("error"))
        {
            //throw socket error to catch on the backend
            throw socket_response_error(socket_response_error::errors::socket_error_message);
        }

        page_title = socket_response["result"]["result"]["value"]["title"].get<std::string>();
        patch(page_title);

        //create a get document request, used to get query from the page, where further patching can happen
        socket.write(boost::asio::buffer(
            nlohmann::json({
                {"id", steam_cef_manager::response::get_document},
                {"method", "DOM.getDocument"},
                {"sessionId", sessionId}
            }).dump()
        ));
    }

    /// <summary>
    /// gets the document callback
    /// instantiated from the cef details callback 
    /// contains a json root of the query on the document, hard to parse all types
    /// </summary>
    void get_doc_callback()
    {
        //get the <head> attributes of the dom which is what is most important in selecting pages
        std::string attributes = std::string(socket_response["result"]["root"]["children"][1]["attributes"][1]);

        //console.log(std::format("[FOR DEVELOPERS]\nselectable <html> query on page [{}]:\n -> [{}]\n", page_title, attributes));

        for (nlohmann::basic_json<>& patch : skin_json_config["patch"])
        {
            bool contains_http = patch["url"].get<std::string>().compare(0, 4, "http") == 0;

            if (contains_http || attributes.find(patch["url"].get<std::string>()) == std::string::npos)
                continue;

            console.imp("[class_name] match => " + std::string(page_title) + " regex: [" + patch["url"].get<std::string>() + "]");

            if (patch.contains("css")) steam_interface.evaluate(socket, patch["css"], steam_interface.script_type::stylesheet, socket_response);
            if (patch.contains("js"))  steam_interface.evaluate(socket, patch["js"], steam_interface.script_type::javascript, socket_response);
        }

        /// <summary>
        /// inject millennium into the settings page, uses query instead of title because title varies between languages
        /// </summary>
        if (attributes.find("settings_SettingsModalRoot_") != std::string::npos) {
            steam_interface.inject_millennium(socket, socket_response);
        }
    }

    /// <summary>
    /// received the sessionId from the cef instance after we attached to it
    /// we then use the sessionId provided to use runtime.evaluate on the instance
    /// </summary>
    void attached_to_target()
    {
        sessionId = socket_response["result"]["sessionId"];
    }

public:
    //create a singleton instance of the class incase it needs to be refrences elsewhere and
    //only one instance needs to be running
    static client& get_instance()
    {
        static client instance;
        return instance;
    }

    client(const client&) = delete;
    client& operator=(const client&) = delete;

    /// <summary>
    /// base derivative, handles all socket responses
    /// </summary>
    void handle_interface()
    {
        //connect to the browser websocket
        asio::connect(
            socket.next_layer(), 
            resolver.resolve(endpoints.debugger.host(), endpoints.debugger.port())
        );

        //create handshake on the remote url where the browser instance is stored
        socket.handshake(endpoints.debugger.host(), network::uri::uri(
            nlohmann::json::parse(steam_interface.discover(endpoints.debugger.string() + "/json/version"))
            ["webSocketDebuggerUrl"]
        ).path());

        //turn on discover targets from the cef browser instance
        socket.write(boost::asio::buffer(nlohmann::json({
            { "id", 1 },
            { "method", "Target.setDiscoverTargets" },
            { "params", {{"discover", true}} }
        }).dump()));

        while (true)
        {
            boost::beast::flat_buffer buffer; socket.read(buffer);
            socket_response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));

            /// <summary>
            /// socket response associated with getting document details
            /// </summary>
            if (socket_response["id"] == steam_cef_manager::response::get_document) {
                try {
                    get_doc_callback();
                }
                //document contains no selectable query on the <head> 
                catch (const nlohmann::json::exception& e) {
                    continue;
                }
            }

            /// <summary>
            /// socket response called when a new CEF target is created with valid parameters
            /// </summary>
            if (socket_response["method"] == "Target.targetCreated" && socket_response.contains("params")) {
                target_created();
            }

            /// <summary>
            /// socket reponse called when a target cef instance changes in any way possible
            /// </summary>
            if (socket_response["method"] == "Target.targetInfoChanged" && socket_response["params"]["targetInfo"]["attached"]) {
                target_info_changed();
            }

            /// <summary>
            /// socket response called when cef details have been received. 
            /// title, url and web debugger url
            /// </summary>
            if (socket_response["id"] == steam_cef_manager::response::received_cef_details) {  
                try {
                    received_cef_details();
                }
                catch (socket_response_error& ex) {
                    //response contains error message, we can discard and continue where we wait for a valid connection
                    if (ex.code() == socket_response_error::errors::socket_error_message) {
                        continue;
                    }
                }
            }

            /// <summary>
            /// socket response when attached to a new target, used to get the sessionId to use later
            /// </summary>
            if (socket_response["id"] == steam_cef_manager::response::attached_to_target) {
                attached_to_target();
            }
        }
    }
};

/// <summary>
/// start the steam IPC to communicate between C++ and js
/// </summary>
void steam_client::steam_to_millennium_ipc()
{
    try {
        boost::asio::io_context ioc;
        millennium_ipc_listener listener(ioc);
        ioc.run();
    }
    catch (std::exception& e) {
        console.log("ipc exception " + std::string(e.what()));
    }
}

/// <summary>
/// initialize millennium components
/// </summary>
steam_client::steam_client()
{
    std::thread steam_client_interface_handler_threadworker([this]() {
        try {
            client::get_instance().handle_interface();
        }
        catch (nlohmann::json::exception& ex) { console.err("client: " + std::string(ex.what())); }
        catch (const boost::system::system_error& ex) { console.err("client: " + std::string(ex.what())); }
        catch (const std::exception& ex) { console.err("client: " + std::string(ex.what())); }
        catch (...) { console.err("client: unkown"); }

    });
    std::thread steam_remote_interface_handler_threadworker([this]() {
        try {
            remote::get_instance().handle_interface();
        }
        catch (nlohmann::json::exception& ex) { console.err("remote: " + std::string(ex.what())); }
        catch (const boost::system::system_error& ex) { console.err("remote: " + std::string(ex.what())); }
        catch (const std::exception& ex) { console.err("remote: " + std::string(ex.what())); }
        catch (...) { console.err("remote: unknown"); }
    });
    std::thread steam_to_millennium_ipc_threadworker([this]() { this->steam_to_millennium_ipc(); });

    //worker threads to run all code at the same time
    steam_client_interface_handler_threadworker.join();
    steam_remote_interface_handler_threadworker.join();
    steam_to_millennium_ipc_threadworker.join();
}

/// <summary>
/// yet another initializer
/// </summary>
/// <param name=""></param>
/// <returns></returns>
unsigned long __stdcall Initialize(void*)
{
    config.verify_registry();
    skin_json_config = config.get_skin_config();

    //skin change event callback functions
    skin_config::skin_change_events::get_instance().add_listener([]() {
        console.log("skin change event fired, updating skin patch config");
        skin_json_config = config.get_skin_config();
    });

    //config file watcher callback function 
    std::thread watcher(skin_config::watch_config, std::format("{}/{}/config.json", config.get_steam_skin_path(), registry::get_registry("active-skin")), []() {
        console.log("skin configuration changed, updating");
        skin_json_config = config.get_skin_config();
    });

    //create steamclient object
    steam_client ISteamHandler;
    watcher.join();
    return true;
}