#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>

namespace stream_buffer 
{
    namespace plugin_mgr 
    {
        struct plugin_t 
        {
            std::filesystem::path base_dir;
            std::filesystem::path backend_abs;
            std::string name;
            std::string frontend_abs;
            nlohmann::basic_json<> pjson;
        };

        std::vector<std::string> get_enabled();
        bool is_enabled(std::string plugin_name);

        bool set_plugin_status(std::string pname, bool enabled);
        std::vector<plugin_t> parse_all();
    };

    void set_config(std::string file_data);
    void setup_config();

    std::filesystem::path steam_path();

    namespace file 
    {
        class io_except : public std::exception {
        public:
            io_except(const std::string& message) : msg(message) {}
            virtual const char* what() const noexcept override {
                return msg.c_str();
            }
        private:
            std::string msg;
        };

        nlohmann::json readJsonSync(const std::string& filename);
        std::string readFileSync(const std::string& filename);
        void writeFileSync(const std::filesystem::path& filePath, std::string content);
        void writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent);
    } 
}