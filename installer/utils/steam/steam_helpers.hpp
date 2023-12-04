#pragma once
#include <string>

class Steam {
public:
	static std::string getInstallPath();
	static bool isRunning();
	static void Start();
	static void terminateProcess();
};