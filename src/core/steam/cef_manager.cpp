#define _WINSOCKAPI_   

#include <core/injector/event_handler.hpp>

#include <stdafx.h>
#include <core/steam/cef_manager.hpp>
#include <utils/config/config.hpp>
#include "colors/accent_colors.hpp"

const std::string cef_dom::runtime::evaluate(std::string remote_url) noexcept {
    return R"(
    (() => fetch(')" + remote_url + R"(').then((response) => response.text()).then((data) => { 
        if (window.usermode === undefined) {
            const establishConnection = () => {
                const millennium = new WebSocket('ws://localhost:)" + std::to_string(steam_client::get_ipc_port()) + R"(');
                millennium.onmessage = (event) => {
                    if (usermode.ipc && usermode.ipc.onmessage) {
                        usermode.ipc.onmessage(JSON.parse(event.data));
                    }
                };
                return {
                    send: (message) => {
                        millennium.send(JSON.stringify(message));
                    },
                    onmessage: null
                };
            };
            window.usermode = {
                ipc: establishConnection(),
                ipc_types: { open_skins_folder: 0, skin_update: 1, open_url: 2, change_javascript: 3, change_console: 4, get_skins: 5 }
            };
            eval(data);
        }
    }))())";
}

const std::basic_string<char, std::char_traits<char>, std::allocator<char>> cef_dom::stylesheet::add(std::string filename) noexcept {
    return std::format("!document.querySelectorAll(`link[href='{}']`).length && document.head.appendChild(Object.assign(document.createElement('link'), {{ rel: 'stylesheet', href: '{}' }}));", filename, filename);
}

const std::basic_string<char, std::char_traits<char>, std::allocator<char>> cef_dom::javascript::add(std::string filename) noexcept {
    return std::format("!document.querySelectorAll(`script[src='{}'][type='module']`).length && document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module' }}));", filename, filename);
}

cef_dom& cef_dom::get() {
    static cef_dom instance;
    return instance;
}

std::string injectSystemColors() {
    return std::format("(document.querySelector('#SystemAccentColorInject') || document.head.appendChild(Object.assign(document.createElement('style'), {{ id: 'SystemAccentColorInject' }}))).innerText = `{}`;", getColorStr());
}
std::string injectGlobalColor() {
    const auto colors = config.getRootColors();
    return colors != "[NoColors]" ? std::format("(document.querySelector('#globalColors') || document.head.appendChild(Object.assign(document.createElement('style'), {{ id: 'globalColors' }}))).innerText = `{}`;", colors) : "";
}

void steam_cef_manager::push_to_socket(ws_Client* c, websocketpp::connection_hdl hdl, std::string raw_script, std::string sessionId) noexcept
{
    try {
        std::function<void(const std::string&)> runtime_evaluate = [&](const std::string& script) -> void {
            nlohmann::json evaluate_script = {
                {"id", response::script_inject_evaluate},
                {"method", "Runtime.evaluate"},
                {"params", {{"expression", script}}}
            };

            if (!sessionId.empty()) evaluate_script["sessionId"] = sessionId;
            c->send(hdl, evaluate_script.dump(4), websocketpp::frame::opcode::text);
        };

        //inject the script that the user wants
        runtime_evaluate(injectGlobalColor() + "\n\n\n" + injectSystemColors() + "\n\n\n" + raw_script);
    }
    catch (const boost::system::system_error& ex) {

        if (ex.code().value() == boost::asio::error::operation_aborted || ex.code().value() == boost::asio::error::eof)
        {
            console.warn("[non-fatal] steam garbage clean-up aborted a connected socket, re-establishing connection");
        }
    }
}

inline std::string steam_cef_manager::discover(std::basic_string<char, std::char_traits<char>, std::allocator<char>> remote_endpoint)
{
    HINTERNET hInternet = InternetOpenA("millennium.patcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlA(hInternet, remote_endpoint.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

    if (!hUrl || !hInternet)
    {
        throw std::runtime_error(std::format("the requested operation [{}()] failed. reason: InternetOpen*A was nullptr", __func__));
    }

    char buffer[1024];
    DWORD total_bytes_read = 0;
    std::string discovery_result;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read) discovery_result.append(buffer, total_bytes_read);
    InternetCloseHandle(hUrl); InternetCloseHandle(hInternet);

    return discovery_result;
}

