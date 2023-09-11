#pragma once
#include <string>

class Installer {
private:
	bool downloadFile(std::string fileUrl, std::string filePath, size_t expectedLen);
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