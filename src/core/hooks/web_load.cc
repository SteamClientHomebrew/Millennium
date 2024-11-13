#include "web_load.h"
#include <nlohmann/json.hpp>
#include <core/loader.h>
#include <core/ffi/ffi.h>
#include <sys/encoding.h>
#include <sys/http.h>   
#include <unordered_set>
// #include <boxer/boxer.h>

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
                { { "urlPattern", "*" }, { "resourceType", "Document" }, { "requestStage", "Request" } },

                
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
    const static std::string API = R"(

    if (typeof window.MILLENNIUM_BACKEND_IPC === 'undefined')
    {
    const IPCMain = {
        postMessage: function(messageId, contents) {
            return new Promise(function(resolve) {

                const message = { id: messageId, iteration: window.CURRENT_IPC_CALL_COUNT++, data: contents };

                const messageHandler = function(data) {
                    const json = JSON.parse(data.data);

                    // Wait to receive the correct message id from the backend
                    if (json.id !== message.iteration) return;

                    resolve(json);
                    window.MILLENNIUM_IPC_SOCKET.removeEventListener('message', messageHandler);
                };

                window.MILLENNIUM_IPC_SOCKET.addEventListener('message', messageHandler);
                window.MILLENNIUM_IPC_SOCKET.send(JSON.stringify(message));
            });
        }
    };

    window.MILLENNIUM_BACKEND_IPC = IPCMain;

    window.Millennium = {
        callServerMethod: function(pluginName, methodName, kwargs) {
            return new Promise(function(resolve, reject) {
                const query = {
                    pluginName: pluginName,
                    methodName: methodName,
                    ...(kwargs && { argumentList: kwargs })
                };

                // Call handled from "src\core\ipc\pipe.cpp @ L:67"
                window.MILLENNIUM_BACKEND_IPC.postMessage(0, query).then(function(response) {
                    if (response && response.failedRequest) {
                        reject(`IPC call from [name: ${pluginName}, method: ${methodName}] failed on exception -> ${response.failMessage}`);
                    }

                    const responseStream = response.returnValue;
                    // FFI backend encodes string responses in base64 to avoid encoding issues
                    resolve(typeof responseStream === 'string' ? atob(responseStream) : responseStream);
                });
            });
        },
        findElement: function(privateDocument, querySelector, timeout) {
            return new Promise(function(resolve, reject) {
                const matchedElements = privateDocument.querySelectorAll(querySelector);

                // Node is already in DOM and doesn't require watchdog
                if (matchedElements.length) {
                    resolve(matchedElements);
                    return;
                }

                let timer = null;

                const observer = new MutationObserver(function() {
                    const matchedElements = privateDocument.querySelectorAll(querySelector);
                    if (matchedElements.length) {
                        if (timer) clearTimeout(timer);

                        observer.disconnect();
                        resolve(matchedElements);
                    }
                });

                // Observe the document body for item changes, assuming we are waiting for target element
                observer.observe(privateDocument.body, {
                    childList: true,
                    subtree: true
                });

                if (timeout) {
                    timer = setTimeout(function() {
                        observer.disconnect();
                        reject();
                    }, timeout);
                }
            });
        }
    };


    function createWebSocket(url) {
        return new Promise((resolve, reject) => {
            const startTime = Date.now();  // Record the start time
            
            try {
                let socket = new WebSocket(url);
                socket.addEventListener('open', () => {
                    const endTime = Date.now();  // Record the end time
                    const connectionTime = endTime - startTime;  // Calculate the connection time
                    console.log('%c Millennium ', 'background: black; color: white', 
                                `Successfully connected to IPC server. Connection time: ${connectionTime}ms.`);
                    resolve(socket);
                });
                socket.addEventListener('close', () => {
                    setTimeout(() => {
                        createWebSocket(url).then(resolve).catch(reject);
                    }, 100);
                });
            } 
            catch (error) {
                console.warn('Failed to connect to IPC server:', error);
            } 
        });
    }

    createWebSocket('ws://localhost:)" + std::to_string(m_ipcPort) + R"(').then((socket) => {
        window.MILLENNIUM_IPC_SOCKET = socket;
        window.CURRENT_IPC_CALL_COUNT = 0;
    })
    .catch((error) => console.error('Initial WebSocket connection failed:', error));
    }
    )";

    std::string scriptTagInject;

    for (auto& hookItem : *m_hookListPtr) 
    {
        if (hookItem.type != TagTypes::JAVASCRIPT) 
        {
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(hookItem.path, SystemIO::GetSteamPath());
        
        scriptTagInject.append(fmt::format(
            "{}\ndocument.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}{}', type: 'module', id: 'millennium-injected' }}));\n", 
            API, this->m_javaScriptVirtualUrl, relativePath.generic_string()
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
        LOG_ERROR("failed to retrieve file info from disk.");
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
        try 
        {
            auto [id, request_id, type] = (*requestIterator);

            if (type == "Document" || message["id"] != id || !message["result"]["base64Encoded"])
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
        catch (const nlohmann::detail::exception& ex) 
        {
            LOG_ERROR("error hooking WebKit -> {}", ex.what());
        }
        catch (const std::exception& ex) 
        {
            LOG_ERROR("error hooking WebKit -> {}", ex.what());
        }
    }
}

void WebkitHandler::DispatchSocketMessage(nlohmann::basic_json<> message)
{
    // if (message.value("method", "").find("Debugger.") == std::string::npos)
    // {
    //     Logger.Log(message.dump(4));
    // }

    try 
    {
        if (message["method"] == "Fetch.requestPaused")
        {
            if (message["params"]["resourceType"] == "Document")
            {

                std::string requestId  = message["params"]["requestId"].get<std::string>();
                std::string requestUrl = message["params"]["request"]["url"].get<std::string>();
                nlohmann::json requestHeaders = message["params"]["request"]["headers"];

                nlohmann::json responseHeaders = nlohmann::json::array({
                    {
                        { "name", "Content-Security-Policy" },
                        { "value", "default-src * 'unsafe-inline' 'unsafe-eval' data: blob:; script-src * 'unsafe-inline' 'unsafe-eval' data: blob:;" }
                    },
                    {
                        { "name", "Access-Control-Allow-Origin" },
                        { "value", "*" }
                    }
                });

                for (auto& [key, value] : requestHeaders.items())
                {
                    responseHeaders.push_back({ { "name", key }, { "value", value } });
                }

                nlohmann::json responseHeadersJson;

                const auto [originalContent, statusCode] = Http::GetWithHeaders(requestUrl.c_str(), requestHeaders, requestHeaders.value("User-Agent", ""), responseHeadersJson);

                for (const auto& item : responseHeadersJson)
                {
                    bool keyExists = std::any_of(responseHeaders.begin(), responseHeaders.end(),
                                                [&](const nlohmann::json& existingHeader) {
                                                    return existingHeader.at("name") == item.at("name");
                                                });

                    if (!keyExists) {
                        responseHeaders.push_back(item);
                    }
                }

                nlohmann::json message = {
                    { "id", 63453 },
                    { "method", "Fetch.fulfillRequest" },
                    { "params", {
                        { "requestId", requestId },
                        { "responseCode", statusCode },
                        { "responseHeaders", responseHeaders },
                        { "body", Base64Encode(originalContent) }
                    }}
                };  

                Sockets::PostGlobal(message);

            }
            else switch (this->IsGetBodyCall(message))
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
        LOG_ERROR("error hooking WebKit -> {}", ex.what());
    }
    catch (const std::exception& ex) 
    {
        LOG_ERROR("error hooking WebKit -> {}", ex.what());
    }
}
