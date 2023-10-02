#pragma once
#include <string>

#include <window/interface/globals.h>
#include <utils/config/config.hpp>
#include <nlohmann/json.hpp>

namespace community
{
	class installer
	{
	private:
		void downloadFolder(const nlohmann::json& folderData, const std::filesystem::path& currentDir);

		void writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent);

	public:
		bool m_downloadInProgess = false;
		std::string m_downloadStatus;

		std::string sanitizeDirectoryName(const std::string& input);

		void downloadTheme(const nlohmann::json& skinData);
	};

	static installer* _installer = new installer;
}