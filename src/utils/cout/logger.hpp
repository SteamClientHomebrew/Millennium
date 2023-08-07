#pragma once
#include <fstream>
#include <vector>
#include <windows.h>
#include <mutex> 

extern std::vector<std::string> output_log;
extern std::vector<std::string> socket_log;

/// <summary>
/// incredibly shitty logger class
/// </summary>
class output_console {
private:
    std::ofstream fileStream;
    std::mutex logMutex;

    std::string get_time();
    void log(std::string type, const std::string& message);

public:
    HANDLE handle;

    output_console();
    ~output_console();

    void log(std::string val);
    void err(std::string val);
    void warn(std::string val);
    void succ(std::string val);
    void imp(std::string val);

    void log_patch(std::string type, std::string what_patched, std::string regex);

    void log_socket(std::string val);
};
static output_console console;