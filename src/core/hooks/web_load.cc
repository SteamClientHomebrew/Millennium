#include "web_load.h"
#include <nlohmann/json.hpp>
#include <core/loader.h>
#include <core/ffi/ffi.h>
#include <sys/encoding.h>
#include <sys/http.h>   
#include <unordered_set>
#include "csp_bypass.h"

unsigned long long g_hookedModuleId;

// These URLS are blacklisted from being hooked, to prevent potential security issues.
static const std::vector<std::string> g_blackListedUrls = {
    "https://checkout\\.steampowered\\.com/.*"
};

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
                { { "urlPattern", "*" }, { "resourceType", "Document" }, { "requestStage", "Response" } },     
                { { "urlPattern", fmt::format("{}*", this->m_javaScriptVirtualUrl) }, { "requestStage", "Request" } }
            }
        }}}
    });
}

bool WebkitHandler::IsGetBodyCall(nlohmann::basic_json<> message) 
{
    return message["params"]["request"]["url"].get<std::string>().find(this->m_javaScriptVirtualUrl) != std::string::npos;
}

std::filesystem::path WebkitHandler::ConvertToLoopBack(std::string requestUrl)
{
    std::size_t pos = requestUrl.find(this->m_javaScriptVirtualUrl);

    if (pos != std::string::npos)
    {
        requestUrl.erase(pos, std::string(this->m_javaScriptVirtualUrl).length());
    }

    return SystemIO::GetSteamPath() / requestUrl;
}

void WebkitHandler::RetrieveRequestFromDisk(nlohmann::basic_json<> message)
{
    std::filesystem::path localFilePath = this->ConvertToLoopBack(message["params"]["request"]["url"]);
    std::ifstream localFileStream(localFilePath);

    bool bFailedRead = !localFileStream.is_open();

    if (bFailedRead)
    {
        LOG_ERROR("failed to retrieve file info from disk.");
    }

    const std::string fileContent((std::istreambuf_iterator<char>(localFileStream)), std::istreambuf_iterator<char>());

    const int responseCode            = bFailedRead ? 404 : 200;
    const std::string responseMessage = bFailedRead ? "millennium" : "millennium couldn't read " + localFilePath.string();

    const nlohmann::json responseHeaders = nlohmann::json::array
    ({
        { {"name", "Access-Control-Allow-Origin"}, {"value", "*"} },
        { {"name", "Content-Type"}, {"value", "application/javascript"} }
    });

    Sockets::PostGlobal({
        { "id", 63453 },
        { "method", "Fetch.fulfillRequest" },
        { "params", {
            { "responseCode", responseCode },
            { "requestId", message["params"]["requestId"] },
            { "responseHeaders", responseHeaders },
            { "responsePhrase", responseMessage },
            { "body", Base64Encode(fileContent) }
        }}
    });
}

void WebkitHandler::GetResponseBody(nlohmann::basic_json<> message)
{
    const RedirectType statusCode = message["params"]["responseStatusCode"].get<RedirectType>();

    // If the status code is a redirect, we just continue the request. 
    if (statusCode == REDIRECT || statusCode == MOVED_PERMANENTLY || statusCode == FOUND || statusCode == TEMPORARY_REDIRECT || statusCode == PERMANENT_REDIRECT)
    {
        Sockets::PostGlobal({
            { "id", 0 },
            { "method", "Fetch.continueRequest" },
            { "params", { { "requestId", message["params"]["requestId"] } }}
        });
    }
    else
    {
        hookMessageId -= 1;
        m_requestMap->push_back({ hookMessageId, message["params"]["requestId"], message["params"]["resourceType"], message });

        Sockets::PostGlobal({
            { "id", hookMessageId },
            { "method", "Fetch.getResponseBody" },
            { "params", { { "requestId", message["params"]["requestId"] } }}
        });
    }
}

