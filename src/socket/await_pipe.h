#include <utilities/http.h>
#include <thread>
#include <nlohmann/json.hpp>

const std::string get_steam_context() {
    nlohmann::basic_json<> instance = nlohmann::json::parse(http_get("http://localhost:8080/json/version"));

    return instance["webSocketDebuggerUrl"];
}

const std::string get_shared_js() {
    std::string context;

    while (context.empty()) {
        nlohmann::basic_json<> instances = nlohmann::json::parse(http_get("http://localhost:8080/json"));

        for (const auto& instance : instances) {
            if (instance["title"] == "SharedJSContext") {
                context = instance["webSocketDebuggerUrl"].get<std::string>();
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return context;
}