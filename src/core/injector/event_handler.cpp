#define _WINSOCKAPI_   
#include <stdafx.h>

//websocket includes used to listen on sockets
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <core/injector/event_handler.hpp>
#include <utils/config/config.hpp>
#include <core/steam/cef_manager.hpp>
#include <utils/json.hpp>
#include <utils/base64.hpp>
#include <utils/thread/thread_handler.hpp>

#include "startup/welcome_modal.hpp"
#include <core/ipc/ipc_main.hpp>

#include <regex>
#include "conditions/conditionals.hpp"
#include "window/interface/globals.h"
#include <utils/io/input-output.hpp>
#include <utils/http/http_client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> ws_Client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using json = nlohmann::json;
namespace fs = std::filesystem;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

nlohmann::basic_json<> skin_json_config;

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
                std::thread([&]() {
                    listener(instance);
                });
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
            console.err(fmt::format("Exception caught on line {} of function {}. Message: {}", __LINE__, __FUNCTION__, exception.what()));
            return;
        }
        catch (const asio::system_error& exception) {
            console.err(fmt::format("Exception caught on line {} of function {}. Message: {}", __LINE__, __FUNCTION__, exception.what()));
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
                catch (const asio::system_error& exception)
                {
                    console.err(fmt::format("exception [{}], code: [{}]", exception.what(), exception.code().value()));
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
            if (!steam::get().params.has("-skin-loader")) {
                this->steam_request_paused();
            }
        }
        /// <summary>
        /// socket response called when a new CEF target is created with valid parameters
        /// </summary>
        if (m_socket_resp["method"] == "Target.targetCreated" && m_socket_resp.contains("params")) {
            this->target_created();
            //client::cef_instance_created::get_instance().triggerUpdate(m_socket_resp);


            //console.log(m_socket_resp.dump(4));
        }
        /// <summary>
        /// socket response called when a target cef instance changes in any way possible
        /// </summary>
        if (m_socket_resp["method"] == "Target.targetInfoChanged" && m_socket_resp["params"]["targetInfo"]["attached"]) {
            //trigger event used for remote handler
            this->target_info_changed();
            //client::cef_instance_created::get_instance().triggerUpdate(m_socket_resp);

            //console.log(m_socket_resp.dump(4));
        }

        if (m_socket_resp["method"] == "Console.messageAdded") {

            std::string input = m_socket_resp["params"]["message"]["text"];

            //console.log(fmt::format("[STEAM] {}", input));

            std::string prefix = "millennium.user.message: ";

            if (input.find(prefix) == 0) {
                input.erase(0, prefix.length());
                try {
                    json j = json::parse(input);
                    IPC::handleMessage(j, m_socket_resp, steam_client, hdl);
                }
                catch (const std::exception&) { }
            }
        }

        msg->recycle();
    }

    const void update_fetch_hook_status()
    {
        nlohmann::json enableFetch;

        if (!steam::get().params.has("-skin-loader")) {
            enableFetch = {
                { "id", 3242 },
                { "method", "Fetch.enable" },
                { "params", {
                    { "patterns", nlohmann::json::array() }
                }}
            };
        }
        else {
            enableFetch = {
                { "id", 3242 },
                { "method", "Fetch.enable" },
            };
        }

        nlohmann::json _buffer = nlohmann::json::array();

        if (skin_json_config.contains("Steam-WebKit")) {
            skin_json_config["Hooks"] = {
                {
                    {"TargetCss", "https://store.*.steamstatic.com/public/css/v6/wishlist.css*"},
                    {"Interpolate", skin_json_config["Steam-WebKit"] }
                },
                {
                    {"TargetCss", "https://*.*.com/public/shared/css/buttons.css*"},
                    {"Interpolate", skin_json_config["Steam-WebKit"] }
                }
            };
        }

        for (const auto& hook : skin_json_config["Hooks"])
        {
            if (hook.contains("TargetCss")) {
                _buffer.push_back({
                    { "urlPattern", hook["TargetCss"] }, { "resourceType", "Stylesheet" }, { "requestStage", "Response" },
                });
            }
            else if (hook.contains("TargetJs")) {
                _buffer.push_back({
                    { "urlPattern", hook["TargetJs"] }, { "resourceType", "Script" }, { "requestStage", "Response" },
                });
            }
        }


        if (!steam::get().params.has("-skin-loader")) {
            enableFetch["params"]["patterns"] = _buffer;
        }

        if ((skin_json_config.contains("Hooks") && skin_json_config["Hooks"].size() > 0) ||
            (skin_json_config.contains("Steam-WebKit") && skin_json_config["Steam-WebKit"])) {
            //console.log(fmt::format("Enabling CSS Hooks with config: \n{}", enableFetch.dump(4)));
            try {
                steam_client->send(hdl, enableFetch.dump(), websocketpp::frame::opcode::text);
            }
            catch (const websocketpp::exception&) {
                console.err("Couldn't send 'Hook' socket message to Steam");
            }
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

    bool m_checkedForUpdates = false;

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

        //std::cout << m_socket_resp.dump(4) << std::endl;

        std::string requestId = m_socket_resp["params"]["requestId"],
            requestUrl = m_socket_resp["params"]["request"]["url"];

        v_bodyCallback.push_back({ requestId, requestUrl, id });

        console.log_hook(fmt::format("[starting] {{ requestId: {}, requestId: {} }}", requestUrl, id));

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
        bool isCss = false;

        const auto checkRegex = [&](nlohmann::json& hook, steam_cef_manager::script_type _type) -> void
        {
            if (!std::regex_search(item.requestUrl, std::regex(hook[_type == steam_cef_manager::script_type::stylesheet ? "TargetCss" : "TargetJs"].get<std::string>()))) {
                return;
            }

            isCss = _type == steam_cef_manager::script_type::stylesheet;

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

            console.log_hook(fmt::format("[receive] hooked! DEBUG:{}", debugInfo.dump(4)));

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

        const auto colors = config.getRootColors(skin_json_config);
        std::string responseBody;

        // if the hooked file is a css file, we want to add the colors structure.
        if (isCss) {
            responseBody = base64_encode(fmt::format(
                "{}\n\n/*\nTHE FOLLOWING WAS INJECTED BY MILLENNIUM\n*/"
                "\n\n{}\n\n"
                "/*\nBEGIN COLOR INSERTION\n*/"
                "\n{}\n"
                "/*\nEND COLORS\n*/\n"
                "/*\nEND INJECTION\n*/", 
            base64_decode(m_socket_resp["result"]["body"]), g_fileResponse, colors == "[NoColors]" ? std::string() : colors));
        }
        else {
            responseBody = base64_encode(fmt::format("{}\n\n/*\nTHE FOLLOWING WAS INJECTED BY MILLENNIUM\n*/\n\n{}\n\n/*\nEND INJECTION\n*/", base64_decode(m_socket_resp["result"]["body"]), g_fileResponse));
        }

        nlohmann::json fulfillRequest = {
            { "id", 6346 },
            { "method", "Fetch.fulfillRequest" },
            { "params", {
                { "requestId", item.requestId },
                { "responseCode", 200 },
                { "body", responseBody }
            }}
        };

        steam_client->send(hdl, fulfillRequest.dump(), websocketpp::frame::opcode::text);
    }

    inline const bool settings_patches(std::string cefctx_title) {

        nlohmann::json evaluate_script = {
            {"id", 5393},
            {"method", "Console.enable"},
            {"sessionId", m_socket_resp["sessionId"]}
        };
        steam_client->send(hdl, evaluate_script.dump(4), websocketpp::frame::opcode::text);

        // interop with lexer for backwards compatability
//        std::string content = file::readFileSync(fmt::format("{}/steamui/skin/index.js", config.getSteamRoot()));
//        steam_interface.push_to_socket(steam_client, hdl, content, m_socket_resp["sessionId"]);

        std::unordered_map<std::string, std::function<void()>> caseActions = {
            //adjust the client notifications position on every launch because it doesnt save it disk or in memory
            {"SharedJSContext", [&]() { 
                uint16_t notificationsPos = Settings::Get<int>("NotificationsPos");
                std::string js = fmt::format(R"((() => SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position = {})())", notificationsPos);

                steam_interface.push_to_socket(steam_client, hdl, js, m_socket_resp["sessionId"]);
            }},
            //adjust the url bar depending on what the user wants
            {"Steam", [&]() { 
                m_steamMainSessionId = m_socket_resp["sessionId"];

                //std::thread([&]() { check_for_updates(); }).detach();

                if (!Settings::Get<bool>("enableUrlBar")) {
                    std::string javaScript = R"((function() { document.head.appendChild(Object.assign(document.createElement("style"), { textContent: ".steamdesktop_URLBar_UkR3s { display: none !important; }" })); })())";
                    steam_interface.push_to_socket(steam_client, hdl, javaScript, m_steamMainSessionId);
                }

                static bool firstInstance = false;

                // first instance is nullptr for some reason, wait for 2nd instance
                if (firstInstance) 
                {
                    static bool hasShown = false;
                    if (!hasShown && !Settings::Get<bool>("shownNewUserPrompt")) 
                    {
                        std::string message = popupModal::getSnippet
                        (
                            "Welcome to Millennium!",

                            "Millennium has updated and your settings have been reset as a result."
                            "<br>Head over to Settings -> Interface -> Open Millennium. "
                            "From there you can install, manage, and edit themes! "
                            "<br>Having problems? Join the Discord Server!"
                        );

                        steam_interface.push_to_socket(steam_client, hdl, message, m_steamMainSessionId);
                        Settings::Set<bool>("shownNewUserPrompt", true);
                        hasShown = true;
                    }
                }
                firstInstance = true;
            }},
        };
        (caseActions.find(cefctx_title) != caseActions.end() ? caseActions[cefctx_title] : []() { 
            return false;
        })();

        return true;
    }

    struct statementReturn
    {
        bool fail;
        steam_cef_manager::script_type scriptType = steam_cef_manager::script_type::javascript;
        const std::string itemRefrence = std::string();
    };

    struct evalStatement
    {
        nlohmann::basic_json<>& statement, selectedItem;
        std::string settingsName;
        std::vector<statementReturn>& returnVal;
	};

    inline const void evauluateStatement(evalStatement ev) {

        //std::cout << ev.statement.dump(4) << std::endl;

        if (ev.statement.contains("Equals") && ev.selectedItem == ev.statement["Equals"]) {
            console.log(fmt::format("Configuration key: [{}] Equals [{}]. Executing 'True' statement", ev.settingsName, ev.statement["Equals"].dump()));

            if (ev.statement.contains("True")) {
                const auto& trueStatement = ev.statement["True"];
                if (trueStatement.contains("TargetCss")) {
                    console.log("inserting CSS module: " + trueStatement["TargetCss"].get<std::string>());
                    ev.returnVal.push_back({ false, steam_cef_manager::script_type::stylesheet, trueStatement["TargetCss"] });
                }
                if (trueStatement.contains("TargetJs")) {
                    console.log("inserting JS module: " + trueStatement["TargetJs"].get<std::string>());
                    ev.returnVal.push_back({ false, steam_cef_manager::script_type::javascript, trueStatement["TargetJs"] });
                }
            }
            else {
                console.warn("Can't execute 'True' statement as it doesn't exist.");
            }
        }
        else {
            console.log(fmt::format("Configuration key: [{}] does NOT Equal [{}]. Executing 'False' statement", ev.settingsName, ev.statement["Equals"].dump()));

            if (ev.statement.contains("False")) {
                const auto& falseStatement = ev.statement["False"];
                if (falseStatement.contains("TargetCss")) {
                    console.log("inserting CSS module: " + falseStatement["TargetCss"].get<std::string>());
                    ev.returnVal.push_back({ false, steam_cef_manager::script_type::stylesheet, falseStatement["TargetCss"] });
                }
                if (falseStatement.contains("TargetJs")) {
                    console.log("inserting JS module: " + falseStatement["TargetJs"].get<std::string>());
                    ev.returnVal.push_back({ false, steam_cef_manager::script_type::javascript, falseStatement["TargetJs"] });
                }
            }
            else {
                console.warn("Can't execute 'False' statement as it doesn't exist.");
            }
        }
    }

    inline const std::vector<statementReturn> check_statement(const nlohmann::basic_json<>& patch)
    {
        std::vector<statementReturn> returnVal;

        if (!patch.contains("Statement")) {
            return { {true} };
        }

        // parse if statement is array object or single
        std::vector<nlohmann::basic_json<>> statements;

        if (patch["Statement"].is_object())
            statements.push_back(patch["Statement"]);
        else if (patch["Statement"].is_array()) {
            for (auto& statement : patch["Statement"]) {
				statements.push_back(statement);
			}
        }

        for (auto& statement : statements) {

            //std::cout << statement.dump(4) << std::endl;

            if (!statement.contains("If")) {
                console.err("Invalid statement structure: 'If' is missing.");
                return { {true} };
            }
            if (!statement.contains("Equals") && !statement.contains("Combo")) {
                console.err("Invalid statement structure: 'Equals' or 'Combo' is missing.");
                return { {true} };
            }
            if (!skin_json_config.contains("Configuration")) {
                console.err("Invalid statement structure: 'Configuration' is missing from JSON storage.");
                return { {true} };
            }

            const auto settingsName = statement["If"].get<std::string>();
            bool isFound = false;

            for (const auto& item : skin_json_config["Configuration"]) {
                if (item.contains("Name") && item["Name"] == settingsName) {
                    isFound = true;
                    const auto& selectedItem = item["Value"];

                    if (statement.contains("Combo")) {
                        for (auto& item : statement["Combo"]) {
                            evauluateStatement({
                                item, selectedItem, settingsName, returnVal
                            });
                        }
                    }

                    evauluateStatement({
                        statement, selectedItem, settingsName, returnVal
                    });
                }
            }

            if (!isFound) {
                console.err("Couldn't find config key with Name = " + settingsName);
            }
        }

        return returnVal.empty() ? std::vector<statementReturn>{ {true} } : returnVal;
    }

    struct item {
        std::string filePath;
        steam_cef_manager::script_type type;
    };

    inline const void inject_items(std::vector<item> items) {

        std::string raw_script;

        const auto jsAllowed = Settings::Get<bool>("allow-javascript");

        for (const auto& item : items) {
            const auto relativeItem = fmt::format("{}/skins/{}/{}", uri.steam_resources, Settings::Get<std::string>("active-skin"), item.filePath);

            if (item.type == steam_cef_manager::script_type::javascript && jsAllowed) {
                raw_script += cef_dom::get().javascript_handler.add(relativeItem).data();
                raw_script += "\n\n\n";
            }
            if (item.type == steam_cef_manager::script_type::stylesheet) {
                raw_script += cef_dom::get().stylesheet_handler.add(relativeItem).data();
                raw_script += "\n\n\n";
            }
        }

        try {
            std::function<void(const std::string&)> runtime_evaluate = [&](const std::string& script) -> void {
                nlohmann::json evaluate_script = {
                    {"id", steam_cef_manager::response::script_inject_evaluate},
                    {"method", "Runtime.evaluate"},
                    {"params", {{"expression", script}}}
                };

                if (m_socket_resp.contains("sessionId")) {
                    evaluate_script["sessionId"] = m_socket_resp["sessionId"];
                }          

                steam_client->send(hdl, evaluate_script.dump(4), websocketpp::frame::opcode::text);
            };

            //inject the script that the user wants
            runtime_evaluate(theme_colors() + "\n\n\n" + system_color_script() + "\n\n\n" + raw_script);
        }
        catch (const asio::system_error& ex) {

            if (ex.code().value() == asio::error::operation_aborted || ex.code().value() == asio::error::eof)
            {
                console.warn("[non-fatal] steam garbage clean-up aborted a connected socket, re-establishing connection");
            }
        }
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

        if (skin_json_config["Patches"].is_null()) {
            return;
        }

        for (const nlohmann::basic_json<>& patch : skin_json_config["Patches"].get<std::vector<nlohmann::basic_json<>>>())
        {
            auto matchRegexString = patch.value("MatchRegexString", "_null_");

            bool contains_http = matchRegexString.find("http") != std::string::npos;
            //used regex match instead of regex find or other sorts, make sure you validate your regex 
            bool regex_match = false;

            try {
                regex_match = std::regex_search(cefctx_title, std::regex(matchRegexString));
            }
            catch (std::regex_error& e) {
                console.err(fmt::format("Invalid regex selector: '{}' is invalid {}", matchRegexString, e.what()));
            }


            if (contains_http || !regex_match)
                continue;

            console.log_patch("client", cefctx_title, matchRegexString);

            const auto result = check_statement(patch);

            std::vector<item> itemQuery;

            if (!result[0].fail) {
                for (const auto statementItem : result) {
                    itemQuery.push_back({ statementItem.itemRefrence, statementItem.scriptType });
                }
            }

            try {
                auto conditional = conditionals::has_patch(skin_json_config, skin_json_config["native-name"], cefctx_title);

                for (auto condition : conditional)
                {
                    itemQuery.push_back({ condition.item_src, condition.type });
                }
            }
            catch (const nlohmann::detail::exception& ex)
            {
                console.err(fmt::format("Error getting conditionals {}", ex.what()));
            }

            if (patch.contains("TargetJsList"))
            {
                for (const auto item : patch["TargetJsList"])
                {
                    itemQuery.push_back({ item, steam_interface.script_type::javascript });
                }
            }
            if (patch.contains("TargetCssList"))
            {
                for (const auto item : patch["TargetCssList"])
                {
                    itemQuery.push_back({ item, steam_interface.script_type::stylesheet });
                }
            }

            if (patch.contains("TargetJs") && patch["TargetJs"].is_string())
                itemQuery.push_back({ patch["TargetJs"].get<std::string>(), steam_interface.script_type::javascript });
            if (patch.contains("TargetCss") && patch["TargetCss"].is_string())
                itemQuery.push_back({ patch["TargetCss"].get<std::string>(), steam_interface.script_type::stylesheet });

            inject_items(itemQuery);
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

        try
        {
            //std::cout << m_socket_resp.dump(4) << std::endl;

            m_header = m_socket_resp["result"]["result"]["value"]["title"].get<std::string>();
            this->patch(m_header);
        }
        catch (const nlohmann::detail::exception& error) {
            console.err(fmt::format("Error patching page or getting page title, message: {}, socket message: {}", error.what(), m_socket_resp.dump(4)));
        }

    }

    const std::string get_class_list(const nlohmann::basic_json<>& node)
    {
        std::string attributes_list;

        if (node.contains("attributes"))
        { 
            const nlohmann::basic_json<>& attributes = node["attributes"];

            for (const auto& attribute : attributes)
            {
                attributes_list += " ." + std::regex_replace(attribute.get<std::string>(), std::regex(" "), " .");
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

            //get the <html> attributes of the dom which is what is most important in selecting pages
            std::string attributes = get_class_list(m_socket_resp["result"]["root"]);


            if (attributes.find("ModalDialogPopup") != std::string::npos) {
                console.log("injecting millennium into settings modal");

                // inject millennium on a different thread to prevent it from interfering with the theme injection
                std::thread([&] {
                    steam_interface.inject_millennium(steam_client, hdl, m_socket_resp["sessionId"].get<std::string>());
                }).detach();

//                std::string content = file::readFileSync(fmt::format("{}/steamui/skins/index.js", config.getSteamRoot()));
//                console.log(fmt::format("injecting: {}", content));
//
//                steam_interface.push_to_socket(steam_client, hdl, content, m_socket_resp["sessionId"]);

            }

            if (!skin_json_config.contains("Patches")) {
                return;
            }

            for (const auto& patch : skin_json_config["Patches"].get<std::vector<nlohmann::basic_json<>>>())
            {
                auto matchRegexString = patch.value("MatchRegexString", "_null_");

                bool contains_http = matchRegexString.find("http") != std::string::npos;

                if (contains_http)
                    continue;
                if (attributes.find(matchRegexString) == std::string::npos) 
                    continue;

                console.log_patch("class name", m_header, matchRegexString);

                const auto result = check_statement(patch);

                std::vector<item> itemQuery;

                if (!result[0].fail) {
                    for (const auto statementItem : result) {
                        itemQuery.push_back({ statementItem.itemRefrence, statementItem.scriptType });
                    }
                }

                try {
                    auto conditional = conditionals::has_patch(skin_json_config, skin_json_config["native-name"], m_header);

                    for (auto condition : conditional)
                    {
                        itemQuery.push_back({ condition.item_src, condition.type });
                    }
                }
                catch (const nlohmann::detail::exception& ex)
                {
                    console.err(fmt::format("Error getting conditionals {}", ex.what()));
                }

                if (patch.contains("TargetJsList"))
                {
                    for (const auto item : patch["TargetJsList"])
                    {
                        itemQuery.push_back({ item, steam_interface.script_type::javascript });
                    }
                }
                if (patch.contains("TargetCssList"))
                {
                    for (const auto item : patch["TargetCssList"])
                    {
                        itemQuery.push_back({ item, steam_interface.script_type::stylesheet });
                    }
                }

                if (patch.contains("TargetJs"))
                    itemQuery.push_back({ patch["TargetJs"].get<std::string>(), steam_interface.script_type::javascript });
                if (patch.contains("TargetCss"))
                    itemQuery.push_back({ patch["TargetCss"].get<std::string>(), steam_interface.script_type::stylesheet });

                inject_items(itemQuery);
            }
        }
        catch (nlohmann::json::exception& ex)
        {
            if ((nlohmann::detail::type_error*)dynamic_cast<const nlohmann::detail::type_error*>(&ex) == nullptr)
            {
                console.err(fmt::format("unexpected error type was thrown from nlohmann::json: {}", ex.what()));
            }
        }
    }

    /// <summary>
    /// received the sessionId from the cef instance after we attached to it
    /// we then use the sessionId provided to use runtime.evaluate on the instance
    /// </summary>
    const void attached_to_target() noexcept
    {
        try
        {
            m_sessionId = m_socket_resp["result"]["sessionId"];
        }
        catch (nlohmann::detail::exception& ex)
        {
            console.err(fmt::format("Error getting session Id: {}", ex.what()));
        }
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

    inline bool should_patch_interface(const json_patch& patch, const nlohmann::json& instance)
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

        try {
            if (std::regex_search(steam_page_url_header, std::regex(patch.matchRegexString)))
            {
                if (steam_page_url_header.find("steamloopback.host") != std::string::npos)
                    return false;

                console.log_patch("remote", steam_page_url_header, patch.matchRegexString);
                //mark that it was successfully patched
                patched.push_back(web_debugger_url);
                return true;
            }
        }
        catch (std::regex_error& e) {
            console.err(fmt::format("Invalid regex selector: '{}' is invalid{}", patch.matchRegexString, e.what()));
        }

        return false;
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
            const auto jsAllowed = Settings::Get<bool>("allow-javascript");
            
            if (!js_eval.empty() && jsAllowed)
                steam_interface.evaluate(c, hdl, js_eval, steam_interface.script_type::javascript);
            if (!css_eval.empty())
                steam_interface.evaluate(c, hdl, css_eval, steam_interface.script_type::stylesheet);
        });

        const auto on_message = [&](ws_Client* c, websocketpp::connection_hdl hdl, message_ptr msg) -> void {

            nlohmann::basic_json<> response = json::parse(msg->get_payload());
            const auto method = response.value("method", "millennium::empty");

            if (method.find("Page") != std::string::npos)
            {
                if (method != "Page.frameResized"
                &&  method != "Page.navigatedWithinDocument")
                {
                    evaluate_scripting(c, hdl);
                }
            }
            msg->recycle();
        };

        try {
            c.set_access_channels(websocketpp::log::alevel::none);
            c.clear_access_channels(websocketpp::log::alevel::all);
            c.init_asio();

            c.set_message_handler(bind(on_message, &c, ::_1, ::_2));
            c.set_open_handler(bind([this](ws_Client* c, websocketpp::connection_hdl hdl) -> void {
                this->page_settings(c, hdl);
            }, &c, ::_1));

            websocketpp::lib::error_code ec;
            ws_Client::connection_ptr con = c.get_connection(url, ec);
            if (ec) {
                console.err(fmt::format("could not create connection because: {}", ec.message()));
                return;
            }

            c.connect(con); c.run();
        }
        catch (websocketpp::exception const& e) {
            console.err(fmt::format("remote exception: {}", e.what()));
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
                instance_url.find("steamloopback.host") != std::string::npos)
                return;

            //check if there is a valid skin currently active
            if (skin_json_config["config_fail"])
                return;

            json_str instances = steam_interface.discover(uri.debugger + "/json");
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
                        catch (const asio::system_error& ex) {
                            if (ex.code() == asio::error::misc_errors::eof) {
                                console.err(fmt::format("thread operation aborted and marked for re-establishment. reason: CEF instance was destroyed", __func__));
                            }
                            else {
                                console.err(fmt::format("exception in {}, message: {}", __func__, ex.what()));
                            }

                            //exception was thrown, patching thread crashed, so mark it as unpatched so it can restart itself, happens when the 
                            //instance is left alone for too long sometimes and steams garbage collector clears memory
                            patched.erase(std::remove_if(patched.begin(), patched.end(), [&](const std::string& element) {
                                return element == instance.value("webSocketDebuggerUrl", "_null_");
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
steam_client::steam_client() { }

#ifdef _WIN32
std::string exec_cmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
#endif

/// <summary>
/// yet another initializer
/// </summary>
/// <param name=""></param>
/// <returns></returns>
void Initialize()
{
#ifdef __linux__
    std::promise<bool> m_keep_alive;
    bool initial_start = false;
#endif

    config.setupMillennium();
    skin_json_config = config.getThemeData();

    //skin change event callback functions
    themeConfig::updateEvents::getInstance().add_listener([]() {
        console.imp("skin change event fired, updating skin patch config");
        skin_json_config = config.getThemeData();
        client::get_instance().update_fetch_hook_status();
        //config.installFonts();
    });


    //config file watcher callback function 
    std::thread watcher(themeConfig::watchPath, config.getSkinDir(), []() {
        console.log("skins folder has changed, updating required information");
        skin_json_config = config.getThemeData();
        client::get_instance().update_fetch_hook_status();
        //config.installFonts();
        m_Client.parseSkinData();
    });


    c_threads::get().add(std::thread([&] {

        while (true)
        {
            ws_Client c;
#ifdef _WIN32 // TODO: Compat with linux
            //std::string output = exec_cmd(R"(for /f "tokens=5" %a in ('netstat -aon ^| findstr 8080') do wmic process where processId=%a get name)");
            //std::string conflictPath = exec_cmd(R"(for /f "tokens=5" %a in ('netstat -aon ^| findstr 8080') do wmic process where processId=%a get ExecutablePath)");

            //std::cout << output << std::endl;

            //if (output.find("steamwebhelper.exe") == std::string::npos && output.find("steam.exe") == std::string::npos) {
            //    //MsgBox("Bootstrap Error", [&](auto open) {

            //    //    ImGui::TextWrapped("Millennium couldn't start because another application is using port 8080, "
            //    //        "which is vital for the functionality of the main app."
            //    //        "\nEither uninstall this conflicting app, uninstall millennium, or disable the conflicting application\n\nConfliction Detected:\n");

            //    //    ImGui::TextWrapped(conflictPath.c_str());

            //    //    if (ImGui::Button("Exit Steam")) {
            //    //        exit(0);
            //    //    }
            //    //});
            //    auto selection = msg::show(fmt::format("Millennium couldn't start because another application is using port 8080, "
            //            "which is vital for the functionality of the main app."
            //        "\nEither uninstall this conflicting app, uninstall millennium, or disable the conflicting application\n\nConfliction Detected:\n{}\n\nMillennium will now close...", conflictPath).c_str(), "Millennium");

            //    if (selection == Selection::OK) {
            //        exit(0);
            //    }
            //}
#endif

            try {
                std::string url = json::parse(http::get(uri.debugger + "/json/version"))["webSocketDebuggerUrl"];

                c.set_access_channels(websocketpp::log::alevel::all);
                c.clear_access_channels(websocketpp::log::alevel::all);
                c.init_asio();

                c.set_message_handler(bind([&](ws_Client* c, websocketpp::connection_hdl hdl, message_ptr msg) -> void {
                    client::get_instance().on_message(c, hdl, msg); }, &c, ::_1, ::_2));
                c.set_open_handler(bind([&](ws_Client* c, websocketpp::connection_hdl hdl) -> void {
                    client::get_instance().on_connection(c, hdl); }, &c, ::_1));

                websocketpp::lib::error_code ec;
                ws_Client::connection_ptr con = c.get_connection(url, ec);
                if (ec) {
                    console.err(fmt::format("could not create connection because: {}", ec.message()));
                    return false;
                }

                initial_start = true;
                c.connect(con); c.run();
            }
            catch (const http_error& error) {
                auto message = fmt::format("Error getting Steams webSocketDebuggerUrl: {}", error.what()).c_str();
                
                auto selection = msg::show(message, "Bootstrap Error");
            }
            catch (websocketpp::exception const& e) {
                console.err(fmt::format("Error occurred on client websocket thread: {}", e.what()));
            }
            catch (const asio::system_error& ex) {
                if (ex.code() == asio::error::misc_errors::eof) {
                    console.err(fmt::format("thread operation aborted and marked for re-establishment. reason: CEF instance was destroyed", __func__));
                }
            }
            catch (const nlohmann::detail::exception& ex) {
                console.err(fmt::format("Received malformed JSON response from websocket: {}", ex.what()));
            }
            catch (const std::exception& ex) {
                console.err(fmt::format("Error occurred on client websocket thread: {}", ex.what()));
            }
            catch (...) {
                console.err(fmt::format("Error occurred on client websocket thread: unknown"));
            }
#ifdef _WIN32
            console.err("client interface handler exited unexpectedly. attempting to restart...");
#elif __linux__

            if (!initial_start) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else {
                console.err("steam is exiting, millennium is shutting down...");
                m_keep_alive.set_value(false);
                break;
            }
#endif
        }
        return false;
    }));

    c_threads::get().add(std::thread([&] {

        try {
            remote::get_instance().handle_interface();
        }
        catch (nlohmann::json::exception& ex) {
            console.err(fmt::format("remote: {}", std::string(ex.what())));
        }
        catch (const asio::system_error& ex) {
            console.err(fmt::format("remote: {}", std::string(ex.what())));
        }
        catch (const std::exception& ex) {
            console.err(fmt::format("remote: {}", std::string(ex.what())));
        }
        catch (...) {
            console.err(fmt::format("remote: unknown"));
        }

        return false;
    }));

    //CreateThread(0, 0, [](LPVOID lpParam) -> DWORD {}, 0, 0, 0);

    m_keep_alive.get_future().wait();
    console.log("unhooking millennium...");
}