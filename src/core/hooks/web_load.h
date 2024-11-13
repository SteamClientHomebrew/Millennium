#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <mutex>

static unsigned long long g_hookedModuleId;

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
        TagTypes type;
        unsigned long long id;
    };

    std::shared_ptr<std::vector<HookType>> m_hookListPtr = std::make_shared<std::vector<HookType>>();

    void DispatchSocketMessage(nlohmann::basic_json<> message);
    void SetupGlobalHooks();

    void SetIPCPort(uint16_t ipcPort) { m_ipcPort = ipcPort; }

private:
    uint16_t m_ipcPort;
    long long hookMessageId = -69;

    // must share the same base url, or be whitelisted.
    const char* m_javaScriptVirtualUrl = "https://s.ytimg.com/millennium-virtual/";
    const char* m_steamLoopback = "https://steamloopback.host/";

    bool IsGetBodyCall(nlohmann::basic_json<> message);

    std::string HandleCssHook(std::string body);
    std::string HandleJsHook(std::string body);
    void HandleHooks(nlohmann::basic_json<> message);

    void RetrieveRequestFromDisk(nlohmann::basic_json<> message);
    void GetResponseBody(nlohmann::basic_json<> message);

    std::filesystem::path ConvertToLoopBack(std::string requestUrl);

    struct WebHookItem {
        long long id;
        std::string requestId;
        std::string type;
    };

    std::shared_ptr<std::vector<WebHookItem>> m_requestMap = std::make_shared<std::vector<WebHookItem>>();
};