const std::string WebkitHandler::PatchDocumentContents(std::string requestUrl, std::string original) 
{
    std::string patched = original;
    const std::string webkitPreloadModule = SystemIO::ReadFileSync((SystemIO::GetInstallPath() / "ext" / "data" / "shims" / "webkit_api.js").string());

    if (webkitPreloadModule.empty()) 
    {
        LOG_ERROR("Missing webkit preload module. Please re-install Millennium.");
        #ifdef _WIN32
        MessageBoxA(NULL, "Missing webkit preload module. Please re-install Millennium.", "Millennium", MB_ICONERROR);
        #endif
        return patched;
    }

    std::vector<std::string> scriptModules;
    std::string cssShimContent, scriptModuleArray;

    for (auto& hookItem : *m_hookListPtr) 
    {
        if (hookItem.type == TagTypes::STYLESHEET) 
        {
            if (!std::regex_match(requestUrl, hookItem.urlPattern)) 
                continue;

            std::filesystem::path relativePath = std::filesystem::relative(hookItem.path, SystemIO::GetSteamPath() / "steamui");
            cssShimContent.append(fmt::format("<link rel=\"stylesheet\" href=\"{}{}\">\n", this->m_steamLoopback, relativePath.generic_string())); 
        }
        else if (hookItem.type == TagTypes::JAVASCRIPT) 
        {
            if (!std::regex_match(requestUrl, hookItem.urlPattern)) 
                continue;
            
            std::filesystem::path relativePath = std::filesystem::relative(hookItem.path, SystemIO::GetSteamPath());
            scriptModules.push_back(fmt::format("{}{}", this->m_javaScriptVirtualUrl, relativePath.generic_string()));
        }
    }

    for (int i = 0; i < scriptModules.size(); i++)
    {
        scriptModuleArray.append(fmt::format("\"{}\"{}", scriptModules[i], (i == scriptModules.size() - 1 ? "" : ",")));
    }

    std::string shimContent = fmt::format("<script type=\"module\" id=\"millennium-injected\" defer>{}millennium_components({}, [{}])\n</script>\n{}", webkitPreloadModule, m_ipcPort, scriptModuleArray, cssShimContent);

    for (const auto& blackListedUrl : g_blackListedUrls)        
    {
        if (std::regex_match(requestUrl, std::regex(blackListedUrl))) 
        {
            shimContent = cssShimContent; // Remove all queried JavaScript from the page. 
        }
    }

    if (patched.find("<head>") == std::string::npos) 
    {
        return patched;
    }

    return patched.replace(patched.find("<head>"), 6, "<head>" + shimContent);
}

void WebkitHandler::HandleHooks(nlohmann::basic_json<> message)
{
    for (auto requestIterator = m_requestMap->begin(); requestIterator != m_requestMap->end();)
    {
        try 
        {
            auto [id, requestId, type, response] = (*requestIterator);

            if (message["id"] != id || (message["id"] == id && !message["result"]["base64Encoded"]))
            {
                requestIterator++;
                continue;
            }

            const std::string patchedContent = this->PatchDocumentContents(response["params"]["request"]["url"], Base64Decode(message["result"]["body"]));

            BypassCSP();

            const int responseCode = response["params"].value("responseStatusCode", 200);
            const std::string responseMessage = response["params"].value("responseStatusText", "OK");

            Sockets::PostGlobal({
                { "id", 63453 },
                { "method", "Fetch.fulfillRequest" },
                { "params", {
                    { "requestId", requestId },
                    { "responseCode", responseCode },
                    { "responseHeaders", response["params"]["responseHeaders"] },
                    { "responsePhrase", responseMessage.length() > 0 ? responseMessage : "OK" },
                    { "body", Base64Encode(patchedContent) }
                }}
            });

            requestIterator = m_requestMap->erase(requestIterator);
        }
        catch (const nlohmann::detail::exception& ex) 
        {
            LOG_ERROR("error hooking WebKit -> {}\n\n{}", ex.what(), message.dump(4));
            requestIterator = m_requestMap->erase(requestIterator);
        }
        catch (const std::exception& ex) 
        {
            LOG_ERROR("error hooking WebKit -> {}\n\n{}", ex.what(), message.dump(4));
            requestIterator = m_requestMap->erase(requestIterator);
        }
    }
}

void WebkitHandler::DispatchSocketMessage(nlohmann::basic_json<> message)
{
    static std::chrono::time_point lastExceptionTime = std::chrono::system_clock::now();

    try 
    {
        if (message["method"] == "Fetch.requestPaused")
        {
            switch (this->IsGetBodyCall(message))
            {
                case true:  { this->RetrieveRequestFromDisk(message); break; }
                case false: { this->GetResponseBody(message);         break; }
            }
        }

        this->HandleHooks(message);
    }
    catch (const nlohmann::detail::exception& ex) 
    {
        if (std::chrono::system_clock::now() - lastExceptionTime > std::chrono::seconds(5)) 
        {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
            lastExceptionTime = std::chrono::system_clock::now();
        }
    }
    catch (const std::exception& ex) 
    {
        if (std::chrono::system_clock::now() - lastExceptionTime > std::chrono::seconds(5)) 
        {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
            lastExceptionTime = std::chrono::system_clock::now();
        }
    }
}
