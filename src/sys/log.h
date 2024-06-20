#pragma once
#include <fstream>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <mutex>
#include <fmt/core.h>
#include <memory>

#define COLOR_WHITE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE

class OutputLogger
{
private:
    #ifdef _WIN32
    HANDLE hConsole{};
    #endif
    std::mutex logMutex;
    std::shared_ptr<std::ostream> teeStreamPtr;
    std::shared_ptr<std::ofstream> outputLogStream;

    std::string GetLocalTime();
    void PrintMessage(std::string type, const std::string &message);

public:
    OutputLogger(const OutputLogger &) = delete;
    OutputLogger &operator=(const OutputLogger &) = delete;

    OutputLogger();
    ~OutputLogger();

    void LogPluginMessage(std::string pname, std::string val);

    template <typename... Args>
    void Log(std::string fmt, Args &&...args)
    {
        PrintMessage(" [info] ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void ErrorTrace(std::string fmt, const char* file, int line, const char* function, Args &&...args)
    {
        #ifdef _WIN32
        {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
        }
        #endif

        this->LogHead((sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), false);
        this->LogItem("function", function, false, false);
        this->LogItem("file",  file, false, false);
        this->LogItem("line", std::to_string(line), true, false);
        //PrintMessage(fmt::format(" [error] [FUNCTION:{},FILE:{},L:{}] ", function, file, line), (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...));
        #ifdef _WIN32
        {
            SetConsoleTextAttribute(hConsole, COLOR_WHITE);
        }
        #endif
    }

    template <typename... Args>
    void Warn(std::string fmt, Args &&...args)
    {
        #ifdef _WIN32
        {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
        }
        #endif
        PrintMessage(" [warn] ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...));
        #ifdef _WIN32
        {
            SetConsoleTextAttribute(hConsole, COLOR_WHITE);
        }
        #endif
    }

    void LogHead(std::string val, bool useColor = true);
    void LogItem(std::string pluginName, std::string data, bool end = false, bool useColor = true);
};

extern OutputLogger Logger;

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) Logger.ErrorTrace(fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif