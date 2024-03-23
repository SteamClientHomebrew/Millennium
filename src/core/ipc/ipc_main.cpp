#include "ipc_main.hpp"
#include "handlers/types.hpp"
#include "window/interface_v2/editor.hpp"
#include "core/injector/event_handler.hpp"
#include "core/injector/conditions/conditionals.hpp"

#include <utils/config/config.hpp>
#include <utils/cout/logger.hpp>

/* create and manage the interface thread */
#include <utils/thread/thread_handler.hpp>

#include <window/win_handler.hpp>
#include <window/interface/globals.h>
#include <window/core/window.hpp>
#include <window/api/installer.hpp>
#include <utils/base64.hpp>
#include <core/steam/window/manager.hpp>

void escapeSpecialChars(nlohmann::json& value) {
    if (value.is_string()) {
        std::string escapedStr = value.get<std::string>();
        for (size_t i = 0; i < escapedStr.length(); ++i)
        {
            if (escapedStr[i] == '\n')      { escapedStr.replace(i, 1, "\\n");  i++; }
            else if (escapedStr[i] == '\\') { escapedStr.replace(i, 1, "\\\\"); i++; }
            else if (escapedStr[i] == '"')  { escapedStr.replace(i, 1, "\\\""); i++; }
        }
        value = escapedStr;
    }
    else if (value.is_object()) {
        for (auto& pair : value.items()) {
            escapeSpecialChars(pair.value());
        }
    }
    else if (value.is_array()) {
        for (nlohmann::json& element : value) {
            escapeSpecialChars(element);
        }
    }
}

/**
 * Handles incoming IPC (Inter-Process Communication) messages.
 *
 * This method processes incoming IPC messages received from the WebSocket server.
 * It performs different actions based on the message ID, such as updating the selected skin,
 * opening the Millennium application, retrieving the theme list, or getting the active theme.
 * Additionally, it interacts with various components, such as theme configurations, UI updates,
 * and WebSocket communication, to fulfill the requested actions.
 *
 * @param message The JSON object representing the incoming IPC message.
 * @param socket The JSON object representing the WebSocket session information.
 * @param steam_client A pointer to the WebSocket client used for communication.
 * @param hdl The connection handle identifying the WebSocket connection.
 */

