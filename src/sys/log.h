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

class OutputLogger
{
private:
    std::mutex logMutex;
    std::shared_ptr<std::ostream> teeStreamPtr;
    std::shared_ptr<std::ofstream> outputLogStream;

    std::string GetLocalTime();
    void PrintMessage(std::string type, const std::string &message, fmt::v10::text_style color = fg(fmt::color::white));

public:
    OutputLogger(const OutputLogger &) = delete;
    OutputLogger &operator=(const OutputLogger &) = delete;

    OutputLogger();
    ~OutputLogger();

    void LogPluginMessage(std::string pname, std::string val);

    template <typename... Args>
    void Log(std::string fmt, Args &&...args)
    {
        PrintMessage(" [info] ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), fg(fmt::color::light_gray));
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
        PrintMessage(" [warn] ", (sizeof...(args) == 0) ? fmt : fmt::format(fmt, std::forward<Args>(args)...), fg(fmt::color::yellow));
    }

    void LogHead(std::string val, fmt::v10::text_style color = fg(fmt::color::magenta));
    void LogItem(std::string pluginName, std::string data, bool end = false, fmt::v10::text_style color = fg(fmt::color::magenta));
};

extern OutputLogger Logger;

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) Logger.ErrorTrace(fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif