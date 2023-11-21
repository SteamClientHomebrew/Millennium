#include <window/api/installer.hpp>
#include <stdafx.h>

namespace community
{
	std::string installer::sanitizeDirectoryName(const std::string& input) {
		return std::regex_replace(input, std::regex("[^a-zA-Z0-9-_.~]"), "");
	}

	void installer::downloadFolder(const nlohmann::json& folderData, const std::filesystem::path& currentDir)
	{
		std::filesystem::create_directories(currentDir);

		for (const auto& item : folderData)
		{
			try {
				if (item["type"] == "dir")
				{
					std::filesystem::path dirPath = currentDir / item["name"];
					std::filesystem::create_directories(dirPath);

					downloadFolder(nlohmann::json::parse(http::get(item["url"])), dirPath);
				}
				else if (item["type"] == "file" && !item["download_url"].is_null())
				{
					const std::string fileName = item["name"].get<std::string>();
					const std::filesystem::path filePath = currentDir / fileName;

					m_downloadStatus = std::format("downloading {}", fileName);
					this->writeFileBytesSync(filePath, http::get_bytes(item["download_url"].get<std::string>().c_str()));
				}
			}
			catch (const nlohmann::detail::exception& excep) {
				if (folderData.contains("message")) {
					const auto message = folderData["message"].get<std::string>();
					MsgBox("Github seems to have rate limited you from downloading to many themes in a short period of time."
						"Try again later (maximum 1 hour)", "GitHub API", MB_ICONERROR);
					return;
				}

				console.err(std::format("Error downloading/updating a skin. message: {}", excep.what()));
				MsgBox(std::format("Error downloading/updating a skin. message: {}", excep.what()).c_str(), "Error", MB_ICONERROR);
			}
		}
	}

	void installer::writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent)
	{
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

	void installer::downloadTheme(const nlohmann::json& skinData) {
		std::filesystem::path skinDir = std::filesystem::path(config.getSkinDir()) / sanitizeDirectoryName(skinData["name"].get<std::string>());

		try {
			nlohmann::json folderData = nlohmann::json::parse(http::get(skinData["source"]));
			downloadFolder(folderData, skinDir);
		}
		catch (const http_error& exception) {
			console.log(std::format("download theme exception: {}", exception.what()));
		}
	}
}