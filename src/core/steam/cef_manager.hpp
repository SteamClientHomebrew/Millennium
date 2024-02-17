#pragma once
#define ASIO_STANDALONE
#define _WINSOCKAPI_ // Prevent WinSock.h from being included

#include <winsock2.h>

#include <wininet.h>
#pragma comment(lib, "Wininet.lib")

//network helpers and other buffer typings to help with socket management
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> ws_Client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using json = nlohmann::json;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;


struct endpoints
{
public:
    //used for serving steam/steamui contents, hosted by the steam client

    std::string steam_resources = "https://steamloopback.host";
    std::string debugger = "http://127.0.0.1:8080";
};

static endpoints uri;

/// <summary>
/// javascript helpers when interacting with the DOM
/// </summary>
class cef_dom {
private:
    class dom_head {
    public:
        virtual const std::basic_string<char, std::char_traits<char>, std::allocator<char>> add(std::string)
        {
            return std::string();
        }
    };

    /// <summary>
    /// evaluate scripts from a url host on a js vm
    /// </summary>
    class runtime {
    public:
        const std::string evaluate(std::string remote_url) noexcept;
    };

    class stylesheet : public dom_head {
    public:
        const std::basic_string<char, std::char_traits<char>, std::allocator<char>> add(std::string filename) noexcept override;
    };

    class javascript : public dom_head {
    public:
        const std::basic_string<char, std::char_traits<char>, std::allocator<char>> add(std::string filename) noexcept override;
    };

public:
    /// <summary>
    /// uses a singleton class on the dom for ease of access
    /// </summary>
    /// <returns>cef_dom instance, statically typed</returns>
    static cef_dom& get();

    runtime runtime_handler;

    javascript javascript_handler;
    stylesheet stylesheet_handler;

private:
    cef_dom() {}
    cef_dom(const cef_dom&) = delete;
    cef_dom& operator=(const cef_dom&) = delete;
};

std::string system_color_script();
std::string theme_colors();

struct steam_cef_manager
{
private:
    const void calculate_endpoint(std::string& endpoint_unparsed);

public:
    /// <summary>
    /// response types that are handled by millennium in the client interface handler
    /// </summary>
    enum response
    {
        attached_to_target = 420,
        script_inject_evaluate = 69,
        received_cef_details = 69420,
        get_document = 9898,
        get_body = 2349
    };

    enum script_type { javascript, stylesheet };

    /// <summary>
    /// https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#method-evaluate
    /// create a javascript pipe to the dom that runs on a vm, sending too much js causes socket crash, use cef_dom runtime eval 
    /// </summary>
    /// <param name="socket">steam cef socket, unique to each page</param>
    /// <param name="raw_script">javascript to be executed</param>
    /// <param name="sessionId">socket identifier, if needed (not required for remote webpages)</param>
    void push_to_socket(ws_Client* c, websocketpp::connection_hdl hdl, std::string raw_script, std::string sessionId = std::string()) noexcept;

    /// <summary>
    /// discover information from remote_endpoint
    /// </summary>
    /// <param name="remote_endpoint">absolute uri</param>
    /// <returns>string value of the result, does not include headers</returns>
    std::string discover(std::basic_string<char, std::char_traits<char>, std::allocator<char>> remote_endpoint);

    /// <summary>
    /// injects millennium into the settings page. its called when the settings page is opened
    /// </summary>
    /// <param name="socket">steam socket</param>
    /// <param name="socket_response">used sessionId to target the setting modal</param>
    /// <returns></returns>
    __declspec(noinline) void __fastcall inject_millennium(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept;
    __declspec(noinline) void __fastcall render_settings_modal(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept;

    /// <summary>
    /// interface to inject js into a cef instance
    /// </summary>
    /// <param name="socket">cef instance from steam</param>
    /// <param name="file">the remote or local endpoint to a file</param>
    /// <param name="type">javascript or stylesheet injections from script_type</param>
    /// <param name="socket_response">if not null, its assumed to use sessionId (i.e for client handling)</param>
    const void evaluate(ws_Client* c, websocketpp::connection_hdl hdl, std::string file, script_type type, nlohmann::basic_json<> socket_response = NULL);

};

static steam_cef_manager steam_interface;

/// <summary>
/// interact with SteamJSContext, which is the host of the steam client, includes functions for the entire steam client under
/// the SteamClient class, all pages are created and destroyed from this instance
/// </summary>
struct steam_js_context {
private:
    websocketpp::client<websocketpp::config::asio_client>* c;
    websocketpp::connection_hdl hdl;

    /// <summary>
    /// connect to the socket of SharedJSContext
    /// </summary>
    /// <param name="callback"></param>
    const void establish_socket_handshake(std::function<void()> callback);


    //prolonging the socket closure causes socket failure and slow exec times all around
    const void close_socket() noexcept;
public:

    steam_js_context();

    /// <summary>
    /// reload the SteamJSContext, which will reload the steam interface, but not the app
    /// </summary>
    __declspec(noinline) const void reload() noexcept;

    /// <summary>
    /// restart the app completely, SharedJsContext and the app intself, cmd args are retained
    /// </summary>
    __declspec(noinline) const void restart() noexcept;

    /// <summary>
    /// execute js on the SharedJSContext, where you can access the SteamClient in full
    /// </summary>
    /// <param name="javascript">code to execute on the page</param>
    /// <returns></returns>
    std::string exec_command(std::string javascript);
};
