#ifndef JSON_PARSER
#define JSON_PARSER

namespace json
{
    nlohmann::json read_json_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        nlohmann::json data;
        file >> data;
        file.close();
        return data;
    }

    void write_json_file(const std::string& filename, const nlohmann::json& data) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        file << data.dump(4);
        file.close();
    }
}

#endif