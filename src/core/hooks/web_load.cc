#include "web_load.h"
#include <nlohmann/json.hpp>
#include <core/loader.hpp>
#include <core/ffi/ffi.hpp>
#include <sys/encoding.h>
#include <boxer/boxer.h>

WebkitHandler WebkitHandler::get() 
{
    static WebkitHandler webkitHandler;
    return webkitHandler;
}

void WebkitHandler::SetupGlobalHooks() 
{
    Sockets::PostGlobal({
        { "id", 3242 }, { "method", "Fetch.enable" },
        { "params", {
            { "patterns", {
                { { "urlPattern", "https://*.*.com/public/shared/css/buttons.css*" }, { "resourceType", "Stylesheet" }, { "requestStage", "Response" }  },
                { { "urlPattern", "https://*.*.com/public/shared/javascript/shared_global.js*" }, { "resourceType", "Script" }, { "requestStage", "Response" } },
                
                { { "urlPattern", fmt::format("{}*", this->m_javaScriptVirtualUrl) }, { "requestStage", "Request" } }
            }
        }}}
    });
}

bool WebkitHandler::IsGetBodyCall(nlohmann::basic_json<> message) 
{
    return message["params"]["request"]["url"].get<std::string>()
        .find(this->m_javaScriptVirtualUrl) != std::string::npos;
}

std::string WebkitHandler::HandleJsHook(std::string body) 
{
    std::string scriptTagInject;

    for (auto& hookItem : *m_hookListPtr) 
    {
        if (hookItem.type != TagTypes::JAVASCRIPT) 
        {
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(hookItem.path, SystemIO::GetSteamPath() / "steamui");
        
        scriptTagInject.append(fmt::format(
            "document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}{}', type: 'module', id: 'millennium-injected' }}));\n", 
            this->m_javaScriptVirtualUrl, relativePath.generic_string()
        ));
    }
    return scriptTagInject + body;
}

std::string WebkitHandler::HandleCssHook(std::string body) 
{
    std::string styleTagInject;

    for (auto& hookItem : *m_hookListPtr) 
    {
        if (hookItem.type != TagTypes::STYLESHEET) 
        {
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(hookItem.path, SystemIO::GetSteamPath() / "steamui");
        styleTagInject.append(fmt::format("@import \"{}{}\";\n", this->m_steamLoopback, relativePath.generic_string()));
    }

    return styleTagInject + body;
}

std::filesystem::path WebkitHandler::ConvertToLoopBack(std::string requestUrl)
{
    std::size_t pos = requestUrl.find(this->m_javaScriptVirtualUrl);

    if (pos != std::string::npos)
    {
        requestUrl.erase(pos, std::string(this->m_javaScriptVirtualUrl).length());
    }

    return SystemIO::GetSteamPath() / "steamui" / requestUrl;
}

void WebkitHandler::RetrieveRequestFromDisk(nlohmann::basic_json<> message)
{
    std::filesystem::path localFilePath = this->ConvertToLoopBack(message["params"]["request"]["url"]);
    std::ifstream localFileStream(localFilePath);

    bool failed = !localFileStream.is_open();

    if (failed)
    {
        Logger.Error("failed to open file [readJsonSync]");
    }

    const std::string fileContent((std::istreambuf_iterator<char>(localFileStream)), std::istreambuf_iterator<char>());

    const std::string successMessage = "MILLENNIUM-VIRTUAL";
    const std::string failedMessage = fmt::format("Millennium couldn't read {}", localFilePath.string());

    const nlohmann::json responseHeaders = nlohmann::json::array
    ({
        { {"name", "Access-Control-Allow-Origin"}, {"value", "*"} },
        { {"name", "Content-Type"}, {"value", "application/javascript"} }
    });

    Sockets::PostGlobal({
        { "id", 63453 },
        { "method", "Fetch.fulfillRequest" },
        { "params", {
            { "requestId",       message["params"]["requestId"] },
            { "responseCode",    failed ? 404 : 200 },
            { "responsePhrase",  failed ? failedMessage : successMessage },
            { "responseHeaders", responseHeaders },
            { "body", Base64Encode(fileContent) }
        }}
    });
}

void WebkitHandler::GetResponseBody(nlohmann::basic_json<> message)
{
    hookMessageId -= 1;
    m_requestMap->push_back({ hookMessageId, message["params"]["requestId"], message["params"]["resourceType"] });

    Sockets::PostGlobal({
        { "id", hookMessageId },
        { "method", "Fetch.getResponseBody" },
        { "params", { { "requestId", message["params"]["requestId"] } }}
    });
}

void WebkitHandler::HandleHooks(nlohmann::basic_json<> message)
{
    for (auto requestIterator = m_requestMap->begin(); requestIterator != m_requestMap->end();)
    {
        auto [id, request_id, type] = (*requestIterator);

        if (message["id"] != id || !message["result"]["base64Encoded"])
        {
            requestIterator++;
            continue;
        }

        std::string hookedBodyResponse = {};

        if (type == "Script")
        {
            hookedBodyResponse = this->HandleJsHook(Base64Decode(message["result"]["body"]));
        }
        else if (type == "Stylesheet")
        {
            hookedBodyResponse = this->HandleCssHook(Base64Decode(message["result"]["body"]));
        }

        Sockets::PostGlobal({
            { "id", 63453 },
            { "method", "Fetch.fulfillRequest" },
            { "params", {
                { "requestId", request_id },
                { "responseCode", 200 },
                { "body", Base64Encode(hookedBodyResponse) }
            }}
        });

        requestIterator = m_requestMap->erase(requestIterator);
    }
}

void WebkitHandler::DispatchSocketMessage(nlohmann::basic_json<> message)
{
    try 
    {
        if (message["method"] == "Fetch.requestPaused")
        {
            switch (this->IsGetBodyCall(message))
            {
                case true: 
                {
                    this->RetrieveRequestFromDisk(message);
                    break;
                }
                case false:
                {
                    this->GetResponseBody(message);
                    break;
                }
            }
        }

        this->HandleHooks(message);
    }
    catch (const nlohmann::detail::exception& ex) 
    {
        Logger.Error("error hooking WebKit -> {}", ex.what());
    }
    catch (const std::exception& ex) 
    {
        Logger.Error("error hooking WebKit -> {}", ex.what());
    }
}
