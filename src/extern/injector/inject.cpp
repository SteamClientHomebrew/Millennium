#include <stdafx.h>
#include <extern/injector/inject.hpp>
#include <include/config.hpp>
#include <extern/steam/cef_manager.hpp>
#include <extern/ipc/steamipc.hpp>

namespace websocket = boost::beast::websocket;
using namespace boost;

skin_config config;
using tcp = boost::asio::ip::tcp;

nlohmann::basic_json<> skin_json_config;

struct socket_helpers {
    static const void async_read_socket(boost::beast::websocket::stream<tcp::socket>& socket, boost::beast::multi_buffer& buffer, 
        std::function<void(const boost::system::error_code&, std::size_t)> handler)
    {
        //consume the buffer to reset last response
        buffer.consume(buffer.size());
        //hang async on socket for a response
        socket.async_read(buffer, handler);
    };
};

/// <summary>
/// responsible for handling the client steam pages
/// </summary>
class client : public std::enable_shared_from_this<client>
{
private:
    //socket variables
    boost::asio::io_context io_context;
    boost::beast::websocket::stream<tcp::socket> socket;
    asio::ip::tcp::resolver resolver;

    mutable std::string sessionId, page_title;

    struct target { const std::string 
        target_created = "Target.targetCreated", 
        target_info_changed = "Target.targetInfoChanged";
    } target;

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

        int code() const {
            return errorCode_;
        }

    private:
        int errorCode_;
    };

    /// <summary>
    /// function responsible for handling css and js injections
    /// </summary>
    /// <param name="title">the title of the cef page to inject into</param>
    inline const void patch(std::string title) noexcept
    {    
        for (const json_patch& patch : skin_json_config["Patches"].get<std::vector<json_patch>>())
        {
            bool contains_http = patch.matchRegexString.find("http") != std::string::npos;
            //used regex match instead of regex find or other sorts, make sure you validate your regex 
            bool regex_match = std::regex_match(title, std::regex(patch.matchRegexString));

            if (contains_http or not regex_match)
                continue;

            console.log_patch("client", title, patch.matchRegexString);

            if (!patch.targetJs.empty())  steam_interface.evaluate(socket, patch.targetJs, steam_interface.script_type::stylesheet, socket_response);
            if (!patch.targetCss.empty()) steam_interface.evaluate(socket, patch.targetCss, steam_interface.script_type::javascript, socket_response);
        }
    }

    /// <summary>
    /// cef instance created, captures remote and client based targets, however only local targets are controlled
    /// </summary>
    const void target_created() noexcept
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
    const void target_info_changed() noexcept
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
    const void received_cef_details()
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

    const std::string get_class_list(const nlohmann::basic_json<>& node)
    {
        std::string attributes_list;

        if (node.contains("attributes"))
        {
            const nlohmann::basic_json<>& attributes = node["attributes"];
            for (const auto& attribute : attributes)
            {
                attributes_list += std::format(".{} ", attribute.get<std::string>());
            }
        }

        if (node.contains("children"))
        {
            const nlohmann::basic_json<>& children = node["children"];
            for (const auto& child : children)
            {
                attributes_list += get_class_list(child);
            }
        }

        return attributes_list;
    }

    /// <summary>
    /// gets the document callback
    /// instantiated from the cef details callback 
    /// contains a json root of the query on the document, hard to parse all types
    /// </summary>
    const void get_doc_callback() noexcept
    {
        try {
            //get the <head> attributes of the dom which is what is most important in selecting pages
            std::string attributes = get_class_list(socket_response["result"]["root"]);

            //console.log(std::format("[FOR DEVELOPERS]\nselectable <html> query on page [{}]:\n -> [{}]\n", page_title, attributes));

            for (const json_patch& patch : skin_json_config["Patches"].get<std::vector<json_patch>>())
            {
                bool contains_http = patch.matchRegexString.find("http") != std::string::npos;

                //if (contains_http || attributes.find(patch.matchRegexString) == std::string::npos)
                //    continue;

                console.log_patch("class name", page_title, patch.matchRegexString);

                if (!patch.targetCss.empty()) steam_interface.evaluate(socket, patch.targetCss, steam_interface.script_type::stylesheet, socket_response);
                if (!patch.targetJs.empty())  steam_interface.evaluate(socket, patch.targetJs, steam_interface.script_type::javascript, socket_response);
            }

            /// <summary>
            /// inject millennium into the settings page, uses query instead of title because title varies between languages
            /// </summary>
            if (attributes.find("settings_SettingsModalRoot_") != std::string::npos) {
                steam_interface.inject_millennium(socket, socket_response);
            }
        }
        catch (nlohmann::json::exception& ex)
        {
            if ((nlohmann::detail::type_error*)dynamic_cast<const nlohmann::detail::type_error*>(&ex) == nullptr)
            {
                console.err(std::format("unexpected error type was thrown from nlohmann::json: {}", ex.what()));
            }
        }
    }

    /// <summary>
    /// received the sessionId from the cef instance after we attached to it
    /// we then use the sessionId provided to use runtime.evaluate on the instance
    /// </summary>
    const void attached_to_target() noexcept
    {
        sessionId = socket_response["result"]["sessionId"];
    }

    /// <summary>
    /// set discover targets on the browser instance so every cef instance is logged
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    const void set_discover_targets(const std::shared_ptr<client>)
    {
        //turn on discover targets from the cef browser instance
        socket.async_write(boost::asio::buffer(
            nlohmann::json({
                { "id", 1 },
                { "method", "Target.setDiscoverTargets" },
                { "params", {
                    { "discover", true }
                }}
            }).dump()),
        [this](const boost::system::error_code& writeEc, std::size_t) {

            if (writeEc) {
                //I've had this occur once, not sure why. 
                console.err("fatal error, couldn't enable Target.setDiscoverTargets");
                return;
            }
        });
    }

    const void socket_handshake(boost::beast::websocket::stream<tcp::socket>& socket)
    {
        //just hope this doesn't fail because async function calling doesn't work for some reason
        this->socket.handshake(endpoints.debugger.host(), network::uri::uri(
            static_cast<json_str>(steam_interface.discover(endpoints.debugger.string()+"/json/version"))
            ["webSocketDebuggerUrl"]
        ).path());
    }

    const void asio_connect(boost::beast::websocket::stream<tcp::socket>& socket)
    {
        asio::async_connect(
            socket.next_layer(),
            resolver.resolve(endpoints.debugger.host(), endpoints.debugger.port()),
        //callback on the connection
        [this](const boost::system::error_code& error_code, const asio::ip::tcp::endpoint&) {
            if (error_code) {
                console.err("couldn't connect to the steam browser host socket");
            }
        });
    }

