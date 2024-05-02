#include "web_load.h"
#include <nlohmann/json.hpp>
#include <core/loader.hpp>
#include <core/impl/mutex_impl.hpp>
#include <utilities/encoding.h>
#include <boxer/boxer.h>

webkit_handler webkit_handler::get() {
    static webkit_handler _w;
    return _w;
}

void webkit_handler::setup_hook() 
{
    tunnel::post_global({
        { "id", 3242 }, { "method", "Fetch.enable" },
        { "params", {
            { "patterns", {
                // hook a global css module from all steam pages
                { { "urlPattern", "https://*.*.com/public/shared/css/buttons.css*" }, { "resourceType", "Stylesheet" }, { "requestStage", "Response" }  },
                // hook a global js module from all steam pages
                { { "urlPattern", "https://*.*.com/public/shared/javascript/shared_global.js*" }, { "resourceType", "Script" }, { "requestStage", "Response" } },
                { 
                    // create a virtual ftp table from a default accept XSS url
                    { "urlPattern", fmt::format("{}*", this->virt_url) }, { "requestStage", "Request" } 
                }
            }
        }}}
    });
}

bool webkit_handler::is_ftp_call(nlohmann::basic_json<> message) {
    return message["params"]["request"]["url"].get<std::string>()
        .find(this->virt_url) != std::string::npos;
}

std::string webkit_handler::js_hook(std::string body) 
{
    std::string inject;
    for (auto& item : *h_list_ptr) 
    {
        if (item.type != type_t::JAVASCRIPT) {
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(item.path, stream_buffer::steam_path() / "steamui");
        inject.append(fmt::format(
            "document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}{}', type: 'module', id: 'millennium-injected' }}));\n", 
            this->virt_url, relativePath.generic_string()
        ));
    }
    return inject + body;
}

std::string webkit_handler::css_hook(std::string body) 
{
    std::string inject;
    for (auto& item : *h_list_ptr) 
    {
        if (item.type != type_t::STYLESHEET) {
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(item.path, stream_buffer::steam_path() / "steamui");
        inject.append(fmt::format("@import \"{}{}\";\n", this->looback, relativePath.generic_string()));
    }

    return inject + body;
}

void webkit_handler::handle_hook(nlohmann::basic_json<> message) 
{
    try {
        static short id = -69;

        if (message["method"] == "Fetch.requestPaused" && !this->is_ftp_call(message)) 
        {
            id--;
            (*request_map).push_back({
                id, message["params"]["requestId"].get<std::string>(), message["params"]["resourceType"].get<std::string>()
            });

            tunnel::post_global({
                { "id", id },
                { "method", "Fetch.getResponseBody" },
                { "params", {
                    { "requestId", message["params"]["requestId"] }
                }}
            });
        }

        else if (message["method"] == "Fetch.requestPaused" && this->is_ftp_call(message))  {

            std::string requestUrl = message["params"]["request"]["url"];
            std::size_t pos = requestUrl.find(this->virt_url);

            if (pos != std::string::npos) {
                requestUrl.erase(pos, std::string(this->virt_url).length());  
            }

            const auto path = stream_buffer::steam_path() / "steamui" / requestUrl;
            bool failed = false;
 
            std::ifstream file(path.string());
            if (!file.is_open()) {
                console.err("failed to open file [readJsonSync]");
                failed = true;
            }

            std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            auto status = failed ? "millennium couldn't read " + path.generic_string() : "MILLENNIUM-VIRTUAL";

            tunnel::post_global({
                { "id", 63453 },
                { "method", "Fetch.fulfillRequest" },
                { "params", {

                    { "requestId", message["params"]["requestId"] },
                    { "responseCode", failed ? 404 : 200 },
                    { "responsePhrase", status }, 

                    { "responseHeaders", nlohmann::json::array({
                        { {"name", "Access-Control-Allow-Origin"}, {"value", "*"} },
                        { {"name", "Content-Type"}, {"value", "application/javascript"} }
                    }) },
                    { "body", base64_encode(fileContent) }
                }}
            });
        }

        // create a temporary scope so the mutex is free'd
        {
            // Lock the mutex before accessing request_map
            // std::lock_guard<std::mutex> lock(request_map_mutex);

            for (auto it = request_map->begin(); it != request_map->end();) 
            {
                auto [id, request_id, type] = (*it);

                if (message["id"] != id || !message["result"]["base64Encoded"]) {
                    it++; continue;
                }

                std::string body = {};
                if      (type == "Script"    ) body = this->js_hook (base64_decode(message["result"]["body"]));
                else if (type == "Stylesheet") body = this->css_hook(base64_decode(message["result"]["body"]));
                
                console.log("responding to -> {} + {} [{} bytes]", request_id, type, body.size());

                tunnel::post_global({
                    { "id", 63453 },
                    { "method", "Fetch.fulfillRequest" },
                    { "params", {
                        { "requestId", request_id },
                        { "responseCode", 200 },
                        { "body", base64_encode(body) }
                    }}
                });

                // done using it, remove it from memory
                it = request_map->erase(it);
            }
        }
    }
    catch (const nlohmann::detail::exception& ex) {
        console.err("error hooking webkit -> {}", ex.what());
    }
    catch (const std::exception& ex) {
        console.err("error hooking webkit -> {}", ex.what());
    }
}
