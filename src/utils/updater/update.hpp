#pragma once
#include <string>

class updater
{
public:
	static void start_update(std::string url);

	static void check_for_updates();
};
