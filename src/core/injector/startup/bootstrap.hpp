#pragma once

#include <window/api/installer.hpp>
#include <window/interface/globals.h>
#include <format>

#include <stdafx.h>

class bootstrapper
{
public:

	const bool parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer, bool _checkForUpdates);

	const void init(bool setCommit = false, std::string newCommit = "");

private:
};

static unsigned long __stdcall warm_boot(void*) {
	
	std::unique_ptr<bootstrapper> bootstrapper_module = std::make_unique<bootstrapper>();

	bootstrapper_module->init();

	return 0;
}