public:

    //initialize the socket from class initializer
    client() : socket(io_context), resolver(io_context) {}

    /// <summary>
    /// event handler for when new windows are created/edited
    /// </summary>
    class cef_instance_created {
    public:
        /// <summary>
        /// singleton class instance for cross reference
        /// </summary>
        /// <returns></returns>
        static cef_instance_created& get_instance() {
            static cef_instance_created instance;
            return instance;
        }

        using event_listener = std::function<void(nlohmann::basic_json<>&)>;

        //await event, remote sided
        void add_listener(const event_listener& listener) {
            listeners.push_back(listener);
        }

        //trigger change, client sided
        void trigger_change(nlohmann::basic_json<>& instance) {
            //loop every listener and execute it, just incase we have multiple listeners
            for (const auto& listener : listeners) {
                listener(instance);
            }
        }

    private:
        cef_instance_created() {}
        //singleton definitions
        cef_instance_created(const cef_instance_created&) = delete;
        cef_instance_created& operator=(const cef_instance_created&) = delete;

        std::vector<event_listener> listeners;
    };

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
        //handle initial connection to the socket
        this->asio_connect(socket);
        this->socket_handshake(socket);

        //enable set discover targets 
        this->set_discover_targets(static_cast<std::shared_ptr<client>>(std::make_shared<client>()));

        boost::beast::multi_buffer buffer;
        //handle reading from the socket asynchronously and continue processing
        std::function<void(const boost::system::error_code&, std::size_t)> handle_read =
            [&](const boost::system::error_code& socket_error_code, std::size_t bytes_transferred)
        {
            if (socket_error_code) {
                console.err("throwing socket exception");
                throw boost::system::system_error(socket_error_code);
            }

            try {
                socket_response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));
            }
            catch (nlohmann::json::exception& exception) {
                console.err(exception.what());
                goto return_socket;
            }

            switch (static_cast<steam_cef_manager::response>(socket_response.value("id", 0)))
            {
                /// <summary>
                /// socket response associated with getting document details
                /// </summary>
                case steam_cef_manager::response::get_document:
                {
                    try {
                        get_doc_callback();
                    }
                    //document contains no selectable query on the <head> 
                    catch (const nlohmann::json::exception&) {
                        goto return_socket;
                    }
                    break;
                }
                /// <summary>
                /// socket response called when cef details have been received. 
                /// title, url and web debugger url
                /// </summary>
                case steam_cef_manager::response::received_cef_details:
                {
                    try {
                        received_cef_details();
                    }
                    catch (const boost::system::system_error& exception)
                    {
                        console.err(std::format("exception [{}], code: [{}]", exception.what(), exception.code().value()));
                        goto return_socket;
                    }
                    catch (socket_response_error& ex) {
                        //response contains error message, we can discard and continue where we wait for a valid connection
                        if (ex.code() == socket_response_error::errors::socket_error_message) {
                            goto return_socket;
                        }
                    }
                    break;
                }
                /// <summary>
                /// socket response when attached to a new target, used to get the sessionId to use later
                /// </summary>
                case steam_cef_manager::response::attached_to_target:
                {
                    attached_to_target();
                    break;
                }
            }
            /// <summary>
            /// socket response called when a new CEF target is created with valid parameters
            /// </summary>
            if (socket_response["method"] == target.target_created && socket_response.contains("params")) {
                target_created();
                client::cef_instance_created::get_instance().trigger_change(socket_response);
            }
            /// <summary>
            /// socket reponse called when a target cef instance changes in any way possible
            /// </summary>
            if (socket_response["method"] == target.target_info_changed && socket_response["params"]["targetInfo"]["attached"]) {
                //trigger event used for remote handler
                target_info_changed();
                client::cef_instance_created::get_instance().trigger_change(socket_response);
            }
            return_socket:
            //reopen socket async reading
            socket_helpers::async_read_socket(socket, buffer, handle_read);
        };
        socket_helpers::async_read_socket(socket, buffer, handle_read);
        io_context.run();
    }
};

