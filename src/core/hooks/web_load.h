#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <mutex>
#include <regex>

extern unsigned long long g_hookedModuleId;

class WebkitHandler 
{
public:
    static WebkitHandler get();

    enum TagTypes {
        STYLESHEET,
        JAVASCRIPT
    };

    struct HookType {
        std::string path;
        std::regex urlPattern;
        TagTypes type;
        unsigned long long id;
    };

    enum RedirectType {
        REDIRECT = 301,
        MOVED_PERMANENTLY = 302,
        FOUND = 303,
        TEMPORARY_REDIRECT = 307,
        PERMANENT_REDIRECT = 308
    };

    std::shared_ptr<std::vector<HookType>> m_hookListPtr = std::make_shared<std::vector<HookType>>();

    void DispatchSocketMessage(nlohmann::basic_json<> message);
    void SetupGlobalHooks();

    void SetIPCPort(uint16_t ipcPort) { m_ipcPort = ipcPort; }

private:
    uint16_t m_ipcPort;
    long long hookMessageId = -69;

    // must share the same base url, or be whitelisted.
    const char* m_javaScriptVirtualUrl = "https://pseudo.millennium.app/";
    const char* m_steamLoopback = "https://steamloopback.host/";

    bool IsGetBodyCall(nlohmann::basic_json<> message);

    std::string HandleCssHook(std::string body);
    std::string HandleJsHook(std::string body);

    const std::string PatchDocumentContents(std::string requestUrl, std::string original);
    void HandleHooks(nlohmann::basic_json<> message);

    void RetrieveRequestFromDisk(nlohmann::basic_json<> message);
    void GetResponseBody(nlohmann::basic_json<> message);

    std::filesystem::path ConvertToLoopBack(std::string requestUrl);

    struct WebHookItem {
        long long id;
        std::string requestId;
        std::string type;
        nlohmann::basic_json<> message;
    };

    std::shared_ptr<std::vector<WebHookItem>> m_requestMap = std::make_shared<std::vector<WebHookItem>>();
};