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

    std::string GetLocalTime()
    {
        std::stringstream bufferStream;
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        bufferStream << std::put_time(std::localtime(&time), "%M:%S");
        bufferStream << fmt::format(".{:03}", ms.count());
        return fmt::format("[{}]", bufferStream.str());
    }

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

    std::string GetPluginName() 
    { 
        const auto toUpper = [](const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::toupper);
            return result;
        };

        return toUpper(pluginName);    
    }

public:
    BackendLogger(const std::string& pluginName) : pluginName(pluginName)
    {
        this->filename = (SystemIO::GetInstallPath() / "ext" / "data" / "logs" / fmt::format("{}_log.txt", pluginName)).generic_string();
        file.open(filename, std::ios::app);

        file << fmt::format("\n\n\n--------------------------------- [{}] ---------------------------------\n", GetLocalDateStr());
        file.flush();
    }

    void Log(const std::string& message) 
    {        
        fmt::print("{} ", GetLocalTime());
        std::string formatted = fmt::format("{} ", GetPluginName());
  
        fmt::print("\033[1m\033[34m{}\033[0m\033[0m", formatted);
        fmt::print(fmt::format("{}\n", message));
        file << formatted << message << "\n";
        file.flush();
    }

    void Warn(const std::string& message) 
    {
        std::string formatted = fmt::format("{}{}{}\n", GetLocalTime(), fmt::format(" {} ", GetPluginName()), message);

        fmt::print(fg(fmt::color::orange), formatted, "\n");
        file << formatted;
        file.flush();
    }

    void Error(const std::string& message) 
    {
        std::string formatted = fmt::format("{}{}{}\n", GetLocalTime(), fmt::format(" {} ", GetPluginName()), message);

        fmt::print(fg(fmt::color::red), formatted, "\n");
        file << formatted;
        file.flush();
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
