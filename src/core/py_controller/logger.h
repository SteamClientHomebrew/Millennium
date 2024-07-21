#pragma once
#include <Python.h>
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <fmt/core.h>
#include <fmt/color.h>
#include <sys/locals.h>

class BackendLogger 
{
private:
    std::ofstream file;
    std::string filename;
    std::string pluginName;

    std::string GetLocalDateStr()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm = *std::localtime(&now_c);

        int year = local_tm.tm_year + 1900;
        int month = local_tm.tm_mon + 1;
        int day = local_tm.tm_mday;
        int hour = local_tm.tm_hour;
        int min = local_tm.tm_min;
        int sec = local_tm.tm_sec;

        return fmt::format("[{}-{}-{} @ {}:{}:{}]", year, month, day, hour, min, sec);
    }

public:
    BackendLogger(const std::string& pluginName) : pluginName(pluginName)
    {
        this->filename = (SystemIO::GetSteamPath() / "ext" / "data" / "logs" / fmt::format("{}_log.txt", pluginName)).generic_string();
        file.open(filename, std::ios::app);

        file << fmt::format("\n\n\n--------------------------------- [{}] ---------------------------------\n", GetLocalDateStr());
        file.flush();
    }

    void Log(const std::string& message) 
    {        
        std::string formatted = fmt::format("({}) ", pluginName);
  
        fmt::print(fg(fmt::color::light_sky_blue), formatted);
        fmt::print(fmt::format("{}\n", message));
        file << formatted << message << "\n";
        file.flush();
    }

    void Warn(const std::string& message) 
    {
        std::string formatted = fmt::format("{}{}\n", fmt::format(" ({}) ", pluginName), message);

        fmt::print(fg(fmt::color::orange), formatted, "\n");
        file << formatted;
        file.flush();
    }

    void Error(const std::string& message) 
    {
        std::string formatted = fmt::format("{}{}\n", fmt::format(" ({}) ", pluginName), message);

        fmt::print(fg(fmt::color::red), formatted, "\n");
        file << formatted;
    }
};

typedef struct 
{
    PyObject ob_base;
    char *prefix;
    std::unique_ptr<BackendLogger> m_loggerPtr;
} 
LoggerObject;

extern PyTypeObject LoggerType;
PyMODINIT_FUNC PyInit_Logger(void);
