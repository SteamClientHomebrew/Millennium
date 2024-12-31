#include <core/loader.h>
#include <core/ffi/ffi.h>

const void BypassCSP(void)
{
    // This function is used to bypass the Content Security Policy (CSP) of a website.
    // The Content Security Policy (CSP) is a security standard that helps prevent cross-site scripting (XSS), clickjacking, and other code injection attacks resulting from execution of malicious content in the context of trusted web pages.
    // The CSP is implemented by the web server and enforced by the web browser.
    // The CSP is a set of rules that the web server sends to the web browser to tell it what content is allowed to be loaded

    JavaScript::SharedJSMessageEmitter::InstanceRef().OnMessage("msg", "BypassCSP", [&](const nlohmann::json& message, std::string listenerId)
    {
        try
        {
            if (message.contains("id") && message.value("id", -1) == 96876) 
            {
                for (auto& target : message["result"]["targetInfos"])
                {
                    const std::string targetUrl = target["url"].get<std::string>();

                    // make sure the only target none client pages. 
                    if (target["type"] == "page" && targetUrl.find("steamloopback.host") == std::string::npos && targetUrl.find("about:blank?") == std::string::npos)
                    {
                        Sockets::PostGlobal({
                            { "id", 567844 },
                            { "method", "Target.attachToTarget" },
                            { "params", {
                                { "targetId", target["targetId"] },
                                { "flatten", true }
                            }}
                        });

                        // Logger.Log("Bypassing CSP for " + target["url"].get<std::string>());
                    }
                }
            }

            if (message.value("method", std::string()) == "Target.attachedToTarget")
            {
                Sockets::PostGlobal({
                    { "id", 1235377 },
                    { "method", "Page.setBypassCSP" },
                    { "sessionId", message["params"]["sessionId"] },
                    { "params", {
                        { "enabled", true },
                    }}
                });

                // Logger.Log("Sent CSP bypass request");
            }

            if (message.contains("id") && message.value("id", -1) == 1235377)
            {
                // Logger.Log("Successfully bypassed CSP");
                JavaScript::SharedJSMessageEmitter::InstanceRef().RemoveListener("msg", listenerId);
            }
        }
        catch (const nlohmann::detail::exception& e)
        {
            LOG_ERROR("error bypassing CSP -> {}", e.what());
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("error bypassing CSP -> {}", e.what());
        }
    });

    Sockets::PostGlobal({
        { "id", 96876 },
        { "method", "Target.getTargets" }
    });
}