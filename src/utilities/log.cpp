#include "log.hpp"

#include <chrono>
#include <string>
#include <iostream>

#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <sstream>

std::vector<std::string> output_log = {};
std::vector<std::string> socket_log = {};

std::string output_console::get_time()
{
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::stringstream ss;
	ss << std::put_time(std::localtime(&time), "%H:%M:%S");
	ss << fmt::format(".{:03}", ms.count());
	return fmt::format("[{}]", ss.str());
}

void output_console::_print(std::string type, const std::string& message)
{
	std::lock_guard<std::mutex> lock(logMutex);

	output_log.push_back(fmt::format("{}{}{}", get_time(), type, message));

    // segmentation fault on linux? not sure why
#ifdef _WIN32
	fileStream << get_time() << type << message << std::endl;
#endif

	std::cout << get_time() << type << message << std::endl;
}

output_console::output_console() 
{
#ifdef _WIN32
	std::filesystem::path fileName = ".millennium/logs/millennium.log";
#elif __linux__
    std::filesystem::path fileName =  fmt::format("{}/.steam/steam/.millennium/logs/millennium.log", std::getenv("HOME"));
#endif

    try {
		std::filesystem::create_directories(fileName.parent_path());
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
	}

	fileStream.open(fileName.string(), std::ios::trunc);
}
output_console::~output_console() {
	fileStream.close();
}

void output_console::log(std::string val) {
	_print(" [info] ", val);
}

void output_console::py(std::string pname, std::string val)
{
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
#endif
	std::cout << get_time() << " [" << pname << "] ";
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
	std::cout << val << std::endl;
}

void output_console::warn(std::string val)
{
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
#endif
	_print(" [warn] ", val);
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}

void output_console::err(std::string val)
{
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
#endif
	_print(" [error] ", val);
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}

template<typename... Args>
void output_console::warn(std::string fmt, Args&&... args)
{
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
#endif
	_print(" [warn] ", fmt::format(fmt, std::forward<Args>(args)...));
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}