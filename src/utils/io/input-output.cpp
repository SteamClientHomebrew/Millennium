#include <stdafx.h>
#include <windows.h>
#include "input-output.hpp"

namespace file {

    class io_except : public std::exception {
    public:
        io_except(const std::string& message) : msg(message) {}
        virtual const char* what() const noexcept override {
            return msg.c_str();
        }
    private:
        std::string msg;
    };

    nlohmann::json readJsonSync(const std::string& filename) {
        // Open the file
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw io_except("Failed to open file: " + filename);
        }

        // Read the content of the file into a string
        std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        try {
            // Parse the string content as JSON
            nlohmann::json jsonData = nlohmann::json::parse(fileContent);
            return jsonData;
        }
        catch (const std::exception& e) {
            throw io_except("Failed to parse JSON from file: " + filename + ". Error: " + e.what());
        }
    }

    std::string readFileSync(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw io_except("Failed to open file: " + filename);
        }

        std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return fileContent;
    }

    void writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent) {
        console.log(std::format("writing file to: {}", filePath.string()));

        std::ofstream fileStream(filePath, std::ios::binary);
        if (!fileStream)
        {
            console.log(std::format("Failed to open file for writing: {}", filePath.string()));
            return;
        }

        fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

        if (!fileStream)
        {
            console.log(std::format("Error writing to file: {}", filePath.string()));
        }

        fileStream.close();
    }
}