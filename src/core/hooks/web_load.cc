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

const std::string WebkitHandler::PatchDocumentContents(std::string requestUrl, std::string original) 
{
    std::string patched = original;
    const std::string webkitPreloadModule = SystemIO::ReadFileSync("C:\\Users\\Desktop-PC\\Documents\\Development\\plugutil\\api\\dist\\webkit_api.js");

    std::vector<std::string> scriptModules;
    std::string cssShimContent;
    std::string scriptModuleArray;

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

    if (patched.find("<head>") == std::string::npos) 
    {
        return patched;
    }

    return patched.replace(patched.find("<head>"), 6, "<head>" + shimContent);
}

void WebkitHandler::DispatchSocketMessage(nlohmann::basic_json<> message)
{
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
                    if (!std::any_of(responseHeaders.begin(), responseHeaders.end(), [&](const nlohmann::json& existingHeader) {
                        return existingHeader.at("name") == item.at("name");
                    })) {
                        responseHeaders.push_back(item);
                    }
                }

                const std::string patchedContent = this->PatchDocumentContents(requestUrl, originalContent);

                nlohmann::json message = {
                    { "id", 63453 },
                    { "method", "Fetch.fulfillRequest" },
                    { "params", {
                        { "requestId", requestId },
                        { "responseCode", statusCode },
                        { "responseHeaders", responseHeaders },
                        { "body", Base64Encode(patchedContent) }
                    }}
                };  

                Sockets::PostGlobal(message);

            }
            else if (this->IsGetBodyCall(message))
            {
                this->RetrieveRequestFromDisk(message);
            }
        }
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
