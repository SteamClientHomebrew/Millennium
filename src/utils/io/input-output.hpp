#include <string>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace file 
{
	nlohmann::json readJsonSync(const std::string& filename);
	std::string readFileSync(const std::string& filename);
	void writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent);
}