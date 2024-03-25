#pragma once
#include <string>

#include <window/interface/globals.h>
#include <utils/config/config.hpp>
#include <nlohmann/json.hpp>

namespace Community
{
	class installer
	{
	public:
		bool m_downloadInProgess = false;
		std::string m_downloadStatus;

		void installUpdate(const nlohmann::json& skinData);

		const void handleFileDrop(const char* _filePath);
		const void handleThemeInstall(std::string fileName, std::string downloadPath, std::function<void(std::string)> cb);
	};

	static installer* Themes = new installer;
}