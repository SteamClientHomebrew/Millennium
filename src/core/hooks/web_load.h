#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <mutex>

static unsigned long long hook_tag;
static std::mutex request_map_mutex;

class webkit_handler {
private:
    // must share the same base url, or be whitelisted.
    const char* virt_url = "https://s.ytimg.com/millennium-virtual/";
    const char* looback = "https://steamloopback.host/";

    bool is_ftp_call(nlohmann::basic_json<> message);

    std::string css_hook(std::string body);
    std::string js_hook(std::string body);

    struct web_hook_t {
        short id;
        std::string request_id;
        std::string type;
    };

    std::shared_ptr<std::vector<web_hook_t>> request_map = std::make_shared<std::vector<web_hook_t>>();

public:
	static webkit_handler get();

    enum type_t {
        STYLESHEET, 
        JAVASCRIPT
    };

    struct shook_t {
        std::string path;
        type_t type;
        unsigned long long id;
    };

    std::shared_ptr<std::vector<shook_t>> h_list_ptr = std::make_shared<std::vector<shook_t>>();

    void handle_hook(nlohmann::basic_json<> message);
    void setup_hook();
};