__declspec(noinline) void __fastcall steam_cef_manager::render_settings_modal(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept
{
    std::string path = std::format("{}/{}", uri.steam_resources.string(), "modal.js");

    steam_interface.push_to_socket(steam_client, hdl, cef_dom::get().javascript_handler.add(path), sessionId);
}


__declspec(noinline) void __fastcall steam_cef_manager::inject_millennium(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept
{
    std::string javaScript = R"(

async function initJQuery() {
	return new Promise((resolve, reject) => {
		document.head.appendChild(Object.assign(document.createElement('script'), { 
			type: 'text/javascript', 
			src: 'https://code.jquery.com/jquery-3.6.0.min.js',
			onload: () => resolve()
		}))
	})
}

function waitForElement(querySelector, timeout) {
    return new Promise((resolve, reject) => {
        const matchedElements = document.querySelectorAll(querySelector);
        if (matchedElements.length) {
            resolve({
                querySelector,
                matchedElements
            });
        }
        let timer = null;
        const observer = new MutationObserver(
            () => {
                const matchedElements = document.querySelectorAll(querySelector);
                if (matchedElements.length) {
                    if (timer) clearTimeout(timer);
                    observer.disconnect();
                    resolve({
                        querySelector,
                        matchedElements
                    });
                }
            });
        observer.observe(document.body, {
            childList: true,
            subtree: true
        });
        if (timeout) {
            timer = setTimeout(() => {
                observer.disconnect();
                reject(querySelector);
            }, timeout);
        }
    });
}

const init = {
    isActive: false,
    settings: () => {
        initJQuery().then(() => {
            const observer = new MutationObserver(() => {
                var setting = document.querySelectorAll('[class*="pagedsettings_PagedSettingsDialog_PageListItem_"]')
                const classList = Array.from(setting[5].classList);

                if (classList.some(className => (/pagedsettings_Active_*/).test(className)) === true) {
                    console.log("Interface tab is selected")
                    if (!init.isActive) {
                        renderer.modal()
                        init.isActive = true
                    }
                }
                else {
                    init.isActive = false
                }
            });
            observer.observe(document.body, {
                childList: true,
                subtree: true
            });
        })
    }
}

const IPC = {
    getMessage: (messageId) => {
        window.opener.console.log("millennium.user.message:", JSON.stringify({id: messageId}))

        return new Promise((resolve) => {
            var originalConsoleLog = window.opener.console.log;

            window.opener.console.log = function(message) {
                originalConsoleLog.apply(console, arguments);

                if (message.returnId === messageId) {
                    window.opener.console.log = originalConsoleLog;
                    resolve(message);
                }
            };
        });
    },
    postMessage: (messageId, contents) => {
        window.opener.console.log("millennium.user.message:", JSON.stringify({id: messageId, data: contents}))

        return new Promise((resolve) => {
            var originalConsoleLog = window.opener.console.log;

            window.opener.console.log = function(message) {
                originalConsoleLog.apply(console, arguments);

                if (message.returnId === messageId) {
                    window.opener.console.log = originalConsoleLog;
                    resolve(message);
                }
            };
        });
    }
};

const renderer = {
    appendList: async (top) => {
        let query = await IPC.getMessage('[get-theme-list]')
        let message = JSON.parse(query.message)

        var dialogMenu = $('<div>', {
            'class': 'DialogMenuPosition visible contextmenu_ContextMenuFocusContainer_2qyBZ',
            'tabindex': 0,
            'style': `visibility: visible; top: ${top}px; right: 26px; height: fit-content;`
        });

        var dropdownContainer = $('<div>', {
            'class': 'dropdown_DialogDropDownMenu_1tiuY _DialogInputContainer'
        });

        dropdownContainer.append(
            $('<div>', {
                'class': 'dropdown_DialogDropDownMenu_Item_1R-DV'
            }).text("< default skin >").click((event) => {
                IPC.postMessage("[update-theme-select]", 'default')
            })
        );

        for (var i = 0; i < message.length; i++) {

            dropdownContainer.append(
                $('<div>', {
                    'class': 'dropdown_DialogDropDownMenu_Item_1R-DV',
                    'native-name': message[i]["native-name"]
                }).text(message[i].name).click((event) => {
                    IPC.postMessage("[update-theme-select]", $(event.target).attr('native-name'))
                })
            );
        }

        dialogMenu.append(dropdownContainer);
        $('body').append(dialogMenu);
    },
    dismiss: () => {
        function clickHandler(event) {
            const modal = document.querySelector('.dropdown_DialogDropDownMenu_1tiuY._DialogInputContainer');

            if (modal && !modal.contains(event.target)) {
                modal.remove();
                document.removeEventListener("click", clickHandler);
            }
        }
        document.addEventListener("click", clickHandler);
    },
    modal: async () => {
        console.log("inserting modal code")
        let query = await IPC.getMessage('[get-active]')

        // Create the HTML structure step by step
        var newElement = $('<div>', {
            class: "gamepaddialog_Field_S-_La gamepaddialog_WithFirstRow_qFXi6 gamepaddialog_VerticalAlignCenter_3XNvA gamepaddialog_WithDescription_3bMIS gamepaddialog_WithBottomSeparatorStandard_3s1Rk gamepaddialog_ChildrenWidthFixed_1ugIU gamepaddialog_ExtraPaddingOnChildrenBelow_5UO-_ gamepaddialog_StandardPadding_XRBFu gamepaddialog_HighlightOnFocus_wE4V6 Panel",
            tabindex: "-1",
            style: "--indent-level:0"
        })
        .append(
            $('<div>', { class: "gamepaddialog_FieldLabelRow_H9WOq" })
            .append( $('<div>', { class: "gamepaddialog_FieldLabel_3b0U-", text: "Steam Skin" }) )
            .append(
                $('<div>', { class: "gamepaddialog_FieldChildrenWithIcon_2ZQ9w" })
                .append(
                    $('<div>', { class: "gamepaddialog_FieldChildrenInner_3N47t" })
                    .append(
                        $('<div>', { class: "DialogDropDown _DialogInputContainer  Panel", tabindex: "-1" })
                        .click(() => {
                            if ($('.dropdown_DialogDropDownMenu_1tiuY._DialogInputContainer').length)
                                return 
                            renderer.appendList($('.DialogDropDown')[0].getBoundingClientRect().bottom)
                            renderer.dismiss()
                        })
                        .append(
                            $('<div>', {
                                class: "DialogDropDown_CurrentDisplay",
                                text: query.message
                            })
                        )
                        .append(
                            $('<div>', {
                                class: "DialogDropDown_Arrow"
                            })
                            .append($('<svg xmlns="http://www.w3.org/2000/svg" class="SVGIcon_Button SVGIcon_DownArrowContextMenu" data-name="Layer 1" viewBox="0 0 128 128" x="0px" y="0px"><polygon points="50 59.49 13.21 22.89 4.74 31.39 50 76.41 95.26 31.39 86.79 22.89 50 59.49"></polygon></svg>'))
                        )
                    )
                )
            )
        )
        .append(
            $('<div>', { class: "gamepaddialog_FieldDescription_2OJfk" })
            .append(
                $('<div>', {
                    text: "Select the skin you wish Steam to use (requires reload) "
                })
            )
            .append(
                $('<a>', {
                    class: "settings_SettingsLink_RmxP9", href: "#",
                    text: "Open Millennium"
                }).click(() => {
                    window.opener.console.log("millennium.user.message:", JSON.stringify({id: '[open-millennium]'}))
                })
            )
        );

        $('.DialogBody.settings_SettingsDialogBodyFade_aFxOa').prepend(newElement);
    },
    init: () => {     
        waitForElement('[class*="settings_DesktopPopup_"]').then(({matchedElements}) => {
            init.settings()
        });
    }
}

//init settings renderer
renderer.init()
)";

    std::cout << "Pushing script to ClientInterface" << std::endl;

    steam_interface.push_to_socket(steam_client, hdl, javaScript, sessionId);
}

const void steam_cef_manager::calculate_endpoint(std::string& endpoint_unparsed)
{
    //calculate endpoint type, if remote url retain info, if local add loopbackhost
    if (endpoint_unparsed.compare(0, 4, "http") != 0) {

        //remote skin use the remote skins as a current directory from github
        if (millennium_remote.is_remote) endpoint_unparsed = std::format("{}/{}", millennium_remote.host, endpoint_unparsed);
        //just use the default from the skin path
        else endpoint_unparsed = std::format("{}/skins/{}/{}", uri.steam_resources.string(), Settings::Get<std::string>("active-skin"), endpoint_unparsed);
    }
}

const void steam_cef_manager::evaluate(ws_Client* c, websocketpp::connection_hdl hdl, std::string file, script_type type, nlohmann::basic_json<> socket_response)
{
    if (type == script_type::javascript && (!Settings::Get<bool>("allow-javascript"))) {
        console.err(std::format("the requested operation [{}()] was cancelled. reason: script_type::javascript is prohibited", __func__));
        return;
    }

    if (type == script_type::stylesheet && (!Settings::Get<bool>("allow-stylesheet"))) {
        console.err(std::format("the requested operation [{}()] was cancelled. reason: script_type::stylesheet is prohibited", __func__));
        return;
    }

    this->calculate_endpoint(file);

    std::optional<std::string> sessionId;

    if (!socket_response.is_null() && socket_response.contains("sessionId")) {
        sessionId = socket_response["sessionId"].get<std::string>();
    }

    //pass in sessionId if available
    switch (type)
    {
    case script_type::javascript:
        steam_interface.push_to_socket(c, hdl, cef_dom::get().javascript_handler.add(file).data(), sessionId.value_or(std::string()));
        break;
    case script_type::stylesheet:
        steam_interface.push_to_socket(c, hdl, cef_dom::get().stylesheet_handler.add(file).data(), sessionId.value_or(std::string()));
        break;
    default:
        throw std::runtime_error("[debug] invalid target scripting type");
    }
}

const void steam_js_context::establish_socket_handshake(std::function<void()> callback)
{
    console.log("attempting to create a connection to SteamJSContext");

    if (callback == nullptr)
        throw std::runtime_error("callback function was nullptr");

    boost::asio::connect(socket.next_layer(), resolver.resolve(uri.debugger.host(), uri.debugger.port()));

    nlohmann::basic_json<> instances = nlohmann::json::parse(steam_interface.discover(uri.debugger.string() + "/json"));

    //instances contains all pages, filter the correct instance
    auto itr_impl = std::find_if(instances.begin(), instances.end(), [](const nlohmann::json& instance) {
        return instance["title"].get<std::string>() == "SharedJSContext";
    });
    if (itr_impl == instances.end()) { throw std::runtime_error("SharedJSContext wasn't instantiated"); }

    boost::network::uri::uri socket_url((*itr_impl)["webSocketDebuggerUrl"].get<std::string>());
    socket.handshake(uri.debugger.host(), socket_url.path());

    //implicit type converge -> interrupt_handler
    static_cast<void>(callback());
    this->close_socket();
}

steam_js_context::steam_js_context() : socket(io_context), resolver(io_context) { }

inline const void steam_js_context::close_socket() noexcept
{
    boost::system::error_code error_code;
    socket.close(boost::beast::websocket::close_code::normal, error_code);
}

__declspec(noinline) const void steam_js_context::reload() noexcept
{
    establish_socket_handshake([this]() {
        //https://chromedevtools.github.io/devtools-protocol/tot/Page/#method-reload
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 89}, {"method", "Page.reload"} }).dump()));
    });
}

__declspec(noinline) const void steam_js_context::restart() noexcept
{
    establish_socket_handshake([=, this]() {
        //not used anymore, but here in case
        socket.write(boost::asio::buffer(nlohmann::json({
            { "id", 8987 },
            { "method", "Runtime.evaluate" },
            { "params", {
                { "expression", "setTimeout(() => {SteamClient.User.StartRestart(true)}, 1000)" }
            }}
        }).dump()));
    });
}

std::string steam_js_context::exec_command(std::string javascript)
{
    std::promise<std::string> promise;

    establish_socket_handshake([&]() {
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1337}, {"method", "Runtime.evaluate"}, {"params", {{"expression", javascript}, {"awaitPromise", true}}} }).dump()));

        boost::beast::multi_buffer buffer; socket.read(buffer);
        promise.set_value(nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()))["result"].dump());
        });

    return promise.get_future().get();
}