void IPC::handleMessage(const nlohmann::basic_json<> message, 
    const nlohmann::basic_json<> socket,
    ws_Client* steam_client, 
    websocketpp::connection_hdl hdl)
{
    /**
     * Sends a message to a WebSocket server through a WebSocket client.
     *
     * This function constructs a JSON message containing the provided message content
     * and necessary parameters, such as message ID, method, and session ID. The message
     * is then sent to the WebSocket server using the specified WebSocket client and connection handle.
     *
     * @param message The message content to be sent.
     * @param socket The JSON object representing the WebSocket session information.
     * @param steam_client A pointer to the WebSocket client used to send the message.
     * @param hdl The connection handle identifying the WebSocket connection.
     */
    const auto respond = [&](std::string type, std::string message) {
        std::string content = fmt::format("console.log({{ returnId: '{}', message: `{}` }})", type, message);
        // Construct the JSON message to be sent.
        const nlohmann::json response = {
            { "id", 8987 },                            // Message ID.
            { "method", "Runtime.evaluate" },          // Method for message evaluation.
            { "params", {{ "expression", content }}},  // Parameters containing the message expression.
            { "sessionId", socket["sessionId"] }      // Session ID associated with the WebSocket.
        };

        // Send the JSON message to the WebSocket server using the WebSocket client.
        steam_client->send(hdl, response.dump(), websocketpp::frame::opcode::text);
    };

    console.log(fmt::format("handling message: {}", message.dump(4)));

    if (message["id"] == "[open-millennium]") {
        console.log("requested to open millennium interface");

        if (!g_windowOpen) {
            c_threads::get().add(std::thread([&] { init_main_window(); }));
        }
        else {
            //Window::bringToFront();
#ifdef _WIN32
            PlaySoundA("SystemExclamation", NULL, SND_ALIAS);
#endif
        }
    }
    if (message["id"] == "[open-theme-folder]")
    {
        OpenURL(config.getSkinDir());
    }
    else if (message["id"] == "[set-StyleSheet Insertion-status]" )
    {
        bool status = message["value"];

        Settings::Set("allow-stylesheet", status);
        console.log(fmt::format("set css usage -> {}", status));
    }
    else if (message["id"] == "[set-Javascript Insertion-status]")
    {
        bool status = message["value"];

        Settings::Set("allow-javascript", status);
        console.log(fmt::format("set js usage -> {}", status));
    }
    else if (message["id"] == "[remove-theme]")
    {
        std::string status = "kept";
        const auto resp = msg::show(
            "Are you sure you want to delete this theme? You cannot undo this.", 
            "Warning", 
            Buttons::YesNo
        );

        if (resp == Selection::Yes) {
            std::string disk_path = fmt::format("{}/{}", config.getSkinDir(), message["name"].get<std::string>());
            if (std::filesystem::exists(disk_path)) {

                try {
                    std::filesystem::remove_all(std::filesystem::path(disk_path));
                }
                catch (const std::exception& ex) {
                    //MsgBox(fmt::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str(), "Non-fatal Error", MB_ICONERROR);
                    console.err(fmt::format("Couldn't remove the selected skin.\nError:{}", ex.what()).c_str());
                }
            }
            m_Client.parseSkinData(false);
            status = "removed";
        }

        respond("[remove-theme-result]", status);
    }
    else if (message["id"] == "[millenniumweb-is-installed]")
    {
        const std::string steamPath = config.getSkinDir();
        std::string status = "false";

        for (const auto& entry : std::filesystem::directory_iterator(steamPath))
        {
            static millennium client;
            if (entry.is_directory() && message["name"] == entry.path().filename().string())
            {
                status = "true";
            }
        }
        respond("[is-installed]", status);
    }
    else if (message["id"] == "[install-theme]")
    {
        console.log("received a theme install message");
        std::string file_name = message["file"];
        std::string download = message["url"];

        Community::Themes->handleThemeInstall(file_name, download, [&](std::string status) {
            if (status == "success") {
                respond("[install-message]", status);
            } else {
                respond("[install-message]", "fail");
            }
        });
    }
    else if (message["id"] == "[get-request]")
    {
        console.log("received a get cor query");

        if (!message.contains("url") || !message.contains("user_agent")) {
            respond("[get-request]", nlohmann::json({
                {"success", false},
                {"message", "you must provide 'url' and 'user_agent' in request"}
            }).dump());
            return;
        }

        auto url = message["url"].get<std::string>();
        auto user_agent = message["user_agent"].get<std::string>();

        const auto http_response = ipc_handler::cor_request(url, user_agent);

        respond("[get-request]", nlohmann::json({
            {"success", http_response.success},
            {"content", http_response.content}
        }).dump());
    }
    else if (message["id"] == "[get-theme-list]") {

        const std::string steamPath = config.getSkinDir();
        std::vector<nlohmann::basic_json<>> jsonBuffer;

        for (const auto& entry : std::filesystem::directory_iterator(steamPath))
        {
            static millennium client;
            if (entry.is_directory())
            {
                if (!client.parseLocalSkin(entry, jsonBuffer, false)) continue;
            }
        }
        nlohmann::json buffer;

        for (auto& item : jsonBuffer) { buffer.push_back(item); }

        escapeSpecialChars(buffer);
        respond("[get-theme-list]", nlohmann::json::parse(buffer.dump()).dump());
    }
    else if (message["id"] == "[get-themetab-data]")
    {
        auto resp = config.getThemeData();
        std::string name = resp.contains("name") ? resp["name"].get<std::string>() : Settings::Get<std::string>("active-skin");

        const auto data = nlohmann::json({
           {"active", name},
           {"is_editable", skin_json_config.contains("Conditions")},
           {"stylesheet", Settings::Get<bool>("allow-stylesheet")},
           {"javascript", Settings::Get<bool>("allow-javascript")},
           {"color", Settings::Get<std::string>("accent-col")},
           {"auto_update", Settings::Get<bool>("auto-update-themes")},
           {"update_notifs", Settings::Get<bool>("auto-update-themes-notifs")},
           {"url_bar", Settings::Get<bool>("enableUrlBar")},
           //"Top Left"/*0*/, "Top Right"/*1*/, "Bottom Left"/*2*/, "Bottom Right"/*3*/
           {"notifs_pos", Settings::Get<int>("NotificationsPos")},
           {"acrylic", Settings::Get<bool>("mica-effect")},
           {"corner_pref", Settings::Get<int>("corner-preference")}
        });

        respond("[get-themetab-data]", data.dump());
    }
    else if (message["id"] == "[update-theme-select]") {

        std::string prevName = Settings::Get<std::string>("active-skin");
        std::string skinName = message["data"];

        // the selected skin was clicked (i.e to deselect)
        if (prevName == skinName) { skinName = "default"; }

        static millennium client;

        //whitelist the default skin
        if (skinName == "default" || !client.isInvalidSkin(skinName))
        {
            console.log("updating selected skin.");
            Settings::Set("active-skin", skinName);
            themeConfig::updateEvents::getInstance().triggerUpdate();

            steam_js_context js_context;
            js_context.reload();

            if (steam::get().params.has("-silent")) {
                msg::show("Steam is launched in -silent mode so you need to open steam again from the task tray for it to re-open", "Millennium");
            }
        }
        else {
            msg::show("the selected skin was not found, therefor can't be loaded.", "Millennium");
        }

        std::cout << message.dump(4) << std::endl;
    }
    else if (message["id"] == "[set-Auto-Update Themes-status]") {

        bool status = message["value"];

        Settings::Set("auto-update-themes", status);
        console.log(fmt::format("set auto-update themes -> {}", status));
    }
    else if (message["id"] == "[set-Auto-Update Notifications-status]") {

        bool status = message["value"];

        Settings::Set("auto-update-themes-notifs", status);
        console.log(fmt::format("set auto-update notifs -> {}", status));
    }
    else if (message["id"] == "[set-Browser URL Bar-status]") {

        bool status = message["value"];

        Settings::Set("enableUrlBar", status);
        console.log(fmt::format("set hide url bar -> {}", status));
    }
    else if (message["id"] == "[set-color-scheme]") {

        std::string status = message["value"];

        Settings::Set("accent-col", status);
        console.log(fmt::format("set accent col -> {}", status));
    }
    else if (message["id"] == "[set-corner-pref]") {
        int pref = message["value"];
        Settings::Set("corner-preference", pref);

        updateHook();

        if (pref == 0) {
            steam_js_context js_context;
            js_context.reload();
        }
    }
    else if (message["id"] == "[set-Acrylic Drop Shadow-status]") {
        bool pref = message["value"];
        Settings::Set("mica-effect", pref);

        console.log(fmt::format("set acrylic -> {}", pref));
    }
    else if (message["id"] == "[set-notifs-pos]") {

        int status = message["value"];

        Settings::Set("NotificationsPos", status);
        console.log(fmt::format("set notifs pos -> {}", status));

        std::string js = fmt::format(R"((() => SteamUIStore.WindowStore.SteamUIWindows[0].m_notificationPosition.position = {})())", status);
        steam_js_context js_context;
        js_context.exec_command(js);
    }
    else if (message["id"] == "[edit-theme]") {
        std::string editor_data = editor::create();
        respond("[edit-theme]", base64_encode(editor_data));
    }
    else if (message.contains("type") && message["type"] == "config")
    {
        std::string id = message["id"];
        std::string new_value = message["value"].is_boolean() ? (message["value"] ? "yes" : "no") : message["value"];

        bool success = conditionals::update(Settings::Get<std::string>("active-skin"), id, new_value);

        console.log(fmt::format("result: {}", success));
        steam_js_context js_context;
        js_context.reload();
    }
}
