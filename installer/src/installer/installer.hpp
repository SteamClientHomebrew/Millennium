#pragma once
#include <string>
#include <windows.h>
#include <nlohmann/json.hpp>

class Installer {
private:
	bool downloadFile(std::string fileUrl);
public:
	float fileProgress;

	float downloadIteration;
	int totalLength;
	float buffer;
	nlohmann::json response;

	std::string currentFileName;

	void installMillennium(bool silent);
	void uninstallMillennium();
	void resetMillennium();
};

static Installer installer;