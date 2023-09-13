#define _WINSOCKAPI_   
#include <stdafx.h>

//websocket includes used to listen on sockets
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <core/injector/event_handler.hpp>
#include <utils/config/config.hpp>
#include <core/steam/cef_manager.hpp>
#include <core/ipc/steam_pipe.hpp>
#include <utils/json.hpp>
#include <utils/base64.hpp>
#include <utils/thread/thread_handler.hpp>

//
typedef websocketpp::client<websocketpp::config::asio_client> ws_Client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using json = nlohmann::json;
namespace fs = std::filesystem;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

using boost::asio::ip::tcp;

nlohmann::basic_json<> skin_json_config;

struct socket_helpers {
    static const void async_read_socket(boost::beast::websocket::stream<tcp::socket>& socket, boost::beast::multi_buffer& buffer, 
        std::function<void(const boost::system::error_code&, std::size_t)> handler)
    {
        buffer.consume(buffer.size());
        socket.async_read(buffer, handler);
    };
};

/// <summary>
/// responsible for handling the client steam pages
/// </summary>
class client : public std::enable_shared_from_this<client>
{
public:

    //initialize the socket from class initializer
    client() /*: m_socket(m_io_context), m_resolver(m_io_context) */{}

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
        void triggerUpdate(nlohmann::basic_json<>& instance) {
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
    void on_message(ws_Client* c, websocketpp::connection_hdl hdl, message_ptr msg)
    {
        try {
            m_socket_resp = nlohmann::json::parse(msg->get_payload());
            //console.log_socket(m_socket_resp.dump(4));
            //console.log(m_socket_resp.dump(4));
        }
        catch (const nlohmann::json::exception& exception) {
            console.err(std::format("Exception caught on line {} of function {}. Message: {}", __LINE__, __FUNCTION__, exception.what()));
            return;
        }
        catch (const boost::system::system_error& exception) {
            console.err(std::format("Exception caught on line {} of function {}. Message: {}", __LINE__, __FUNCTION__, exception.what()));
            return;
        }

        switch (static_cast<steam_cef_manager::response>(m_socket_resp.value("id", 0)))
        {
            /// <summary>
            /// socket response associated with getting document details
            /// </summary>
            case steam_cef_manager::response::get_document:
            {
                try {
                    this->get_doc_callback();
                }
                //document contains no selectable query on the <head> 
                catch (const nlohmann::json::exception&) {
                    return;
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
                    this->received_cef_details();
                }
                catch (const boost::system::system_error& exception)
                {
                    console.err(std::format("exception [{}], code: [{}]", exception.what(), exception.code().value()));
                    return;
                }
                catch (socket_response_error& ex) {
                    //response contains error message, we can discard and continue where we wait for a valid connection
                    if (ex.code() == socket_response_error::errors::socket_error_message) {
                        return;
                    }
                }
                break;
            }
            /// <summary>
            /// socket response when attached to a new target, used to get the sessionId to use later
            /// </summary>
            case steam_cef_manager::response::attached_to_target:
            {
                this->attached_to_target();
                break;
            }
        }

        for (auto it = v_bodyCallback.begin(); it != v_bodyCallback.end();) {
            if (m_socket_resp["id"] == it->id) {
                this->get_body_callback(*it);
                it = v_bodyCallback.erase(it); // Remove the item from the vector
            } else {
                ++it;
            }
        }

        if (m_socket_resp["method"] == "Fetch.requestPaused") {
            this->steam_request_paused();
        }
        /// <summary>
        /// socket response called when a new CEF target is created with valid parameters
        /// </summary>
        if (m_socket_resp["method"] == "Target.targetCreated" && m_socket_resp.contains("params")) {
            this->target_created();
            client::cef_instance_created::get_instance().triggerUpdate(m_socket_resp);
        }
        /// <summary>
        /// socket response called when a target cef instance changes in any way possible
        /// </summary>
        if (m_socket_resp["method"] == "Target.targetInfoChanged" && m_socket_resp["params"]["targetInfo"]["attached"]) {
            //trigger event used for remote handler
            this->target_info_changed();
            client::cef_instance_created::get_instance().triggerUpdate(m_socket_resp);
        }
    }

    const void update_fetch_hook_status()
    {
        nlohmann::json enableFetch = {
            { "id", 3242 },
            { "method", "Fetch.enable" },
            { "params", {
                { "patterns", nlohmann::json::array() }
            }}
        };

        nlohmann::json _buffer = nlohmann::json::array();

        for (const auto& hook : skin_json_config["Hooks"])
        {
            if (hook.contains("TargetCss"))
            {
                _buffer.push_back({
                    { "urlPattern", hook["TargetCss"] },
                    { "resourceType", "Stylesheet" },
                    { "requestStage", "Response" },
                    });
            }
            else if (hook.contains("TargetJs"))
            {
                _buffer.push_back({
                    { "urlPattern", hook["TargetJs"] },
                    { "resourceType", "Script" },
                    { "requestStage", "Response" },
                    });
            }
        }

        enableFetch["params"]["patterns"] = _buffer;

        if (skin_json_config.contains("Hooks") && skin_json_config["Hooks"].size() > 0) {
            console.log(std::format("Enabling CSS Hooks with config: \n{}", enableFetch.dump(4)));
            steam_client->send(hdl, enableFetch.dump(), websocketpp::frame::opcode::text);
        }
    }

    /// <summary>
    /// set discover targets on the browser instance so every cef instance is logged
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    const void on_connection(ws_Client* _c, websocketpp::connection_hdl _hdl/*, const std::shared_ptr<client>*/)
    {
        //save the handle and client for later
        steam_client = _c; 
        hdl = _hdl;

        //turn on discover targets from the cef browser instance
        nlohmann::json setDiscoverTargets = {
            { "id", 1 },
            { "method", "Target.setDiscoverTargets" },
            { "params", {
                { "discover", true }
            }}
        };

        steam_client->send(hdl, setDiscoverTargets.dump(), websocketpp::frame::opcode::text);

        this->update_fetch_hook_status();
    }

private:

    mutable std::string m_sessionId, m_header;
    mutable std::string m_steamMainSessionId;

    ws_Client* steam_client;
    websocketpp::connection_hdl hdl;

    nlohmann::json m_socket_resp;

    struct getBodyCallbackInfo {
        std::string requestId;
        std::string requestUrl;
        int id;
    };

    std::vector<getBodyCallbackInfo> v_bodyCallback;

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
    /// when the steam client makes a request, and the request is queried to be hooked, the response body is sent here.
    /// </summary>
    inline const void steam_request_paused() 
    {
        static int id = 0;
        id++;

        std::string requestId = m_socket_resp["params"]["requestId"],
            requestUrl = m_socket_resp["params"]["request"]["url"];

        v_bodyCallback.push_back({ requestId, requestUrl, id });

        console.log_hook(std::format("[starting] {{ requestId: {}, requestId: {} }}", requestUrl, id));

        nlohmann::json getBody = {
            { "id", id },
            { "method", "Fetch.getResponseBody" },
            { "params", {
                { "requestId", requestId }
            }}
        };

        steam_client->send(hdl, getBody.dump(), websocketpp::frame::opcode::text);
    }

    inline const void get_body_callback(const getBodyCallbackInfo& item) {

        std::string g_fileResponse = "/*Couldn't Interpolate the file, millennium is working however your config is off*/\n";

        const auto checkRegex = [&](nlohmann::json& hook, steam_cef_manager::script_type _type) -> void
        {
            if (!std::regex_search(item.requestUrl, std::regex(hook[_type == steam_cef_manager::script_type::stylesheet ? "TargetCss" : "TargetJs"].get<std::string>()))) {
                return;
            }

            //the absolute path to the file we want to interpolate
            const fs::path filePath = fs::path(config.getSkinDir())/Settings::Get<std::string>("active-skin")/hook["Interpolate"].get<std::string>();

            if (!fs::exists(filePath)) {
                console.err("File to Interpolate into hooked request couldnt be found on the disk!");
                return;
            }

            std::ifstream localTheme(filePath);

            if (!localTheme.is_open()) {
                console.err("Failed to open the file to interpolate!");
                return;
            }

            nlohmann::json debugInfo{
                {"requestUrl", item.requestUrl },
                {"requestId", item.requestId },
                {"callbackId", item.id},
                {"fileIntep", filePath.string()}
            };

            console.log_hook(std::format("[receive] hooked! DEBUG:{}", debugInfo.dump(4)));

            std::stringstream buffer; buffer << localTheme.rdbuf(); localTheme.close();
            g_fileResponse = buffer.str();
        };

        for (auto& hook : skin_json_config["Hooks"])
        {
            if (hook.contains("TargetCss"))
                checkRegex(hook, steam_cef_manager::script_type::stylesheet);
            else if (hook.contains("TargetJs"))
                checkRegex(hook, steam_cef_manager::script_type::javascript);
        }

        nlohmann::json fulfillRequest = {
            { "id", 6346 },
            { "method", "Fetch.fulfillRequest" },
            { "params", {
                { "requestId", item.requestId },
                { "responseCode", 200 },
                { "body", base64_encode(std::format("/*\nTHE FOLLOWING WAS INJECTED BY MILLENNIUM\n*/\n\n{}\n\n/*\nEND INJECTION\n*/\n{}", g_fileResponse, base64_decode(m_socket_resp["result"]["body"]))) }
            }}
        };

        steam_client->send(hdl, fulfillRequest.dump(), websocketpp::frame::opcode::text);
    }

    inline const bool settings_patches(std::string cefctx_title) {

        std::unordered_map<std::string, std::function<void()>> caseActions = {
            //adjust the client notifications position on every launch because it doesnt save it disk or in memory
            {"SharedJSContext", [&]() { 
                uint16_t notificationsPos = Settings::Get<int>("NotificationsPos");
                std::string js = std::format(R"((() => SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position = {})())", notificationsPos);

                steam_interface.push_to_socket(steam_client, hdl, js, m_socket_resp["sessionId"]);
            }},
            //if the steam root menu is clicked we want to display millennium's popup modal
            {"Steam Root Menu", [&]() { 
                steam_interface.inject_millennium(steam_client, hdl, m_socket_resp["sessionId"]);
            }},
            //adjust the url bar depending on what the user wants
            {"Steam", [&]() { 
                 m_steamMainSessionId = m_socket_resp["sessionId"];

                if (!Settings::Get<bool>("enableUrlBar")) {
                    std::string javaScript = R"((function() { document.head.appendChild(Object.assign(document.createElement("style"), { textContent: ".steamdesktop_URLBar_UkR3s { display: none !important; }" })); })())";
                    steam_interface.push_to_socket(steam_client, hdl, javaScript, m_steamMainSessionId);
                }
            }},
        };
        (caseActions.find(cefctx_title) != caseActions.end() ? caseActions[cefctx_title] : []() { 
            return false;
        })();

        return true;
    }
    /// <summary>
    /// function responsible for handling css and js injections
    /// </summary>
    /// <param name="title">the title of the cef page to inject into</param>
    inline const void patch(std::string cefctx_title) noexcept
    {    
        this->settings_patches(cefctx_title);

        if (skin_json_config["config_fail"]) {
            return;
        }

        for (const json_patch& patch : skin_json_config["Patches"].get<std::vector<json_patch>>())
        {
            bool contains_http = patch.matchRegexString.find("http") != std::string::npos;
            //used regex match instead of regex find or other sorts, make sure you validate your regex 
            bool regex_match = std::regex_match(cefctx_title, std::regex(patch.matchRegexString));

            if (contains_http or not regex_match)
                continue;

            console.log_patch("client", cefctx_title, patch.matchRegexString);

            if (!patch.targetJs.empty())  steam_interface.evaluate(steam_client, hdl, patch.targetJs, steam_interface.script_type::javascript,  m_socket_resp);
            if (!patch.targetCss.empty()) steam_interface.evaluate(steam_client, hdl, patch.targetCss, steam_interface.script_type::stylesheet, m_socket_resp);
        }
    }

    /// <summary>
    /// cef instance created, captures remote and client based targets, however only local targets are controlled
    /// </summary>
    const void target_created() noexcept
    {
        steam_client->send(hdl, nlohmann::json({
            {"id", steam_cef_manager::response::attached_to_target},
            {"method", "Target.attachToTarget"},
            {"params", { {"targetId", m_socket_resp["params"]["targetInfo"]["targetId"]}, {"flatten", true} } }
        }).dump(), websocketpp::frame::opcode::text);
    }

    /// <summary>
    /// cef instance target info changed, happens when something ab the instance changes, i.e the client title or others
    /// </summary>
    const void target_info_changed() noexcept
    {
        //run a js vm to get cef info
        steam_client->send(hdl, 
            nlohmann::json({
                {"id", steam_cef_manager::response::received_cef_details},
                {"method", "Runtime.evaluate"},
                {"sessionId", m_sessionId},
                {"params", {
                    //create an event to get the page title and url to use it later since it isnt evaluated properly on
                    //https://chromedevtools.github.io/devtools-protocol/tot/Target/#event-targetInfoChanged
                    {"expression", "(() => { return { title: document.title, url: document.location.href }; })()"},
                    {"returnByValue", true}
                }}
            }).dump()
        , websocketpp::frame::opcode::text);

        //create a get document request, used to get query from the page, where further patching can happen
        steam_client->send(hdl,
            nlohmann::json({
                {"id", steam_cef_manager::response::get_document},
                {"method", "DOM.getDocument"},
                {"sessionId", m_sessionId}
            }).dump(), websocketpp::frame::opcode::text
        );
    }

    /// <summary>
    /// received cef details from the js query, returns title and url
    /// </summary>
    const void received_cef_details()
    {
        if (m_socket_resp.contains("error"))
        {
            //throw socket error to catch on the backend
            throw socket_response_error(socket_response_error::errors::socket_error_message);
        }

        m_header = m_socket_resp["result"]["result"]["value"]["title"].get<std::string>();
        patch(m_header);
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
            std::string attributes = get_class_list(m_socket_resp["result"]["root"]);

            console.log(std::format("[DEVELOPER] valid QUERY from page {}", nlohmann::json({
                {"pageTitle", m_header},
                {"querySelector", attributes}
            }).dump(4)));

            /// <summary>
            /// inject millennium into the settings page, uses query instead of title because title varies between languages
            /// </summary>
            if (attributes.find("settings_SettingsModalRoot_") != std::string::npos) {
                console.succ("injecting millennium into settings modal");
                //steam_interface.inject_millennium(c, m_socket_resp);
            }

            for (const json_patch& patch : skin_json_config["Patches"].get<std::vector<json_patch>>())
            {
                bool contains_http = patch.matchRegexString.find("http") != std::string::npos;

                if (contains_http)
                    continue;

                if (attributes.find(patch.matchRegexString) == std::string::npos) {
                    continue;
                }

                console.log_patch("class name", m_header, patch.matchRegexString);

                //cef_target_patched_list.push_back()

                if (!patch.targetCss.empty()) steam_interface.evaluate(steam_client, hdl, patch.targetCss, steam_interface.script_type::stylesheet, m_socket_resp);
                if (!patch.targetJs.empty())  steam_interface.evaluate(steam_client, hdl, patch.targetJs, steam_interface.script_type::javascript, m_socket_resp);
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
        m_sessionId = m_socket_resp["result"]["sessionId"];
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

    inline bool __cdecl should_patch_interface(const json_patch& patch, const nlohmann::json& instance)
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

        if (std::regex_match(steam_page_url_header, std::regex(patch.matchRegexString)))
        {
            if (steam_page_url_header.find(uri.steam_resources.host()) != std::string::npos)
                return false;

            console.log_patch("remote", steam_page_url_header, patch.matchRegexString);
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
    const void page_settings(ws_Client* c, websocketpp::connection_hdl hdl) noexcept {
        //remove CSP on the remote page to allow cross origin scripting, specifically
        //from the steamloopbackhost
        //https://chromedevtools.github.io/devtools-protocol/

        c->send(hdl, nlohmann::json({
                {"id", 8},
                {"method", "Page.setBypassCSP"},
                {"params", {{"enabled", true}}}
            }).dump(), websocketpp::frame::opcode::text);

        //enable page event logging
        c->send(hdl, nlohmann::json({ {"id", 1}, {"method", "Page.enable"} }).dump(), websocketpp::frame::opcode::text);
        //reload page
        c->send(hdl, nlohmann::json({ {"id", 1}, {"method", "Page.reload"} }).dump(), websocketpp::frame::opcode::text);
    }

    /// <summary>
    /// handle remote interface, that are not part of the steam client.
    /// </summary>
    /// <param name="page">cef page instance</param>
    /// <param name="css_to_evaluate"></param>
    /// <param name="js_to_evaluate"></param>
    const void patch(const nlohmann::json& page, std::string css_eval, std::string js_eval)
    {
        ws_Client c;
        std::string url = page["webSocketDebuggerUrl"];

        std::function<void(ws_Client*, websocketpp::connection_hdl)> evaluate_scripting = ([&](ws_Client* c, websocketpp::connection_hdl hdl) -> void {
            if (not js_eval.empty())
                steam_interface.evaluate(c, hdl, js_eval, steam_interface.script_type::javascript);
            if (not css_eval.empty())
                steam_interface.evaluate(c, hdl, css_eval, steam_interface.script_type::stylesheet);
        });

        const auto on_message = [&](ws_Client* c, websocketpp::connection_hdl hdl, message_ptr msg) -> void {

            nlohmann::basic_json<> response = json::parse(msg->get_payload());

            if (response.value("method", std::string()) != "Page.frameResized")
            {
                evaluate_scripting(c, hdl);

                //evaluate scripts over and over again until they succeed, because sometimes it doesn't.
                //usually around 2 iterations max
                //while (true)
                //{
                //    //evaluating scripting
                //    evaluate_scripting(c, hdl);
                //    //create a new response buffer but on the same socket stream to prevent shared memory usage
                //    boost::beast::multi_buffer buffer; socket.read(buffer);
                //    //the js evaluation was successful so we can break.
                //    if (nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()))
                //        ["result"]["exceptionDetails"]["exception"]["className"] not_eq "TypeError") {
                //        break;
                //    }
                //}
            }
        };

        try {
            c.set_access_channels(
                websocketpp::log::alevel::none);
            c.clear_access_channels(
                websocketpp::log::alevel::all);

            c.init_asio();

            c.set_message_handler(
                bind(on_message, &c, ::_1, ::_2));

            c.set_open_handler(
                bind([this, &c](ws_Client* c, websocketpp::connection_hdl hdl) -> void {
                    this->page_settings(c, hdl);
                }, &c, ::_1));

            websocketpp::lib::error_code ec;
            ws_Client::connection_ptr con = c.get_connection(url, ec);
            if (ec) {
                console.err(std::format("could not create connection because: {}", ec.message()));
                return;
            }

            c.connect(con);
            c.run();
        }
        catch (websocketpp::exception const& e) {
            console.err(std::format("remote exception: {}", e.what()));
        }
    }

public:
    //create a singleton instance of the class incase it needs to be references elsewhere and
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
            if (instance_url.find("http") == std::string::npos || 
                instance_url.find(uri.steam_resources.host()) != std::string::npos)
                return;

            //check if there is a valid skin currently active
            if (skin_json_config["config_fail"])
                return;

            json_str instances = steam_interface.discover(uri.debugger.string()+"/json");
            std::vector<std::thread> threads;

            // Iterate over instances and addresses to create threads
            for (nlohmann::basic_json<>& instance : instances)
            {
                for (const json_patch& address : skin_json_config["Patches"].get<std::vector<json_patch>>())
                {
                    // Check if the patch address is a URL (remote) and should be patched
                    if (address.matchRegexString.find("http") == std::string::npos || !should_patch_interface(address, instance))
                        continue;

                    // Create a new thread and add it to the vector
                    threads.emplace_back([=, &self = remote::get_instance()]() {

                        try {
                            // Use CSS and JS if available
                            self.patch(instance, address.targetCss, address.targetJs);
                        }
                        catch (const boost::system::system_error& ex) {
                            if (ex.code() == boost::asio::error::misc_errors::eof) {
                                console.err(std::format("thread operation aborted and marked for re-establishment. reason: CEF instance was destroyed", __func__));
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
    threadContainer::getInstance().addThread(CreateThread(0, 0, [](LPVOID lpParam) -> DWORD {
        console.succ(std::format("starting: client_thread [{}]", __FUNCSIG__));

        ws_Client c;
        std::string url = json::parse(http::get(uri.debugger.string() + "/json/version"))["webSocketDebuggerUrl"];

        try {
            c.set_access_channels  (websocketpp::log::alevel::none);
            c.clear_access_channels(websocketpp::log::alevel::all);
            c.init_asio();

            c.set_message_handler(bind([&c](ws_Client* c, websocketpp::connection_hdl hdl, message_ptr msg) -> void {
                client::get_instance().on_message(c, hdl, msg); }, &c, ::_1, ::_2));
            c.set_open_handler(bind([&c](ws_Client* c, websocketpp::connection_hdl hdl) -> void {
                client::get_instance().on_connection(c, hdl); }, &c, ::_1));

            websocketpp::lib::error_code ec;
            ws_Client::connection_ptr con = c.get_connection(url, ec);
            if (ec) {
                console.err(std::format("could not create connection because: {}", ec.message()));
                return false;
            }

            c.connect(con); c.run();
        }
        catch (websocketpp::exception const& e) {
            console.err(std::format("Error occurred on client websocket thread: {}", e.what()));
        }
        return false;
    }, 0, 0, 0));

    threadContainer::getInstance().addThread(CreateThread(0, 0, [](LPVOID lpParam) -> DWORD {  
        console.succ(std::format("starting: remote_thread [{}]", __FUNCSIG__));

        try {
            remote::get_instance().handle_interface();
        }
        catch (nlohmann::json::exception& ex) { 
            console.err(std::format("remote: {}", std::string(ex.what()))); 
        }
        catch (const boost::system::system_error& ex) { 
            console.err(std::format("remote: {}", std::string(ex.what()))); 
        }
        catch (const std::exception& ex) { 
            console.err(std::format("remote: {}", std::string(ex.what())));
        }
        catch (...) { 
            console.err(std::format("remote: unknown")); 
        }
        return false;
    }, 0, 0, 0));

    threadContainer::getInstance().addThread(CreateThread(0, 0, [](LPVOID lpParam) -> DWORD {
        try {
            boost::asio::io_context io_context;
            millennium_ipc_listener listener(io_context);

            running_port = listener.get_ipc_port();
            io_context.run();
        }
        catch (std::exception& e) {
            console.err("ipc exception: " + std::string(e.what()));
        }
        return false;
    }, 0, 0, 0));
}

/// <summary>
/// yet another initializer
/// </summary>
/// <param name=""></param>
/// <returns></returns>
unsigned long __stdcall Initialize(void*)
{
    config.setupMillennium();
    skin_json_config = config.getThemeData();

    //std::cout << skin_json_config.dump(4) << std::endl;

    //skin change event callback functions
    themeConfig::updateEvents::getInstance().add_listener([]() {
        console.imp("skin change event fired, updating skin patch config");
        skin_json_config = config.getThemeData();
        client::get_instance().update_fetch_hook_status();
    });

    //config file watcher callback function 
    std::thread watcher(themeConfig::watchPath, config.getSkinDir(), []() {
        console.log("skins folder has changed, updating required information");
        skin_json_config = config.getThemeData();
        client::get_instance().update_fetch_hook_status();

        //m_Client.parseSkinData();
    });

    //create steamclient object
    steam_client ISteamHandler;
    watcher.join();

    return true;
}