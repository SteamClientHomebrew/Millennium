#pragma once
#include <fstream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include <mutex> 
#include <fmt/core.h>

/// <summary>
/// incredibly shitty logger class
/// </summary>
class output_console {
private:
    std::ofstream fileStream;
    std::mutex logMutex;

    std::string get_time();
    void _print(std::string type, const std::string& message);

public:
    bool consoleAllocated;

    output_console();
    ~output_console();

    void py(std::string pname, std::string val);

    template<typename... Args>
    void log(std::string fmt, Args&&... args);
    void log(std::string val);

    template<typename... Args>
    void err(std::string fmt, Args&&... args);
    void err(std::string val);

    template<typename... Args>
    void warn(std::string fmt, Args&&... args);
    void warn(std::string val);

    void head(std::string val);
    void log_item(std::string name, std::string data, bool end=false);
};
static output_console console;

template<typename... Args>
void output_console::log(std::string fmt, Args&&... args) {
	_print(" [info] ", fmt::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void output_console::err(std::string fmt, Args&&... args)
{
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
#endif
	_print(" [error] ", fmt::format(fmt, std::forward<Args>(args)...));
#ifdef _WIN32
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}