/// <summary>
/// responsible for handling the remote steam pages
/// </summary>
class remote
{
private:
    remote() {}
    /// <summary>
    /// used by the remote interface handler to check if the cef instance needs to be injected into
    /// </summary>
    /// <param name="patch">iteration of the patch from config</param>
    /// <param name="instance">cef instance</param>
    /// <returns>should patch</returns>
    std::vector<std::string> patched = {};

    inline bool __cdecl should_patch_interface(nlohmann::json& patch, const nlohmann::json& instance)
    {
        const std::string web_debugger_url = instance["webSocketDebuggerUrl"].get<std::string>();
        //check if the current instance was already patched
        for (const std::string& element : patched) {
            if (element == web_debugger_url) {
                return false;
            }
        }

        //get the url of the page we are active on 
        std::string steam_page_url_header = instance["url"].get<std::string>();

        if (std::regex_match(steam_page_url_header, std::regex(static_cast<json_patch>(patch).matchRegexString)))
        {
            constexpr const std::string_view exclude = "steamloopback.host";

            if (steam_page_url_header.find(exclude.data()) not_eq std::string::npos)
                return false;

            console.log_patch("remote", steam_page_url_header, static_cast<json_patch>(patch).matchRegexString);
            //mark that it was successfully patched
            patched.push_back(web_debugger_url);
            return true;
        }
        else return false;
    }

