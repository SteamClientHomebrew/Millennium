#pragma once
#include <string>
#include <windows.h>

class Installer {
private:
	bool downloadFile(std::string fileUrl);
public:
	float fileProgress;

	int downloadIteration;
	int totalLength;

	std::string currentFileName;

	void installMillennium();
	void uninstallMillennium();
	void resetMillennium();
};

static Installer installer;