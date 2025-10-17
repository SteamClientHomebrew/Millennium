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

/**
 *
 * csp_bypass.h
 *
 * This file is used to bypass the Content Security Policy (CSP) of the browser frame context.
 * The Content Security Policy (CSP) is a security standard that helps prevent cross-site scripting (XSS), click-jacking, and other code injection attacks resulting from execution
 * of malicious content in the context of trusted web pages. The CSP is implemented by the web server and enforced by the web browser. The CSP is a set of rules that the web server
 * sends to the web browser to tell it what content is allowed to be loaded
 *
 * In this context, CSP is used to protect a websites integrity from malicious scripts and code injections, not you (directly).
 * Here, we bypass CSP as a means to unlock the JavaScript VM's full potential, allowing us to run scripts that would otherwise be blocked by the CSP.
 * i.e making requests to external servers, loading scripts from external sources, etc.
 *
 */
#include "millennium/ffi.h"
#include "millennium/init.h"

static const void BypassCSP(void)
{
    CefSocketDispatcher::get().OnMessage("msg", "BypassCSP", [&](const nlohmann::json& message, std::string listenerId)
    {
        try {
            if (message.contains("id") && message.value("id", -1) == 96876) {
                for (auto& target : message["result"]["targetInfos"]) {
                    const std::string targetUrl = target["url"].get<std::string>();

                    // make sure the only target none client pages.
                    if (target["type"] == "page" && targetUrl.find("steamloopback.host") == std::string::npos && targetUrl.find("about:blank?") == std::string::npos) {
                        Sockets::PostGlobal({
                            { "id",     567844                                                      },
                            { "method", "Target.attachToTarget"                                     },
                            { "params", { { "targetId", target["targetId"] }, { "flatten", true } } }
                        });
                    }
                }
            }

            if (message.value("method", std::string()) == "Target.attachedToTarget") {
                Sockets::PostGlobal({
                    { "id",        1235377                        },
                    { "method",    "Page.setBypassCSP"            },
                    { "sessionId", message["params"]["sessionId"] },
                    { "params",
                     {
                          { "enabled", true },
                      }                                           }
                });
            }

            if (message.contains("id") && message.value("id", -1) == 1235377) {
                CefSocketDispatcher::get().RemoveListener("msg", listenerId);
            }
        } catch (const nlohmann::detail::exception& e) {
            LOG_ERROR("error bypassing CSP -> {}", e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("error bypassing CSP -> {}", e.what());
        }
    });

    Sockets::PostGlobal({
        { "id",     96876               },
        { "method", "Target.getTargets" }
    });
}