    /// <summary>
    /// set page settings using the chrome dev tools protocol
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    const void page_settings(boost::beast::websocket::stream<tcp::socket>& socket) noexcept {
        //remove CSP on the remote page to allow cross origin scripting, specifically
        //from the steamloopbackhost
        //https://chromedevtools.github.io/devtools-protocol/
        socket.write(boost::asio::buffer(
            nlohmann::json({
                {"id", 8},
                {"method", "Page.setBypassCSP"},
                {"params", {{"enabled", true}}}
            }).dump()
        ));
        //enable page event logging
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1}, {"method", "Page.enable"} }).dump()));
        //reload page
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1}, {"method", "Page.reload"} }).dump()));
    }

    /// <summary>
    /// handle remote interface, that are not part of the steam client.
    /// </summary>
    /// <param name="page">cef page instance</param>
    /// <param name="css_to_evaluate"></param>
    /// <param name="js_to_evaluate"></param>
    const void patch(const nlohmann::json& page, std::string css_eval, std::string js_eval)
    {
        boost::asio::io_context io_context;
        boost::beast::websocket::stream<tcp::socket> socket(io_context);
        asio::ip::tcp::resolver resolver(io_context);

        boost::network::uri::uri socket_url(page["webSocketDebuggerUrl"].get<std::string>());
        boost::asio::connect(socket.next_layer(), resolver.resolve(endpoints.debugger.host(), endpoints.debugger.port()));

        //connect to the socket associated with the page directly instead of using browser instance.
        //this way after the first injection we can inject immediatly into the page because we dont need to wait for the title 
        //of the remote page, we can just inject into it in a friendly way
        socket.handshake(endpoints.debugger.host(), socket_url.path());

        //activiate page settings needed
        this->page_settings(socket);

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

        boost::beast::multi_buffer buffer;
        //handle reading from the socket asynchronously and continue processing
        std::function<void(const boost::system::error_code&, std::size_t)> handle_read = 
            [&](const boost::system::error_code& socket_error_code, std::size_t bytes_transferred) 
        {
            if (socket_error_code) 
            {
                console.err("throwing socket exception");
                throw boost::system::system_error(socket_error_code);
            }
            //get socket response and parse as a json object
            nlohmann::basic_json<> response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));

            if (response.value("method", std::string()) != "Page.frameResized")
            {
                //evaluate scripts over and over again until they succeed, because sometimes it doesnt.
                //usually around 2 iterations max
                while (true)
                {
                    //evaluating scripting
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
            //reopen socket async reading
            socket_helpers::async_read_socket(socket, buffer, handle_read);
        };
        socket_helpers::async_read_socket(socket, buffer, handle_read);
        io_context.run();
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

    const void handle_interface() 
    {
        //called when a new cef window is instantiated, used to check if its a remote page
        client::cef_instance_created::get_instance().add_listener([=](nlohmann::basic_json<>& instance) {

            //CEF instance information stored in instance["params"]["targetInfo"]
            const std::string instance_url = instance["params"]["targetInfo"]["url"].get<std::string>();

            //check if the updated CEF instance is remotely hosted.
            if (instance_url.find("http") == std::string::npos || instance_url.find("steamloopback.host") != std::string::npos)
                return;

            //check if there is a valid skin currently active
            if (skin_json_config["config_fail"])
                return;

            json_str instances = steam_interface.discover(endpoints.debugger.string()+"/json");
            std::vector<std::thread> threads;

            // Iterate over instances and addresses to create threads
            for (nlohmann::basic_json<>& instance : instances)
            {
                for (nlohmann::basic_json<>& address : skin_json_config["Patches"])
                {
                    // Check if the patch address is a URL (remote) and should be patched
                    if (address["MatchRegexString"].get<std::string>().find("http") == std::string::npos || !should_patch_interface(address, instance))
                        continue;

                    // Create a new thread and add it to the vector
                    threads.emplace_back([=, &self = remote::get_instance()]() {
                        try {
                            // Use CSS and JS if available
                            self.patch(instance, address.value("TargetCss", std::string()), address.value("TargetJs", std::string()));
                        }
                        catch (const boost::system::system_error& ex) {
                            if (ex.code() == boost::asio::error::misc_errors::eof) {
                                console.log(std::format("thread operation aborted and marked for re-establishment. reason: CEF instance was destroyed", __func__));
                            }
                            //exception was thrown, patching thread crashed, so mark it as unpatched so it can restart itself, happens when the 
                            //instance is left alone for too long sometimes and steams garbage collector clears memory
                            patched.erase(std::remove_if(patched.begin(), patched.end(), [&](const std::string& element) {
                                return element == instance["webSocketDebuggerUrl"];
                            }), patched.end());
                        }
                    });
                }
            }
            // Detach all the threads and let them run concurrently
            for (auto& thread : threads) {
                console.log("detaching thread operator from std::this_thread");
                thread.detach();
            }
        });
    }
};

static uint16_t running_port;
uint16_t steam_client::get_ipc_port() {
    return running_port;
}

/// <summary>
/// initialize millennium components
/// </summary>
steam_client::steam_client()
{   
    /// <summary>
    /// thread to handle the client instances of the steam interface
    /// </summary>
    std::thread([this]() 
    {        
        try {
            client::get_instance().handle_interface();
        }
        catch (nlohmann::json::exception& ex) { 
            console.err("client: " + std::string(ex.what())); 
        }
        catch (const boost::system::system_error& ex) { 
            console.err("client: " + std::string(ex.what())); 
        }
        catch (const std::exception& ex) { 
            console.err("client: " + std::string(ex.what())); 
        }
        catch (...) { 
            console.err("client: unkown"); 
        }

    }).detach();
    /// <summary>
    /// thread to handle the remote instances of the steam interface
    /// </summary>
    std::thread([this]() 
    {
        try {
            remote::get_instance().handle_interface();
        }
        catch (nlohmann::json::exception& ex) { 
            console.err("remote: " + std::string(ex.what())); 
        }
        catch (const boost::system::system_error& ex) { 
            console.err("remote: " + std::string(ex.what())); 
        }
        catch (const std::exception& ex) { 
            console.err("remote: " + std::string(ex.what())); 
        }
        catch (...) { 
            console.err("remote: unknown"); 
        }
    }).detach();
    /// <summary>
    /// start the millennium to steam ipc
    /// </summary>
    std::thread([this]() 
    { 
        try {
            boost::asio::io_context io_context;
            millennium_ipc_listener listener(io_context);

            running_port = listener.get_ipc_port();
            io_context.run();
        }
        catch (std::exception& e) {
            console.log("ipc exception " + std::string(e.what()));
        }
    }).detach();
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
    std::thread watcher(skin_config::watch_config, std::format("{}/{}/skin.json", config.get_steam_skin_path(), registry::get_registry("active-skin")), []() {
        console.log("skin configuration changed, updating");
        skin_json_config = config.get_skin_config();
    });

    //create steamclient object
    steam_client ISteamHandler;
    watcher.join();
    return true;
}