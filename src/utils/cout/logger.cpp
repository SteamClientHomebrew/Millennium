#include <utils/cout/logger.hpp>

#include <chrono>
#include <string>
#include <iostream>

#include <filesystem>
#include <fmt/color.h>

//std::vector<std::string> output_log = {};
std::vector<std::string> socket_log = {};

std::string output_console::get_time()
{
	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	std::stringstream ss;
	ss << std::put_time(&*std::localtime(&time), "%H:%M:%S");
	return fmt::format("[{}]", ss.str());
}

void output_console::log(std::string type, const std::string& message)
{
	std::lock_guard<std::mutex> lock(logMutex);

	//output_log.push_back(fmt::format("{}{}{}", get_time(), type, message));

    // segmentation fault on linux? not sure why
#ifdef _WIN32
	fileStream << get_time() << type << message << std::endl;
#endif


	std::cout << get_time() << type << message << std::endl;
}

void output_console::log_socket(std::string val) {
	socket_log.push_back(fmt::format("{} {}", get_time(), val));
}

output_console::output_console() 
{
	std::filesystem::path fileName = ".millennium/logs/millennium.log";

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
	log(" [info] ", val);
}

void output_console::log_hook(std::string val) {
	log(" [hook] ", val);
}
void output_console::err(std::string val) {

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
#endif

	log(" [fail] ", val);

#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}
void output_console::warn(std::string val) {

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
#endif
	log(" [warn] ", val);
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}
void output_console::succ(std::string val) {
	log(" [okay] ", val);
}
void output_console::imp(std::string val) {
	log(" [+] ", val);
}

void output_console::log_patch(std::string type, std::string what_patched, std::string regex) 
{
	std::lock_guard<std::mutex> lock(logMutex);

	const auto message = fmt::format(" [{}] match -> [windowName: '{}'][Regex: '{}']", type, what_patched, regex);

	//output_log.push_back(message);
	fileStream << get_time() << message << std::endl;
	std::cout << get_time() << message << std::endl;
}