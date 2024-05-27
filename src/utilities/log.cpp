#include "log.hpp"
#include <chrono>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <_win32/cmd.h>
#endif

output_console console;

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

	std::cout << get_time() << type << message << "\n";
	*file << get_time() << type << message << "\n";

	(*file).flush();
}

output_console::output_console()
{
#ifdef _WIN32
    if (steam::get().params.has("-dev") && static_cast<bool>(AllocConsole())) {
        void(freopen("CONOUT$", "w", stdout));
        void(freopen("CONOUT$", "w", stderr));
    }
	SetConsoleOutputCP(CP_UTF8);
#endif
	file = std::make_shared<std::ofstream>("output.txt");

    // Check if the file stream is open
    if (!file->is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return;
    }


#ifdef _WIN32
	std::filesystem::path fileName = ".millennium/debug.log";
#elif __linux__
    std::filesystem::path fileName =  fmt::format("{}/.steam/steam/.millennium/debug.log", std::getenv("HOME"));
#endif

    try {
		std::filesystem::create_directories(fileName.parent_path());
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
	}
}
output_console::~output_console() {
	file->close();
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
	std::cout << val << "\n";
	(*file) << get_time() << " [" << pname << "] " << val << "\n";
	(*file).flush();
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

void output_console::head(std::string val) {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
	const auto message = fmt::format("\n[┬] {}", val);

#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
#endif
    std::cout << "\n[┬] ";
#ifdef _WIN32
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
	std::cout << val << "\n";
	*file << message << "\n";
	(*file).flush();
}

 void output_console::log_item(std::string name, std::string data, bool end) {

#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
	std::string connector = end ? "└" : "├";
	const auto message = fmt::format(" {}──[{}]: {}", connector, name, data);

#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
#endif
    std::cout << " " << connector << "──[" <<name << "]: ";
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
	std::cout << data << "\n";

	*file << message << "\n";
	(*file).flush();
 }
