#pragma once
#include <fstream>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <mutex>
#include <fmt/core.h>
#include <fmt/color.h>
#include <memory>
#include <iostream>

#define DEFAULT_ACCENT_COL fg(fmt::color::light_sky_blue)

#define COL_RED "\033[31m"
#define COL_GREEN "\033[32m"
#define COL_YELLOW "\033[33m"
#define COL_BLUE "\033[34m"
#define COL_MAGENTA "\033[35m"
#define COL_CYAN "\033[36m"
#define COL_WHITE "\033[37m"
#define COL_RESET "\033[0m"

class OutputLogger
{
private:
#ifdef _WIN32
    bool m_bIsVersbose = false;
#elif __linux__
    bool m_bIsVersbose = true;
#endif
    bool m_bIsConsoleEnabled = false;
    std::mutex logMutex;
    std::shared_ptr<std::ostream> teeStreamPtr;
    std::shared_ptr<std::ofstream> outputLogStream;
    std::string GetLocalTime();

public:
    void PrintMessage(std::string type, const std::string &message, std::string color = COL_WHITE);

    OutputLogger(const OutputLogger &) = delete;
    OutputLogger &operator=(const OutputLogger &) = delete;

    OutputLogger();
    ~OutputLogger();

    void LogPluginMessage(std::string pname, std::string val);

    template <typename... Args>
    void Log(std::string fmt, Args &&...args)
    {
        PrintMessage(" INFO ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_GREEN);
    }

    template <typename... Args>
    void ErrorTrace(std::string fmt, const char* file, int line, const char* function, Args &&...args)
    {
        this->LogHead((sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), fg(fmt::color::red));
        this->LogItem("function", function, false, fg(fmt::color::red));
        this->LogItem("file",  file, false, fg(fmt::color::red));
        this->LogItem("line", std::to_string(line), true, fg(fmt::color::red));
    }

    template <typename... Args>
    void Warn(std::string fmt, Args &&...args)
    {
        PrintMessage(" WARN ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), COL_YELLOW);
    }

    void LogHead(std::string val, fmt::text_style color = fg(fmt::color::magenta));
    void LogItem(std::string pluginName, std::string data, bool end = false, fmt::text_style color = fg(fmt::color::magenta));
};

extern OutputLogger Logger;

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) Logger.ErrorTrace(fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif