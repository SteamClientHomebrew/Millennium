/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "web_load.h"
#include <nlohmann/json.hpp>
#include "loader.h"
#include "ffi.h"
#include "encoding.h"
#include "http.h"
#include <unordered_set>
#include "csp_bypass.h"
#include "url_parser.h"

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
                /** Maintain backwards compatibility for themes that explicitly rely on this url */
                { { "urlPattern", fmt::format("{}*", this->m_oldHookAddress      ) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_javaScriptVirtualUrl) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_styleSheetVirtualUrl) }, { "requestStage", "Request" } }
            }
        }}}
    });
}

bool WebkitHandler::IsGetBodyCall(nlohmann::basic_json<> message) 
{
    std::string requestUrl = message["params"]["request"]["url"].get<std::string>();

    return requestUrl.find(this->m_javaScriptVirtualUrl) != std::string::npos
        || requestUrl.find(this->m_styleSheetVirtualUrl) != std::string::npos
        || requestUrl.find(this->m_oldHookAddress)       != std::string::npos;
}

std::filesystem::path WebkitHandler::ConvertToLoopBack(std::string requestUrl)
{
    size_t jsPos  = requestUrl.find(this->m_javaScriptVirtualUrl);
    size_t cssPos = requestUrl.find(this->m_styleSheetVirtualUrl);
    size_t oldPos = requestUrl.find(this->m_oldHookAddress);

    if (jsPos != std::string::npos)
    {
        requestUrl.erase(jsPos, std::string(this->m_javaScriptVirtualUrl).length());
    }
    else if (cssPos != std::string::npos)
    {
        requestUrl.erase(cssPos, std::string(this->m_styleSheetVirtualUrl).length());
    }
    else if (oldPos != std::string::npos)
    {
        requestUrl.erase(oldPos, std::string(this->m_oldHookAddress).length());
    }

    return std::filesystem::path(PathFromUrl(requestUrl));
}

void WebkitHandler::RetrieveRequestFromDisk(nlohmann::basic_json<> message)
{
    std::filesystem::path localFilePath = this->ConvertToLoopBack(message["params"]["request"]["url"]);
    std::ifstream localFileStream(localFilePath);

    bool bFailedRead = !localFileStream.is_open();

    if (bFailedRead)
    {
        LOG_ERROR("failed to retrieve file '{}' info from disk.", localFilePath.string());
    }

    const std::string fileContent((std::istreambuf_iterator<char>(localFileStream)), std::istreambuf_iterator<char>());

    const int responseCode            = bFailedRead ? 404 : 200;
    const std::string responseMessage = bFailedRead ? "millennium" : "millennium couldn't read " + localFilePath.string();

    // Get file extension from the file path.
    const std::string fileExtension = localFilePath.extension().string();
    const std::string contentType = fileExtension == ".css" ? "text/css" : "application/javascript";

    const nlohmann::json responseHeaders = nlohmann::json::array
    ({
        { {"name", "Access-Control-Allow-Origin"}, {"value", "*"} },
        { {"name", "Content-Type"}, {"value", contentType} }
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

            cssShimContent.append(fmt::format("<link rel=\"stylesheet\" href=\"{}\">\n", UrlFromPath("https://css.millennium.app/", hookItem.path))); 
        }
        else if (hookItem.type == TagTypes::JAVASCRIPT) 
        {
            if (!std::regex_match(requestUrl, hookItem.urlPattern)) 
                continue;

            scriptModules.push_back(UrlFromPath(this->m_javaScriptVirtualUrl, hookItem.path));
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
